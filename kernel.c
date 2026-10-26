#include "kernel.h"
#include "common.h"
#include "scheduler.h"

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef uint32_t size_t;

/* addresses obtained from kernel.ld symbols */
/* extern char __bss: 0th byte of .bss section; extern char __bss[]: start address of the .bss section */
extern char __bss[], __bss_end[], __stack_top[], __free_ram[], __free_ram_end[], __kernel_base[], _binary_shell_bin_start[], _binary_shell_bin_size[];
static char bitmap[MAX_PAGE]; // keeps track of the memory pages (allocated or not)
static size_t memsize;

/* ========================================================================== */
/* MEMORY MANAGING                                                            */
/* ========================================================================== */

/* initializes the global variable memsize: memory size (in pages) of free ram */
void memory_init(void) {
    size_t ram_bytes = (size_t)(__free_ram_end - __free_ram);

    if (ram_bytes % PAGE_SIZE != 0)
        PANIC("RAM size is not page-aligned");

    memsize = ram_bytes / PAGE_SIZE;
    printf("memsize %d\n", memsize);    // debug

    if (memsize > MAX_PAGE)
        PANIC("bitmap too small");
}

/* prints the bitmap keeping track of the memory pages */
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

/* frees memory pages */
void free_pages(paddr_t paddr, size_t n) {
    if (n == 0)
        PANIC("free_pages: invalid size");

    // physical address before or after designated free ram
    if (paddr < (paddr_t)__free_ram ||
        paddr >= (paddr_t)__free_ram_end)
        PANIC("free_pages: invalid address");

    // physical address unaligned
    if ((paddr - (paddr_t)__free_ram) % PAGE_SIZE != 0)
        PANIC("free_pages: unaligned address");

    size_t i = (paddr - (paddr_t)__free_ram) / PAGE_SIZE;

    // checks if the memory size fits in the designated memory
    if (n > memsize - i)
        PANIC("free_pages: out of range");

    // frees memory (clause for double free)
    for (size_t j = 0; j < n; j++) {
        if (bitmap[i + j] == 0)
            PANIC("free_pages: double free");

        bitmap[i + j] = 0;
    }
}

/* allocate memory pages, keeping track */
paddr_t alloc_pages(uint32_t n) {
    size_t i, j;
    // search linearly for the first contiguous available space
    for (i = 0; i < memsize; i++) {
        if (bitmap[i] == 0) {
            for (j = 0; j < n; j++) {
                if (i + j >= memsize) PANIC("out of memory");
                if (bitmap[i + j] != 0) {
                    i = i + j;
                    break;
                }
            }
            if (j == n) {
                for (j = 0; j < n; j++)
                    bitmap[i + j] = 1;
                return (paddr_t)(__free_ram + i * PAGE_SIZE);
            }
        }
    }
    PANIC("allocation error!");
}

/* ========================================================================== */
/* PAGE TABLE                                                                 */
/* ========================================================================== */

void map_page(uint32_t *table1, uint32_t vaddr, paddr_t paddr, uint32_t flags) {
    if (!is_aligned(vaddr, PAGE_SIZE))
        PANIC("unaligned vaddr %x", vaddr);

    if (!is_aligned(paddr, PAGE_SIZE))
        PANIC("unaligned paddr %x", paddr);

    uint32_t vpn1 = (vaddr >> 22) & 0x3ff;  // extract the 10 m-s bits for level 1
    if ((table1[vpn1] & PAGE_V) == 0) {
        // Create the 1st level page table if it doesn't exist.
        uint32_t pt_paddr = alloc_pages(1);
        table1[vpn1] = ((pt_paddr / PAGE_SIZE) << 10) | PAGE_V;
    }

    // Set the 2nd level page table entry to map the physical page.
    uint32_t vpn0 = (vaddr >> 12) & 0x3ff;
    uint32_t *table0 = (uint32_t *) ((table1[vpn1] >> 10) * PAGE_SIZE);
    table0[vpn0] = ((paddr / PAGE_SIZE) << 10) | flags | PAGE_V;
}

