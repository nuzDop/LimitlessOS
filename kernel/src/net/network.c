/*
 * Network Stack Implementation
 * TCP/IP protocol suite
 */

#include "kernel.h"
#include "microkernel.h"
#include "net.h"

/* Global network stack */
static net_stack_t net_stack = {0};
static net_stats_t net_stats = {0};

/* Byte order conversion */
uint16_t htons(uint16_t n) {
    return ((n & 0xFF) << 8) | ((n & 0xFF00) >> 8);
}

uint32_t htonl(uint32_t n) {
    return ((n & 0xFF) << 24) | ((n & 0xFF00) << 8) |
           ((n & 0xFF0000) >> 8) | ((n & 0xFF000000) >> 24);
}

uint16_t ntohs(uint16_t n) {
    return htons(n);
}

uint32_t ntohl(uint32_t n) {
    return htonl(n);
}

/* MAC address utilities */
bool mac_addr_equals(const mac_addr_t* a, const mac_addr_t* b) {
    for (int i = 0; i < 6; i++) {
        if (a->addr[i] != b->addr[i]) {
            return false;
        }
    }
    return true;
}

void mac_addr_copy(mac_addr_t* dst, const mac_addr_t* src) {
    for (int i = 0; i < 6; i++) {
        dst->addr[i] = src->addr[i];
    }
}

/* IPv4 address utilities */
bool ipv4_addr_equals(const ipv4_addr_t* a, const ipv4_addr_t* b) {
    for (int i = 0; i < 4; i++) {
        if (a->addr[i] != b->addr[i]) {
            return false;
        }
    }
    return true;
}

void ipv4_addr_copy(ipv4_addr_t* dst, const ipv4_addr_t* src) {
    for (int i = 0; i < 4; i++) {
        dst->addr[i] = src->addr[i];
    }
}

/* IP checksum calculation */
uint16_t ip_checksum(const void* data, size_t length) {
    const uint16_t* ptr = (const uint16_t*)data;
    uint32_t sum = 0;

    while (length > 1) {
        sum += *ptr++;
        length -= 2;
    }

    if (length > 0) {
        sum += *(const uint8_t*)ptr;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~sum;
}

/* Initialize network stack */
status_t net_init(void) {
    if (net_stack.initialized) {
        return STATUS_EXISTS;
    }

    KLOG_INFO("NET", "Initializing network stack");

    /* Initialize interfaces */
    for (uint32_t i = 0; i < NET_MAX_INTERFACES; i++) {
        net_stack.interfaces[i].id = i;
        net_stack.interfaces[i].up = false;
        net_stack.interfaces[i].lock = 0;
    }

    net_stack.interface_count = 0;

    /* Initialize ARP cache */
    for (uint32_t i = 0; i < ARP_CACHE_SIZE; i++) {
        net_stack.arp_cache[i].valid = false;
    }

    /* Initialize sockets */
    for (uint32_t i = 0; i < SOCKET_MAX; i++) {
        net_stack.sockets[i].id = i;
        net_stack.sockets[i].bound = false;
        net_stack.sockets[i].connected = false;
        net_stack.sockets[i].lock = 0;
    }

    net_stack.socket_count = 0;
    net_stack.lock = 0;
    net_stack.initialized = true;

    KLOG_INFO("NET", "Network stack initialized");
    return STATUS_OK;
}

/* Shutdown network stack */
void net_shutdown(void) {
    if (!net_stack.initialized) {
        return;
    }

    /* Close all sockets */
    for (uint32_t i = 0; i < SOCKET_MAX; i++) {
        if (net_stack.sockets[i].bound || net_stack.sockets[i].connected) {
            net_socket_close(&net_stack.sockets[i]);
        }
    }

    /* Bring down all interfaces */
    for (uint32_t i = 0; i < NET_MAX_INTERFACES; i++) {
        if (net_stack.interfaces[i].up) {
            net_interface_down(&net_stack.interfaces[i]);
        }
    }

    net_stack.initialized = false;
    KLOG_INFO("NET", "Network stack shutdown");
}

/* Register network interface */
status_t net_register_interface(net_interface_t* iface) {
    if (!iface || !iface->send) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&net_stack.lock, 1);

    if (net_stack.interface_count >= NET_MAX_INTERFACES) {
        __sync_lock_release(&net_stack.lock);
        return STATUS_NOMEM;
    }

    uint8_t id = net_stack.interface_count;
    net_stack.interfaces[id] = *iface;
    net_stack.interfaces[id].id = id;
    net_stack.interface_count++;

    __sync_lock_release(&net_stack.lock);

    KLOG_INFO("NET", "Registered interface %s (id=%d)", iface->name, id);
    return STATUS_OK;
}

