/*
 * LimitlessOS Desktop Compositor
 * Minimalist + Futuristic design
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compositor.h"

/* Global compositor instance */
static compositor_t* g_compositor = NULL;

/* Initialize compositor */
status_t compositor_init(uint32_t width, uint32_t height) {
    if (g_compositor) {
        return STATUS_EXISTS;
    }

    printf("[COMPOSITOR] Initializing %ux%u compositor\n", width, height);

    g_compositor = (compositor_t*)calloc(1, sizeof(compositor_t));
    if (!g_compositor) {
        return STATUS_NOMEM;
    }

    /* Allocate framebuffer */
    g_compositor->fb = (framebuffer_t*)calloc(1, sizeof(framebuffer_t));
    if (!g_compositor->fb) {
        free(g_compositor);
        g_compositor = NULL;
        return STATUS_NOMEM;
    }

    g_compositor->fb->width = width;
    g_compositor->fb->height = height;
    g_compositor->fb->bpp = 32;
    g_compositor->fb->pitch = width * 4;

    /* Allocate pixel buffer */
    g_compositor->fb->pixels = (uint32_t*)calloc(width * height, sizeof(uint32_t));
    if (!g_compositor->fb->pixels) {
        free(g_compositor->fb);
        free(g_compositor);
        g_compositor = NULL;
        return STATUS_NOMEM;
    }

    /* Initialize state */
    g_compositor->windows = NULL;
    g_compositor->focused_window = NULL;
    g_compositor->window_count = 0;
    g_compositor->next_window_id = 1;
    g_compositor->running = true;

    g_compositor->cursor_x = width / 2;
    g_compositor->cursor_y = height / 2;
    g_compositor->cursor_visible = true;

    g_compositor->frame_count = 0;
    g_compositor->last_fps_time = 0;
    g_compositor->fps = 0;

    printf("[COMPOSITOR] Compositor initialized\n");
    return STATUS_OK;
}

/* Shutdown compositor */
void compositor_shutdown(void) {
    if (!g_compositor) {
        return;
    }

    /* Destroy all windows */
    window_t* window = g_compositor->windows;
    while (window) {
        window_t* next = window->next;
        compositor_destroy_window(window);
        window = next;
    }

    /* Free framebuffer */
    if (g_compositor->fb) {
        if (g_compositor->fb->pixels) {
            free(g_compositor->fb->pixels);
        }
        free(g_compositor->fb);
    }

    free(g_compositor);
    g_compositor = NULL;

    printf("[COMPOSITOR] Compositor shut down\n");
}

/* Get compositor instance */
compositor_t* compositor_get_instance(void) {
    return g_compositor;
}

/* Create window */
window_t* compositor_create_window(const char* title, int32_t x, int32_t y, uint32_t width, uint32_t height) {
    if (!g_compositor) {
        return NULL;
    }

    window_t* window = (window_t*)calloc(1, sizeof(window_t));
    if (!window) {
        return NULL;
    }

    window->id = g_compositor->next_window_id++;
    strncpy(window->title, title ? title : "Untitled", sizeof(window->title) - 1);
    window->rect.x = x;
    window->rect.y = y;
    window->rect.width = width;
    window->rect.height = height;

    /* Create surface */
    window->surface = compositor_create_surface(width, height);
    if (!window->surface) {
        free(window);
        return NULL;
    }

    window->visible = false;
    window->focused = false;
    window->decorated = true;
    window->z_order = g_compositor->window_count;

    /* Add to window list */
    window->next = g_compositor->windows;
    g_compositor->windows = window;
    g_compositor->window_count++;

    printf("[COMPOSITOR] Created window %llu: '%s' at (%d,%d) %ux%u\n",
           window->id, window->title, x, y, width, height);

    return window;
}

/* Destroy window */
void compositor_destroy_window(window_t* window) {
    if (!g_compositor || !window) {
        return;
    }

    /* Remove from list */
    if (g_compositor->windows == window) {
        g_compositor->windows = window->next;
    } else {
        window_t* prev = g_compositor->windows;
        while (prev && prev->next != window) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = window->next;
        }
    }

    /* Destroy surface */
    if (window->surface) {
        compositor_destroy_surface(window->surface);
    }

    if (g_compositor->focused_window == window) {
        g_compositor->focused_window = NULL;
    }

    g_compositor->window_count--;
    free(window);
}

