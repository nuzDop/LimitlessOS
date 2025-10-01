/*
 * LimitlessOS AI Companion Implementation
 * Conversational system interface with Action Cards
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "companion.h"

/* Global companion state */
static struct {
    bool initialized;
    uint64_t next_action_id;
} companion_state = {0};

/* Initialize companion */
status_t companion_init(companion_context_t** out_ctx) {
    if (!out_ctx) {
        return STATUS_INVALID;
    }

    companion_context_t* ctx = (companion_context_t*)calloc(1, sizeof(companion_context_t));
    if (!ctx) {
        return STATUS_NOMEM;
    }

    ctx->state = COMPANION_STATE_IDLE;
    strcpy(ctx->current_directory, "/");
    strcpy(ctx->user_name, "user");
    ctx->admin_mode = false;
    ctx->verbose = false;
    ctx->force_mode = false;

    /* Initialize history */
    ctx->history_capacity = 100;
    ctx->history = (action_card_t*)calloc(ctx->history_capacity, sizeof(action_card_t));
    ctx->history_count = 0;

    ctx->pending_action = NULL;

    /* Reset statistics */
    ctx->commands_executed = 0;
    ctx->actions_approved = 0;
    ctx->actions_denied = 0;
    ctx->clarifications_requested = 0;

    companion_state.initialized = true;
    companion_state.next_action_id = 1;

    *out_ctx = ctx;

    printf("[AI COMPANION] Initialized\n");
    return STATUS_OK;
}

/* Shutdown companion */
void companion_shutdown(companion_context_t* ctx) {
    if (!ctx) {
        return;
    }

    if (ctx->history) {
        free(ctx->history);
    }

    if (ctx->pending_action) {
        companion_free_action_card(ctx->pending_action);
    }

    free(ctx);

    printf("[AI COMPANION] Shutdown\n");
}

/* Create action card */
action_card_t* companion_create_action_card(const char* title, action_type_t type) {
    action_card_t* card = (action_card_t*)calloc(1, sizeof(action_card_t));
    if (!card) {
        return NULL;
    }

    card->id = companion_state.next_action_id++;
    strncpy(card->title, title ? title : "Action", sizeof(card->title) - 1);
    card->type = type;
    card->impact = IMPACT_SAFE;
    card->permission = PERM_USER;

    card->steps = NULL;
    card->step_count = 0;

    card->approved = false;
    card->executed = false;
    card->can_undo = false;
    card->undo_command[0] = '\0';

    card->created_time = (uint64_t)time(NULL);
    card->approved_time = 0;
    card->executed_time = 0;

    card->show_preview = true;
    card->allow_edit = true;
    card->force_mode = false;

    return card;
}

/* Free action card */
void companion_free_action_card(action_card_t* card) {
    if (!card) {
        return;
    }

    action_step_t* step = card->steps;
    while (step) {
        action_step_t* next = step->next;
        free(step);
        step = next;
    }

    free(card);
}

/* Add step to action card */
void companion_add_step(action_card_t* card, const char* description, const char* command) {
    if (!card || !description || !command) {
        return;
    }

    action_step_t* step = (action_step_t*)calloc(1, sizeof(action_step_t));
    if (!step) {
        return;
    }

    strncpy(step->description, description, sizeof(step->description) - 1);
    strncpy(step->command, command, sizeof(step->command) - 1);
    step->completed = false;
    step->exit_code = 0;
    step->next = NULL;

    /* Add to end of list */
    if (!card->steps) {
        card->steps = step;
    } else {
        action_step_t* last = card->steps;
        while (last->next) {
            last = last->next;
        }
        last->next = step;
    }

    card->step_count++;
}

/* Set impact level */
void companion_set_impact(action_card_t* card, impact_level_t impact) {
    if (card) {
        card->impact = impact;
    }
}

/* Set permission level */
void companion_set_permission(action_card_t* card, permission_level_t permission) {
    if (card) {
        card->permission = permission;
    }
}

/* Parse intent from input */
action_type_t companion_parse_intent(const char* input) {
    if (!input) {
        return ACTION_TYPE_COMMAND;
    }

    /* Convert to lowercase for matching */
    char lower[512] = {0};
    for (size_t i = 0; i < sizeof(lower) - 1 && input[i]; i++) {
        lower[i] = tolower(input[i]);
    }

    /* Check for install keywords */
    if (strstr(lower, "install") || strstr(lower, "add package")) {
        return ACTION_TYPE_INSTALL;
    }

    /* Check for settings keywords */
    if (strstr(lower, "change setting") || strstr(lower, "configure") || strstr(lower, "set")) {
        return ACTION_TYPE_SETTINGS;
    }

    /* Check for file operations */
    if (strstr(lower, "create file") || strstr(lower, "delete file") ||
        strstr(lower, "copy") || strstr(lower, "move")) {
        return ACTION_TYPE_FILE_OP;
    }

    /* Check for network operations */
    if (strstr(lower, "connect") || strstr(lower, "network") || strstr(lower, "wifi")) {
        return ACTION_TYPE_NETWORK;
    }

    /* Check for system operations */
    if (strstr(lower, "reboot") || strstr(lower, "shutdown") || strstr(lower, "restart")) {
        return ACTION_TYPE_SYSTEM;
    }

    /* Check for code operations */
    if (strstr(lower, "generate code") || strstr(lower, "write script") || strstr(lower, "run code")) {
        return ACTION_TYPE_CODE;
    }

    /* Default to command */
    return ACTION_TYPE_COMMAND;
}

