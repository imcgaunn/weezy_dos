/* weezyDOS kernel linker script */
/* Kernel is loaded at 0x10000 by the Stage 2 bootloader */

ENTRY(kmain)

SECTIONS
{
    . = 0x10000;

    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
        *(COMMON)
    }

    /DISCARD/ : {
        *(.comment)
        *(.note.*)
    }
}
