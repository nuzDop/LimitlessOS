#ifndef LIMITLESS_SETTINGS_H
#define LIMITLESS_SETTINGS_H

/*
 * LimitlessOS Settings Application
 * System configuration management
 */

#include <stdint.h>
#include <stdbool.h>

/* Setting types */
typedef enum {
    SETTING_TYPE_BOOL,
    SETTING_TYPE_INT,
    SETTING_TYPE_STRING,
    SETTING_TYPE_ENUM,
    SETTING_TYPE_COLOR,
} setting_type_t;

/* Setting entry */
typedef struct setting {
    char key[128];
    char name[128];
    char description[256];
    setting_type_t type;

    union {
        bool bool_value;
        int64_t int_value;
        char string_value[256];
        uint32_t enum_value;
        uint32_t color_value;
    } value;

    /* For enum types */
    char** enum_options;
    uint32_t enum_count;

    /* Constraints */
    int64_t min_value;
    int64_t max_value;

    /* Metadata */
    char category[64];
    bool requires_restart;
    bool requires_admin;

    struct setting* next;
} setting_t;

/* Setting categories */
#define CATEGORY_DISPLAY    "display"
#define CATEGORY_INPUT      "input"
#define CATEGORY_NETWORK    "network"
#define CATEGORY_SYSTEM     "system"
#define CATEGORY_PRIVACY    "privacy"
#define CATEGORY_APPEARANCE "appearance"
#define CATEGORY_SOUND      "sound"
#define CATEGORY_POWER      "power"
#define CATEGORY_USERS      "users"

/* Settings database */
typedef struct settings_db {
    setting_t* settings;
    uint32_t count;
    char config_file[4096];
    bool modified;
} settings_db_t;

/* Settings API */
status_t settings_init(const char* config_file);
void settings_shutdown(void);
settings_db_t* settings_get_db(void);

/* Setting operations */
status_t settings_register(const char* key, const char* name, const char* description,
                          setting_type_t type, const char* category);
status_t settings_set_bool(const char* key, bool value);
status_t settings_set_int(const char* key, int64_t value);
status_t settings_set_string(const char* key, const char* value);
status_t settings_set_enum(const char* key, uint32_t value);
status_t settings_set_color(const char* key, uint32_t color);

status_t settings_get_bool(const char* key, bool* out_value);
status_t settings_get_int(const char* key, int64_t* out_value);
status_t settings_get_string(const char* key, char* out_value, size_t max_len);
status_t settings_get_enum(const char* key, uint32_t* out_value);
status_t settings_get_color(const char* key, uint32_t* out_color);

/* Persistence */
status_t settings_load(void);
status_t settings_save(void);
status_t settings_reset(const char* category);
status_t settings_export(const char* file);
status_t settings_import(const char* file);

/* Query */
setting_t* settings_find(const char* key);
setting_t** settings_get_by_category(const char* category, uint32_t* out_count);
setting_t** settings_get_all(uint32_t* out_count);

/* System information */
typedef struct system_info {
    char os_name[64];
    char os_version[32];
    char kernel_version[32];
    char architecture[32];

    /* Hardware */
    char cpu_model[128];
    uint32_t cpu_cores;
    uint64_t cpu_frequency_mhz;
    uint64_t total_memory_mb;
    uint64_t available_memory_mb;

    /* Storage */
    uint64_t total_disk_mb;
    uint64_t available_disk_mb;

    /* Display */
    uint32_t screen_width;
    uint32_t screen_height;
    uint32_t screen_refresh_rate;

    /* Network */
    char hostname[256];
    char ip_address[64];
    char mac_address[32];

    /* Uptime */
    uint64_t uptime_seconds;
} system_info_t;

status_t settings_get_system_info(system_info_t* info);

/* Default settings registration */
void settings_register_defaults(void);

#endif /* LIMITLESS_SETTINGS_H */
