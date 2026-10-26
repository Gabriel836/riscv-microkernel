#pragma once
#include "kernel.h"

extern struct process *current_proc;

void sched_init(void);
void sched_enqueue(struct process *p);   // coloca um processo RUNNABLE no fim da fila
void yield(void);
