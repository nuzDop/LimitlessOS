/*
 * Server Edition Features
 * Enterprise-ready, CLI-only server configuration
 */

#ifndef SERVER_EDITION_H
#define SERVER_EDITION_H

#include <stdint.h>
#include <stdbool.h>

/* Server edition modes */
typedef enum server_mode {
    SERVER_MODE_STANDALONE,      /* Single server */
    SERVER_MODE_CLUSTER_NODE,    /* Part of cluster */
    SERVER_MODE_LOAD_BALANCER,   /* Load balancer */
    SERVER_MODE_DATABASE,        /* Database server */
    SERVER_MODE_WEB,             /* Web server */
    SERVER_MODE_APPLICATION      /* Application server */
} server_mode_t;

/* Server configuration */
typedef struct server_config {
    server_mode_t mode;
    bool gui_disabled;           /* GUI completely disabled */
    bool headless;               /* No display output */
    bool remote_management;      /* Remote management enabled */
    bool clustering_enabled;     /* Clustering support */
    uint32_t max_connections;    /* Max concurrent connections */
    uint32_t worker_threads;     /* Worker thread count */
    uint16_t management_port;    /* Remote management port */
    char hostname[256];          /* Server hostname */
    char cluster_name[128];      /* Cluster name */
} server_config_t;

/* Remote management protocols */
typedef enum remote_protocol {
    REMOTE_SSH,                  /* SSH (default) */
    REMOTE_RDP,                  /* Remote Desktop Protocol */
    REMOTE_VNC,                  /* VNC */
    REMOTE_WEBUI,                /* Web-based UI */
    REMOTE_API                   /* REST API */
} remote_protocol_t;

/* Enterprise login types */
typedef enum enterprise_login_type {
    ENTERPRISE_LDAP,             /* LDAP/Active Directory */
    ENTERPRISE_KERBEROS,         /* Kerberos */
    ENTERPRISE_SAML,             /* SAML 2.0 */
    ENTERPRISE_OAUTH2,           /* OAuth 2.0 */
    ENTERPRISE_OPENID            /* OpenID Connect */
} enterprise_login_type_t;

/* Enterprise login configuration */
typedef struct enterprise_login_config {
    enterprise_login_type_t type;
    bool enabled;
    char server_url[512];        /* Auth server URL */
    char domain[256];            /* Domain/realm */
    char base_dn[512];           /* LDAP base DN */
    uint16_t port;               /* Server port */
    bool use_tls;                /* Use TLS/SSL */
    bool cache_credentials;      /* Cache for offline login */
    uint32_t cache_duration_sec; /* Cache duration */
    char client_id[256];         /* OAuth/OIDC client ID */
    char client_secret[512];     /* OAuth/OIDC client secret */
} enterprise_login_config_t;

/* Health check status */
typedef enum health_status {
    HEALTH_OK,                   /* Operating normally */
    HEALTH_WARNING,              /* Warning state */
    HEALTH_CRITICAL,             /* Critical issues */
    HEALTH_DOWN                  /* Service down */
} health_status_t;

/* Service health */
typedef struct service_health {
    char service_name[128];
    health_status_t status;
    uint64_t uptime_seconds;
    uint32_t cpu_usage;          /* 0-100% */
    uint32_t memory_usage_mb;
    uint64_t requests_total;
    uint64_t requests_failed;
    uint64_t last_check_timestamp;
    char status_message[256];
} service_health_t;

/* Cluster node info */
typedef struct cluster_node {
    char node_id[64];
    char hostname[256];
    char ip_address[64];
    bool is_leader;
    bool active;
    uint64_t last_heartbeat;
    service_health_t health;
} cluster_node_t;

/* Resource limits (for containers/isolation) */
typedef struct resource_limits {
    uint64_t max_memory_bytes;
    uint32_t max_cpu_percent;
    uint64_t max_disk_bytes;
    uint32_t max_network_mbps;
    uint32_t max_file_descriptors;
    uint32_t max_processes;
} resource_limits_t;

/* Audit event types */
typedef enum audit_event_type {
    AUDIT_LOGIN,
    AUDIT_LOGOUT,
    AUDIT_AUTH_FAILURE,
    AUDIT_PERMISSION_DENIED,
    AUDIT_CONFIG_CHANGE,
    AUDIT_SERVICE_START,
    AUDIT_SERVICE_STOP,
    AUDIT_FILE_ACCESS,
    AUDIT_NETWORK_CONNECTION,
    AUDIT_SECURITY_ALERT
} audit_event_type_t;

/* Audit log entry */
typedef struct audit_entry {
    uint64_t id;
    audit_event_type_t type;
    uint64_t timestamp;
    char user[128];
    char source_ip[64];
    char action[256];
    char resource[512];
    bool success;
    char details[1024];
} audit_entry_t;

/* Initialize server edition */
int server_edition_init(const server_config_t* config);

/* Server mode management */
int server_set_mode(server_mode_t mode);
server_mode_t server_get_mode(void);

/* Disable GUI permanently */
int server_disable_gui(void);
bool server_is_gui_disabled(void);

/* Remote management */
int server_enable_remote_management(remote_protocol_t protocol, uint16_t port);
int server_disable_remote_management(void);
bool server_is_remote_management_enabled(void);

/* Enterprise login */
int server_configure_enterprise_login(const enterprise_login_config_t* config);
int server_authenticate_enterprise(const char* username, const char* password, const char* domain);
int server_cache_credentials(const char* username, const char* credential_hash);
int server_validate_cached_credentials(const char* username, const char* credential_hash);

/* Health monitoring */
int server_check_health(service_health_t* health);
int server_register_health_check(const char* service_name,
                                  int (*check_fn)(service_health_t* health));
int server_get_all_health(service_health_t* services, uint32_t* count, uint32_t max);

/* Clustering */
int server_join_cluster(const char* cluster_name, const char* leader_ip);
int server_leave_cluster(void);
int server_get_cluster_nodes(cluster_node_t* nodes, uint32_t* count, uint32_t max);
int server_send_heartbeat(void);
bool server_is_cluster_leader(void);

/* Resource management */
int server_set_resource_limits(const resource_limits_t* limits);
int server_get_resource_usage(resource_limits_t* usage);
int server_enforce_limits(void);

/* Audit logging */
int server_audit_log(audit_event_type_t type, const char* user, const char* action,
                     const char* resource, bool success, const char* details);
int server_get_audit_log(audit_entry_t* entries, uint32_t* count, uint32_t max,
                         uint64_t since_timestamp);
int server_export_audit_log(const char* filename, uint64_t since_timestamp);

/* Service management */
int server_start_service(const char* service_name);
int server_stop_service(const char* service_name);
int server_restart_service(const char* service_name);
int server_get_service_status(const char* service_name, service_health_t* health);

/* Configuration management */
int server_get_config(server_config_t* config);
int server_set_config(const server_config_t* config);
int server_reload_config(void);

/* Graceful shutdown */
int server_graceful_shutdown(uint32_t timeout_sec);

#endif /* SERVER_EDITION_H */
