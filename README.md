# RISC-V Operating System

A small experimental operating system kernel for the RISC-V architecture (RV32), written in C, based on the concepts and initial implementation presented in [Writing an Operating System in 1,000 Lines](https://operating-system-in-1000-lines.vercel.app/en/) by Nuta.

The project follows the book up to chapter 13 (by now) and then goes beyond it, replacing some of the original mechanisms with alternative implementations and adding new ones. It serves as a practical environment for studying operating-system internals, computer architecture, low-level C programming, and the RISC-V instruction set.

## Status

This is an ongoing project. Current state of the kernel:

| Subsystem | State | Notes |
|---|---|---|
| Boot and console | Done | Boot routine, stack setup, `putchar` through SBI |
| Kernel panic | Done | `PANIC` macro with file and line |
| Trap handling | Done | `kernel_entry` / `handle_trap`; handles traps from both S-mode and U-mode |
| Physical memory allocator | Done | Page allocator backed by a bitmap, with `free_pages` and checks for invalid, unaligned and double frees |
| Processes and context switch | Done | Process control blocks, per-process kernel stack, `switch_context` in assembly |
| Round Robin scheduler | In validation | FIFO ready queue, idle process, preemption driven by the supervisor timer |

## Differences from the original

The original book is the starting point, not the target. Where this repository diverges:

* **Page allocator.** A bitmap tracks every page, so memory can be freed and reused, and invalid operations are detected (out of range, unaligned address, double free).
* **Scheduler.** The ready queue is an intrusive FIFO list (the `next` pointer lives inside `struct process`, so no dynamic allocation is needed). The running process is never in the queue, and an idle process represents the boot flow and runs only when the queue is empty.
* **Preemption.** The quantum is measured by the hardware timer (SBI `set_timer`). On expiration, the timer interrupt calls `yield()`, so processes no longer need to yield voluntarily.
* **Trap entry.** `sscratch` holds `0` while the CPU runs kernel code and the top of the kernel stack while it runs user code. This allows a single `kernel_entry` to handle traps coming from S-mode and U-mode. `sepc` and `sstatus` are saved in the trap frame, which makes switching processes inside the trap handler safe.
* **Process start.** New processes begin in a small trampoline that enables interrupts before calling the real entry point, since they are first scheduled from inside a trap handler.

## Building and running

<!-- Adjust to your actual build script and toolchain. -->

Requirements: **See Nuta's website for requirements and dependencies installation**

Build and run:

```sh
./run.sh
```

The script compiles the kernel with the linker script and starts it on QEMU's `virt` machine. To exit QEMU, press `Ctrl-A` then `C`, and write `q`.

## Project structure

```
.
├── kernel.c      # boot, memory management, SBI, processes, context switch, trap handling
├── kernel.h      # process and trap frame structures, PANIC, CSR helpers
├── scheduler.c   # ready queue, idle process, yield
├── scheduler.h   # scheduler interface
├── common.c      # freestanding C library (printf, memset, ...)
├── common.h
├── kernel.ld     # linker script
└── run.sh        # build and run script
```

## Known limitations

* Processes cannot terminate yet (there is no `exit`)
* At most 8 processes (`PROCS_MAX`)
* Fixed 8 KB kernel stack per process; traps taken in S-mode reuse the stack in use
* Single hart (no SMP)
* The scheduler is purely Round Robin, with a fixed quantum and no priorities

## Roadmap

- [x] Boot and SBI console
- [x] Kernel panic
- [x] Trap and exception handling
- [x] Physical page allocator
- [x] Processes and context switching
- [x] Round Robin scheduler with timer preemption (validation in progress)
- [ ] Process termination (`exit`) and reuse of process slots
- [ ] Blocking and waking processes
- [ ] Virtual memory (page tables)
- [ ] User mode and system calls
- [ ] Synchronization primitives
- [ ] Device interaction and drivers
- [ ] File system

<!-- Move items between "done" and "planned" according to what the repository actually contains. -->

## RISC-V topics explored

* Assembly inline programming and calling conventions
* Privilege levels (M, S and U modes)
* Control and Status Registers (`sstatus`, `sepc`, `scause`, `stvec`, `sscratch`, `sie`, `time`)
* Traps, exceptions and timer interrupts
* SBI calls (`ecall`) for console output and timer programming
* Low-level context switching

## Origin

The initial implementation is based on the work from:

> Nuta — *Writing an Operating System in 1,000 Lines*

* [Project repository](https://github.com/nuta/operating-system-in-1000-lines)
* [Online book](https://operating-system-in-1000-lines.vercel.app/en/)

The original project focuses on implementing the essential mechanisms of a small operating system while keeping the codebase compact and understandable. This repository is not intended to simply reproduce it. It uses it as a foundation for further experimentation and development.

## References

* Nuta, *Writing an Operating System in 1,000 Lines*
  https://operating-system-in-1000-lines.vercel.app/en/

* Nuta, *operating-system-in-1000-lines*
  https://github.com/nuta/operating-system-in-1000-lines

* The RISC-V Instruction Set Manual, Volume II: Privileged Architecture
  https://riscv.org/specifications/ratified/

* RISC-V Supervisor Binary Interface (SBI) Specification
  https://github.com/riscv-non-isa/riscv-sbi-doc