/* ========================================================================== */
/* SBI / CONSOLE                                                              */
/* ========================================================================== */

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

/* ========================================================================== */
/* TIMER (preemption): the hardware timer is what measures the quantum        */
/* ========================================================================== */

#define SBI_EXT_TIME    0x54494D45      // "TIME" extension id
#define QUANTUM_TICKS   100000          // ~10 ms (QEMU virt: 10 MHz timer)
#define SCAUSE_S_TIMER  0x80000005      // interrupt bit | cause 5 (supervisor timer)

/* RV32: the time CSR is 64 bits wide, split in time/timeh */
static unsigned long long read_time(void) {
    uint32_t hi, lo, hi2;
    do {
        __asm__ __volatile__("rdtimeh %0" : "=r"(hi));
        __asm__ __volatile__("rdtime  %0" : "=r"(lo));
        __asm__ __volatile__("rdtimeh %0" : "=r"(hi2));
    } while (hi != hi2);
    return ((unsigned long long)hi << 32) | lo;
}

/* schedules the next timer interrupt one quantum from now */
static void timer_set_next(void) {
    unsigned long long t = read_time() + QUANTUM_TICKS;
    sbi_call((uint32_t)t, (uint32_t)(t >> 32), 0, 0, 0, 0,
             0 /* fid: set_timer */, SBI_EXT_TIME);
}


static void timer_init(void) {
    __asm__ __volatile__("csrs sie, %0" :: "r"(1 << 5));     // sie.STIE
    timer_set_next();
    __asm__ __volatile__("csrs sstatus, %0" :: "r"(1 << 1)); // sstatus.SIE (last)
}

/* ========================================================================== */
/* APPLICATION HANDLING (U-MODE)                                              */
/* ========================================================================== */

// lowers privilleges 
__attribute__((naked)) void user_entry(void) {
    __asm__ __volatile__(
        "csrw sepc, %[sepc]        \n"
        "csrw sstatus, %[sstatus]  \n"
        "sret                      \n"
        :
        : [sepc] "r" (USER_BASE),           // points to the start of the application
          [sstatus] "r" (SSTATUS_SPIE)
    );
}

/* ========================================================================== */
/* PROCESS HANDLING                                                           */
/* ========================================================================== */

struct process procs[PROCS_MAX]; // All process control structures.

/* every new process starts here: enables interrupts, then calls the real entry
   point (kept in s0 by create_process). A new process is started by a `ret`
   inside switch_context, usually called from the timer handler (SIE=0), and it
   never goes through an `sret` that would re-enable interrupts. */
__attribute__((naked)) void proc_trampoline(void) {
    __asm__ __volatile__(
        "csrsi sstatus, 2\n"    // sstatus.SIE = 1
        "jalr s0\n"             // call the real entry
        "1: wfi\n"              // entry returned: for now, just park (later: exit)
        "j 1b\n"
    );
}

struct process *create_process(const void *image, size_t image_size) {
    // find an empty slot in the proc array
    struct process *proc = NULL;
    int i;
    for (i = 0; i < PROCS_MAX; i++) {           //[cite: 3, 4]
        if (procs[i].state == PROC_UNUSED) {    //[cite: 3, 4]
            proc = &procs[i];
            break;
        }
    }

    if (!proc)
        PANIC("no free process slots");         //

    // fakes a context in the kernel stack
    uint32_t *sp = (uint32_t *) &proc->stack[sizeof(proc->stack)]; 
    *--sp = 0;                      // s11
    *--sp = 0;                      // s10
    *--sp = 0;                      // s9
    *--sp = 0;                      // s8
    *--sp = 0;                      // s7
    *--sp = 0;                      // s6
    *--sp = 0;                      // s5
    *--sp = 0;                      // s4
    *--sp = 0;                      // s3
    *--sp = 0;                      // s2
    *--sp = 0;                      // s1
    *--sp = 0;                      // s0
    *--sp = (uint32_t) user_entry;  // ra

