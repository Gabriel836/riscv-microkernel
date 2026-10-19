#include "kernel.h"
#include "common.h"

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef uint32_t size_t;

/* addresses obtained from kernel.ld symbols */
/* extern char __bss: 0th byte of .bss section; extern char __bss[]: start address of the .bss section */
extern char __bss[], __bss_end[], __stack_top[], __free_ram[], __free_ram_end[];
static char bitmap[MAX_PAGE];
static size_t memsize;

void memory_init(void) {
    size_t ram_bytes = (size_t)(__free_ram_end - __free_ram);

    if (ram_bytes % PAGE_SIZE != 0)
        PANIC("RAM size is not page-aligned");

    memsize = ram_bytes / PAGE_SIZE;
    printf("memesize %d\n", memsize);

    if (memsize > MAX_PAGE)
        PANIC("bitmap too small");
}

void print_bitmap(size_t n) {
    for (size_t i = 0; i < n && i < memsize; i++) {
        printf("%d", bitmap[i]);

        if ((i + 1) % 8 == 0)
            printf(" ");

        if ((i + 1) % 32 == 0)
            printf("\n");
    }

    printf("\n");
}

void free_pages(paddr_t paddr, size_t n) {
    if (n == 0)
        PANIC("free_pages: invalid size");

    if (paddr < (paddr_t)__free_ram ||
        paddr >= (paddr_t)__free_ram_end)
        PANIC("free_pages: invalid address");

    if ((paddr - (paddr_t)__free_ram) % PAGE_SIZE != 0)
        PANIC("free_pages: unaligned address");

    size_t i =
        (paddr - (paddr_t)__free_ram) / PAGE_SIZE;

    if (n > memsize - i)
        PANIC("free_pages: out of range");

    for (size_t j = 0; j < n; j++) {
        if (bitmap[i + j] == 0)
            PANIC("free_pages: double free");

        bitmap[i + j] = 0;
    }
}

paddr_t alloc_pages(uint32_t n) {
    size_t i, j;
    for(i=0; i<memsize; i++) {
        if(bitmap[i] == 0) {
            for(j=0; j<n; j++) {
                if(i+j >= memsize) PANIC("out of memory");
                if(bitmap[i+j] != 0) {
                    i=i+j;
                    break;
                }
            }
            if(j==n) {
                for(j=0; j<n; j++) {
                    bitmap[i+j] = 1;
                }
            return (paddr_t)
                (__free_ram + i * PAGE_SIZE);
            }
        }
    }
    PANIC("allocation error!");
}

struct sbiret sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4,
                       long arg5, long fid, long eid) {
    register long a0 __asm__("a0") = arg0;
    register long a1 __asm__("a1") = arg1;
    register long a2 __asm__("a2") = arg2;
    register long a3 __asm__("a3") = arg3;
    register long a4 __asm__("a4") = arg4;
    register long a5 __asm__("a5") = arg5;
    register long a6 __asm__("a6") = fid;
    register long a7 __asm__("a7") = eid;

    __asm__ __volatile__("ecall"
                         : "=r"(a0), "=r"(a1)
                         : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5),
                           "r"(a6), "r"(a7)
                         : "memory");
    return (struct sbiret){.error = a0, .value = a1};
}

void putchar(char ch) {
    sbi_call(ch, 0, 0, 0, 0, 0, 0, 1 /* Console Putchar */);
}