/* Get network interface */
net_interface_t* net_get_interface(uint8_t id) {
    if (id >= NET_MAX_INTERFACES) {
        return NULL;
    }

    return &net_stack.interfaces[id];
}

/* Bring interface up */
status_t net_interface_up(net_interface_t* iface) {
    if (!iface) {
        return STATUS_INVALID;
    }

    iface->up = true;
    KLOG_INFO("NET", "Interface %s is up", iface->name);
    return STATUS_OK;
}

/* Bring interface down */
status_t net_interface_down(net_interface_t* iface) {
    if (!iface) {
        return STATUS_INVALID;
    }

    iface->up = false;
    KLOG_INFO("NET", "Interface %s is down", iface->name);
    return STATUS_OK;
}

/* Send Ethernet packet */
status_t eth_send_packet(net_interface_t* iface, const mac_addr_t* dst_mac,
                         uint16_t ethertype, const void* payload, size_t length) {
    if (!iface || !dst_mac || !payload || !iface->up) {
        return STATUS_INVALID;
    }

    /* Allocate buffer for frame */
    uint8_t frame[NET_BUF_SIZE];
    eth_header_t* eth = (eth_header_t*)frame;

    /* Build Ethernet header */
    mac_addr_copy(&eth->dst, dst_mac);
    mac_addr_copy(&eth->src, &iface->mac);
    eth->ethertype = htons(ethertype);

    /* Copy payload */
    size_t total_length = sizeof(eth_header_t) + length;
    if (total_length > NET_BUF_SIZE) {
        return STATUS_INVALID;
    }

    uint8_t* payload_ptr = frame + sizeof(eth_header_t);
    for (size_t i = 0; i < length; i++) {
        payload_ptr[i] = ((const uint8_t*)payload)[i];
    }

    /* Send through driver */
    status_t result = iface->send(iface, frame, total_length);

    if (SUCCESS(result)) {
        iface->tx_packets++;
        iface->tx_bytes += total_length;
        net_stats.total_tx_packets++;
        net_stats.total_tx_bytes += total_length;
    } else {
        iface->tx_errors++;
    }

    return result;
}

/* ARP lookup */
status_t arp_lookup(const ipv4_addr_t* ip, mac_addr_t* out_mac) {
    if (!ip || !out_mac) {
        return STATUS_INVALID;
    }

    for (uint32_t i = 0; i < ARP_CACHE_SIZE; i++) {
        if (net_stack.arp_cache[i].valid &&
            ipv4_addr_equals(&net_stack.arp_cache[i].ip, ip)) {
            mac_addr_copy(out_mac, &net_stack.arp_cache[i].mac);
            return STATUS_OK;
        }
    }

    return STATUS_NOTFOUND;
}

/* Add ARP entry */
status_t arp_add_entry(const ipv4_addr_t* ip, const mac_addr_t* mac) {
    if (!ip || !mac) {
        return STATUS_INVALID;
    }

    /* Find free slot */
    for (uint32_t i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!net_stack.arp_cache[i].valid) {
            ipv4_addr_copy(&net_stack.arp_cache[i].ip, ip);
            mac_addr_copy(&net_stack.arp_cache[i].mac, mac);
            net_stack.arp_cache[i].valid = true;
            net_stack.arp_cache[i].timestamp = 0;
            return STATUS_OK;
        }
    }

    return STATUS_NOMEM;
}

/* Send ARP request */
status_t arp_request(net_interface_t* iface, const ipv4_addr_t* target_ip) {
    if (!iface || !target_ip) {
        return STATUS_INVALID;
    }

    arp_packet_t arp;
    arp.hw_type = htons(ARP_HW_ETHERNET);
    arp.proto_type = htons(ETHERTYPE_IP);
    arp.hw_len = 6;
    arp.proto_len = 4;
    arp.opcode = htons(ARP_OP_REQUEST);

    mac_addr_copy(&arp.sender_mac, &iface->mac);
    ipv4_addr_copy(&arp.sender_ip, &iface->ip);

    /* Broadcast MAC for target */
    for (int i = 0; i < 6; i++) {
        arp.target_mac.addr[i] = 0;
    }
    ipv4_addr_copy(&arp.target_ip, target_ip);

    /* Broadcast */
    mac_addr_t broadcast = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};

    net_stats.arp_requests++;
    return eth_send_packet(iface, &broadcast, ETHERTYPE_ARP, &arp, sizeof(arp));
}

