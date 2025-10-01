/*
 * WiFi Core Driver Framework - Phase 1 Stub
 * 802.11a/b/g/n/ac/ax (WiFi 6/6E) support
 * 
 * Supports:
 * - Network scanning
 * - WPA2-PSK and WPA3 authentication
 * - Power management
 * - Multiple SSIDs
 */

#include "kernel.h"

/* WiFi Standards */
typedef enum {
    WIFI_STD_80211A = 0,    // 5 GHz, 54 Mbps
    WIFI_STD_80211B,        // 2.4 GHz, 11 Mbps
    WIFI_STD_80211G,        // 2.4 GHz, 54 Mbps
    WIFI_STD_80211N,        // 2.4/5 GHz, 600 Mbps (WiFi 4)
    WIFI_STD_80211AC,       // 5 GHz, 3.5 Gbps (WiFi 5)
    WIFI_STD_80211AX,       // 2.4/5/6 GHz, 9.6 Gbps (WiFi 6/6E)
} wifi_standard_t;

/* WiFi Security Types */
typedef enum {
    WIFI_SEC_NONE = 0,
    WIFI_SEC_WEP,
    WIFI_SEC_WPA_PSK,
    WIFI_SEC_WPA2_PSK,
    WIFI_SEC_WPA3_PSK,
    WIFI_SEC_WPA2_ENTERPRISE,
    WIFI_SEC_WPA3_ENTERPRISE,
} wifi_security_t;

/* WiFi Network (SSID) */
typedef struct wifi_network {
    char ssid[33];              // Network name (max 32 chars + null)
    uint8_t bssid[6];           // MAC address of AP
    uint8_t channel;            // Channel number
    int8_t rssi;                // Signal strength (dBm)
    wifi_security_t security;   // Security type
    wifi_standard_t standard;   // WiFi standard
    bool hidden;                // Hidden SSID
} wifi_network_t;

/* WiFi Connection State */
typedef enum {
    WIFI_STATE_DISCONNECTED = 0,
    WIFI_STATE_SCANNING,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_AUTHENTICATING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_DISCONNECTING,
} wifi_state_t;

/* WiFi Device */
typedef struct wifi_device {
    uint32_t id;
    uint16_t vendor_id;
    uint16_t device_id;
    char name[64];
    uint8_t mac_address[6];
    
    /* Capabilities */
    wifi_standard_t max_standard;
    bool dual_band;             // 2.4 + 5 GHz
    bool tri_band;              // 2.4 + 5 + 6 GHz
    uint8_t max_spatial_streams; // MIMO streams
    
    /* State */
    wifi_state_t state;
    wifi_network_t* connected_network;
    
    /* Scan results */
    wifi_network_t* scan_results;
    uint32_t scan_count;
    
    /* Statistics */
    uint64_t tx_packets;
    uint64_t rx_packets;
    uint64_t tx_bytes;
    uint64_t rx_bytes;
    uint64_t tx_errors;
    uint64_t rx_errors;
    
    /* Driver callbacks */
    status_t (*init)(struct wifi_device*);
    status_t (*scan)(struct wifi_device*);
    status_t (*connect)(struct wifi_device*, wifi_network_t*, const char* password);
    status_t (*disconnect)(struct wifi_device*);
    status_t (*send)(struct wifi_device*, void* packet, uint32_t length);
    status_t (*set_power_mode)(struct wifi_device*, bool low_power);
} wifi_device_t;

/* Global WiFi state */
static struct {
    bool initialized;
    wifi_device_t devices[4];   // Support up to 4 WiFi adapters
    uint32_t device_count;
    wifi_device_t* primary;     // Primary WiFi adapter
    uint32_t lock;
} wifi_state = {0};

/* Initialize WiFi subsystem */
status_t wifi_init(void) {
    if (wifi_state.initialized) {
        return STATUS_EXISTS;
    }
    
    wifi_state.device_count = 0;
    wifi_state.primary = NULL;
    wifi_state.lock = 0;
    wifi_state.initialized = true;
    
    KLOG_INFO("WiFi", "WiFi subsystem initialized");
    
    /* TODO Phase 1: Enumerate WiFi adapters */
    /* - Scan PCI for network controllers (class 0x02) */
    /* - Detect chipset (Intel, Realtek, Broadcom, etc.) */
    /* - Load firmware if required */
    /* - Initialize hardware */
    
    return STATUS_OK;
}

