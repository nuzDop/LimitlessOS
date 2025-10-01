/*
 * UEFI Console Output
 */

#include "efi.h"

static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *console = NULL;

void efi_console_init(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *con) {
    console = con;
    if (console) {
        console->ClearScreen(console);
    }
}

void efi_print(const CHAR16 *str) {
    if (console && str) {
        console->OutputString(console, (CHAR16*)str);
    }
}

void efi_print_ascii(const char *str) {
    if (!console || !str) {
        return;
    }

    CHAR16 buffer[256];
    int i = 0;

    while (str[i] && i < 255) {
        buffer[i] = (CHAR16)str[i];
        i++;
    }
    buffer[i] = 0;

    console->OutputString(console, buffer);
}

void efi_clear_screen(void) {
    if (console) {
        console->ClearScreen(console);
    }
}
