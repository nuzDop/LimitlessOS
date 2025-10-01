/* LimitlessOS Kernel Linker Script */

ENTRY(_start)

SECTIONS
{
    /* Kernel loads at 1MB physical */
    . = 1M;

    .boot :
    {
        *(.multiboot)
    }

    /* Read-only sections */
    .text ALIGN(4K) : AT(ADDR(.text))
    {
        *(.text)
        *(.text.*)
    }

    .rodata ALIGN(4K) : AT(ADDR(.rodata))
    {
        *(.rodata)
        *(.rodata.*)
    }

    /* Read-write sections */
    .data ALIGN(4K) : AT(ADDR(.data))
    {
        *(.data)
        *(.data.*)
    }

    /* BSS section */
    .bss ALIGN(4K) : AT(ADDR(.bss))
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    /* Global constructors */
    .init_array ALIGN(8) :
    {
        __init_array_start = .;
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        __init_array_end = .;
    }

    /* Global destructors */
    .fini_array ALIGN(8) :
    {
        __fini_array_start = .;
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array))
        __fini_array_end = .;
    }

    /* Kernel end marker */
    . = ALIGN(4K);
    _kernel_end = .;

    /* Discard unnecessary sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note.*)
    }
}
