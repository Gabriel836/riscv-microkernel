#include "user.h"

void main(void) {
    // just for testing !! illegal!!
    *((volatile int *) 0x80200000) = 0x1234;
    for (;;);
}