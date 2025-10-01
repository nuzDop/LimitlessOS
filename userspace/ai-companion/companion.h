#ifndef LIMITLESS_AI_COMPANION_H
#define LIMITLESS_AI_COMPANION_H

/*
 * LimitlessOS AI Companion
 * Conversational interface for system control and assistance
 */

#include <stdint.h>
#include <stdbool.h>

/* Action types */
typedef enum {
    ACTION_TYPE_COMMAND,        // Execute shell command
    ACTION_TYPE_FILE_OP,        // File operation
    ACTION_TYPE_SETTINGS,       // Change system settings
    ACTION_TYPE_INSTALL,        // Install software
    ACTION_TYPE_NETWORK,        // Network operation
    ACTION_TYPE_SYSTEM,         // System management
    ACTION_TYPE_CODE,           // Generate/run code
} action_type_t;

/* Action impact level */
typedef enum {
    IMPACT_SAFE,        // No risk (read-only)
    IMPACT_LOW,         // Low risk (minor changes)
    IMPACT_MEDIUM,      // Medium risk (significant changes)
    IMPACT_HIGH,        // High risk (system-wide changes)
    IMPACT_CRITICAL,    // Critical (potential data loss)
} impact_level_t;

/* Permission level */
typedef enum {
    PERM_USER,          // User-level permission
    PERM_ADMIN,         // Admin permission required
    PERM_ROOT,          // Root permission required
} permission_level_t;

/* Action step */
typedef struct action_step {
    char description[256];
    char command[512];
    bool completed;
    int exit_code;
    struct action_step* next;
} action_step_t;

/* Action Card */
typedef struct action_card {
    uint64_t id;
    char title[128];
    char summary[512];
    action_type_t type;
    impact_level_t impact;
    permission_level_t permission;

    /* Steps */
    action_step_t* steps;
    uint32_t step_count;

    /* Status */
    bool approved;
    bool executed;
    bool can_undo;
    char undo_command[512];

    /* Timestamps */
    uint64_t created_time;
    uint64_t approved_time;
    uint64_t executed_time;

    /* User interaction */
    bool show_preview;
    bool allow_edit;
    bool force_mode;
} action_card_t;

/* Companion state */
typedef enum {
    COMPANION_STATE_IDLE,
    COMPANION_STATE_LISTENING,
    COMPANION_STATE_PROCESSING,
    COMPANION_STATE_AWAITING_APPROVAL,
    COMPANION_STATE_EXECUTING,
    COMPANION_STATE_ERROR,
} companion_state_t;

/* Companion context */
typedef struct companion_context {
    companion_state_t state;
    char current_directory[4096];
    char user_name[64];
    bool admin_mode;
    bool verbose;
    bool force_mode;

    /* Action history */
    action_card_t* history;
    uint32_t history_count;
    uint32_t history_capacity;

    /* Pending action */
    action_card_t* pending_action;

    /* Statistics */
    uint64_t commands_executed;
    uint64_t actions_approved;
    uint64_t actions_denied;
    uint64_t clarifications_requested;
} companion_context_t;

/* Response type */
typedef enum {
    RESPONSE_ACTION_CARD,       // Generated action card
    RESPONSE_CLARIFICATION,     // Need more info
    RESPONSE_INFORMATION,       // Informational response
    RESPONSE_ERROR,             // Error message
} response_type_t;

/* Companion response */
typedef struct companion_response {
    response_type_t type;
    char message[1024];
    action_card_t* action_card;
} companion_response_t;

/* Companion API */
status_t companion_init(companion_context_t** out_ctx);
void companion_shutdown(companion_context_t* ctx);

/* Interaction */
companion_response_t* companion_process_input(companion_context_t* ctx, const char* input);
void companion_free_response(companion_response_t* response);

/* Action Card Management */
action_card_t* companion_create_action_card(const char* title, action_type_t type);
void companion_free_action_card(action_card_t* card);
void companion_add_step(action_card_t* card, const char* description, const char* command);
void companion_set_impact(action_card_t* card, impact_level_t impact);
void companion_set_permission(action_card_t* card, permission_level_t permission);

/* Action execution */
status_t companion_approve_action(companion_context_t* ctx, action_card_t* card);
status_t companion_deny_action(companion_context_t* ctx, action_card_t* card);
status_t companion_execute_action(companion_context_t* ctx, action_card_t* card);
status_t companion_undo_action(companion_context_t* ctx, action_card_t* card);

/* Preview and editing */
void companion_show_preview(action_card_t* card);
status_t companion_edit_action(action_card_t* card, uint32_t step_index, const char* new_command);

/* Intent parsing */
action_type_t companion_parse_intent(const char* input);
bool companion_requires_clarification(const char* input);
char* companion_generate_clarification(const char* input);

/* Safety checks */
bool companion_is_safe_operation(const char* command);
bool companion_requires_permission(const char* command, permission_level_t* out_level);
impact_level_t companion_assess_impact(const char* command);

/* History */
void companion_add_to_history(companion_context_t* ctx, action_card_t* card);
action_card_t* companion_get_last_action(companion_context_t* ctx);
void companion_clear_history(companion_context_t* ctx);

/* Configuration */
void companion_set_force_mode(companion_context_t* ctx, bool enable);
void companion_set_verbose(companion_context_t* ctx, bool enable);
void companion_set_admin_mode(companion_context_t* ctx, bool enable);

/* Capabilities */
typedef struct companion_capability {
    char name[64];
    char description[256];
    bool enabled;
    void* handler;
} companion_capability_t;

status_t companion_register_capability(const char* name, const char* description, void* handler);
status_t companion_enable_capability(const char* name, bool enable);
companion_capability_t* companion_list_capabilities(uint32_t* out_count);

/* Audit logging */
void companion_log_action(companion_context_t* ctx, action_card_t* card, const char* result);
char* companion_get_audit_log(companion_context_t* ctx);

#endif /* LIMITLESS_AI_COMPANION_H */
