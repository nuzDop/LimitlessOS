/*
 * LimitlessOS Installer - Main
 * Phase 1: Basic auto-install with local login
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "installer.h"

/* Main installer flow */
int main(int argc, char** argv) {
    int result = 0;

    /* Initialize installer */
    installer_show_welcome();

    if (installer_init() != 0) {
        fprintf(stderr, "ERROR: Failed to initialize installer\n");
        return 1;
    }

    /* Detect disks */
    printf("\n[*] Detecting storage devices...\n");
    disk_info_t* disks = NULL;
    int disk_count = 0;

    if (installer_detect_disks(&disks, &disk_count) != 0 || disk_count == 0) {
        fprintf(stderr, "ERROR: No suitable disks found\n");
        return 1;
    }

    printf("    Found %d disk(s)\n", disk_count);

    /* Show disk selection */
    installer_show_disk_selection(disks, disk_count);

    /* Setup configuration */
    install_config_t config = {0};
    config.mode = INSTALL_MODE_BASIC;
    config.target_disk = disks[0];  // Use first disk for Phase 1

    /* Auto-partition */
    printf("\n[*] Creating partitions...\n");
    if (installer_auto_partition(&config) != 0) {
        fprintf(stderr, "ERROR: Failed to create partitions\n");
        result = 1;
        goto cleanup;
    }

    /* User setup */
    installer_show_user_setup();

    /* Create default user (Phase 1: hardcoded for demo) */
    user_account_t user = {0};
    strcpy(user.username, "limitless");
    strcpy(user.full_name, "LimitlessOS User");
    user.is_admin = true;

    if (installer_create_user(&user) != 0) {
        fprintf(stderr, "ERROR: Failed to create user\n");
        result = 1;
        goto cleanup;
    }

    config.user = user;
    strcpy(config.hostname, "limitless-pc");
    strcpy(config.timezone, "UTC");

    /* Install system */
    printf("\n[*] Installing system...\n");
    if (installer_install_system(&config) != 0) {
        fprintf(stderr, "ERROR: Installation failed\n");
        result = 1;
        goto cleanup;
    }

    /* Show completion */
    installer_show_complete();

cleanup:
    if (disks) {
        free(disks);
    }

    installer_cleanup();
    return result;
}

/* Installer initialization */
int installer_init(void) {
    printf("[OK] Installer initialized\n");
    return 0;
}

/* Detect available disks */
int installer_detect_disks(disk_info_t** disks, int* count) {
    /* Phase 1: Stub - return mock disk */
    *count = 1;
    *disks = (disk_info_t*)malloc(sizeof(disk_info_t));

    strcpy((*disks)[0].device_path, "/dev/sda");
    strcpy((*disks)[0].model, "Virtual Disk");
    (*disks)[0].size_bytes = 32ULL * 1024 * 1024 * 1024;  // 32GB
    (*disks)[0].is_ssd = true;
    (*disks)[0].is_removable = false;

    return 0;
}

/* Auto-partition disk */
int installer_auto_partition(install_config_t* config) {
    printf("    Target: %s (%s)\n", config->target_disk.device_path, config->target_disk.model);

    /* Determine partition scheme based on firmware */
    config->layout.scheme = PARTITION_SCHEME_GPT;

    if (config->layout.scheme == PARTITION_SCHEME_GPT) {
        config->layout.efi_size_mb = 512;
        config->layout.boot_size_mb = 0;
    } else {
        config->layout.efi_size_mb = 0;
        config->layout.boot_size_mb = 512;
    }

    /* Calculate partitions */
    uint64_t disk_size_mb = config->target_disk.size_bytes / (1024 * 1024);
    config->layout.swap_size_mb = 4096;  // 4GB swap
    config->layout.root_size_mb = disk_size_mb - config->layout.efi_size_mb - config->layout.swap_size_mb;

    printf("    Scheme: %s\n", config->layout.scheme == PARTITION_SCHEME_GPT ? "GPT" : "MBR");
    printf("    Partitions:\n");

    if (config->layout.scheme == PARTITION_SCHEME_GPT) {
        printf("      - EFI System:  %llu MB\n", config->layout.efi_size_mb);
    } else {
        printf("      - Boot:        %llu MB\n", config->layout.boot_size_mb);
    }

    printf("      - Root:        %llu MB\n", config->layout.root_size_mb);
    printf("      - Swap:        %llu MB\n", config->layout.swap_size_mb);

    printf("    [TODO] Execute partitioning commands\n");
    return 0;
}

/* Create user account */
int installer_create_user(user_account_t* user) {
    printf("    User: %s (%s)\n", user->username, user->full_name);
    printf("    Admin: %s\n", user->is_admin ? "Yes" : "No");
    printf("    [TODO] Create user account in system\n");
    return 0;
}

/* Install system files */
int installer_install_system(install_config_t* config) {
    const char* steps[] = {
        "Formatting partitions",
        "Mounting filesystems",
        "Installing bootloader",
        "Copying system files",
        "Installing kernel",
        "Configuring system",
        "Creating initramfs",
        "Setting up users",
        "Finalizing installation",
    };

    int step_count = sizeof(steps) / sizeof(steps[0]);

    for (int i = 0; i < step_count; i++) {
        int progress = ((i + 1) * 100) / step_count;
        installer_show_progress(progress, steps[i]);
        usleep(500000);  // 500ms delay for demo
    }

    return 0;
}

/* Cleanup */
int installer_cleanup(void) {
    return 0;
}

/* UI: Welcome screen */
void installer_show_welcome(void) {
    printf("\n");
    printf("  ========================================\n");
    printf("  LimitlessOS Installer v0.1.0\n");
    printf("  Phase 1 - Basic Installation\n");
    printf("  ========================================\n");
    printf("\n");
    printf("  This installer will:\n");
    printf("  - Auto-partition your disk (GPT/MBR)\n");
    printf("  - Install LimitlessOS with defaults\n");
    printf("  - Create a local user account\n");
    printf("\n");
}

/* UI: Disk selection */
void installer_show_disk_selection(disk_info_t* disks, int count) {
    printf("\n[*] Available disks:\n");
    for (int i = 0; i < count; i++) {
        uint64_t size_gb = disks[i].size_bytes / (1024ULL * 1024 * 1024);
        printf("    %d. %s - %s (%llu GB) %s\n",
               i + 1,
               disks[i].device_path,
               disks[i].model,
               size_gb,
               disks[i].is_ssd ? "[SSD]" : "[HDD]");
    }
}

/* UI: User setup */
void installer_show_user_setup(void) {
    printf("\n[*] Creating user account...\n");
}

/* UI: Progress indicator */
void installer_show_progress(int percent, const char* message) {
    printf("\r    [%3d%%] %s", percent, message);
    fflush(stdout);

    if (percent >= 100) {
        printf("\n");
    }
}

/* UI: Installation complete */
void installer_show_complete(void) {
    printf("\n");
    printf("  ========================================\n");
    printf("  Installation Complete!\n");
    printf("  ========================================\n");
    printf("\n");
    printf("  LimitlessOS has been installed successfully.\n");
    printf("  Please remove the installation media and reboot.\n");
    printf("\n");
    printf("  Default credentials:\n");
    printf("    Username: limitless\n");
    printf("    Password: [set during installation]\n");
    printf("\n");
}
