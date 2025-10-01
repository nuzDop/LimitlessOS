/*
 * Command Parser
 * Parses and normalizes package manager commands
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "terminal.h"

/* Parse command line arguments into command structure */
command_t* parse_command(int argc, char** argv) {
    if (argc < 1) {
        return NULL;
    }

    command_t* cmd = (command_t*)calloc(1, sizeof(command_t));
    if (!cmd) {
        return NULL;
    }

    /* Detect package manager */
    const char* prog_name = argv[0];

    if (strcmp(prog_name, "apt") == 0 || strcmp(prog_name, "apt-get") == 0) {
        cmd->manager = PKG_MGR_APT;
    }
    else if (strcmp(prog_name, "yum") == 0) {
        cmd->manager = PKG_MGR_YUM;
    }
    else if (strcmp(prog_name, "dnf") == 0) {
        cmd->manager = PKG_MGR_DNF;
    }
    else if (strcmp(prog_name, "pacman") == 0) {
        cmd->manager = PKG_MGR_PACMAN;
    }
    else if (strcmp(prog_name, "apk") == 0) {
        cmd->manager = PKG_MGR_APK;
    }
    else if (strcmp(prog_name, "zypper") == 0) {
        cmd->manager = PKG_MGR_ZYPPER;
    }
    else if (strcmp(prog_name, "brew") == 0) {
        cmd->manager = PKG_MGR_BREW;
    }
    else if (strcmp(prog_name, "choco") == 0) {
        cmd->manager = PKG_MGR_CHOCO;
    }
    else if (strcmp(prog_name, "winget") == 0) {
        cmd->manager = PKG_MGR_WINGET;
    }
    else if (strcmp(prog_name, "npm") == 0) {
        cmd->manager = PKG_MGR_NPM;
    }
    else if (strcmp(prog_name, "pip") == 0) {
        cmd->manager = PKG_MGR_PIP;
    }
    else if (strcmp(prog_name, "cargo") == 0) {
        cmd->manager = PKG_MGR_CARGO;
    }

    /* Parse action if present */
    if (argc >= 2) {
        const char* action = argv[1];

        if (strcmp(action, "install") == 0 || strcmp(action, "add") == 0) {
            cmd->type = CMD_TYPE_INSTALL;
        }
        else if (strcmp(action, "remove") == 0 || strcmp(action, "uninstall") == 0) {
            cmd->type = CMD_TYPE_REMOVE;
        }
        else if (strcmp(action, "update") == 0) {
            cmd->type = CMD_TYPE_UPDATE;
        }
        else if (strcmp(action, "upgrade") == 0) {
            cmd->type = CMD_TYPE_UPGRADE;
        }
        else if (strcmp(action, "search") == 0) {
            cmd->type = CMD_TYPE_SEARCH;
        }
        else if (strcmp(action, "info") == 0 || strcmp(action, "show") == 0) {
            cmd->type = CMD_TYPE_INFO;
        }
        else if (strcmp(action, "list") == 0) {
            cmd->type = CMD_TYPE_LIST;
        }
        else if (strcmp(action, "clean") == 0) {
            cmd->type = CMD_TYPE_CLEAN;
        }
    }

    /* Store remaining arguments */
    if (argc > 2) {
        cmd->arg_count = argc - 2;
        cmd->args = (char**)malloc(sizeof(char*) * cmd->arg_count);

        for (int i = 0; i < cmd->arg_count; i++) {
            cmd->args[i] = strdup(argv[i + 2]);
        }
    }

    return cmd;
}

/* Free command structure */
void free_command(command_t* cmd) {
    if (!cmd) {
        return;
    }

    if (cmd->args) {
        for (int i = 0; i < cmd->arg_count; i++) {
            free(cmd->args[i]);
        }
        free(cmd->args);
    }

    free(cmd);
}

/* Execute normalized command */
int execute_command(command_t* cmd) {
    if (!cmd) {
        return 1;
    }

    printf("[EXEC] Manager: %d, Type: %d, Args: %d\n",
           cmd->manager, cmd->type, cmd->arg_count);

    /* Phase 1: Stub implementation */
    printf("[TODO] Execute package operation via native backend\n");

    return 0;
}
