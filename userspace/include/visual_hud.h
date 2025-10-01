/*
 * Visual HUD and Style System
 * Military-grade, minimalist, futuristic design elements
 */

#ifndef VISUAL_HUD_H
#define VISUAL_HUD_H

#include <stdint.h>
#include <stdbool.h>

/* Color palette - Military/Professional theme */
typedef struct color_rgba {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} color_rgba_t;

/* Predefined color scheme */
#define COLOR_MATTE_BLACK       (color_rgba_t){0x12, 0x12, 0x12, 0xFF}
#define COLOR_GUNMETAL_GRAY     (color_rgba_t){0x2C, 0x3E, 0x50, 0xFF}
#define COLOR_DEEP_NAVY         (color_rgba_t){0x1A, 0x25, 0x35, 0xFF}
#define COLOR_NEON_CYAN         (color_rgba_t){0x00, 0xFF, 0xFF, 0xFF}
#define COLOR_NEON_GREEN        (color_rgba_t){0x00, 0xFF, 0x41, 0xFF}
#define COLOR_NEON_ORANGE       (color_rgba_t){0xFF, 0x66, 0x00, 0xFF}
#define COLOR_GLASS_OVERLAY     (color_rgba_t){0xFF, 0xFF, 0xFF, 0x33}
#define COLOR_WARNING_AMBER     (color_rgba_t){0xFF, 0xAA, 0x00, 0xFF}
#define COLOR_ERROR_RED         (color_rgba_t){0xFF, 0x00, 0x33, 0xFF}

/* HUD element types */
typedef enum hud_element_type {
    HUD_SYSTEM_MONITOR,          /* CPU/RAM/NET monitor */
    HUD_NOTIFICATION,            /* Toast notification */
    HUD_ACTION_CARD,             /* Action preview card */
    HUD_TERMINAL_OVERLAY,        /* Terminal overlay */
    HUD_QUICK_ACTIONS,           /* Quick action menu */
    HUD_STATUS_BAR,              /* Status bar */
    HUD_PROGRESS_INDICATOR,      /* Progress bar/spinner */
    HUD_CONTEXT_MENU             /* Context menu */
} hud_element_type_t;

/* HUD position */
typedef enum hud_position {
    HUD_POS_TOP_LEFT,
    HUD_POS_TOP_CENTER,
    HUD_POS_TOP_RIGHT,
    HUD_POS_BOTTOM_LEFT,
    HUD_POS_BOTTOM_CENTER,
    HUD_POS_BOTTOM_RIGHT,
    HUD_POS_CENTER,
    HUD_POS_CUSTOM
} hud_position_t;

/* Animation types */
typedef enum animation_type {
    ANIM_NONE,
    ANIM_FADE_IN,
    ANIM_FADE_OUT,
    ANIM_SLIDE_IN,
    ANIM_SLIDE_OUT,
    ANIM_SCALE_IN,
    ANIM_SCALE_OUT,
    ANIM_PULSE,
    ANIM_GLOW
} animation_type_t;

/* Sound effect types */
typedef enum sound_effect {
    SOUND_NONE,
    SOUND_CLICK,                 /* Precise click */
    SOUND_CONFIRM,               /* Confirmation tone */
    SOUND_ERROR,                 /* Error beep */
    SOUND_NOTIFICATION,          /* Notification chime */
    SOUND_SUCCESS,               /* Success tone */
    SOUND_WARNING,               /* Warning beep */
    SOUND_STARTUP,               /* System startup */
    SOUND_SHUTDOWN               /* System shutdown */
} sound_effect_t;

/* HUD element */
typedef struct hud_element {
    uint64_t id;
    hud_element_type_t type;
    hud_position_t position;
    int32_t x;                   /* X position (custom position) */
    int32_t y;                   /* Y position (custom position) */
    uint32_t width;
    uint32_t height;
    bool visible;
    bool transparent;
    uint8_t opacity;             /* 0-255 */
    color_rgba_t background;
    color_rgba_t border;
    uint32_t border_width;
    animation_type_t animation;
    uint32_t animation_duration_ms;
    void* content;               /* Element-specific content */
    uint32_t content_size;
} hud_element_t;