/* Show window */
void compositor_show_window(window_t* window) {
    if (window) {
        window->visible = true;
    }
}

/* Hide window */
void compositor_hide_window(window_t* window) {
    if (window) {
        window->visible = false;
    }
}

/* Focus window */
void compositor_focus_window(window_t* window) {
    if (!g_compositor || !window) {
        return;
    }

    if (g_compositor->focused_window) {
        g_compositor->focused_window->focused = false;
    }

    g_compositor->focused_window = window;
    window->focused = true;
}

/* Create surface */
surface_t* compositor_create_surface(uint32_t width, uint32_t height) {
    surface_t* surface = (surface_t*)calloc(1, sizeof(surface_t));
    if (!surface) {
        return NULL;
    }

    surface->pixels = (uint32_t*)calloc(width * height, sizeof(uint32_t));
    if (!surface->pixels) {
        free(surface);
        return NULL;
    }

    surface->width = width;
    surface->height = height;
    surface->dirty = true;

    return surface;
}

/* Destroy surface */
void compositor_destroy_surface(surface_t* surface) {
    if (!surface) {
        return;
    }

    if (surface->pixels) {
        free(surface->pixels);
    }

    free(surface);
}

/* Fill rectangle */
void compositor_fill_rect(surface_t* surface, rect_t* rect, color_t color) {
    if (!surface || !rect) {
        return;
    }

    int32_t x1 = rect->x;
    int32_t y1 = rect->y;
    int32_t x2 = rect->x + rect->width;
    int32_t y2 = rect->y + rect->height;

    /* Clip to surface bounds */
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > (int32_t)surface->width) x2 = surface->width;
    if (y2 > (int32_t)surface->height) y2 = surface->height;

    for (int32_t y = y1; y < y2; y++) {
        for (int32_t x = x1; x < x2; x++) {
            surface->pixels[y * surface->width + x] = color;
        }
    }

    surface->dirty = true;
}

/* Draw rectangle outline */
void compositor_draw_rect(surface_t* surface, rect_t* rect, color_t color) {
    if (!surface || !rect) {
        return;
    }

    /* Draw four lines */
    compositor_draw_line(surface, rect->x, rect->y, rect->x + rect->width, rect->y, color);  // Top
    compositor_draw_line(surface, rect->x, rect->y + rect->height, rect->x + rect->width, rect->y + rect->height, color);  // Bottom
    compositor_draw_line(surface, rect->x, rect->y, rect->x, rect->y + rect->height, color);  // Left
    compositor_draw_line(surface, rect->x + rect->width, rect->y, rect->x + rect->width, rect->y + rect->height, color);  // Right
}

/* Draw line */
void compositor_draw_line(surface_t* surface, int32_t x1, int32_t y1, int32_t x2, int32_t y2, color_t color) {
    if (!surface) {
        return;
    }

    /* Bresenham's line algorithm */
    int32_t dx = abs(x2 - x1);
    int32_t dy = abs(y2 - y1);
    int32_t sx = (x1 < x2) ? 1 : -1;
    int32_t sy = (y1 < y2) ? 1 : -1;
    int32_t err = dx - dy;

    while (true) {
        if (x1 >= 0 && x1 < (int32_t)surface->width && y1 >= 0 && y1 < (int32_t)surface->height) {
            surface->pixels[y1 * surface->width + x1] = color;
        }

        if (x1 == x2 && y1 == y2) break;

        int32_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }

    surface->dirty = true;
}

/* Draw text (very simple) */
void compositor_draw_text(surface_t* surface, int32_t x, int32_t y, const char* text, color_t color) {
    if (!surface || !text) {
        return;
    }

    /* Simplified text rendering - just render placeholders */
    int32_t cursor_x = x;
    while (*text) {
        /* Draw a simple rectangle per character */
        rect_t char_rect = {cursor_x, y, 8, 16};
        compositor_draw_rect(surface, &char_rect, color);
        cursor_x += 10;
        text++;
    }
}

