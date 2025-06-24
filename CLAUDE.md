# GX-Kernel AI Assistant Guide

## Project Overview
GX-Kernel is a microkernel designed for embedded systems and microcontrollers,
emphasizing real-time performance, multi-node support, and deterministic behavior.

## Architecture Philosophy
- Minimal kernel with user-space drivers
- Fixed-priority preemptive scheduling
- Message-passing IPC (no shared memory between processes)
- Support for distributed multi-node systems

## Key Design Decisions
1. **Memory Model**: Two-tier system with regions (dynamic) and partitions (fixed)
   - Regions use first-fit allocation
   - Partitions guarantee O(1) allocation time
   
2. **IPC Mechanisms**: Multiple options for different use cases
   - Queues for async message passing
   - Events for lightweight synchronization
   - Semaphores for resource protection

## Coding Conventions
- Function prefixes indicate subsystem (t_ for task, q_ for queue, etc.)
- All kernel functions return ULONG error codes
- 4-character names for kernel objects
- No dynamic memory allocation in kernel space

## Critical Sections
- Interrupt handling in `/kernel/arch/*/interrupt.s`
- Scheduler core in `/kernel/sched/scheduler.c`
- These files require extreme care when modifying

## Testing Requirements
- Any IPC changes must pass the stress test suite
- Real-time guarantees must be validated with timing tests
- Multi-node features require cluster testing

## Common Tasks

### Adding a New System Call
1. Add prototype to `gxkernel.h`
2. Implement in appropriate subsystem
3. Add error codes if needed
4. Update system call table
5. Add test cases

### Porting to New Architecture
Focus on:
- Context switching code
- Interrupt management
- Memory management unit interface
- Timer implementation

## Areas Needing Improvement
- Power management subsystem (not yet implemented)
- Advanced debugging support
- Security model enhancement
