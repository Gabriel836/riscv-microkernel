#include "scheduler.h"

LIST_RR* createList_RR() {
    LIST_RR* list = (LIST_RR*)malloc(sizeof(LIST_RR));
    list->first = NULL;
    list->last = NULL;
    return list;
}

void addReadyProc_RR(LIST_RR *list, struct process *proc) {
    if(list == NULL) PANIC("unnavailable scheduler list");
    if(proc->state != PROC_RUNNABLE) PANIC("illegal: trying to add a non-ready process to ready-list.");

    // creating node for the process
    NODE_RR *node = (NODE_RR*)malloc(sizeof(NODE_RR));
    node->next = NULL;
    node->proc = proc;

    // empty list
    if(list->first == NULL) {
        list->first = node;
        list->last = node;
        return;
    }

    // non-empty list
    list->last->next = node;
    list->last = node;
    return;
}

// since process switch activates always that a process finish running or is blocked, this function does not have further protections
void removeProc_RR(LIST_RR *list, struct process *proc) {
    if(list == NULL) PANIC("unnavailable scheduler list");
    if(proc->state == PROC_RUNNABLE) PANIC("illegal: trying to remove a ready process from ready-list.");

    NODE_RR *nodeBefore = list->first;
    NODE_RR *nodeAfter = nodeBefore->next;

    if(nodeAfter == NULL) PANIC("SCHEDULER: unexpected behaviour");

    do{
        if(nodeAfter->proc->pid == proc->pid) {
            if(nodeAfter->proc->pid == list->last) list->last == nodeBefore;
            nodeBefore->next == nodeAfter->next;
            free(nodeAfter);
            return;
        }
        nodeBefore = nodeAfter;
        nodeAfter = nodeAfter->next;
    } while(nodeAfter != NULL);
}

void switchProc_RR(LIST_RR *list) {
    if(list == NULL) PANIC("unnavailable scheduler list");

    NODE_RR *node = list->first;

    // one item list
    if(node->next == NULL) return;
    
    list->first = node->next;
    list->last->next = node;
    list->last = node;
    node->next = NULL;
    return;
}