/* Send IP packet */
status_t ip_send_packet(net_interface_t* iface, const ipv4_addr_t* dst_ip,
                        uint8_t protocol, const void* payload, size_t length) {
    if (!iface || !dst_ip || !payload) {
        return STATUS_INVALID;
    }

    /* Look up MAC address */
    mac_addr_t dst_mac;
    status_t result = arp_lookup(dst_ip, &dst_mac);
    if (FAILED(result)) {
        /* Send ARP request and fail for now */
        arp_request(iface, dst_ip);
        return STATUS_NOTFOUND;
    }

    /* Build IP header */
    uint8_t packet[NET_BUF_SIZE];
    ipv4_header_t* ip = (ipv4_header_t*)packet;

    ip->version_ihl = 0x45;  // IPv4, IHL=5 (20 bytes)
    ip->dscp_ecn = 0;
    ip->total_length = htons(sizeof(ipv4_header_t) + length);
    ip->identification = 0;
    ip->flags_fragment = 0;
    ip->ttl = 64;
    ip->protocol = protocol;
    ip->checksum = 0;
    ipv4_addr_copy(&ip->src, &iface->ip);
    ipv4_addr_copy(&ip->dst, dst_ip);

    ip->checksum = ip_checksum(ip, sizeof(ipv4_header_t));

    /* Copy payload */
    uint8_t* payload_ptr = packet + sizeof(ipv4_header_t);
    for (size_t i = 0; i < length; i++) {
        payload_ptr[i] = ((const uint8_t*)payload)[i];
    }

    return eth_send_packet(iface, &dst_mac, ETHERTYPE_IP, packet,
                          sizeof(ipv4_header_t) + length);
}

/* Send ICMP echo request (ping) */
status_t icmp_send_echo_request(const ipv4_addr_t* dst_ip, uint16_t id, uint16_t seq) {
    if (!dst_ip) {
        return STATUS_INVALID;
    }

    /* Get first interface */
    net_interface_t* iface = net_get_interface(0);
    if (!iface || !iface->up) {
        return STATUS_ERROR;
    }

    icmp_header_t icmp;
    icmp.type = ICMP_TYPE_ECHO_REQUEST;
    icmp.code = 0;
    icmp.checksum = 0;
    icmp.id = htons(id);
    icmp.sequence = htons(seq);

    icmp.checksum = ip_checksum(&icmp, sizeof(icmp));

    net_stats.icmp_echo_requests++;
    return ip_send_packet(iface, dst_ip, IP_PROTO_ICMP, &icmp, sizeof(icmp));
}

/* Send ICMP echo reply */
status_t icmp_send_echo_reply(net_interface_t* iface, const ipv4_addr_t* dst_ip,
                               uint16_t id, uint16_t seq, const void* data, size_t length) {
    if (!iface || !dst_ip) {
        return STATUS_INVALID;
    }

    uint8_t packet[NET_BUF_SIZE];
    icmp_header_t* icmp = (icmp_header_t*)packet;

    icmp->type = ICMP_TYPE_ECHO_REPLY;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->id = id;
    icmp->sequence = seq;

    /* Copy echo data */
    if (data && length > 0) {
        size_t copy_len = (length < (NET_BUF_SIZE - sizeof(icmp_header_t))) ?
                          length : (NET_BUF_SIZE - sizeof(icmp_header_t));
        uint8_t* data_ptr = packet + sizeof(icmp_header_t);
        for (size_t i = 0; i < copy_len; i++) {
            data_ptr[i] = ((const uint8_t*)data)[i];
        }
        length = copy_len;
    }

    icmp->checksum = ip_checksum(packet, sizeof(icmp_header_t) + length);

    net_stats.icmp_echo_replies++;
    return ip_send_packet(iface, dst_ip, IP_PROTO_ICMP, packet,
                          sizeof(icmp_header_t) + length);
}

