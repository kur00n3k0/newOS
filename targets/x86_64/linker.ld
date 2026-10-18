ENTRY(start)

SECTIONS
{
    . = 1M;
    _kernel_start = .;

    .boot :
    {
        KEEP(*(.multiboot_header))
    }

    .text :
    {
        *(.text .text.*)
    }

    .rodata :
    {
        *(.rodata .rodata.*)
    }

    .data :
    {
        *(.data .data.*)
    }

    .bss :
    {
        *(COMMON)
        *(.bss .bss.*)
    }

    _kernel_end = .;

    /DISCARD/ :
    {
        *(.eh_frame .comment .note.*)
    }
}
