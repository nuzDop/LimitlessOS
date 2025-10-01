/*
 * Action Card System Implementation
 * Handles action preview, approval, and execution
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "action_card.h"

/* Maximum stored action cards */
#define MAX_ACTION_CARDS 1024
#define MAX_CONSENT_RULES 256

/* Global state */
static struct {
    action_card_t* cards[MAX_ACTION_CARDS];
    uint32_t card_count;
    consent_rule_t rules[MAX_CONSENT_RULES];
    uint32_t rule_count;
    action_card_settings_t settings;
    uint64_t next_id;
    bool initialized;
} action_state = {0};

/* Get current timestamp (nanoseconds) */
static uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

/* Initialize action card system */
int action_card_init(void) {
    if (action_state.initialized) {
        return 0;
    }

    /* Default settings */
    action_state.settings.enabled = true;
    action_state.settings.force_bypass_enabled = false;  /* Disabled by default for security */
    action_state.settings.show_technical_details = true;
    action_state.settings.require_confirmation = true;
    action_state.settings.approval_timeout_sec = 300;  /* 5 minutes */
    action_state.settings.audit_log_enabled = true;
    action_state.settings.undo_enabled = true;
    action_state.settings.undo_history_size = 100;

    action_state.card_count = 0;
    action_state.rule_count = 0;
    action_state.next_id = 1;
    action_state.initialized = true;

    return 0;
}

/* Create new action card */
action_card_t* action_card_create(action_type_t type, const char* title, const char* summary) {
    if (!title || !summary) {
        return NULL;
    }

    if (action_state.card_count >= MAX_ACTION_CARDS) {
        return NULL;
    }

    action_card_t* card = (action_card_t*)calloc(1, sizeof(action_card_t));
    if (!card) {
        return NULL;
    }

    card->id = action_state.next_id++;
    card->type = type;
    card->privilege = PRIVILEGE_USER;
    card->response = RESPONSE_PENDING;

    strncpy(card->title, title, ACTION_TITLE_SIZE - 1);
    strncpy(card->summary, summary, ACTION_SUMMARY_SIZE - 1);

    card->step_count = 0;
    card->permission_count = 0;
    card->can_undo = false;
    card->created_at = get_timestamp_ns();
    card->executed = false;
    card->force = false;

    /* Get current user (would use actual user ID in production) */
    card->user_id = getuid();
    strncpy(card->user_name, getenv("USER") ?: "unknown", 63);

    action_state.cards[action_state.card_count++] = card;

    return card;
}

/* Set command */
void action_card_set_command(action_card_t* card, const char* command) {
    if (card && command) {
        strncpy(card->command, command, ACTION_COMMAND_SIZE - 1);
    }
}

/* Set details */
void action_card_set_details(action_card_t* card, const char* details) {
    if (card && details) {
        strncpy(card->details, details, ACTION_DETAIL_SIZE - 1);
    }
}

/* Set privilege level */
void action_card_set_privilege(action_card_t* card, action_privilege_t privilege) {
    if (card) {
        card->privilege = privilege;
    }
}

/* Set force flag */
void action_card_set_force(action_card_t* card, bool force) {
    if (card) {
        card->force = force;
    }
}

/* Assess action impact */
void action_card_assess_impact(action_card_t* card) {
    if (!card) {
        return;
    }

    action_impact_t* impact = &card->impact;

    /* Default: not reversible */
    impact->reversible = card->can_undo;
    impact->persistent = true;
    impact->affects_security = false;
    impact->affects_privacy = false;
    impact->affects_data = false;
    impact->risk_level = 0;

    /* Assess based on action type */
    switch (card->type) {
        case ACTION_FILE_DELETE:
            impact->reversible = false;
            impact->affects_data = true;
            impact->risk_level = 70;
            break;

        case ACTION_FILE_WRITE:
            impact->reversible = true;
            impact->affects_data = true;
            impact->risk_level = 30;
            break;

        case ACTION_INSTALL:
        case ACTION_UNINSTALL:
            impact->reversible = true;
            impact->affects_security = true;
            impact->risk_level = 40;
            break;

        case ACTION_SETTING_CHANGE:
            impact->reversible = true;
            impact->risk_level = 25;
            break;

        case ACTION_PERMISSION:
            impact->reversible = true;
            impact->affects_security = true;
            impact->affects_privacy = true;
            impact->risk_level = 80;
            break;

        case ACTION_CODE_EXEC:
            impact->reversible = false;
            impact->affects_security = true;
            impact->affects_data = true;
            impact->risk_level = 60;
            break;

        case ACTION_NETWORK:
            impact->reversible = false;
            impact->affects_privacy = true;
            impact->risk_level = 50;
            break;

        case ACTION_COMMAND:
            /* Analyze command for risk */
            if (strstr(card->command, "rm ") || strstr(card->command, "del ")) {
                impact->risk_level = 65;
                impact->affects_data = true;
            } else if (strstr(card->command, "sudo") || strstr(card->command, "admin")) {
                impact->risk_level = 75;
                impact->affects_security = true;
            } else {
                impact->risk_level = 20;
            }
            break;

        default:
            impact->risk_level = 50;
            break;
    }

    /* Increase risk for admin/system operations */
    if (card->privilege == PRIVILEGE_ADMIN) {
        impact->risk_level += 15;
    } else if (card->privilege == PRIVILEGE_SYSTEM) {
        impact->risk_level += 25;
    } else if (card->privilege == PRIVILEGE_SECURITY) {
        impact->risk_level += 35;
    }

    /* Cap at 100 */
    if (impact->risk_level > 100) {
        impact->risk_level = 100;
    }
}