__attribute__((naked))
__attribute__((aligned(4)))
void kernel_entry(void) {
    __asm__ __volatile__(
        "csrw sscratch, sp\n"           // saving sp to use it later (register values will be saved in the stack)
        "addi sp, sp, -4 * 31\n"        // Allocate space for the trap_frame struct
        "sw ra,  4 * 0(sp)\n"           // saving the state of the machine (register values) in the stack
        "sw gp,  4 * 1(sp)\n"
        "sw tp,  4 * 2(sp)\n"
        "sw t0,  4 * 3(sp)\n"
        "sw t1,  4 * 4(sp)\n"
        "sw t2,  4 * 5(sp)\n"
        "sw t3,  4 * 6(sp)\n"
        "sw t4,  4 * 7(sp)\n"
        "sw t5,  4 * 8(sp)\n"
        "sw t6,  4 * 9(sp)\n"
        "sw a0,  4 * 10(sp)\n"
        "sw a1,  4 * 11(sp)\n"
        "sw a2,  4 * 12(sp)\n"
        "sw a3,  4 * 13(sp)\n"
        "sw a4,  4 * 14(sp)\n"
        "sw a5,  4 * 15(sp)\n"
        "sw a6,  4 * 16(sp)\n"
        "sw a7,  4 * 17(sp)\n"
        "sw s0,  4 * 18(sp)\n"
        "sw s1,  4 * 19(sp)\n"
        "sw s2,  4 * 20(sp)\n"
        "sw s3,  4 * 21(sp)\n"
        "sw s4,  4 * 22(sp)\n"
        "sw s5,  4 * 23(sp)\n"
        "sw s6,  4 * 24(sp)\n"
        "sw s7,  4 * 25(sp)\n"
        "sw s8,  4 * 26(sp)\n"
        "sw s9,  4 * 27(sp)\n"
        "sw s10, 4 * 28(sp)\n"
        "sw s11, 4 * 29(sp)\n"

        "csrr a0, sscratch\n"           // sscratch = sp; a0 (after saved) = sscratch -> a0 = sp;
        "sw a0, 4 * 30(sp)\n"           // saves the value of sp in the stack

        "mv a0, sp\n"                   // a0 receives the value of sp (end of trap frame)
        "call handle_trap\n"            // a0 passes the argument

        "lw ra,  4 * 0(sp)\n"
        "lw gp,  4 * 1(sp)\n"
        "lw tp,  4 * 2(sp)\n"
        "lw t0,  4 * 3(sp)\n"
        "lw t1,  4 * 4(sp)\n"
        "lw t2,  4 * 5(sp)\n"
        "lw t3,  4 * 6(sp)\n"
        "lw t4,  4 * 7(sp)\n"
        "lw t5,  4 * 8(sp)\n"
        "lw t6,  4 * 9(sp)\n"
        "lw a0,  4 * 10(sp)\n"
        "lw a1,  4 * 11(sp)\n"
        "lw a2,  4 * 12(sp)\n"
        "lw a3,  4 * 13(sp)\n"
        "lw a4,  4 * 14(sp)\n"
        "lw a5,  4 * 15(sp)\n"
        "lw a6,  4 * 16(sp)\n"
        "lw a7,  4 * 17(sp)\n"
        "lw s0,  4 * 18(sp)\n"
        "lw s1,  4 * 19(sp)\n"
        "lw s2,  4 * 20(sp)\n"
        "lw s3,  4 * 21(sp)\n"
        "lw s4,  4 * 22(sp)\n"
        "lw s5,  4 * 23(sp)\n"
        "lw s6,  4 * 24(sp)\n"
        "lw s7,  4 * 25(sp)\n"
        "lw s8,  4 * 26(sp)\n"
        "lw s9,  4 * 27(sp)\n"
        "lw s10, 4 * 28(sp)\n"
        "lw s11, 4 * 29(sp)\n"
        "lw sp,  4 * 30(sp)\n"
        "sret\n"
    );
}

// after concluding the cause, and where the trap happened, it kills the kernel with PANIC
void handle_trap(struct trap_frame *f) {
    uint32_t scause = READ_CSR(scause);
    uint32_t stval = READ_CSR(stval);
    uint32_t user_pc = READ_CSR(sepc);

    PANIC("unexpected trap scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);
}

void kernel_main(void) {
    memset(__bss, 0, (size_t) __bss_end - (size_t) __bss);
    memory_init();
    paddr_t paddr0 = alloc_pages(2);
    paddr_t paddr1 = alloc_pages(1);
    printf("alloc_pages test: paddr0=%x\n", paddr0);
    printf("alloc_pages test: paddr1=%x\n", paddr1);
    printf("alloc_pages test: __free_ram_end=%x\n", (paddr_t)__free_ram_end);
    print_bitmap(8);
    free_pages(paddr0, 2);
    print_bitmap(8);
    paddr0 = alloc_pages(1);
    print_bitmap(8);
    paddr_t paddr2 = alloc_pages(2);
    print_bitmap(8);

    PANIC("booted!");
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