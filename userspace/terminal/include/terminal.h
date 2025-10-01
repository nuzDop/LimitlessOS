#ifndef LIMITLESS_TERMINAL_H
#define LIMITLESS_TERMINAL_H

/*
 * Universal Terminal
 * Understands all package manager commands and maps to unified backend
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Command types */
typedef enum {
    CMD_TYPE_UNKNOWN,
    CMD_TYPE_INSTALL,
    CMD_TYPE_REMOVE,
    CMD_TYPE_UPDATE,
    CMD_TYPE_UPGRADE,
    CMD_TYPE_SEARCH,
    CMD_TYPE_INFO,
    CMD_TYPE_LIST,
    CMD_TYPE_CLEAN,
} command_type_t;

/* Package manager types */
typedef enum {
    PKG_MGR_APT,
    PKG_MGR_YUM,
    PKG_MGR_DNF,
    PKG_MGR_PACMAN,
    PKG_MGR_APK,
    PKG_MGR_ZYPPER,
    PKG_MGR_BREW,
    PKG_MGR_CHOCO,
    PKG_MGR_WINGET,
    PKG_MGR_NPM,
    PKG_MGR_PIP,
    PKG_MGR_CARGO,
    PKG_MGR_LIMITLESS,  // Native package manager
} pkg_manager_t;

/* Command structure */
typedef struct {
    command_type_t type;
    pkg_manager_t manager;
    char** args;
    int arg_count;
    bool sudo;
    bool force;
    bool yes;
} command_t;

/* Terminal configuration */
typedef struct {
    bool enable_shims;
    bool verbose;
    bool color_output;
    char* default_shell;
} terminal_config_t;

/* Command shim functions */
int shim_apt(int argc, char** argv);
int shim_yum(int argc, char** argv);
int shim_dnf(int argc, char** argv);
int shim_pacman(int argc, char** argv);
int shim_apk(int argc, char** argv);
int shim_zypper(int argc, char** argv);
int shim_brew(int argc, char** argv);
int shim_choco(int argc, char** argv);
int shim_winget(int argc, char** argv);
int shim_npm(int argc, char** argv);
int shim_pip(int argc, char** argv);
int shim_cargo(int argc, char** argv);

/* Command parser */
command_t* parse_command(int argc, char** argv);
void free_command(command_t* cmd);

/* Command executor */
int execute_command(command_t* cmd);

/* Terminal utilities */
void terminal_init(void);
void terminal_shutdown(void);
void terminal_print(const char* str);
void terminal_print_error(const char* str);
void terminal_print_success(const char* str);

#endif /* LIMITLESS_TERMINAL_H */
