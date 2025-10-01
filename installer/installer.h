#ifndef LIMITLESS_INSTALLER_H
#define LIMITLESS_INSTALLER_H

/*
 * LimitlessOS Installer
 * Phase 1: Basic installer with auto-partitioning and local login
 */

#include <stdint.h>
#include <stdbool.h>

/* Installation mode */
typedef enum {
    INSTALL_MODE_BASIC,     // Auto-partition, recommended defaults
    INSTALL_MODE_ADVANCED,  // Full control (not in Phase 1)
} install_mode_t;

/* Disk information */
typedef struct {
    char device_path[256];
    char model[128];
    uint64_t size_bytes;
    bool is_ssd;
    bool is_removable;
} disk_info_t;

/* Partition scheme */
typedef enum {
    PARTITION_SCHEME_GPT,   // UEFI (default)
    PARTITION_SCHEME_MBR,   // BIOS Legacy
} partition_scheme_t;

/* Partition layout */
typedef struct {
    partition_scheme_t scheme;
    uint64_t efi_size_mb;      // EFI System Partition (GPT only)
    uint64_t boot_size_mb;     // Boot partition (MBR only)
    uint64_t root_size_mb;     // Root partition (0 = use remaining)
    uint64_t swap_size_mb;     // Swap partition
} partition_layout_t;

/* User account */
typedef struct {
    char username[64];
    char password_hash[128];
    char full_name[128];
    bool is_admin;
} user_account_t;

/* Installation configuration */
typedef struct {
    install_mode_t mode;
    disk_info_t target_disk;
    partition_layout_t layout;
    user_account_t user;
    char hostname[64];
    char timezone[64];
    bool enable_encryption;
} install_config_t;

/* Installer functions */
int installer_init(void);
int installer_detect_disks(disk_info_t** disks, int* count);
int installer_auto_partition(install_config_t* config);
int installer_create_user(user_account_t* user);
int installer_install_system(install_config_t* config);
int installer_cleanup(void);

/* UI functions (Phase 1: Simple CLI) */
void installer_show_welcome(void);
void installer_show_disk_selection(disk_info_t* disks, int count);
void installer_show_user_setup(void);
void installer_show_progress(int percent, const char* message);
void installer_show_complete(void);

#endif /* LIMITLESS_INSTALLER_H */
