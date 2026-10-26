#include "scheduler.h"

struct process *current_proc;
static struct process idle_proc;
static struct process *rq_head, *rq_tail;

/* desliga interrupções e devolve o estado anterior (bit SIE) */
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
    idle_proc.state = PROC_RUNNABLE;   // seu sp é preenchido no primeiro switch_context
    current_proc = &idle_proc;         // "quem está rodando agora" = o kernel_main
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
    uint32_t f = irq_save();                 // a fila não pode mudar no meio disso

    struct process *prev = current_proc;

    // o atual volta para o fim da fila, se ainda puder rodar (idle nunca entra)
    if (prev != &idle_proc && prev->state == PROC_RUNNABLE)
        sched_enqueue(prev);                 // irq_save aninhado é ok

    struct process *next = sched_dequeue();
    if (!next) next = &idle_proc;            // fila vazia: roda o idle

    if (next != prev) {
        current_proc = next;
        switch_context(&prev->sp, &next->sp);
        // só chegamos aqui quando `prev` for escalonado de novo
    }
    irq_restore(f);
}