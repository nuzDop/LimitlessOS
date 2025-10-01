/*
 * LimitlessOS Settings Application
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "settings.h"

void print_banner(void) {
    printf("\n");
    printf("  ╔═══════════════════════════════════════════════════════════╗\n");
    printf("  ║                                                           ║\n");
    printf("  ║            LimitlessOS Settings v0.2.0                   ║\n");
    printf("  ║                                                           ║\n");
    printf("  ║     Configure your system with ease                      ║\n");
    printf("  ║                                                           ║\n");
    printf("  ╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

void print_menu(void) {
    printf("\n");
    printf("Settings Categories:\n");
    printf("  1. Display\n");
    printf("  2. Appearance\n");
    printf("  3. System\n");
    printf("  4. Network\n");
    printf("  5. Privacy\n");
    printf("  6. System Information\n");
    printf("  7. Save Settings\n");
    printf("  8. Reset Settings\n");
    printf("  0. Exit\n");
    printf("\n");
}

void show_category_settings(const char* category) {
    printf("\n=== %s Settings ===\n\n", category);

    uint32_t count = 0;
    setting_t** settings = settings_get_by_category(category, &count);

    if (!settings || count == 0) {
        printf("No settings in this category\n");
        return;
    }

    for (uint32_t i = 0; i < count; i++) {
        setting_t* setting = settings[i];
        printf("%u. %s\n", i + 1, setting->name);
        printf("   %s\n", setting->description);
        printf("   Current value: ");

        switch (setting->type) {
            case SETTING_TYPE_BOOL:
                printf("%s", setting->value.bool_value ? "Enabled" : "Disabled");
                break;
            case SETTING_TYPE_INT:
                printf("%lld", setting->value.int_value);
                break;
            case SETTING_TYPE_STRING:
                printf("%s", setting->value.string_value);
                break;
            default:
                printf("(unknown)");
                break;
        }

        printf("\n\n");
    }

    free(settings);
}

void edit_setting(setting_t* setting) {
    printf("\nEditing: %s\n", setting->name);
    printf("Current value: ");

    switch (setting->type) {
        case SETTING_TYPE_BOOL:
            printf("%s\n", setting->value.bool_value ? "true" : "false");
            printf("New value (true/false): ");
            break;
        case SETTING_TYPE_INT:
            printf("%lld\n", setting->value.int_value);
            printf("New value: ");
            break;
        case SETTING_TYPE_STRING:
            printf("%s\n", setting->value.string_value);
            printf("New value: ");
            break;
        default:
            printf("Unsupported type\n");
            return;
    }

    char input[256];
    if (!fgets(input, sizeof(input), stdin)) {
        return;
    }

    input[strcspn(input, "\n")] = 0;

    switch (setting->type) {
        case SETTING_TYPE_BOOL:
            if (strcmp(input, "true") == 0 || strcmp(input, "1") == 0) {
                settings_set_bool(setting->key, true);
                printf("✓ Set to true\n");
            } else if (strcmp(input, "false") == 0 || strcmp(input, "0") == 0) {
                settings_set_bool(setting->key, false);
                printf("✓ Set to false\n");
            }
            break;
        case SETTING_TYPE_INT:
            settings_set_int(setting->key, atoll(input));
            printf("✓ Set to %lld\n", atoll(input));
            break;
        case SETTING_TYPE_STRING:
            settings_set_string(setting->key, input);
            printf("✓ Set to %s\n", input);
            break;
        default:
            break;
    }
}

void show_system_info(void) {
    system_info_t info;
    if (FAILED(settings_get_system_info(&info))) {
        printf("ERROR: Failed to get system information\n");
        return;
    }

    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                     SYSTEM INFORMATION                        ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║                                                              ║\n");
    printf("║ Operating System:  %-40s ║\n", info.os_name);
    printf("║ OS Version:        %-40s ║\n", info.os_version);
    printf("║ Kernel Version:    %-40s ║\n", info.kernel_version);
    printf("║ Architecture:      %-40s ║\n", info.architecture);
    printf("║                                                              ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ HARDWARE                                                     ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║                                                              ║\n");
    printf("║ CPU:               %-40s ║\n", info.cpu_model);
    printf("║ CPU Cores:         %-40u ║\n", info.cpu_cores);
    printf("║ CPU Frequency:     %-37llu MHz ║\n", info.cpu_frequency_mhz);
    printf("║ Total Memory:      %-37llu MB  ║\n", info.total_memory_mb);
    printf("║ Available Memory:  %-37llu MB  ║\n", info.available_memory_mb);
    printf("║                                                              ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ DISPLAY                                                      ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║                                                              ║\n");
    printf("║ Resolution:        %ux%u                                     \n", info.screen_width, info.screen_height);
    printf("║ Refresh Rate:      %u Hz                                     \n", info.screen_refresh_rate);
    printf("║                                                              ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ NETWORK                                                      ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║                                                              ║\n");
    printf("║ Hostname:          %-40s ║\n", info.hostname);
    printf("║ IP Address:        %-40s ║\n", info.ip_address);
    printf("║ MAC Address:       %-40s ║\n", info.mac_address);
    printf("║                                                              ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ STORAGE                                                      ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║                                                              ║\n");
    printf("║ Total Disk:        %-37llu MB  ║\n", info.total_disk_mb);
    printf("║ Available Disk:    %-37llu MB  ║\n", info.available_disk_mb);
    printf("║                                                              ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ Uptime:            %-37llu sec ║\n", info.uptime_seconds);
    printf("║                                                              ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

int main(int argc, char** argv) {
    print_banner();

    /* Initialize settings */
    status_t status = settings_init(NULL);
    if (FAILED(status)) {
        fprintf(stderr, "ERROR: Failed to initialize settings\n");
        return 1;
    }

    bool running = true;
    char input[32];

    while (running) {
        print_menu();
        printf("Select option: ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }

        input[strcspn(input, "\n")] = 0;

        int choice = atoi(input);

        switch (choice) {
            case 0:
                running = false;
                break;

            case 1:
                show_category_settings(CATEGORY_DISPLAY);
                break;

            case 2:
                show_category_settings(CATEGORY_APPEARANCE);
                break;

            case 3:
                show_category_settings(CATEGORY_SYSTEM);
                break;

            case 4:
                show_category_settings(CATEGORY_NETWORK);
                break;

            case 5:
                show_category_settings(CATEGORY_PRIVACY);
                break;

            case 6:
                show_system_info();
                break;

            case 7:
                printf("\nSaving settings...\n");
                if (SUCCESS(settings_save())) {
                    printf("✓ Settings saved successfully\n");
                } else {
                    printf("✗ Failed to save settings\n");
                }
                break;

            case 8:
                printf("\nReset which category? (all/display/appearance/system/network/privacy): ");
                if (fgets(input, sizeof(input), stdin)) {
                    input[strcspn(input, "\n")] = 0;
                    if (strcmp(input, "all") == 0) {
                        printf("Resetting all settings...\n");
                        printf("✓ Settings reset (not implemented yet)\n");
                    } else {
                        printf("Resetting %s settings...\n", input);
                        printf("✓ Settings reset (not implemented yet)\n");
                    }
                }
                break;

            default:
                printf("Invalid option\n");
                break;
        }
    }

    /* Save on exit */
    settings_shutdown();

    printf("\nGoodbye!\n\n");
    return 0;
}