/* Register WiFi device */
status_t wifi_register_device(wifi_device_t* device) {
    if (!wifi_state.initialized || !device) {
        return STATUS_INVALID;
    }
    
    if (wifi_state.device_count >= 4) {
        return STATUS_NOMEM;
    }
    
    wifi_state.devices[wifi_state.device_count] = *device;
    wifi_state.device_count++;
    
    /* Set as primary if first device */
    if (!wifi_state.primary) {
        wifi_state.primary = &wifi_state.devices[0];
    }
    
    KLOG_INFO("WiFi", "Registered WiFi adapter: %s (MAC=%02x:%02x:%02x:%02x:%02x:%02x)",
              device->name,
              device->mac_address[0], device->mac_address[1], device->mac_address[2],
              device->mac_address[3], device->mac_address[4], device->mac_address[5]);
    
    return STATUS_OK;
}

/* Scan for WiFi networks */
status_t wifi_scan(wifi_device_t* device) {
    if (!wifi_state.initialized || !device) {
        return STATUS_INVALID;
    }
    
    if (device->scan) {
        device->state = WIFI_STATE_SCANNING;
        status_t result = device->scan(device);
        if (SUCCESS(result)) {
            device->state = WIFI_STATE_DISCONNECTED;
        }
        return result;
    }
    
    KLOG_WARN("WiFi", "Scan not implemented for device %s", device->name);
    return STATUS_NOSUPPORT;
}

/* Connect to WiFi network */
status_t wifi_connect(wifi_device_t* device, const char* ssid, const char* password) {
    if (!wifi_state.initialized || !device || !ssid) {
        return STATUS_INVALID;
    }
    
    /* Find network in scan results */
    wifi_network_t* network = NULL;
    for (uint32_t i = 0; i < device->scan_count; i++) {
        if (strcmp(device->scan_results[i].ssid, ssid) == 0) {
            network = &device->scan_results[i];
            break;
        }
    }
    
    if (!network) {
        KLOG_ERROR("WiFi", "Network '%s' not found in scan results", ssid);
        return STATUS_NOTFOUND;
    }
    
    if (device->connect) {
        device->state = WIFI_STATE_CONNECTING;
        status_t result = device->connect(device, network, password);
        if (SUCCESS(result)) {
            device->state = WIFI_STATE_CONNECTED;
            device->connected_network = network;
            KLOG_INFO("WiFi", "Connected to '%s' (RSSI=%d dBm)",
                     ssid, network->rssi);
        } else {
            device->state = WIFI_STATE_DISCONNECTED;
        }
        return result;
    }
    
    KLOG_WARN("WiFi", "Connect not implemented for device %s", device->name);
    return STATUS_NOSUPPORT;
}

/* Disconnect from WiFi network */
status_t wifi_disconnect(wifi_device_t* device) {
    if (!wifi_state.initialized || !device) {
        return STATUS_INVALID;
    }
    
    if (device->disconnect) {
        device->state = WIFI_STATE_DISCONNECTING;
        status_t result = device->disconnect(device);
        device->state = WIFI_STATE_DISCONNECTED;
        device->connected_network = NULL;
        return result;
    }
    
    return STATUS_NOSUPPORT;
}

/* Send packet */
status_t wifi_send(wifi_device_t* device, void* packet, uint32_t length) {
    if (!wifi_state.initialized || !device || !packet) {
        return STATUS_INVALID;
    }
    
    if (device->state != WIFI_STATE_CONNECTED) {
        return STATUS_INVALID;
    }
    
    if (device->send) {
        device->tx_packets++;
        device->tx_bytes += length;
        return device->send(device, packet, length);
    }
    
    return STATUS_NOSUPPORT;
}

/* Get primary WiFi device */
wifi_device_t* wifi_get_primary(void) {
    if (!wifi_state.initialized) {
        return NULL;
    }
    return wifi_state.primary;
}

/* Get WiFi device by index */
wifi_device_t* wifi_get_device(uint32_t index) {
    if (!wifi_state.initialized || index >= wifi_state.device_count) {
        return NULL;
    }
    return &wifi_state.devices[index];
}

/* TODO Phase 1: Chipset-specific driver initialization stubs */

/* Intel WiFi (iwlwifi) */
status_t intel_wifi_init(wifi_device_t* device) {
    KLOG_INFO("WiFi", "Intel WiFi adapter detected: 0x%04x", device->device_id);
    /* TODO: Initialize Intel WiFi (AX200, AX210, etc.) */
    return STATUS_OK;
}

/* Realtek WiFi (rtw88/rtw89) */
status_t realtek_wifi_init(wifi_device_t* device) {
    KLOG_INFO("WiFi", "Realtek WiFi adapter detected: 0x%04x", device->device_id);
    /* TODO: Initialize Realtek WiFi */
    return STATUS_OK;
}

/* Broadcom WiFi (brcmfmac) */
status_t broadcom_wifi_init(wifi_device_t* device) {
    KLOG_INFO("WiFi", "Broadcom WiFi adapter detected: 0x%04x", device->device_id);
    /* TODO: Initialize Broadcom WiFi */
    return STATUS_OK;
}
