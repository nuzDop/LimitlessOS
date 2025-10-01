#ifndef LIMITLESS_NET_H
#define LIMITLESS_NET_H

/*
 * Network Stack (TCP/IP)
 * Ethernet, ARP, IP, ICMP, UDP, TCP
 */

#include "kernel.h"

/* Ethernet frame types */
#define ETHERTYPE_IP   0x0800
#define ETHERTYPE_ARP  0x0806
#define ETHERTYPE_IPV6 0x86DD

/* IP protocol numbers */
#define IP_PROTO_ICMP  1
#define IP_PROTO_TCP   6
#define IP_PROTO_UDP   17

/* MAC address */
typedef struct mac_addr {
    uint8_t addr[6];
} PACKED mac_addr_t;

/* IPv4 address */
typedef struct ipv4_addr {
    uint8_t addr[4];
} PACKED ipv4_addr_t;

/* Ethernet header */
typedef struct eth_header {
    mac_addr_t dst;
    mac_addr_t src;
    uint16_t ethertype;
} PACKED eth_header_t;

/* ARP packet */
#define ARP_HW_ETHERNET 1
#define ARP_OP_REQUEST  1
#define ARP_OP_REPLY    2

typedef struct arp_packet {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t hw_len;
    uint8_t proto_len;
    uint16_t opcode;
    mac_addr_t sender_mac;
    ipv4_addr_t sender_ip;
    mac_addr_t target_mac;
    ipv4_addr_t target_ip;
} PACKED arp_packet_t;

/* IPv4 header */
typedef struct ipv4_header {
    uint8_t version_ihl;      // Version (4 bits) + IHL (4 bits)
    uint8_t dscp_ecn;         // DSCP (6 bits) + ECN (2 bits)
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_fragment;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    ipv4_addr_t src;
    ipv4_addr_t dst;
} PACKED ipv4_header_t;

/* ICMP header */
#define ICMP_TYPE_ECHO_REPLY   0
#define ICMP_TYPE_ECHO_REQUEST 8

typedef struct icmp_header {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
} PACKED icmp_header_t;

/* UDP header */
typedef struct udp_header {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} PACKED udp_header_t;

/* TCP header */
typedef struct tcp_header {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t data_offset_reserved;
    uint8_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_ptr;
} PACKED tcp_header_t;

/* TCP flags */
#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_FLAG_URG 0x20

/* TCP states */
typedef enum {
    TCP_STATE_CLOSED = 0,
    TCP_STATE_LISTEN,
    TCP_STATE_SYN_SENT,
    TCP_STATE_SYN_RECEIVED,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_FIN_WAIT_1,
    TCP_STATE_FIN_WAIT_2,
    TCP_STATE_CLOSE_WAIT,
    TCP_STATE_CLOSING,
    TCP_STATE_LAST_ACK,
    TCP_STATE_TIME_WAIT,
} tcp_state_t;

/* Network buffer */
#define NET_BUF_SIZE 2048

typedef struct net_buffer {
    uint8_t data[NET_BUF_SIZE];
    size_t length;
    size_t offset;
    struct list_head list_node;
} net_buffer_t;

/* Network interface */
#define NET_MAX_INTERFACES 8

typedef struct net_interface {
    uint8_t id;
    char name[16];
    mac_addr_t mac;
    ipv4_addr_t ip;
    ipv4_addr_t netmask;
    ipv4_addr_t gateway;
    bool up;
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_errors;
    uint64_t tx_errors;

    /* Driver callbacks */
    status_t (*send)(struct net_interface* iface, const void* data, size_t length);
    void* driver_data;

    uint32_t lock;
} net_interface_t;

/* ARP cache entry */
#define ARP_CACHE_SIZE 256

typedef struct arp_entry {
    ipv4_addr_t ip;
    mac_addr_t mac;
    uint64_t timestamp;
    bool valid;
} arp_entry_t;

/* Socket */
#define SOCKET_MAX 1024

typedef enum {
    SOCKET_TYPE_RAW = 0,
    SOCKET_TYPE_UDP,
    SOCKET_TYPE_TCP,
} socket_type_t;

