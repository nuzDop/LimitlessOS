/*
 * Action Card System
 * Preview, approve/deny/edit system for AI companion actions
 */

#ifndef ACTION_CARD_H
#define ACTION_CARD_H

#include <stdint.h>
#include <stdbool.h>

/* Action types */
typedef enum action_type {
    ACTION_COMMAND,          /* Execute shell command */
    ACTION_FILE_WRITE,       /* Write/modify file */
    ACTION_FILE_DELETE,      /* Delete file */
    ACTION_INSTALL,          /* Install application */
    ACTION_UNINSTALL,        /* Uninstall application */
    ACTION_SETTING_CHANGE,   /* Modify system setting */
    ACTION_NETWORK,          /* Network operation */
    ACTION_CODE_EXEC,        /* Execute generated code */
    ACTION_PERMISSION,       /* Grant/revoke permission */
    ACTION_CUSTOM            /* Custom extensible action */
} action_type_t;

/* Action privilege level */
typedef enum action_privilege {
    PRIVILEGE_USER,          /* Normal user operations */
    PRIVILEGE_ADMIN,         /* Administrative operations */
    PRIVILEGE_SYSTEM,        /* System-level operations */
    PRIVILEGE_SECURITY       /* Security-critical operations */
} action_privilege_t;

/* User response to action card */
typedef enum action_response {
    RESPONSE_PENDING,        /* No response yet */
    RESPONSE_APPROVED,       /* User approved action */
    RESPONSE_DENIED,         /* User denied action */
    RESPONSE_EDITED,         /* User edited and approved */
    RESPONSE_TIMEOUT         /* Timed out waiting for response */
} action_response_t;

/* Action impact assessment */
typedef struct action_impact {
    bool reversible;         /* Can be undone */
    bool persistent;         /* Survives reboot */
    bool affects_security;   /* Security implications */
    bool affects_privacy;    /* Privacy implications */
    bool affects_data;       /* Data modification */
    uint32_t risk_level;     /* 0-100 risk score */
} action_impact_t;

/* Action step (for multi-step actions) */
#define ACTION_STEP_DESC_SIZE 256
typedef struct action_step {
    char description[ACTION_STEP_DESC_SIZE];
    bool completed;
    uint64_t timestamp;
} action_step_t;

/* Maximum action card sizes */
#define ACTION_TITLE_SIZE 128
#define ACTION_SUMMARY_SIZE 512
#define ACTION_COMMAND_SIZE 1024
#define ACTION_DETAIL_SIZE 2048
#define ACTION_MAX_STEPS 16
#define ACTION_MAX_PERMISSIONS 16

/* Action card structure */
typedef struct action_card {
    uint64_t id;                           /* Unique action ID */
    action_type_t type;                    /* Type of action */
    action_privilege_t privilege;          /* Required privilege */
    action_response_t response;            /* User response */

    char title[ACTION_TITLE_SIZE];         /* Human-readable title */
    char summary[ACTION_SUMMARY_SIZE];     /* Brief summary */
    char command[ACTION_COMMAND_SIZE];     /* Command to execute */
    char details[ACTION_DETAIL_SIZE];      /* Detailed description */

    action_impact_t impact;                /* Impact assessment */

    /* Multi-step actions */
    uint32_t step_count;
    action_step_t steps[ACTION_MAX_STEPS];

    /* Permissions required */
    uint32_t permission_count;
    char permissions[ACTION_MAX_PERMISSIONS][64];

    /* Undo information */
    bool can_undo;
    char undo_command[ACTION_COMMAND_SIZE];
    void* undo_data;
    uint32_t undo_data_size;

    /* Timestamps */
    uint64_t created_at;
    uint64_t presented_at;
    uint64_t responded_at;
    uint64_t executed_at;

    /* Execution results */
    bool executed;
    int exit_code;
    char output[1024];

    /* Force execution flag (bypass approval if configured) */
    bool force;

    /* Audit trail */
    uint64_t user_id;
    char user_name[64];
} action_card_t;

/* Action card consent policy */
typedef enum consent_policy {
    CONSENT_ALWAYS_ASK,      /* Always require approval */
    CONSENT_ALLOW_ONCE,      /* Allow this once */
    CONSENT_ALLOW_ALWAYS,    /* Always allow (trust) */
    CONSENT_DENY_ALWAYS      /* Always deny (block) */
} consent_policy_t;

/* Consent rule */
typedef struct consent_rule {
    action_type_t action_type;
    consent_policy_t policy;
    char pattern[256];       /* Pattern to match (e.g., "install *") */
    uint64_t created_at;
    bool active;
} consent_rule_t;

/* Action card settings */
typedef struct action_card_settings {
    bool enabled;                    /* Action cards enabled/disabled */
    bool force_bypass_enabled;       /* Allow --force flag */
    bool show_technical_details;     /* Show technical info */
    bool require_confirmation;       /* Require explicit confirmation */
    uint32_t approval_timeout_sec;   /* Timeout for user response */
    bool audit_log_enabled;          /* Log all actions */
    bool undo_enabled;               /* Enable undo functionality */
    uint32_t undo_history_size;      /* Max undo history entries */
} action_card_settings_t;

/* Initialize action card system */
int action_card_init(void);

/* Create new action card */
action_card_t* action_card_create(action_type_t type, const char* title, const char* summary);

/* Set action details */
void action_card_set_command(action_card_t* card, const char* command);
void action_card_set_details(action_card_t* card, const char* details);
void action_card_set_privilege(action_card_t* card, action_privilege_t privilege);
void action_card_set_force(action_card_t* card, bool force);

/* Impact assessment */
void action_card_assess_impact(action_card_t* card);
void action_card_set_reversible(action_card_t* card, bool reversible, const char* undo_cmd);

/* Steps management */
void action_card_add_step(action_card_t* card, const char* description);
void action_card_complete_step(action_card_t* card, uint32_t step_index);

/* Permissions */
void action_card_add_permission(action_card_t* card, const char* permission);

/* Present action card to user */
int action_card_present(action_card_t* card);

/* Wait for user response */
action_response_t action_card_wait_response(action_card_t* card, uint32_t timeout_sec);

/* Edit action card (user modification) */
int action_card_edit(action_card_t* card, const char* new_command);

/* Execute action */
int action_card_execute(action_card_t* card);

/* Undo action */
int action_card_undo(uint64_t action_id);

/* Consent management */
int action_card_add_consent_rule(action_type_t type, consent_policy_t policy, const char* pattern);
int action_card_remove_consent_rule(uint64_t rule_id);
consent_policy_t action_card_check_consent(action_type_t type, const char* command);

/* Settings */
void action_card_get_settings(action_card_settings_t* settings);
void action_card_set_settings(const action_card_settings_t* settings);

/* Audit log */
int action_card_audit_log(const action_card_t* card);
int action_card_get_audit_history(action_card_t** cards, uint32_t* count, uint32_t max);

/* Destroy action card */
void action_card_destroy(action_card_t* card);

/* Render action card for display */
void action_card_render(const action_card_t* card, char* output, uint32_t output_size);

#endif /* ACTION_CARD_H */
