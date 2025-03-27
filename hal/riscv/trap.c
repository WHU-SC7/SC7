#include "riscv.h"
#include "types.h"



// extern char trampoline[], uservec[];
// extern void *userret(uint64);

// // set up to take exceptions and traps while in the kernel.
// void set_usertrap(void)
// {
// 	w_stvec((uint64)uservec & ~0x3); // DIRECT
// }

// void set_kerneltrap(void)
// {
// 	w_stvec((uint64)kerneltrap & ~0x3); // DIRECT
// }

// set up to take exceptions and traps while in the kernel.
// void trap_init(void)
// {
// 	set_kerneltrap();
// }

