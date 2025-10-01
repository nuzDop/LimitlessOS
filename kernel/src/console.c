/*
 * Early Boot Console
 * VGA text mode console for x86_64
 */

#include "kernel.h"

/* VGA text mode buffer */
#define VGA_MEMORY 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

/* VGA colors */
enum vga_color {
    VGA_BLACK = 0,
    VGA_BLUE = 1,
    VGA_GREEN = 2,
    VGA_CYAN = 3,
    VGA_RED = 4,
    VGA_MAGENTA = 5,
    VGA_BROWN = 6,
    VGA_LIGHT_GREY = 7,
    VGA_DARK_GREY = 8,
    VGA_LIGHT_BLUE = 9,
    VGA_LIGHT_GREEN = 10,
    VGA_LIGHT_CYAN = 11,
    VGA_LIGHT_RED = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW = 14,
    VGA_WHITE = 15,
};

static uint16_t* vga_buffer = (uint16_t*)VGA_MEMORY;
static size_t console_row = 0;
static size_t console_column = 0;
static uint8_t console_color = 0;

/* Create VGA color attribute byte */
static ALWAYS_INLINE uint8_t vga_color_byte(enum vga_color fg, enum vga_color bg) {
    return fg | (bg << 4);
}

/* Create VGA entry */
static ALWAYS_INLINE uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

/* Scroll console up by one line */
static void console_scroll(void) {
    /* Move all lines up */
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }

    /* Clear bottom line */
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', console_color);
    }

    console_row = VGA_HEIGHT - 1;
}

/* Initialize console */
void console_init(void) {
    console_row = 0;
    console_column = 0;
    console_color = vga_color_byte(VGA_LIGHT_GREY, VGA_BLACK);

    /* Clear screen */
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_entry(' ', console_color);
        }
    }
}

/* Set console color */
void console_set_color(uint8_t fg, uint8_t bg) {
    console_color = vga_color_byte(fg, bg);
}

/* Put character at position */
void console_putchar_at(char c, uint8_t color, size_t x, size_t y) {
    if (x >= VGA_WIDTH || y >= VGA_HEIGHT) {
        return;
    }
    vga_buffer[y * VGA_WIDTH + x] = vga_entry(c, color);
}

/* Put character at cursor */
void console_putchar(char c) {
    if (c == '\n') {
        console_column = 0;
        console_row++;
    } else if (c == '\r') {
        console_column = 0;
    } else if (c == '\t') {
        console_column = (console_column + 8) & ~7;
    } else if (c == '\b') {
        if (console_column > 0) {
            console_column--;
            console_putchar_at(' ', console_color, console_column, console_row);
        }
    } else {
        console_putchar_at(c, console_color, console_column, console_row);
        console_column++;
    }

    if (console_column >= VGA_WIDTH) {
        console_column = 0;
        console_row++;
    }

    if (console_row >= VGA_HEIGHT) {
        console_scroll();
    }
}

/* Write string to console */
void console_write(const char* str) {
    if (!str) {
        return;
    }

    while (*str) {
        console_putchar(*str);
        str++;
    }
}

/* Write string with length */
void console_write_len(const char* str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        console_putchar(str[i]);
    }
}

/* Clear console */
void console_clear(void) {
    console_row = 0;
    console_column = 0;

    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_entry(' ', console_color);
        }
    }
}

/* Get cursor position */
void console_get_cursor(size_t* x, size_t* y) {
    if (x) *x = console_column;
    if (y) *y = console_row;
}

/* Set cursor position */
void console_set_cursor(size_t x, size_t y) {
    if (x >= VGA_WIDTH || y >= VGA_HEIGHT) {
        return;
    }
    console_column = x;
    console_row = y;
}