    // creates the process page table
    uint32_t *page_table = (uint32_t *) alloc_pages(1); //

    // maps the process pages (identity mapping)
    for (paddr_t paddr = (paddr_t) 0x80200000;          // 0x80200000 is the base address
        paddr < (paddr_t) __free_ram_end; paddr += PAGE_SIZE)
        map_page(page_table, paddr, paddr, PAGE_R | PAGE_W | PAGE_X);

    // maps and copy the application binary (image)
    for (uint32_t off = 0; off < image_size; off += PAGE_SIZE) {
        paddr_t page = alloc_pages(1);
        
        size_t remaining = image_size - off;
        size_t copy_size = PAGE_SIZE <= remaining ? PAGE_SIZE : remaining;
        
        memcpy((void *) page, image + off, copy_size);
        
        // maps to a virtual region
        map_page(page_table, USER_BASE + off, page, PAGE_U | PAGE_R | PAGE_W | PAGE_X);
    }

    // updates control and add it to the scheduler
    proc->pid = i + 1;
    proc->state = PROC_RUNNABLE;
    proc->sp = (uint32_t) sp;
    proc->page_table = page_table;

    sched_enqueue(proc);
    return proc;
}

/* context switching between process */
/* execution context is saved in the stack as local variables */
/* callee-saved registers (stacked) must be restored */
__attribute__((naked)) void switch_context(uint32_t *prev_sp,
                                           uint32_t *next_sp) {
    __asm__ __volatile__(
        // Save callee-saved registers onto the current process's stack.
        "addi sp, sp, -13 * 4\n" // Allocate stack space for 13 4-byte registers
        "sw ra,  0  * 4(sp)\n"   // Save callee-saved registers only
        "sw s0,  1  * 4(sp)\n"
        "sw s1,  2  * 4(sp)\n"
        "sw s2,  3  * 4(sp)\n"
        "sw s3,  4  * 4(sp)\n"
        "sw s4,  5  * 4(sp)\n"
        "sw s5,  6  * 4(sp)\n"
        "sw s6,  7  * 4(sp)\n"
        "sw s7,  8  * 4(sp)\n"
        "sw s8,  9  * 4(sp)\n"
        "sw s9,  10 * 4(sp)\n"
        "sw s10, 11 * 4(sp)\n"
        "sw s11, 12 * 4(sp)\n"

        // Switch the stack pointer.
        "sw sp, (a0)\n"         // *prev_sp = sp;
        "lw sp, (a1)\n"         // Switch stack pointer (sp) here

        // Restore callee-saved registers from the next process's stack.
        "lw ra,  0  * 4(sp)\n"  // Restore callee-saved registers only
        "lw s0,  1  * 4(sp)\n"
        "lw s1,  2  * 4(sp)\n"
        "lw s2,  3  * 4(sp)\n"
        "lw s3,  4  * 4(sp)\n"
        "lw s4,  5  * 4(sp)\n"
        "lw s5,  6  * 4(sp)\n"
        "lw s6,  7  * 4(sp)\n"
        "lw s7,  8  * 4(sp)\n"
        "lw s8,  9  * 4(sp)\n"
        "lw s9,  10 * 4(sp)\n"
        "lw s10, 11 * 4(sp)\n"
        "lw s11, 12 * 4(sp)\n"
        "addi sp, sp, 13 * 4\n"  // We've popped 13 4-byte registers from the stack
        "ret\n"
    );
}

/* the scheduler itself (ready queue, idle process, yield) lives in scheduler.c */

/* ========================================================================== */
/* EXCEPTION HANDLER                                                          */
/* ========================================================================== */

/* Works for traps coming from both U-mode and S-mode.
   Convention: sscratch == 0        -> CPU is running kernel code
               sscratch == k_stack  -> CPU is running user code (k_stack = top
                                       of the current process's kernel stack)
   sepc and sstatus are saved in the trap frame, so switching processes inside
   handle_trap (preemption) is safe. */