/* System monitor data */
typedef struct system_monitor_data {
    uint32_t cpu_usage;          /* 0-100% */
    uint32_t ram_usage;          /* 0-100% */
    uint32_t disk_usage;         /* 0-100% */
    uint32_t network_rx_mbps;
    uint32_t network_tx_mbps;
    uint32_t temperature_celsius;
    uint32_t fan_rpm;
    bool show_graphs;
    bool show_details;
} system_monitor_data_t;

/* Notification */
typedef struct notification_data {
    char title[128];
    char message[512];
    sound_effect_t sound;
    color_rgba_t accent_color;
    uint32_t duration_ms;        /* 0 = persistent */
    bool dismissible;
} notification_data_t;

/* Progress indicator */
typedef struct progress_data {
    char label[128];
    uint32_t current;
    uint32_t total;
    bool indeterminate;          /* Spinner if true */
    color_rgba_t bar_color;
} progress_data_t;

/* Visual style settings */
typedef struct visual_style {
    /* Theme */
    bool dark_mode;
    color_rgba_t primary_color;
    color_rgba_t secondary_color;
    color_rgba_t accent_color;
    color_rgba_t background_color;
    color_rgba_t text_color;

    /* Typography */
    char font_family[64];
    uint32_t font_size_base;
    uint32_t font_weight;        /* 100-900 */

    /* Effects */
    bool glass_effects;
    bool neon_accents;
    bool animations_enabled;
    uint32_t animation_speed;    /* 0-100, 50=normal */
    bool sound_enabled;
    uint32_t sound_volume;       /* 0-100 */

    /* Layout */
    uint32_t spacing_base;       /* Base spacing unit (px) */
    uint32_t border_radius;      /* Corner rounding (px) */
    bool show_shadows;
} visual_style_t;

/* Initialize visual system */
int visual_init(void);

/* HUD management */
hud_element_t* hud_create_element(hud_element_type_t type, hud_position_t position);
int hud_destroy_element(uint64_t element_id);
int hud_show_element(uint64_t element_id);
int hud_hide_element(uint64_t element_id);
int hud_set_opacity(uint64_t element_id, uint8_t opacity);
int hud_set_position(uint64_t element_id, int32_t x, int32_t y);
int hud_animate(uint64_t element_id, animation_type_t animation, uint32_t duration_ms);

/* System monitor */
hud_element_t* hud_create_system_monitor(hud_position_t position);
int hud_update_system_monitor(uint64_t element_id, const system_monitor_data_t* data);

/* Notifications */
hud_element_t* hud_show_notification(const char* title, const char* message,
                                      uint32_t duration_ms, sound_effect_t sound);
int hud_dismiss_notification(uint64_t element_id);

/* Progress indicators */
hud_element_t* hud_show_progress(const char* label, uint32_t current, uint32_t total);
int hud_update_progress(uint64_t element_id, uint32_t current, uint32_t total);
hud_element_t* hud_show_spinner(const char* label);

/* Quick actions */
hud_element_t* hud_show_quick_actions(const char** actions, uint32_t count);

/* Sound effects */
int sound_play(sound_effect_t effect);
int sound_set_volume(uint32_t volume);
int sound_enable(void);
int sound_disable(void);

/* Visual style */
int visual_set_style(const visual_style_t* style);
int visual_get_style(visual_style_t* style);
int visual_set_dark_mode(bool enabled);
int visual_set_animations(bool enabled);
int visual_set_glass_effects(bool enabled);
int visual_set_neon_accents(bool enabled);

/* Render HUD (called by compositor) */
int hud_render_all(void);
int hud_render_element(uint64_t element_id, uint8_t* framebuffer, uint32_t width, uint32_t height);

/* HUD cleanup */
void hud_cleanup(void);

#endif /* VISUAL_HUD_H */
