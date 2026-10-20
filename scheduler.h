#pragma once
#include "kernel.h"

/* ROUND-ROBIN READY PROCESS LIST */
typedef struct rr_node {
    struct process *proc;
    NODE_RR *next;
} NODE_RR;

typedef struct rr_cursor {
    NODE_RR *first;
    NODE_RR *last;
} LIST_RR;

// managing functions
LIST_RR* createList_RR();
void addReadyProc_RR(LIST_RR *list, struct process *proc);
void removeProc_RR(LIST_RR *list, struct process *proc);
void switchProc_RR(LIST_RR *list);

