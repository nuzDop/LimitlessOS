/*
 * LimitlessOS Settings Implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "settings.h"

/* Global settings database */
static settings_db_t* g_settings_db = NULL;

/* Initialize settings system */
status_t settings_init(const char* config_file) {
    if (g_settings_db) {
        return STATUS_EXISTS;
    }

    g_settings_db = (settings_db_t*)calloc(1, sizeof(settings_db_t));
    if (!g_settings_db) {
        return STATUS_NOMEM;
    }

    g_settings_db->settings = NULL;
    g_settings_db->count = 0;
    g_settings_db->modified = false;

    if (config_file) {
        strncpy(g_settings_db->config_file, config_file, sizeof(g_settings_db->config_file) - 1);
    } else {
        strcpy(g_settings_db->config_file, "/etc/limitless/settings.conf");
    }

    /* Register default settings */
    settings_register_defaults();

    /* Try to load existing config */
    settings_load();

    printf("[SETTINGS] Initialized (%u settings)\n", g_settings_db->count);
    return STATUS_OK;
}

/* Shutdown settings */
void settings_shutdown(void) {
    if (!g_settings_db) {
        return;
    }

    /* Save if modified */
    if (g_settings_db->modified) {
        settings_save();
    }

    /* Free all settings */
    setting_t* setting = g_settings_db->settings;
    while (setting) {
        setting_t* next = setting->next;
        if (setting->enum_options) {
            for (uint32_t i = 0; i < setting->enum_count; i++) {
                free(setting->enum_options[i]);
            }
            free(setting->enum_options);
        }
        free(setting);
        setting = next;
    }

    free(g_settings_db);
    g_settings_db = NULL;

    printf("[SETTINGS] Shutdown\n");
}

/* Get settings database */
settings_db_t* settings_get_db(void) {
    return g_settings_db;
}

/* Register setting */
status_t settings_register(const char* key, const char* name, const char* description,
                          setting_type_t type, const char* category) {
    if (!g_settings_db || !key || !name) {
        return STATUS_INVALID;
    }

    /* Check if already exists */
    if (settings_find(key)) {
        return STATUS_EXISTS;
    }

    setting_t* setting = (setting_t*)calloc(1, sizeof(setting_t));
    if (!setting) {
        return STATUS_NOMEM;
    }

    strncpy(setting->key, key, sizeof(setting->key) - 1);
    strncpy(setting->name, name, sizeof(setting->name) - 1);
    if (description) {
        strncpy(setting->description, description, sizeof(setting->description) - 1);
    }
    setting->type = type;
    if (category) {
        strncpy(setting->category, category, sizeof(setting->category) - 1);
    }

    setting->enum_options = NULL;
    setting->enum_count = 0;
    setting->min_value = 0;
    setting->max_value = INT64_MAX;
    setting->requires_restart = false;
    setting->requires_admin = false;

    /* Add to list */
    setting->next = g_settings_db->settings;
    g_settings_db->settings = setting;
    g_settings_db->count++;

    return STATUS_OK;
}

/* Find setting */
setting_t* settings_find(const char* key) {
    if (!g_settings_db || !key) {
        return NULL;
    }

    setting_t* setting = g_settings_db->settings;
    while (setting) {
        if (strcmp(setting->key, key) == 0) {
            return setting;
        }
        setting = setting->next;
    }

    return NULL;
}

/* Set bool value */
status_t settings_set_bool(const char* key, bool value) {
    setting_t* setting = settings_find(key);
    if (!setting) {
        return STATUS_NOTFOUND;
    }

    if (setting->type != SETTING_TYPE_BOOL) {
        return STATUS_INVALID;
    }

    setting->value.bool_value = value;
    g_settings_db->modified = true;

    return STATUS_OK;
}

/* Set int value */
status_t settings_set_int(const char* key, int64_t value) {
    setting_t* setting = settings_find(key);
    if (!setting) {
        return STATUS_NOTFOUND;
    }

    if (setting->type != SETTING_TYPE_INT) {
        return STATUS_INVALID;
    }

    /* Check constraints */
    if (value < setting->min_value || value > setting->max_value) {
        return STATUS_INVALID;
    }

    setting->value.int_value = value;
    g_settings_db->modified = true;

    return STATUS_OK;
}