/* Check if requires clarification */
bool companion_requires_clarification(const char* input) {
    if (!input || strlen(input) < 5) {
        return true;
    }

    /* Check for ambiguous terms */
    if (strstr(input, "it") || strstr(input, "that") || strstr(input, "this")) {
        return true;
    }

    return false;
}

/* Assess impact level */
impact_level_t companion_assess_impact(const char* command) {
    if (!command) {
        return IMPACT_SAFE;
    }

    char lower[512] = {0};
    for (size_t i = 0; i < sizeof(lower) - 1 && command[i]; i++) {
        lower[i] = tolower(command[i]);
    }

    /* Critical operations */
    if (strstr(lower, "rm -rf /") || strstr(lower, "format") || strstr(lower, "dd if=")) {
        return IMPACT_CRITICAL;
    }

    /* High impact */
    if (strstr(lower, "rm -rf") || strstr(lower, "shutdown") || strstr(lower, "reboot")) {
        return IMPACT_HIGH;
    }

    /* Medium impact */
    if (strstr(lower, "rm ") || strstr(lower, "kill") || strstr(lower, "chmod")) {
        return IMPACT_MEDIUM;
    }

    /* Low impact */
    if (strstr(lower, "mkdir") || strstr(lower, "touch") || strstr(lower, "cp")) {
        return IMPACT_LOW;
    }

    /* Safe (read-only) */
    return IMPACT_SAFE;
}

/* Check if requires permission */
bool companion_requires_permission(const char* command, permission_level_t* out_level) {
    if (!command) {
        return false;
    }

    char lower[512] = {0};
    for (size_t i = 0; i < sizeof(lower) - 1 && command[i]; i++) {
        lower[i] = tolower(command[i]);
    }

    /* Root-level operations */
    if (strstr(lower, "sudo") || strstr(lower, "su ") ||
        strstr(lower, "/etc/") || strstr(lower, "/sys/")) {
        if (out_level) *out_level = PERM_ROOT;
        return true;
    }

    /* Admin-level operations */
    if (strstr(lower, "systemctl") || strstr(lower, "service") ||
        strstr(lower, "firewall") || strstr(lower, "iptables")) {
        if (out_level) *out_level = PERM_ADMIN;
        return true;
    }

    if (out_level) *out_level = PERM_USER;
    return false;
}

/* Process user input */
companion_response_t* companion_process_input(companion_context_t* ctx, const char* input) {
    if (!ctx || !input) {
        return NULL;
    }

    companion_response_t* response = (companion_response_t*)calloc(1, sizeof(companion_response_t));
    if (!response) {
        return NULL;
    }

    ctx->state = COMPANION_STATE_PROCESSING;

    /* Check for clarification needed */
    if (companion_requires_clarification(input)) {
        response->type = RESPONSE_CLARIFICATION;
        snprintf(response->message, sizeof(response->message),
                "I need more information. Could you be more specific about what you want to do?");
        ctx->clarifications_requested++;
        ctx->state = COMPANION_STATE_IDLE;
        return response;
    }

    /* Parse intent */
    action_type_t intent = companion_parse_intent(input);

    /* Create action card */
    action_card_t* card = companion_create_action_card(input, intent);
    if (!card) {
        response->type = RESPONSE_ERROR;
        snprintf(response->message, sizeof(response->message), "Failed to create action card");
        ctx->state = COMPANION_STATE_ERROR;
        return response;
    }

    /* Generate action based on intent */
    switch (intent) {
        case ACTION_TYPE_INSTALL:
            companion_add_step(card, "Update package database", "apt update");
            companion_add_step(card, "Install package", input);
            companion_set_impact(card, IMPACT_MEDIUM);
            companion_set_permission(card, PERM_ADMIN);
            break;

        case ACTION_TYPE_FILE_OP:
            companion_add_step(card, "Execute file operation", input);
            companion_set_impact(card, companion_assess_impact(input));
            break;

        case ACTION_TYPE_SETTINGS:
            companion_add_step(card, "Change system setting", input);
            companion_set_impact(card, IMPACT_MEDIUM);
            companion_set_permission(card, PERM_ADMIN);
            break;

        case ACTION_TYPE_COMMAND:
        default:
            companion_add_step(card, "Execute command", input);
            companion_set_impact(card, companion_assess_impact(input));

            permission_level_t perm;
            if (companion_requires_permission(input, &perm)) {
                companion_set_permission(card, perm);
            }
            break;
    }

    /* Set summary */
    snprintf(card->summary, sizeof(card->summary),
             "Execute: %s\nImpact: %s\nPermission: %s",
             input,
             card->impact == IMPACT_SAFE ? "Safe" :
             card->impact == IMPACT_LOW ? "Low" :
             card->impact == IMPACT_MEDIUM ? "Medium" :
             card->impact == IMPACT_HIGH ? "High" : "Critical",
             card->permission == PERM_USER ? "User" :
             card->permission == PERM_ADMIN ? "Admin" : "Root");

    /* Store as pending action */
    ctx->pending_action = card;
    ctx->state = COMPANION_STATE_AWAITING_APPROVAL;

    /* Return action card */
    response->type = RESPONSE_ACTION_CARD;
    snprintf(response->message, sizeof(response->message), "Action card created");
    response->action_card = card;

    return response;
}