/* Set reversibility */
void action_card_set_reversible(action_card_t* card, bool reversible, const char* undo_cmd) {
    if (!card) {
        return;
    }

    card->can_undo = reversible;
    if (reversible && undo_cmd) {
        strncpy(card->undo_command, undo_cmd, ACTION_COMMAND_SIZE - 1);
    }
}

/* Add step */
void action_card_add_step(action_card_t* card, const char* description) {
    if (!card || !description || card->step_count >= ACTION_MAX_STEPS) {
        return;
    }

    action_step_t* step = &card->steps[card->step_count++];
    strncpy(step->description, description, ACTION_STEP_DESC_SIZE - 1);
    step->completed = false;
    step->timestamp = 0;
}

/* Complete step */
void action_card_complete_step(action_card_t* card, uint32_t step_index) {
    if (!card || step_index >= card->step_count) {
        return;
    }

    card->steps[step_index].completed = true;
    card->steps[step_index].timestamp = get_timestamp_ns();
}

/* Add permission */
void action_card_add_permission(action_card_t* card, const char* permission) {
    if (!card || !permission || card->permission_count >= ACTION_MAX_PERMISSIONS) {
        return;
    }

    strncpy(card->permissions[card->permission_count++], permission, 63);
}

/* Present action card to user */
int action_card_present(action_card_t* card) {
    if (!card) {
        return -1;
    }

    /* Check if action cards are disabled */
    if (!action_state.settings.enabled) {
        /* Auto-approve if disabled */
        card->response = RESPONSE_APPROVED;
        return 0;
    }

    /* Check if force bypass is enabled and card has force flag */
    if (action_state.settings.force_bypass_enabled && card->force) {
        card->response = RESPONSE_APPROVED;
        return 0;
    }

    /* Check consent rules */
    consent_policy_t policy = action_card_check_consent(card->type, card->command);
    if (policy == CONSENT_ALLOW_ALWAYS) {
        card->response = RESPONSE_APPROVED;
        return 0;
    } else if (policy == CONSENT_DENY_ALWAYS) {
        card->response = RESPONSE_DENIED;
        return -1;
    }

    /* Assess impact before presenting */
    action_card_assess_impact(card);

    card->presented_at = get_timestamp_ns();

    /* Render action card for display */
    char rendered[4096];
    action_card_render(card, rendered, sizeof(rendered));
    printf("%s\n", rendered);

    return 0;
}

/* Wait for user response */
action_response_t action_card_wait_response(action_card_t* card, uint32_t timeout_sec) {
    if (!card) {
        return RESPONSE_DENIED;
    }

    /* If already responded, return that response */
    if (card->response != RESPONSE_PENDING) {
        return card->response;
    }

    /* Read user input */
    printf("\n[Approve/Deny/Edit] (a/d/e): ");
    fflush(stdout);

    char input[256] = {0};
    if (fgets(input, sizeof(input), stdin)) {
        /* Trim newline */
        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "a") == 0 || strcmp(input, "approve") == 0) {
            card->response = RESPONSE_APPROVED;
        } else if (strcmp(input, "e") == 0 || strcmp(input, "edit") == 0) {
            card->response = RESPONSE_EDITED;
        } else {
            card->response = RESPONSE_DENIED;
        }
    } else {
        card->response = RESPONSE_DENIED;
    }

    card->responded_at = get_timestamp_ns();
    return card->response;
}

/* Edit action card */
int action_card_edit(action_card_t* card, const char* new_command) {
    if (!card || !new_command) {
        return -1;
    }

    /* Save original command for audit */
    char original[ACTION_COMMAND_SIZE];
    strncpy(original, card->command, ACTION_COMMAND_SIZE - 1);

    /* Update command */
    strncpy(card->command, new_command, ACTION_COMMAND_SIZE - 1);

    /* Re-assess impact */
    action_card_assess_impact(card);

    return 0;
}

