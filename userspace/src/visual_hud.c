/*
 * Visual HUD and Style System Implementation
 * Military-grade minimalist futuristic design
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "visual_hud.h"

/* Maximum HUD elements */
#define MAX_HUD_ELEMENTS 256

/* Global HUD state */
static struct {
    hud_element_t* elements[MAX_HUD_ELEMENTS];
    uint32_t element_count;
    uint64_t next_id;
    visual_style_t style;
    bool initialized;
    bool sound_enabled;
    uint32_t sound_volume;
} hud_state = {0};

/* Default military/professional style */
static const visual_style_t default_style = {
    .dark_mode = true,
    .primary_color = COLOR_GUNMETAL_GRAY,
    .secondary_color = COLOR_DEEP_NAVY,
    .accent_color = COLOR_NEON_CYAN,
    .background_color = COLOR_MATTE_BLACK,
    .text_color = {0xFF, 0xFF, 0xFF, 0xFF},
    .font_family = "Roboto Mono",
    .font_size_base = 14,
    .font_weight = 500,
    .glass_effects = true,
    .neon_accents = true,
    .animations_enabled = true,
    .animation_speed = 50,
    .sound_enabled = true,
    .sound_volume = 50,
    .spacing_base = 8,
    .border_radius = 4,
    .show_shadows = true
};

/* Initialize visual system */
int visual_init(void) {
    if (hud_state.initialized) {
        return -1;
    }

    hud_state.element_count = 0;
    hud_state.next_id = 1;
    hud_state.style = default_style;
    hud_state.sound_enabled = true;
    hud_state.sound_volume = 50;
    hud_state.initialized = true;

    fprintf(stderr, "[VISUAL] HUD system initialized (military theme)\n");
    return 0;
}

/* Create HUD element */
hud_element_t* hud_create_element(hud_element_type_t type, hud_position_t position) {
    if (hud_state.element_count >= MAX_HUD_ELEMENTS) {
        return NULL;
    }

    hud_element_t* element = (hud_element_t*)calloc(1, sizeof(hud_element_t));
    if (!element) {
        return NULL;
    }

    element->id = hud_state.next_id++;
    element->type = type;
    element->position = position;
    element->visible = true;
    element->transparent = true;
    element->opacity = 230;  /* Slightly transparent */
    element->background = hud_state.style.background_color;
    element->border = hud_state.style.accent_color;
    element->border_width = 1;
    element->animation = ANIM_FADE_IN;
    element->animation_duration_ms = 200;

    /* Set default size based on type */
    switch (type) {
        case HUD_SYSTEM_MONITOR:
            element->width = 300;
            element->height = 200;
            break;
        case HUD_NOTIFICATION:
            element->width = 350;
            element->height = 80;
            break;
        case HUD_PROGRESS_INDICATOR:
            element->width = 400;
            element->height = 60;
            break;
        case HUD_ACTION_CARD:
            element->width = 500;
            element->height = 300;
            break;
        default:
            element->width = 200;
            element->height = 100;
            break;
    }

    /* Calculate position */
    /* In production, this would use screen dimensions */
    uint32_t screen_width = 1920;
    uint32_t screen_height = 1080;

    switch (position) {
        case HUD_POS_TOP_LEFT:
            element->x = 20;
            element->y = 20;
            break;
        case HUD_POS_TOP_RIGHT:
            element->x = screen_width - element->width - 20;
            element->y = 20;
            break;
        case HUD_POS_BOTTOM_LEFT:
            element->x = 20;
            element->y = screen_height - element->height - 20;
            break;
        case HUD_POS_BOTTOM_RIGHT:
            element->x = screen_width - element->width - 20;
            element->y = screen_height - element->height - 20;
            break;
        case HUD_POS_TOP_CENTER:
            element->x = (screen_width - element->width) / 2;
            element->y = 20;
            break;
        case HUD_POS_BOTTOM_CENTER:
            element->x = (screen_width - element->width) / 2;
            element->y = screen_height - element->height - 20;
            break;
        case HUD_POS_CENTER:
            element->x = (screen_width - element->width) / 2;
            element->y = (screen_height - element->height) / 2;
            break;
        default:
            element->x = 0;
            element->y = 0;
            break;
    }

    hud_state.elements[hud_state.element_count++] = element;

    return element;
}

/* Destroy HUD element */
int hud_destroy_element(uint64_t element_id) {
    for (uint32_t i = 0; i < hud_state.element_count; i++) {
        if (hud_state.elements[i]->id == element_id) {
            hud_element_t* element = hud_state.elements[i];

            /* Free content if allocated */
            if (element->content) {
                free(element->content);
            }

            free(element);

            /* Shift remaining elements */
            for (uint32_t j = i; j < hud_state.element_count - 1; j++) {
                hud_state.elements[j] = hud_state.elements[j + 1];
            }
            hud_state.element_count--;

            return 0;
        }
    }
    return -1;
}

