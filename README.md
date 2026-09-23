# RISC-V Operating System

A small experimental operating system kernel for the RISC-V architecture, based on the concepts and initial implementation presented in [Writing an Operating System in 1,000 Lines](https://operating-system-in-1000-lines.vercel.app/en/) by Nuta.

The project starts from a minimal RISC-V operating system and aims to serve as a practical environment for studying operating-system internals, computer architecture, low-level C programming, and the RISC-V instruction set.

The original project provides a compact reference implementation designed to introduce the fundamental components of an operating system. This repository uses that implementation as a starting point and progressively extends it with additional kernel functionality and alternative implementations.

## Goals

The main goal is to understand how an operating system works at a low level by implementing and experimenting with its fundamental components rather than relying exclusively on existing abstractions.

The project will explore topics such as:

* RISC-V architecture and privileged execution
* Kernel initialization and boot process
* Memory management
* Virtual memory
* Interrupts and exceptions
* System calls
* Process management
* Scheduling
* Synchronization
* Device interaction
* File systems
* Drivers
* Kernel-level data structures
* Concurrency and parallelism

As the project develops, different approaches to implementing the same subsystem may also be explored and compared.

## Origin

The initial implementation is based on the work from:

> Nuta — *Writing an Operating System in 1,000 Lines*

* [Project repository](https://github.com/nuta/operating-system-in-1000-lines)
* [Online book](https://operating-system-in-1000-lines.vercel.app/en/)

The original project focuses on implementing the essential mechanisms of a small operating system while keeping the codebase compact and understandable.

This repository is not intended to simply reproduce the original implementation. Instead, it uses it as a foundation for further experimentation and development.

## Development Approach

The kernel will be developed incrementally.

Each subsystem will be studied from both the theoretical and implementation perspectives, with the goal of understanding not only *what* a component does, but also *why* it is implemented in a particular way.

Whenever possible, implementations will be kept small and isolated so that their behavior can be tested and understood independently.


### RISC-V

The project will also be used to explore the RISC-V ISA more deeply, including:

* Assembly programming
* Registers and calling conventions
* Privilege levels
* Control and Status Registers (CSRs)
* Traps and interrupts
* Memory-management mechanisms
* Low-level context switching

## Project Structure

The repository is organized around the kernel and the components required to build and run it.

As new subsystems are implemented, the project structure will evolve accordingly. The intention is to keep the relationship between each subsystem and the underlying hardware visible rather than hiding implementation details behind large abstractions.

## Status

This is an ongoing project.

The current implementation should be considered a starting point rather than a finished operating system. New kernel components, experiments, and alternative implementations will be added progressively.

The long-term objective is to turn the initial minimal kernel into a broader collection of low-level operating-system experiments while keeping the code understandable enough to serve as a learning resource.

## References

* Nuta, *Writing an Operating System in 1,000 Lines*
  https://operating-system-in-1000-lines.vercel.app/en/

* Nuta, *operating-system-in-1000-lines*
  https://github.com/nuta/operating-system-in-1000-lines
