/* first executable instruction in the output file, program entry */
ENTRY(boot)

SECTIONS {
    /* base address */
    . = 0x80200000;

    /* program source code */
    .text :{
        KEEP(*(.text.boot));
        *(.text .text.*);
    }

    /* read only data */
    .rodata : ALIGN(4) {
        *(.rodata .rodata.*);
    }

    /* read/write data */
    .data : ALIGN(4) {
        *(.data .data.*);
    }

    /* read/write data with itial value zero */
    .bss : ALIGN(4) {
        __bss = .;
        *(.bss .bss.* .sbss .sbss.*);
        __bss_end = .;
    }

    /* dot (.) represents current address */
    . = ALIGN(4);
    . += 128 * 1024; /* 128KB */
    __stack_top = .;
}

/* the variables defined in the end of the each section are used in the kernel.c */