/* Process received Ethernet frame */
static void net_process_ethernet(net_interface_t* iface, const uint8_t* data, size_t length) {
    if (length < sizeof(eth_header_t)) {
        return;
    }

    const eth_header_t* eth = (const eth_header_t*)data;
    uint16_t ethertype = ntohs(eth->ethertype);
    const uint8_t* payload = data + sizeof(eth_header_t);
    size_t payload_length = length - sizeof(eth_header_t);

    if (ethertype == ETHERTYPE_ARP) {
        /* Process ARP */
        if (payload_length < sizeof(arp_packet_t)) {
            return;
        }

        const arp_packet_t* arp = (const arp_packet_t*)payload;
        uint16_t opcode = ntohs(arp->opcode);

        if (opcode == ARP_OP_REQUEST) {
            net_stats.arp_requests++;
            /* Check if request is for us */
            if (ipv4_addr_equals(&arp->target_ip, &iface->ip)) {
                /* Send ARP reply */
                arp_packet_t reply;
                reply.hw_type = htons(ARP_HW_ETHERNET);
                reply.proto_type = htons(ETHERTYPE_IP);
                reply.hw_len = 6;
                reply.proto_len = 4;
                reply.opcode = htons(ARP_OP_REPLY);
                mac_addr_copy(&reply.sender_mac, &iface->mac);
                ipv4_addr_copy(&reply.sender_ip, &iface->ip);
                mac_addr_copy(&reply.target_mac, &arp->sender_mac);
                ipv4_addr_copy(&reply.target_ip, &arp->sender_ip);

                eth_send_packet(iface, &arp->sender_mac, ETHERTYPE_ARP, &reply, sizeof(reply));
                net_stats.arp_replies++;
            }
        } else if (opcode == ARP_OP_REPLY) {
            net_stats.arp_replies++;
            /* Add to ARP cache */
            arp_add_entry(&arp->sender_ip, &arp->sender_mac);
        }
    } else if (ethertype == ETHERTYPE_IP) {
        /* Process IP */
        if (payload_length < sizeof(ipv4_header_t)) {
            return;
        }

        const ipv4_header_t* ip = (const ipv4_header_t*)payload;
        uint8_t protocol = ip->protocol;
        const uint8_t* ip_payload = payload + sizeof(ipv4_header_t);
        size_t ip_payload_length = ntohs(ip->total_length) - sizeof(ipv4_header_t);

        if (protocol == IP_PROTO_ICMP) {
            /* Process ICMP */
            if (ip_payload_length < sizeof(icmp_header_t)) {
                return;
            }

            const icmp_header_t* icmp = (const icmp_header_t*)ip_payload;

            if (icmp->type == ICMP_TYPE_ECHO_REQUEST) {
                /* Send echo reply */
                const uint8_t* echo_data = ip_payload + sizeof(icmp_header_t);
                size_t echo_length = ip_payload_length - sizeof(icmp_header_t);
                icmp_send_echo_reply(iface, &ip->src, icmp->id, icmp->sequence,
                                    echo_data, echo_length);
            }
        } else if (protocol == IP_PROTO_UDP) {
            net_stats.udp_packets++;
            /* UDP processing would go here */
        } else if (protocol == IP_PROTO_TCP) {
            net_stats.tcp_packets++;
            /* TCP processing would go here */
        }
    }
}

/* Receive packet (called by driver) */
status_t net_receive_packet(net_interface_t* iface, const void* data, size_t length) {
    if (!iface || !data || length == 0) {
        return STATUS_INVALID;
    }

    iface->rx_packets++;
    iface->rx_bytes += length;
    net_stats.total_rx_packets++;
    net_stats.total_rx_bytes += length;

    /* Process Ethernet frame */
    net_process_ethernet(iface, (const uint8_t*)data, length);

    return STATUS_OK;
}

/* Create socket */
status_t net_socket_create(socket_type_t type, uint8_t protocol, socket_t** out_sock) {
    if (!out_sock) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&net_stack.lock, 1);

    /* Find free socket */
    socket_t* sock = NULL;
    for (uint32_t i = 0; i < SOCKET_MAX; i++) {
        if (!net_stack.sockets[i].bound && !net_stack.sockets[i].connected) {
            sock = &net_stack.sockets[i];
            break;
        }
    }

    if (!sock) {
        __sync_lock_release(&net_stack.lock);
        return STATUS_NOMEM;
    }

    sock->type = type;
    sock->protocol = protocol;
    sock->bound = false;
    sock->connected = false;
    sock->tcp_state = TCP_STATE_CLOSED;

    net_stack.socket_count++;

    __sync_lock_release(&net_stack.lock);

    *out_sock = sock;
    return STATUS_OK;
}

