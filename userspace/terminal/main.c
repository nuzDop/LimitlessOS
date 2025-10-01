/*
 * Universal Terminal - Main
 * Phase 1: Basic CLI with command shim framework
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "terminal.h"

/* Global configuration */
static terminal_config_t config = {
    .enable_shims = true,
    .verbose = false,
    .color_output = true,
    .default_shell = "/bin/sh",
};

/* Print usage */
static void print_usage(const char* program_name) {
    printf("LimitlessOS Universal Terminal v0.1.0\n");
    printf("Usage: %s [options] [command]\n\n", program_name);
    printf("Options:\n");
    printf("  -h, --help      Show this help message\n");
    printf("  -v, --verbose   Enable verbose output\n");
    printf("  --no-color      Disable colored output\n");
    printf("  --version       Show version information\n");
    printf("\n");
    printf("Supported Commands:\n");
    printf("  apt, yum, dnf, pacman, apk, zypper\n");
    printf("  brew, choco, winget\n");
    printf("  npm, pip, cargo\n");
    printf("\n");
    printf("All package manager commands are automatically translated\n");
    printf("to the native LimitlessOS package manager.\n");
}

/* Print version */
static void print_version(void) {
    printf("LimitlessOS Universal Terminal v0.1.0\n");
    printf("Phase 1 - Basic CLI Edition\n");
    printf("Built: %s %s\n", __DATE__, __TIME__);
}

/* Main entry point */
int main(int argc, char** argv) {
    /* Handle no arguments */
    if (argc < 2) {
        print_usage(argv[0]);
        return 0;
    }

    /* Parse flags */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        }
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            config.verbose = true;
            continue;
        }
        if (strcmp(argv[i], "--no-color") == 0) {
            config.color_output = false;
            continue;
        }
    }

    /* Initialize terminal */
    terminal_init();

    /* Determine command type */
    const char* cmd_name = argv[1];

    /* Route to appropriate shim */
    int result = 0;

    if (strcmp(cmd_name, "apt") == 0 || strcmp(cmd_name, "apt-get") == 0) {
        result = shim_apt(argc - 1, &argv[1]);
    }
    else if (strcmp(cmd_name, "yum") == 0) {
        result = shim_yum(argc - 1, &argv[1]);
    }
    else if (strcmp(cmd_name, "dnf") == 0) {
        result = shim_dnf(argc - 1, &argv[1]);
    }
    else if (strcmp(cmd_name, "pacman") == 0) {
        result = shim_pacman(argc - 1, &argv[1]);
    }
    else if (strcmp(cmd_name, "apk") == 0) {
        result = shim_apk(argc - 1, &argv[1]);
    }
    else if (strcmp(cmd_name, "zypper") == 0) {
        result = shim_zypper(argc - 1, &argv[1]);
    }
    else if (strcmp(cmd_name, "brew") == 0) {
        result = shim_brew(argc - 1, &argv[1]);
    }
    else if (strcmp(cmd_name, "choco") == 0) {
        result = shim_choco(argc - 1, &argv[1]);
    }
    else if (strcmp(cmd_name, "winget") == 0) {
        result = shim_winget(argc - 1, &argv[1]);
    }
    else if (strcmp(cmd_name, "npm") == 0) {
        result = shim_npm(argc - 1, &argv[1]);
    }
    else if (strcmp(cmd_name, "pip") == 0) {
        result = shim_pip(argc - 1, &argv[1]);
    }
    else if (strcmp(cmd_name, "cargo") == 0) {
        result = shim_cargo(argc - 1, &argv[1]);
    }
    else {
        terminal_print_error("Unknown command. Use --help for usage information.\n");
        result = 1;
    }

    /* Cleanup */
    terminal_shutdown();

    return result;
}

/* Terminal initialization */
void terminal_init(void) {
    /* Phase 1: Basic initialization */
    if (config.verbose) {
        terminal_print("Terminal initialized\n");
    }
}

/* Terminal shutdown */
void terminal_shutdown(void) {
    /* Phase 1: Basic cleanup */
    if (config.verbose) {
        terminal_print("Terminal shutdown\n");
    }
}

/* Print functions */
void terminal_print(const char* str) {
    if (str) {
        printf("%s", str);
    }
}

void terminal_print_error(const char* str) {
    if (str) {
        if (config.color_output) {
            fprintf(stderr, "\033[31m[ERROR]\033[0m %s", str);
        } else {
            fprintf(stderr, "[ERROR] %s", str);
        }
    }
}

void terminal_print_success(const char* str) {
    if (str) {
        if (config.color_output) {
            printf("\033[32m[OK]\033[0m %s", str);
        } else {
            printf("[OK] %s", str);
        }
    }
}