typedef struct socket {
    uint32_t id;
    socket_type_t type;
    uint8_t protocol;

    ipv4_addr_t local_ip;
    uint16_t local_port;
    ipv4_addr_t remote_ip;
    uint16_t remote_port;

    tcp_state_t tcp_state;
    uint32_t tcp_seq;
    uint32_t tcp_ack;

    struct list_head rx_queue;
    struct list_head tx_queue;

    bool bound;
    bool connected;
    uint32_t lock;
} socket_t;

/* Network stack state */
typedef struct net_stack {
    net_interface_t interfaces[NET_MAX_INTERFACES];
    uint32_t interface_count;

    arp_entry_t arp_cache[ARP_CACHE_SIZE];

    socket_t sockets[SOCKET_MAX];
    uint32_t socket_count;

    bool initialized;
    uint32_t lock;
} net_stack_t;

/* Network stack API */
status_t net_init(void);
void net_shutdown(void);

/* Interface management */
status_t net_register_interface(net_interface_t* iface);
net_interface_t* net_get_interface(uint8_t id);
status_t net_interface_up(net_interface_t* iface);
status_t net_interface_down(net_interface_t* iface);

/* Packet reception (called by drivers) */
status_t net_receive_packet(net_interface_t* iface, const void* data, size_t length);

/* Ethernet layer */
status_t eth_send_packet(net_interface_t* iface, const mac_addr_t* dst_mac,
                         uint16_t ethertype, const void* payload, size_t length);

/* ARP layer */
status_t arp_request(net_interface_t* iface, const ipv4_addr_t* target_ip);
status_t arp_lookup(const ipv4_addr_t* ip, mac_addr_t* out_mac);
status_t arp_add_entry(const ipv4_addr_t* ip, const mac_addr_t* mac);

/* IP layer */
status_t ip_send_packet(net_interface_t* iface, const ipv4_addr_t* dst_ip,
                        uint8_t protocol, const void* payload, size_t length);
uint16_t ip_checksum(const void* data, size_t length);

/* ICMP layer */
status_t icmp_send_echo_request(const ipv4_addr_t* dst_ip, uint16_t id, uint16_t seq);
status_t icmp_send_echo_reply(net_interface_t* iface, const ipv4_addr_t* dst_ip,
                               uint16_t id, uint16_t seq, const void* data, size_t length);

/* UDP layer */
status_t udp_send(socket_t* sock, const void* data, size_t length);
ssize_t udp_recv(socket_t* sock, void* buffer, size_t length);

/* TCP layer */
status_t tcp_connect(socket_t* sock, const ipv4_addr_t* dst_ip, uint16_t dst_port);
status_t tcp_listen(socket_t* sock, uint16_t port);
status_t tcp_accept(socket_t* listen_sock, socket_t** out_sock);
status_t tcp_send(socket_t* sock, const void* data, size_t length);
ssize_t tcp_recv(socket_t* sock, void* buffer, size_t length);
status_t tcp_close(socket_t* sock);

/* Socket API */
status_t net_socket_create(socket_type_t type, uint8_t protocol, socket_t** out_sock);
status_t net_socket_bind(socket_t* sock, const ipv4_addr_t* ip, uint16_t port);
status_t net_socket_connect(socket_t* sock, const ipv4_addr_t* ip, uint16_t port);
status_t net_socket_close(socket_t* sock);

/* Utility functions */
bool mac_addr_equals(const mac_addr_t* a, const mac_addr_t* b);
bool ipv4_addr_equals(const ipv4_addr_t* a, const ipv4_addr_t* b);
void mac_addr_copy(mac_addr_t* dst, const mac_addr_t* src);
void ipv4_addr_copy(ipv4_addr_t* dst, const ipv4_addr_t* src);
uint16_t htons(uint16_t n);
uint32_t htonl(uint32_t n);
uint16_t ntohs(uint16_t n);
uint32_t ntohl(uint32_t n);

/* Statistics */
typedef struct net_stats {
    uint64_t total_rx_packets;
    uint64_t total_tx_packets;
    uint64_t total_rx_bytes;
    uint64_t total_tx_bytes;
    uint64_t arp_requests;
    uint64_t arp_replies;
    uint64_t icmp_echo_requests;
    uint64_t icmp_echo_replies;
    uint64_t udp_packets;
    uint64_t tcp_packets;
} net_stats_t;

status_t net_get_stats(net_stats_t* stats);

#endif /* LIMITLESS_NET_H */