/* Bind socket */
status_t net_socket_bind(socket_t* sock, const ipv4_addr_t* ip, uint16_t port) {
    if (!sock) {
        return STATUS_INVALID;
    }

    if (ip) {
        ipv4_addr_copy(&sock->local_ip, ip);
    }
    sock->local_port = port;
    sock->bound = true;

    return STATUS_OK;
}

/* Connect socket */
status_t net_socket_connect(socket_t* sock, const ipv4_addr_t* ip, uint16_t port) {
    if (!sock || !ip) {
        return STATUS_INVALID;
    }

    ipv4_addr_copy(&sock->remote_ip, ip);
    sock->remote_port = port;
    sock->connected = true;

    if (sock->type == SOCKET_TYPE_TCP) {
        return tcp_connect(sock, ip, port);
    }

    return STATUS_OK;
}

/* Close socket */
status_t net_socket_close(socket_t* sock) {
    if (!sock) {
        return STATUS_INVALID;
    }

    if (sock->type == SOCKET_TYPE_TCP) {
        tcp_close(sock);
    }

    sock->bound = false;
    sock->connected = false;
    net_stack.socket_count--;

    return STATUS_OK;
}

/* Get statistics */
status_t net_get_stats(net_stats_t* stats) {
    if (!stats) {
        return STATUS_INVALID;
    }

    *stats = net_stats;
    return STATUS_OK;
}

/* Stub implementations for TCP (to be fully implemented) */
status_t tcp_connect(socket_t* sock, const ipv4_addr_t* dst_ip, uint16_t dst_port) {
    if (!sock || !dst_ip) {
        return STATUS_INVALID;
    }

    sock->tcp_state = TCP_STATE_SYN_SENT;
    /* Would send SYN packet here */
    return STATUS_OK;
}

status_t tcp_listen(socket_t* sock, uint16_t port) {
    if (!sock) {
        return STATUS_INVALID;
    }

    sock->tcp_state = TCP_STATE_LISTEN;
    sock->local_port = port;
    return STATUS_OK;
}

status_t tcp_accept(socket_t* listen_sock, socket_t** out_sock) {
    /* Would accept incoming connection */
    return STATUS_NOSUPPORT;
}

status_t tcp_send(socket_t* sock, const void* data, size_t length) {
    /* Would send TCP data */
    return STATUS_NOSUPPORT;
}

ssize_t tcp_recv(socket_t* sock, void* buffer, size_t length) {
    /* Would receive TCP data */
    return -1;
}

status_t tcp_close(socket_t* sock) {
    if (!sock) {
        return STATUS_INVALID;
    }

    sock->tcp_state = TCP_STATE_CLOSED;
    return STATUS_OK;
}

/* UDP implementations (simplified) */
status_t udp_send(socket_t* sock, const void* data, size_t length) {
    if (!sock || !data || !sock->connected) {
        return STATUS_INVALID;
    }

    uint8_t packet[NET_BUF_SIZE];
    udp_header_t* udp = (udp_header_t*)packet;

    udp->src_port = htons(sock->local_port);
    udp->dst_port = htons(sock->remote_port);
    udp->length = htons(sizeof(udp_header_t) + length);
    udp->checksum = 0;

    /* Copy data */
    uint8_t* data_ptr = packet + sizeof(udp_header_t);
    for (size_t i = 0; i < length && i < (NET_BUF_SIZE - sizeof(udp_header_t)); i++) {
        data_ptr[i] = ((const uint8_t*)data)[i];
    }

    /* Get interface */
    net_interface_t* iface = net_get_interface(0);
    if (!iface) {
        return STATUS_ERROR;
    }

    return ip_send_packet(iface, &sock->remote_ip, IP_PROTO_UDP, packet,
                          sizeof(udp_header_t) + length);
}

ssize_t udp_recv(socket_t* sock, void* buffer, size_t length) {
    /* Would receive from RX queue */
    return -1;
}
