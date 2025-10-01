/*
 * AI Companion - Interactive Demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "companion.h"

void print_banner(void) {
    printf("\n");
    printf("  ╔═══════════════════════════════════════════════════════════╗\n");
    printf("  ║                                                           ║\n");
    printf("  ║           LimitlessOS AI Companion v0.2.0                ║\n");
    printf("  ║                                                           ║\n");
    printf("  ║  Your AI-first interface to the operating system         ║\n");
    printf("  ║                                                           ║\n");
    printf("  ╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

void print_help(void) {
    printf("\n");
    printf("Commands:\n");
    printf("  help              - Show this help message\n");
    printf("  exit, quit        - Exit the companion\n");
    printf("  force [on|off]    - Enable/disable force mode (auto-approve)\n");
    printf("  verbose [on|off]  - Enable/disable verbose output\n");
    printf("  history           - Show action history\n");
    printf("  stats             - Show statistics\n");
    printf("\n");
    printf("Try asking me to do something, like:\n");
    printf("  - \"Install nginx\"\n");
    printf("  - \"Create a file named test.txt\"\n");
    printf("  - \"Show me the current directory\"\n");
    printf("  - \"List all files\"\n");
    printf("\n");
}

void print_stats(companion_context_t* ctx) {
    printf("\n");
    printf("=== Statistics ===\n");
    printf("Commands executed:        %llu\n", ctx->commands_executed);
    printf("Actions approved:         %llu\n", ctx->actions_approved);
    printf("Actions denied:           %llu\n", ctx->actions_denied);
    printf("Clarifications requested: %llu\n", ctx->clarifications_requested);
    printf("History entries:          %u\n", ctx->history_count);
    printf("\n");
}

void print_history(companion_context_t* ctx) {
    printf("\n");
    printf("=== Action History ===\n");

    if (ctx->history_count == 0) {
        printf("No actions in history\n");
        printf("\n");
        return;
    }

    for (uint32_t i = 0; i < ctx->history_count; i++) {
        action_card_t* card = &ctx->history[i];
        printf("%u. [#%llu] %s\n", i + 1, card->id, card->title);
        printf("   Status: %s\n", card->executed ? "Executed" : "Not executed");
        printf("   Impact: %s\n",
               card->impact == IMPACT_SAFE ? "Safe" :
               card->impact == IMPACT_LOW ? "Low" :
               card->impact == IMPACT_MEDIUM ? "Medium" :
               card->impact == IMPACT_HIGH ? "High" : "Critical");
        printf("\n");
    }
}

void handle_user_decision(companion_context_t* ctx, action_card_t* card) {
    char choice[32];

    printf("Your choice [A/D/E]: ");
    fflush(stdout);

    if (!fgets(choice, sizeof(choice), stdin)) {
        return;
    }

    /* Remove newline */
    choice[strcspn(choice, "\n")] = 0;

    /* Convert to uppercase */
    for (int i = 0; choice[i]; i++) {
        choice[i] = toupper(choice[i]);
    }

    if (strcmp(choice, "A") == 0 || strcmp(choice, "APPROVE") == 0) {
        companion_approve_action(ctx, card);
        companion_execute_action(ctx, card);
    } else if (strcmp(choice, "D") == 0 || strcmp(choice, "DENY") == 0) {
        companion_deny_action(ctx, card);
        printf("Action denied.\n");
    } else if (strcmp(choice, "E") == 0 || strcmp(choice, "EDIT") == 0) {
        printf("Edit mode not yet implemented.\n");
    } else {
        printf("Invalid choice. Action cancelled.\n");
    }
}

int main(int argc, char** argv) {
    print_banner();

    /* Initialize companion */
    companion_context_t* ctx = NULL;
    status_t status = companion_init(&ctx);

    if (FAILED(status) || !ctx) {
        fprintf(stderr, "ERROR: Failed to initialize AI companion\n");
        return 1;
    }

    printf("Type 'help' for commands, or ask me anything!\n");
    printf("\n");

    char input[1024];
    bool running = true;

    while (running) {
        printf("limitless> ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }

        /* Remove newline */
        input[strcspn(input, "\n")] = 0;

        /* Skip empty input */
        if (strlen(input) == 0) {
            continue;
        }

        /* Handle built-in commands */
        if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
            running = false;
            continue;
        }

        if (strcmp(input, "help") == 0) {
            print_help();
            continue;
        }

        if (strcmp(input, "stats") == 0) {
            print_stats(ctx);
            continue;
        }

        if (strcmp(input, "history") == 0) {
            print_history(ctx);
            continue;
        }

        if (strncmp(input, "force ", 6) == 0) {
            if (strcmp(input + 6, "on") == 0) {
                companion_set_force_mode(ctx, true);
            } else if (strcmp(input + 6, "off") == 0) {
                companion_set_force_mode(ctx, false);
            }
            continue;
        }

        if (strncmp(input, "verbose ", 8) == 0) {
            if (strcmp(input + 8, "on") == 0) {
                companion_set_verbose(ctx, true);
                printf("Verbose mode enabled\n");
            } else if (strcmp(input + 8, "off") == 0) {
                companion_set_verbose(ctx, false);
                printf("Verbose mode disabled\n");
            }
            continue;
        }

        /* Process input through companion */
        companion_response_t* response = companion_process_input(ctx, input);

        if (!response) {
            printf("ERROR: Failed to process input\n");
            continue;
        }

        /* Handle response */
        switch (response->type) {
            case RESPONSE_CLARIFICATION:
                printf("\n🤔 %s\n\n", response->message);
                break;

            case RESPONSE_INFORMATION:
                printf("\nℹ️  %s\n\n", response->message);
                break;

            case RESPONSE_ERROR:
                printf("\n❌ %s\n\n", response->message);
                break;

            case RESPONSE_ACTION_CARD:
                if (response->action_card) {
                    companion_show_preview(response->action_card);

                    if (ctx->force_mode) {
                        /* Auto-approve in force mode */
                        printf("Force mode enabled - auto-approving...\n");
                        companion_approve_action(ctx, response->action_card);
                        companion_execute_action(ctx, response->action_card);
                    } else {
                        /* Wait for user decision */
                        handle_user_decision(ctx, response->action_card);
                    }
                }
                break;
        }

        companion_free_response(response);
        printf("\n");
    }

    /* Cleanup */
    print_stats(ctx);
    companion_shutdown(ctx);

    printf("\nGoodbye!\n\n");
    return 0;
}
