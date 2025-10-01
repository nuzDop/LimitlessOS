#ifndef LIMITLESS_COMPOSITOR_H
#define LIMITLESS_COMPOSITOR_H

/*
 * LimitlessOS Desktop Compositor
 * Minimalist + futuristic design with military-grade aesthetics
 */

#include <stdint.h>
#include <stdbool.h>

/* Color definitions (ARGB format) */
typedef uint32_t color_t;

#define COLOR_ARGB(a, r, g, b) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))
#define COLOR_RGB(r, g, b) COLOR_ARGB(255, r, g, b)

/* Theme colors - Minimalist + Military Grade */
#define THEME_BG_DEEP_NAVY      COLOR_RGB(10, 15, 25)
#define THEME_BG_MATTE_BLACK    COLOR_RGB(18, 18, 20)
#define THEME_BG_GUNMETAL       COLOR_RGB(40, 44, 52)
#define THEME_FG_WHITE          COLOR_RGB(240, 242, 245)
#define THEME_FG_LIGHT_GREY     COLOR_RGB(180, 184, 190)
#define THEME_ACCENT_NEON_CYAN  COLOR_RGB(0, 200, 255)
#define THEME_ACCENT_NEON_GREEN COLOR_RGB(80, 250, 123)
#define THEME_ACCENT_WARNING    COLOR_RGB(255, 184, 108)
#define THEME_ACCENT_ERROR      COLOR_RGB(255, 85, 85)

/* Display mode */
typedef struct display_mode {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t refresh_rate;
} display_mode_t;

/* Framebuffer */
typedef struct framebuffer {
    uint32_t* pixels;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
} framebuffer_t;

/* Rectangle */
typedef struct rect {
    int32_t x, y;
    uint32_t width, height;
} rect_t;

/* Surface (off-screen buffer) */
typedef struct surface {
    uint32_t* pixels;
    uint32_t width;
    uint32_t height;
    bool dirty;
} surface_t;

/* Window */
typedef struct window {
    uint64_t id;
    char title[256];
    rect_t rect;
    surface_t* surface;
    bool visible;
    bool focused;
    bool decorated;
    int32_t z_order;
    struct window* next;
} window_t;

/* Compositor state */
typedef struct compositor {
    framebuffer_t* fb;
    window_t* windows;
    window_t* focused_window;
    uint32_t window_count;
    uint64_t next_window_id;
    bool running;

    /* Cursor */
    int32_t cursor_x;
    int32_t cursor_y;
    bool cursor_visible;

    /* FPS tracking */
    uint64_t frame_count;
    uint64_t last_fps_time;
    uint32_t fps;
} compositor_t;

/* Font (simple bitmap font) */
typedef struct font {
    uint8_t* glyph_data;
    uint32_t glyph_width;
    uint32_t glyph_height;
    uint32_t glyph_count;
} font_t;

/* Compositor API */
status_t compositor_init(uint32_t width, uint32_t height);
void compositor_shutdown(void);
compositor_t* compositor_get_instance(void);

/* Window management */
window_t* compositor_create_window(const char* title, int32_t x, int32_t y, uint32_t width, uint32_t height);
void compositor_destroy_window(window_t* window);
void compositor_show_window(window_t* window);
void compositor_hide_window(window_t* window);
void compositor_focus_window(window_t* window);
void compositor_move_window(window_t* window, int32_t x, int32_t y);
void compositor_resize_window(window_t* window, uint32_t width, uint32_t height);

/* Rendering */
void compositor_render_frame(void);
void compositor_clear(color_t color);
void compositor_present(void);

/* Drawing primitives */
void compositor_draw_rect(surface_t* surface, rect_t* rect, color_t color);
void compositor_fill_rect(surface_t* surface, rect_t* rect, color_t color);
void compositor_draw_line(surface_t* surface, int32_t x1, int32_t y1, int32_t x2, int32_t y2, color_t color);
void compositor_draw_text(surface_t* surface, int32_t x, int32_t y, const char* text, color_t color);
void compositor_draw_gradient(surface_t* surface, rect_t* rect, color_t start, color_t end, bool vertical);

/* Surface management */
surface_t* compositor_create_surface(uint32_t width, uint32_t height);
void compositor_destroy_surface(surface_t* surface);
void compositor_blit_surface(surface_t* dest, surface_t* src, int32_t dx, int32_t dy);

/* Effects */
void compositor_apply_blur(surface_t* surface, uint32_t radius);
void compositor_apply_shadow(surface_t* surface, int32_t offset_x, int32_t offset_y, uint32_t blur);
void compositor_apply_alpha(surface_t* surface, uint8_t alpha);

/* Input handling */
void compositor_handle_mouse_move(int32_t x, int32_t y);
void compositor_handle_mouse_button(uint8_t button, bool pressed);
void compositor_handle_key(uint32_t keycode, bool pressed);

/* Cursor */
void compositor_set_cursor_position(int32_t x, int32_t y);
void compositor_show_cursor(bool show);

/* Theme */
void compositor_set_theme_color(const char* name, color_t color);
color_t compositor_get_theme_color(const char* name);

/* Utilities */
bool rect_contains_point(rect_t* rect, int32_t x, int32_t y);
bool rect_intersects(rect_t* r1, rect_t* r2);
void rect_clip(rect_t* rect, rect_t* bounds);

#endif /* LIMITLESS_COMPOSITOR_H */