/* Show element */
int hud_show_element(uint64_t element_id) {
    for (uint32_t i = 0; i < hud_state.element_count; i++) {
        if (hud_state.elements[i]->id == element_id) {
            hud_state.elements[i]->visible = true;
            return 0;
        }
    }
    return -1;
}

/* Hide element */
int hud_hide_element(uint64_t element_id) {
    for (uint32_t i = 0; i < hud_state.element_count; i++) {
        if (hud_state.elements[i]->id == element_id) {
            hud_state.elements[i]->visible = false;
            return 0;
        }
    }
    return -1;
}

/* Set opacity */
int hud_set_opacity(uint64_t element_id, uint8_t opacity) {
    for (uint32_t i = 0; i < hud_state.element_count; i++) {
        if (hud_state.elements[i]->id == element_id) {
            hud_state.elements[i]->opacity = opacity;
            return 0;
        }
    }
    return -1;
}

/* Set position */
int hud_set_position(uint64_t element_id, int32_t x, int32_t y) {
    for (uint32_t i = 0; i < hud_state.element_count; i++) {
        if (hud_state.elements[i]->id == element_id) {
            hud_state.elements[i]->x = x;
            hud_state.elements[i]->y = y;
            hud_state.elements[i]->position = HUD_POS_CUSTOM;
            return 0;
        }
    }
    return -1;
}

/* Animate element */
int hud_animate(uint64_t element_id, animation_type_t animation, uint32_t duration_ms) {
    for (uint32_t i = 0; i < hud_state.element_count; i++) {
        if (hud_state.elements[i]->id == element_id) {
            hud_state.elements[i]->animation = animation;
            hud_state.elements[i]->animation_duration_ms = duration_ms;
            return 0;
        }
    }
    return -1;
}

/* Create system monitor */
hud_element_t* hud_create_system_monitor(hud_position_t position) {
    hud_element_t* element = hud_create_element(HUD_SYSTEM_MONITOR, position);
    if (!element) {
        return NULL;
    }

    /* Allocate monitor data */
    system_monitor_data_t* data = (system_monitor_data_t*)calloc(1, sizeof(system_monitor_data_t));
    if (!data) {
        hud_destroy_element(element->id);
        return NULL;
    }

    data->show_graphs = true;
    data->show_details = true;

    element->content = data;
    element->content_size = sizeof(system_monitor_data_t);

    return element;
}

/* Update system monitor */
int hud_update_system_monitor(uint64_t element_id, const system_monitor_data_t* data) {
    if (!data) {
        return -1;
    }

    for (uint32_t i = 0; i < hud_state.element_count; i++) {
        if (hud_state.elements[i]->id == element_id &&
            hud_state.elements[i]->type == HUD_SYSTEM_MONITOR) {

            system_monitor_data_t* monitor = (system_monitor_data_t*)hud_state.elements[i]->content;
            if (monitor) {
                *monitor = *data;
            }
            return 0;
        }
    }
    return -1;
}

/* Show notification */
hud_element_t* hud_show_notification(const char* title, const char* message,
                                      uint32_t duration_ms, sound_effect_t sound) {
    if (!title || !message) {
        return NULL;
    }

    hud_element_t* element = hud_create_element(HUD_NOTIFICATION, HUD_POS_TOP_RIGHT);
    if (!element) {
        return NULL;
    }

    /* Allocate notification data */
    notification_data_t* data = (notification_data_t*)calloc(1, sizeof(notification_data_t));
    if (!data) {
        hud_destroy_element(element->id);
        return NULL;
    }

    strncpy(data->title, title, sizeof(data->title) - 1);
    strncpy(data->message, message, sizeof(data->message) - 1);
    data->sound = sound;
    data->accent_color = hud_state.style.accent_color;
    data->duration_ms = duration_ms;
    data->dismissible = true;

    element->content = data;
    element->content_size = sizeof(notification_data_t);

    /* Play sound */
    sound_play(sound);

    fprintf(stderr, "[HUD] Notification: %s - %s\n", title, message);

    return element;
}

/* Dismiss notification */
int hud_dismiss_notification(uint64_t element_id) {
    return hud_destroy_element(element_id);
}

/* Show progress */
hud_element_t* hud_show_progress(const char* label, uint32_t current, uint32_t total) {
    hud_element_t* element = hud_create_element(HUD_PROGRESS_INDICATOR, HUD_POS_BOTTOM_CENTER);
    if (!element) {
        return NULL;
    }

    /* Allocate progress data */
    progress_data_t* data = (progress_data_t*)calloc(1, sizeof(progress_data_t));
    if (!data) {
        hud_destroy_element(element->id);
        return NULL;
    }

    if (label) {
        strncpy(data->label, label, sizeof(data->label) - 1);
    }
    data->current = current;
    data->total = total;
    data->indeterminate = (total == 0);
    data->bar_color = hud_state.style.accent_color;

    element->content = data;
    element->content_size = sizeof(progress_data_t);

    return element;
}

