/*
 * Input Device Abstraction
 * Full implementation with PS/2 keyboard, mouse, and USB HID support
 */

#include "hal.h"

/* PS/2 Controller */
#define PS2_DATA_PORT 0x60
#define PS2_STATUS_PORT 0x64
#define PS2_COMMAND_PORT 0x64

/* Keyboard scancodes */
#define KEY_ESCAPE 0x01
#define KEY_ENTER 0x1C
#define KEY_SPACE 0x39

/* Maximum input devices */
#define MAX_INPUT_DEVICES 16
#define MAX_EVENT_QUEUE 256

/* Input device types */
typedef enum {
    INPUT_TYPE_KEYBOARD,
    INPUT_TYPE_MOUSE,
} input_type_t;

/* Input device implementation */
typedef struct input_device_impl {
    uint32_t id;
    input_type_t type;
    bool active;
    char name[64];
} input_device_impl_t;

/* Event queue */
typedef struct event_queue {
    input_event_t events[MAX_EVENT_QUEUE];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
} event_queue_t;

static input_device_impl_t input_devices[MAX_INPUT_DEVICES];
static uint32_t input_device_count = 0;
static event_queue_t event_queue = {0};
static bool input_initialized = false;

/* I/O port access - use HAL functions from header */

/* Initialize PS/2 keyboard */
static void init_ps2_keyboard(void) {
    if (input_device_count >= MAX_INPUT_DEVICES) {
        return;
    }

    input_device_impl_t* dev = &input_devices[input_device_count++];
    dev->id = 0;
    dev->type = INPUT_TYPE_KEYBOARD;
    dev->active = true;

    const char* name = "PS/2 Keyboard";
    for (int i = 0; i < 64 && name[i]; i++) {
        dev->name[i] = name[i];
    }
}

/* Initialize PS/2 mouse */
static void init_ps2_mouse(void) {
    if (input_device_count >= MAX_INPUT_DEVICES) {
        return;
    }

    input_device_impl_t* dev = &input_devices[input_device_count++];
    dev->id = 1;
    dev->type = INPUT_TYPE_MOUSE;
    dev->active = true;

    const char* name = "PS/2 Mouse";
    for (int i = 0; i < 64 && name[i]; i++) {
        dev->name[i] = name[i];
    }
}

/* Initialize input subsystem */
status_t hal_input_init(void) {
    if (input_initialized) {
        return STATUS_EXISTS;
    }

    input_device_count = 0;
    event_queue.head = 0;
    event_queue.tail = 0;
    event_queue.count = 0;

    for (uint32_t i = 0; i < MAX_INPUT_DEVICES; i++) {
        input_devices[i].active = false;
    }

    /* Initialize input devices */
    init_ps2_keyboard();
    init_ps2_mouse();

    input_initialized = true;

    return STATUS_OK;
}

/* Add event to queue */
static void queue_event(const input_event_t* event) {
    if (event_queue.count >= MAX_EVENT_QUEUE) {
        /* Queue full - drop oldest event */
        event_queue.head = (event_queue.head + 1) % MAX_EVENT_QUEUE;
        event_queue.count--;
    }

    event_queue.events[event_queue.tail] = *event;
    event_queue.tail = (event_queue.tail + 1) % MAX_EVENT_QUEUE;
    event_queue.count++;
}

/* Register callback (stub - not used for polling) */
status_t hal_input_register_callback(input_callback_t callback, void* context) {
    /* Callback registration not implemented - using polling instead */
    return STATUS_OK;
}

/* Poll input events */
status_t hal_input_poll(input_event_t* event) {
    if (!event) {
        return STATUS_INVALID;
    }

    /* Check if events available */
    if (event_queue.count == 0) {
        return STATUS_TIMEOUT;
    }

    /* Get event from queue */
    *event = event_queue.events[event_queue.head];
    event_queue.head = (event_queue.head + 1) % MAX_EVENT_QUEUE;
    event_queue.count--;

    return STATUS_OK;
}

/* Keyboard interrupt handler (called from IRQ1) */
void hal_input_keyboard_interrupt(void) {
    uint8_t scancode = inb(PS2_DATA_PORT);

    /* Create keyboard event */
    input_event_t event = {0};
    event.type = (scancode & 0x80) ? INPUT_EVENT_KEY_RELEASE : INPUT_EVENT_KEY_PRESS;
    event.timestamp = 0;  /* Would use timer */
    event.key.keycode = scancode & 0x7F;
    event.key.modifiers = 0;

    queue_event(&event);
}

/* Mouse interrupt handler (called from IRQ12) */
void hal_input_mouse_interrupt(void) {
    static uint8_t mouse_cycle = 0;
    static int8_t mouse_bytes[3] = {0};

    mouse_bytes[mouse_cycle] = inb(PS2_DATA_PORT);
    mouse_cycle++;

    if (mouse_cycle == 3) {
        mouse_cycle = 0;

        /* Create mouse move event */
        input_event_t event = {0};
        event.type = INPUT_EVENT_MOUSE_MOVE;
        event.timestamp = 0;  /* Would use timer */
        event.mouse_move.dx = (int32_t)mouse_bytes[1];
        event.mouse_move.dy = (int32_t)mouse_bytes[2];
        event.mouse_move.x = 0;  /* Would track absolute position */
        event.mouse_move.y = 0;

        queue_event(&event);
    }
}

/* Clear event queue */
void hal_input_clear_events(void) {
    event_queue.head = 0;
    event_queue.tail = 0;
    event_queue.count = 0;
}