/* Set string value */
status_t settings_set_string(const char* key, const char* value) {
    setting_t* setting = settings_find(key);
    if (!setting || !value) {
        return STATUS_NOTFOUND;
    }

    if (setting->type != SETTING_TYPE_STRING) {
        return STATUS_INVALID;
    }

    strncpy(setting->value.string_value, value, sizeof(setting->value.string_value) - 1);
    g_settings_db->modified = true;

    return STATUS_OK;
}

/* Get bool value */
status_t settings_get_bool(const char* key, bool* out_value) {
    setting_t* setting = settings_find(key);
    if (!setting || !out_value) {
        return STATUS_NOTFOUND;
    }

    if (setting->type != SETTING_TYPE_BOOL) {
        return STATUS_INVALID;
    }

    *out_value = setting->value.bool_value;
    return STATUS_OK;
}

/* Get int value */
status_t settings_get_int(const char* key, int64_t* out_value) {
    setting_t* setting = settings_find(key);
    if (!setting || !out_value) {
        return STATUS_NOTFOUND;
    }

    if (setting->type != SETTING_TYPE_INT) {
        return STATUS_INVALID;
    }

    *out_value = setting->value.int_value;
    return STATUS_OK;
}

/* Get string value */
status_t settings_get_string(const char* key, char* out_value, size_t max_len) {
    setting_t* setting = settings_find(key);
    if (!setting || !out_value) {
        return STATUS_NOTFOUND;
    }

    if (setting->type != SETTING_TYPE_STRING) {
        return STATUS_INVALID;
    }

    strncpy(out_value, setting->value.string_value, max_len - 1);
    out_value[max_len - 1] = '\0';

    return STATUS_OK;
}

/* Load settings from file */
status_t settings_load(void) {
    if (!g_settings_db) {
        return STATUS_ERROR;
    }

    FILE* file = fopen(g_settings_db->config_file, "r");
    if (!file) {
        /* File doesn't exist yet - not an error */
        return STATUS_OK;
    }

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }

        /* Parse key=value */
        char* equals = strchr(line, '=');
        if (!equals) {
            continue;
        }

        *equals = '\0';
        char* key = line;
        char* value = equals + 1;

        /* Trim whitespace */
        while (*key == ' ' || *key == '\t') key++;
        while (*value == ' ' || *value == '\t') value++;

        /* Remove newline */
        value[strcspn(value, "\n")] = 0;

        /* Find setting and set value */
        setting_t* setting = settings_find(key);
        if (setting) {
            switch (setting->type) {
                case SETTING_TYPE_BOOL:
                    setting->value.bool_value = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
                    break;
                case SETTING_TYPE_INT:
                    setting->value.int_value = atoll(value);
                    break;
                case SETTING_TYPE_STRING:
                    strncpy(setting->value.string_value, value, sizeof(setting->value.string_value) - 1);
                    break;
                default:
                    break;
            }
        }
    }

    fclose(file);
    g_settings_db->modified = false;

    printf("[SETTINGS] Loaded from %s\n", g_settings_db->config_file);
    return STATUS_OK;
}

/* Save settings to file */
status_t settings_save(void) {
    if (!g_settings_db) {
        return STATUS_ERROR;
    }

    FILE* file = fopen(g_settings_db->config_file, "w");
    if (!file) {
        return STATUS_ERROR;
    }

    fprintf(file, "# LimitlessOS Settings\n");
    fprintf(file, "# Auto-generated configuration file\n\n");

    /* Group by category */
    const char* categories[] = {
        CATEGORY_SYSTEM, CATEGORY_DISPLAY, CATEGORY_INPUT,
        CATEGORY_NETWORK, CATEGORY_APPEARANCE, CATEGORY_SOUND,
        CATEGORY_POWER, CATEGORY_PRIVACY, CATEGORY_USERS
    };

    for (size_t i = 0; i < sizeof(categories) / sizeof(categories[0]); i++) {
        bool header_printed = false;

        setting_t* setting = g_settings_db->settings;
        while (setting) {
            if (strcmp(setting->category, categories[i]) == 0) {
                if (!header_printed) {
                    fprintf(file, "\n[%s]\n", categories[i]);
                    header_printed = true;
                }

                fprintf(file, "%s=", setting->key);

                switch (setting->type) {
                    case SETTING_TYPE_BOOL:
                        fprintf(file, "%s", setting->value.bool_value ? "true" : "false");
                        break;
                    case SETTING_TYPE_INT:
                        fprintf(file, "%lld", setting->value.int_value);
                        break;
                    case SETTING_TYPE_STRING:
                        fprintf(file, "%s", setting->value.string_value);
                        break;
                    default:
                        fprintf(file, "0");
                        break;
                }

                fprintf(file, "\n");
            }
            setting = setting->next;
        }
    }

    fclose(file);
    g_settings_db->modified = false;

    printf("[SETTINGS] Saved to %s\n", g_settings_db->config_file);
    return STATUS_OK;
}