__attribute__((naked))
__attribute__((aligned(4)))
void kernel_entry(void) {
    __asm__ __volatile__(
        "csrrw sp, sscratch, sp\n"      // swap sp sscratch
        "bnez  sp, 1f\n"                // sp != 0: came from U-mode, sp = kernel stack
        "csrr  sp, sscratch\n"          // sp == 0: came from S-mode, recover original sp
        "1:\n"
        "addi sp, sp, -4 * 36\n"        // trap frame (33 words used + padding, 16-byte aligned)
        "sw ra,  4 * 0(sp)\n"
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

        "csrr a0, sscratch\n"           // sp of whoever trapped (user or kernel)
        "sw   a0, 4 * 30(sp)\n"
        "csrr a0, sepc\n"
        "sw   a0, 4 * 31(sp)\n"
        "csrr a0, sstatus\n"
        "sw   a0, 4 * 32(sp)\n"

        "csrw sscratch, zero\n"         // now in the kernel: nested traps see 0

        "mv   a0, sp\n"                 // a0 = pointer to the trap frame
        "call handle_trap\n"

        "lw   a0, 4 * 31(sp)\n"
        "csrw sepc, a0\n"
        "lw   a0, 4 * 32(sp)\n"
        "csrw sstatus, a0\n"

        "andi a0, a0, 1 << 8\n"         // sstatus.SPP
        "bnez a0, 2f\n"                 // SPP=1: returning to kernel, sscratch stays 0
        "addi a0, sp, 4 * 36\n"         // SPP=0: returning to user, sscratch = top of kernel stack
        "csrw sscratch, a0\n"
        "2:\n"

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

/* timer interrupt -> end of quantum -> yield. Anything else is still fatal. */
void handle_trap(struct trap_frame *f) {
    uint32_t scause = READ_CSR(scause);

    if (scause == SCAUSE_S_TIMER) {
        timer_set_next();   // re-arm first, otherwise the interrupt stays pending
        yield();            // may switch to another process; we return here when rescheduled
        return;
    }

    uint32_t stval = READ_CSR(stval);
    PANIC("unexpected trap");
}



/* ========================================================================== */
/* PROCESS TESTING                                                            */
/* ========================================================================== */

/* busy-wait; tune DELAY_ITERS so that several chars are printed per quantum */
#define DELAY_ITERS 300000
void delay(void) {
    for (int i = 0; i < DELAY_ITERS; i++)
        __asm__ __volatile__("nop");
}

struct process *proc_a;
struct process *proc_b;

void proc_a_entry(void) {
    printf("starting process A\n");
    while (1) {
        //putchar('A');
        delay();
    }
}

void proc_b_entry(void) {
    printf("starting process B\n");
    while (1) {
        //putchar('B');
        delay();
        //yield();
    }
}

void kernel_main(void) {
    memset(__bss, 0, (size_t) __bss_end - (size_t) __bss);

    printf("\n\n");

    // initializes memory
    memory_init();

    // traps config
    WRITE_CSR(stvec, (uint32_t) kernel_entry);
    WRITE_CSR(sscratch, 0);

    // initializes queue and idle_proc
    sched_init(); 
    
    // idle proc needs a page table
    uint32_t *idle_page_table = (uint32_t *) alloc_pages(1);
    for (paddr_t paddr = (paddr_t) 0x80200000; paddr < (paddr_t) __free_ram_end; paddr += PAGE_SIZE) {
        map_page(idle_page_table, paddr, paddr, PAGE_R | PAGE_W | PAGE_X);
    }
    current_proc->page_table = idle_page_table;

    // creates an user procces injection the binary
    create_process(_binary_shell_bin_start, (size_t) _binary_shell_bin_size);

    // itializes timer
    timer_init();

    // idle loop
    for (;;) {
        yield();
        __asm__ __volatile__("wfi");
    }
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