/* Render frame */
void compositor_render_frame(void) {
    if (!g_compositor || !g_compositor->fb) {
        return;
    }

    /* Clear framebuffer */
    compositor_clear(THEME_BG_DEEP_NAVY);

    /* Render windows from back to front */
    window_t* windows[256];
    int count = 0;

    window_t* w = g_compositor->windows;
    while (w && count < 256) {
        windows[count++] = w;
        w = w->next;
    }

    /* Sort by z-order (simple bubble sort) */
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (windows[j]->z_order > windows[j + 1]->z_order) {
                window_t* temp = windows[j];
                windows[j] = windows[j + 1];
                windows[j + 1] = temp;
            }
        }
    }

    /* Render each window */
    for (int i = 0; i < count; i++) {
        window_t* window = windows[i];
        if (!window->visible || !window->surface) {
            continue;
        }

        /* Blit window surface to framebuffer */
        for (uint32_t y = 0; y < window->surface->height && (window->rect.y + (int32_t)y) < (int32_t)g_compositor->fb->height; y++) {
            for (uint32_t x = 0; x < window->surface->width && (window->rect.x + (int32_t)x) < (int32_t)g_compositor->fb->width; x++) {
                int32_t fb_x = window->rect.x + x;
                int32_t fb_y = window->rect.y + y;

                if (fb_x >= 0 && fb_y >= 0) {
                    g_compositor->fb->pixels[fb_y * g_compositor->fb->width + fb_x] = window->surface->pixels[y * window->surface->width + x];
                }
            }
        }
    }

    /* Draw cursor */
    if (g_compositor->cursor_visible) {
        int32_t cx = g_compositor->cursor_x;
        int32_t cy = g_compositor->cursor_y;

        /* Simple crosshair cursor */
        for (int i = -5; i <= 5; i++) {
            if (cx + i >= 0 && cx + i < (int32_t)g_compositor->fb->width && cy >= 0 && cy < (int32_t)g_compositor->fb->height) {
                g_compositor->fb->pixels[cy * g_compositor->fb->width + (cx + i)] = THEME_ACCENT_NEON_CYAN;
            }
            if (cy + i >= 0 && cy + i < (int32_t)g_compositor->fb->height && cx >= 0 && cx < (int32_t)g_compositor->fb->width) {
                g_compositor->fb->pixels[(cy + i) * g_compositor->fb->width + cx] = THEME_ACCENT_NEON_CYAN;
            }
        }
    }

    g_compositor->frame_count++;
}

/* Clear framebuffer */
void compositor_clear(color_t color) {
    if (!g_compositor || !g_compositor->fb) {
        return;
    }

    uint32_t pixel_count = g_compositor->fb->width * g_compositor->fb->height;
    for (uint32_t i = 0; i < pixel_count; i++) {
        g_compositor->fb->pixels[i] = color;
    }
}

/* Present framebuffer (in real system, would swap buffers) */
void compositor_present(void) {
    /* In production, would send to display driver */
}

/* Handle mouse move */
void compositor_handle_mouse_move(int32_t x, int32_t y) {
    if (!g_compositor) {
        return;
    }

    g_compositor->cursor_x = x;
    g_compositor->cursor_y = y;

    /* Clamp to screen bounds */
    if (g_compositor->cursor_x < 0) g_compositor->cursor_x = 0;
    if (g_compositor->cursor_y < 0) g_compositor->cursor_y = 0;
    if (g_compositor->cursor_x >= (int32_t)g_compositor->fb->width)
        g_compositor->cursor_x = g_compositor->fb->width - 1;
    if (g_compositor->cursor_y >= (int32_t)g_compositor->fb->height)
        g_compositor->cursor_y = g_compositor->fb->height - 1;
}

/* Utility: rect contains point */
bool rect_contains_point(rect_t* rect, int32_t x, int32_t y) {
    if (!rect) return false;
    return (x >= rect->x && x < rect->x + (int32_t)rect->width &&
            y >= rect->y && y < rect->y + (int32_t)rect->height);
}