/* Get system information */
status_t settings_get_system_info(system_info_t* info) {
    if (!info) {
        return STATUS_INVALID;
    }

    memset(info, 0, sizeof(system_info_t));

    /* OS information */
    strcpy(info->os_name, "LimitlessOS");
    strcpy(info->os_version, "0.2.0-dev");
    strcpy(info->kernel_version, "0.2.0");
    strcpy(info->architecture, "x86_64");

    /* Hardware - would query HAL in production */
    strcpy(info->cpu_model, "Unknown CPU");
    info->cpu_cores = 4;
    info->cpu_frequency_mhz = 2400;
    info->total_memory_mb = 8192;
    info->available_memory_mb = 4096;

    info->total_disk_mb = 512000;
    info->available_disk_mb = 256000;

    info->screen_width = 1920;
    info->screen_height = 1080;
    info->screen_refresh_rate = 60;

    strcpy(info->hostname, "limitless-system");
    strcpy(info->ip_address, "192.168.1.100");
    strcpy(info->mac_address, "00:00:00:00:00:00");

    info->uptime_seconds = 3600;

    return STATUS_OK;
}

/* Register default settings */
void settings_register_defaults(void) {
    /* Display settings */
    settings_register("display.resolution_width", "Screen Width", "Display width in pixels",
                     SETTING_TYPE_INT, CATEGORY_DISPLAY);
    settings_set_int("display.resolution_width", 1920);

    settings_register("display.resolution_height", "Screen Height", "Display height in pixels",
                     SETTING_TYPE_INT, CATEGORY_DISPLAY);
    settings_set_int("display.resolution_height", 1080);

    settings_register("display.refresh_rate", "Refresh Rate", "Display refresh rate in Hz",
                     SETTING_TYPE_INT, CATEGORY_DISPLAY);
    settings_set_int("display.refresh_rate", 60);

    /* Appearance */
    settings_register("appearance.theme", "Theme", "Color theme",
                     SETTING_TYPE_STRING, CATEGORY_APPEARANCE);
    settings_set_string("appearance.theme", "dark");

    settings_register("appearance.animations", "Enable Animations", "Enable UI animations",
                     SETTING_TYPE_BOOL, CATEGORY_APPEARANCE);
    settings_set_bool("appearance.animations", true);

    /* System */
    settings_register("system.hostname", "Hostname", "System hostname",
                     SETTING_TYPE_STRING, CATEGORY_SYSTEM);
    settings_set_string("system.hostname", "limitless-system");

    settings_register("system.auto_update", "Automatic Updates", "Enable automatic system updates",
                     SETTING_TYPE_BOOL, CATEGORY_SYSTEM);
    settings_set_bool("system.auto_update", true);

    /* Network */
    settings_register("network.wifi_enabled", "WiFi", "Enable WiFi",
                     SETTING_TYPE_BOOL, CATEGORY_NETWORK);
    settings_set_bool("network.wifi_enabled", true);

    /* Privacy */
    settings_register("privacy.telemetry", "Telemetry", "Send anonymous usage data",
                     SETTING_TYPE_BOOL, CATEGORY_PRIVACY);
    settings_set_bool("privacy.telemetry", false);

    printf("[SETTINGS] Registered %u default settings\n", g_settings_db->count);
}