/* Update progress */
int hud_update_progress(uint64_t element_id, uint32_t current, uint32_t total) {
    for (uint32_t i = 0; i < hud_state.element_count; i++) {
        if (hud_state.elements[i]->id == element_id &&
            hud_state.elements[i]->type == HUD_PROGRESS_INDICATOR) {

            progress_data_t* progress = (progress_data_t*)hud_state.elements[i]->content;
            if (progress) {
                progress->current = current;
                progress->total = total;
                progress->indeterminate = (total == 0);
            }
            return 0;
        }
    }
    return -1;
}

/* Show spinner */
hud_element_t* hud_show_spinner(const char* label) {
    return hud_show_progress(label, 0, 0);  /* Indeterminate progress */
}

/* Play sound effect */
int sound_play(sound_effect_t effect) {
    if (!hud_state.sound_enabled || effect == SOUND_NONE) {
        return 0;
    }

    const char* sound_name = "unknown";
    switch (effect) {
        case SOUND_CLICK: sound_name = "click"; break;
        case SOUND_CONFIRM: sound_name = "confirm"; break;
        case SOUND_ERROR: sound_name = "error"; break;
        case SOUND_NOTIFICATION: sound_name = "notification"; break;
        case SOUND_SUCCESS: sound_name = "success"; break;
        case SOUND_WARNING: sound_name = "warning"; break;
        case SOUND_STARTUP: sound_name = "startup"; break;
        case SOUND_SHUTDOWN: sound_name = "shutdown"; break;
        default: break;
    }

    /* In production, this would play actual sound file */
    fprintf(stderr, "[SOUND] Playing: %s (volume: %u)\n", sound_name, hud_state.sound_volume);

    return 0;
}

/* Set sound volume */
int sound_set_volume(uint32_t volume) {
    if (volume > 100) {
        volume = 100;
    }
    hud_state.sound_volume = volume;
    return 0;
}

/* Enable sound */
int sound_enable(void) {
    hud_state.sound_enabled = true;
    return 0;
}

/* Disable sound */
int sound_disable(void) {
    hud_state.sound_enabled = false;
    return 0;
}

/* Set visual style */
int visual_set_style(const visual_style_t* style) {
    if (!style) {
        return -1;
    }

    hud_state.style = *style;
    fprintf(stderr, "[VISUAL] Style updated\n");
    return 0;
}

/* Get visual style */
int visual_get_style(visual_style_t* style) {
    if (!style) {
        return -1;
    }

    *style = hud_state.style;
    return 0;
}

/* Set dark mode */
int visual_set_dark_mode(bool enabled) {
    hud_state.style.dark_mode = enabled;

    if (enabled) {
        hud_state.style.background_color = COLOR_MATTE_BLACK;
        hud_state.style.text_color = (color_rgba_t){0xFF, 0xFF, 0xFF, 0xFF};
    } else {
        hud_state.style.background_color = (color_rgba_t){0xF0, 0xF0, 0xF0, 0xFF};
        hud_state.style.text_color = (color_rgba_t){0x12, 0x12, 0x12, 0xFF};
    }

    fprintf(stderr, "[VISUAL] Dark mode: %s\n", enabled ? "enabled" : "disabled");
    return 0;
}

/* Set animations */
int visual_set_animations(bool enabled) {
    hud_state.style.animations_enabled = enabled;
    return 0;
}

/* Set glass effects */
int visual_set_glass_effects(bool enabled) {
    hud_state.style.glass_effects = enabled;
    return 0;
}

/* Set neon accents */
int visual_set_neon_accents(bool enabled) {
    hud_state.style.neon_accents = enabled;

    if (enabled) {
        hud_state.style.accent_color = COLOR_NEON_CYAN;
    } else {
        hud_state.style.accent_color = COLOR_GUNMETAL_GRAY;
    }

    return 0;
}

/* Render all HUD elements */
int hud_render_all(void) {
    /* In production, this would render to framebuffer */
    /* For now, just count visible elements */
    uint32_t visible_count = 0;
    for (uint32_t i = 0; i < hud_state.element_count; i++) {
        if (hud_state.elements[i]->visible) {
            visible_count++;
        }
    }

    return visible_count;
}

/* Cleanup HUD */
void hud_cleanup(void) {
    for (uint32_t i = 0; i < hud_state.element_count; i++) {
        if (hud_state.elements[i]) {
            if (hud_state.elements[i]->content) {
                free(hud_state.elements[i]->content);
            }
            free(hud_state.elements[i]);
        }
    }
    hud_state.element_count = 0;
}
