#pragma once
#include "kernel.h"

extern struct process *current_proc;

void sched_init(void);
void sched_enqueue(struct process *p);
void yield(void);