/* Free response */
void companion_free_response(companion_response_t* response) {
    if (response) {
        free(response);
    }
}

/* Show preview of action card */
void companion_show_preview(action_card_t* card) {
    if (!card) {
        return;
    }

    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║ ACTION CARD #%llu                                            \n", card->id);
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ Title: %s\n", card->title);
    printf("║\n");
    printf("║ Summary:\n");
    printf("║ %s\n", card->summary);
    printf("║\n");
    printf("║ Steps:\n");

    uint32_t step_num = 1;
    action_step_t* step = card->steps;
    while (step) {
        printf("║   %d. %s\n", step_num, step->description);
        printf("║      → %s\n", step->command);
        step = step->next;
        step_num++;
    }

    printf("║\n");
    printf("║ Impact: %s\n",
           card->impact == IMPACT_SAFE ? "SAFE" :
           card->impact == IMPACT_LOW ? "LOW" :
           card->impact == IMPACT_MEDIUM ? "MEDIUM" :
           card->impact == IMPACT_HIGH ? "HIGH" : "CRITICAL");

    printf("║ Permission: %s\n",
           card->permission == PERM_USER ? "USER" :
           card->permission == PERM_ADMIN ? "ADMIN" : "ROOT");

    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ Actions: [A]pprove  [D]eny  [E]dit                          \n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

/* Approve action */
status_t companion_approve_action(companion_context_t* ctx, action_card_t* card) {
    if (!ctx || !card) {
        return STATUS_INVALID;
    }

    card->approved = true;
    card->approved_time = (uint64_t)time(NULL);
    ctx->actions_approved++;

    printf("[AI COMPANION] Action #%llu approved\n", card->id);
    return STATUS_OK;
}

/* Deny action */
status_t companion_deny_action(companion_context_t* ctx, action_card_t* card) {
    if (!ctx || !card) {
        return STATUS_INVALID;
    }

    card->approved = false;
    ctx->actions_denied++;

    printf("[AI COMPANION] Action #%llu denied\n", card->id);
    return STATUS_OK;
}

/* Execute action */
status_t companion_execute_action(companion_context_t* ctx, action_card_t* card) {
    if (!ctx || !card || !card->approved) {
        return STATUS_INVALID;
    }

    ctx->state = COMPANION_STATE_EXECUTING;

    printf("[AI COMPANION] Executing action #%llu...\n", card->id);

    action_step_t* step = card->steps;
    uint32_t step_num = 1;

    while (step) {
        printf("  [%d/%d] %s\n", step_num, card->step_count, step->description);
        printf("        → %s\n", step->command);

        /* Execute command */
        int result = system(step->command);
        step->exit_code = result;
        step->completed = true;

        if (result != 0) {
            printf("        ✗ Failed (exit code: %d)\n", result);
            ctx->state = COMPANION_STATE_ERROR;
            return STATUS_ERROR;
        } else {
            printf("        ✓ Success\n");
        }

        step = step->next;
        step_num++;
    }

    card->executed = true;
    card->executed_time = (uint64_t)time(NULL);
    ctx->commands_executed++;
    ctx->state = COMPANION_STATE_IDLE;

    /* Add to history */
    companion_add_to_history(ctx, card);

    printf("[AI COMPANION] Action #%llu completed successfully\n", card->id);
    return STATUS_OK;
}

/* Add to history */
void companion_add_to_history(companion_context_t* ctx, action_card_t* card) {
    if (!ctx || !card || ctx->history_count >= ctx->history_capacity) {
        return;
    }

    ctx->history[ctx->history_count] = *card;
    ctx->history_count++;
}

/* Set force mode */
void companion_set_force_mode(companion_context_t* ctx, bool enable) {
    if (ctx) {
        ctx->force_mode = enable;
        printf("[AI COMPANION] Force mode: %s\n", enable ? "ENABLED" : "DISABLED");
    }
}

/* Set verbose mode */
void companion_set_verbose(companion_context_t* ctx, bool enable) {
    if (ctx) {
        ctx->verbose = enable;
    }
}
