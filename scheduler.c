#include "scheduler.h"

struct process *current_proc;
static struct process idle_proc;
static struct process *rq_head, *rq_tail;

/* turn off interrupts and return previous state (bit SIE) */
static inline uint32_t irq_save(void) {
    uint32_t old;
    __asm__ __volatile__("csrrci %0, sstatus, 2" : "=r"(old));
    return old & 2;
}
static inline void irq_restore(uint32_t flags) {
    if (flags)
        __asm__ __volatile__("csrsi sstatus, 2");
}

void sched_init(void) {
    idle_proc.pid = 0;
    idle_proc.state = PROC_RUNNABLE;
    current_proc = &idle_proc;         // kernel main running now
}

void sched_enqueue(struct process *p) {
    uint32_t f = irq_save();
    p->next = NULL;
    if (rq_tail) rq_tail->next = p;
    else         rq_head = p;
    rq_tail = p;
    irq_restore(f);
}

static struct process *sched_dequeue(void) {
    struct process *p = rq_head;
    if (!p) return NULL;
    rq_head = p->next;
    if (!rq_head) rq_tail = NULL;
    p->next = NULL;
    return p;
}

void yield(void) {
    uint32_t f = irq_save();                 // queue cant change now

    struct process *prev = current_proc;

    // current process go to the end of the line
    if (prev != &idle_proc && prev->state == PROC_RUNNABLE)
        sched_enqueue(prev);

    struct process *next = sched_dequeue();
    if (!next) next = &idle_proc;            // empty queue: idle

    if (next != prev) {
        current_proc = next;
        __asm__ __volatile__(
            "sfence.vma\n"
            "csrw satp, %[satp]\n"
            "sfence.vma\n"
            "csrw sscratch, %[sscratch]\n"
            :
            : [satp] "r" (SATP_SV32 | ((uint32_t) next->page_table / PAGE_SIZE)),
              [sscratch] "r" ((uint32_t) &next->stack[sizeof(next->stack)])
        );
        switch_context(&prev->sp, &next->sp);
    }
    irq_restore(f);
}