/* Execute action */
int action_card_execute(action_card_t* card) {
    if (!card) {
        return -1;
    }

    /* Check if approved */
    if (card->response != RESPONSE_APPROVED && card->response != RESPONSE_EDITED) {
        return -1;
    }

    card->executed_at = get_timestamp_ns();

    /* Execute command */
    FILE* fp = popen(card->command, "r");
    if (!fp) {
        card->executed = false;
        card->exit_code = -1;
        return -1;
    }

    /* Read output */
    size_t output_len = 0;
    while (output_len < sizeof(card->output) - 1 &&
           fgets(card->output + output_len, sizeof(card->output) - output_len, fp)) {
        output_len = strlen(card->output);
    }

    /* Get exit code */
    card->exit_code = pclose(fp);
    card->executed = true;

    /* Log to audit if enabled */
    if (action_state.settings.audit_log_enabled) {
        action_card_audit_log(card);
    }

    return card->exit_code;
}

/* Undo action */
int action_card_undo(uint64_t action_id) {
    /* Find action card */
    action_card_t* card = NULL;
    for (uint32_t i = 0; i < action_state.card_count; i++) {
        if (action_state.cards[i]->id == action_id) {
            card = action_state.cards[i];
            break;
        }
    }

    if (!card || !card->can_undo || !card->executed) {
        return -1;
    }

    /* Execute undo command */
    int result = system(card->undo_command);
    return result;
}

/* Check consent */
consent_policy_t action_card_check_consent(action_type_t type, const char* command) {
    for (uint32_t i = 0; i < action_state.rule_count; i++) {
        consent_rule_t* rule = &action_state.rules[i];
        if (!rule->active) {
            continue;
        }

        if (rule->action_type == type) {
            /* Check pattern match if provided */
            if (rule->pattern[0] != '\0' && command) {
                /* Simple substring match (production would use regex) */
                if (strstr(command, rule->pattern)) {
                    return rule->policy;
                }
            } else {
                return rule->policy;
            }
        }
    }

    return CONSENT_ALWAYS_ASK;
}

/* Add consent rule */
int action_card_add_consent_rule(action_type_t type, consent_policy_t policy, const char* pattern) {
    if (action_state.rule_count >= MAX_CONSENT_RULES) {
        return -1;
    }

    consent_rule_t* rule = &action_state.rules[action_state.rule_count++];
    rule->action_type = type;
    rule->policy = policy;
    if (pattern) {
        strncpy(rule->pattern, pattern, sizeof(rule->pattern) - 1);
    }
    rule->created_at = get_timestamp_ns();
    rule->active = true;

    return 0;
}

/* Get settings */
void action_card_get_settings(action_card_settings_t* settings) {
    if (settings) {
        *settings = action_state.settings;
    }
}

/* Set settings */
void action_card_set_settings(const action_card_settings_t* settings) {
    if (settings) {
        action_state.settings = *settings;
    }
}

/* Audit log */
int action_card_audit_log(const action_card_t* card) {
    if (!card) {
        return -1;
    }

    /* In production, this would write to a persistent log file */
    /* For now, just print to console */
    fprintf(stderr, "[AUDIT] Action %llu: %s (type=%d, status=%s, exit=%d)\n",
            card->id, card->title, card->type,
            card->response == RESPONSE_APPROVED ? "approved" :
            card->response == RESPONSE_DENIED ? "denied" : "other",
            card->exit_code);

    return 0;
}

/* Destroy action card */
void action_card_destroy(action_card_t* card) {
    if (card) {
        free(card);
    }
}

/* Render action card */
void action_card_render(const action_card_t* card, char* output, uint32_t output_size) {
    if (!card || !output || output_size == 0) {
        return;
    }

    char buffer[4096];
    int offset = 0;

    /* Header */
    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                      "\n╔════════════════════════════════════════════════════════════════╗\n");
    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                      "║ ACTION PREVIEW                                                 ║\n");
    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                      "╠════════════════════════════════════════════════════════════════╣\n");

    /* Title */
    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                      "║ %s\n", card->title);

    /* Summary */
    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                      "║ %s\n", card->summary);

    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                      "╠════════════════════════════════════════════════════════════════╣\n");

    /* Command */
    if (card->command[0]) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                          "║ Command: %s\n", card->command);
    }

    /* Steps */
    if (card->step_count > 0) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                          "║ Steps:\n");
        for (uint32_t i = 0; i < card->step_count; i++) {
            offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                              "║   %d. %s\n", i + 1, card->steps[i].description);
        }
    }

    /* Impact */
    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                      "║ Impact:\n");
    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                      "║   Risk Level: %u/100\n", card->impact.risk_level);
    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                      "║   Reversible: %s\n", card->impact.reversible ? "Yes" : "No");
    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                      "║   Security: %s\n", card->impact.affects_security ? "Affected" : "Not affected");

    /* Permissions */
    if (card->permission_count > 0) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                          "║ Permissions required:\n");
        for (uint32_t i = 0; i < card->permission_count; i++) {
            offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                              "║   - %s\n", card->permissions[i]);
        }
    }

    /* Footer */
    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                      "╚════════════════════════════════════════════════════════════════╝\n");

    /* Copy to output */
    strncpy(output, buffer, output_size - 1);
    output[output_size - 1] = '\0';
}
