typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef uint32_t size_t;

/* addresses obtained from kernel.ld symbols */
/* extern char __bss: 0th byte of .bss section; extern char __bss[]: start address of the .bss section */
extern char __bss[], __bss_end[], __stack_top[];


// sets a certain volume of memory with a given value c
void *memset(void *buf, char c, size_t n) {
    uint8_t *p = (uint8_t *) buf;
    while (n--)
        *p++ = c;
    return buf;
}

void kernel_main(void) {
    memset(__bss, 0, (size_t) __bss_end - (size_t) __bss);

    for (;;);
}

__attribute__((section(".text.boot")))  // Positions the function boot in 0x80200000
__attribute__((naked))          // Instructs the compiler not to generate extra code
/* first routine to be executed, defined by the linker script */
void boot(void) {
    __asm__ __volatile__(
        "mv sp, %[stack_top]\n" // Set the stack pointer to the (assembly code)
        "j kernel_main\n"       // Jump to the kernel main function
        :                       // (output operands)
        : [stack_top] "r" (__stack_top) // Pass the stack top address as %[stack_top] (input operands)
    );
}