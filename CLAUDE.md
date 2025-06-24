# GX-Kernel AI Assistant Guide

## Project Overview

GX-Kernel (also referred to as "Core" microkernel) is a supervisory space component that provides essential real-time services for hard-deadline embedded systems. It serves as the foundational component for implementing applications, device drivers, middleware, and full-featured operating systems. The microkernel is specifically designed for industrial control systems, automotive applications, and other embedded real-time scenarios where reliability, determinism, and distributed operation are paramount.

## Architecture Philosophy

### Core Design Principles
1. **Microkernel Architecture**: Minimal kernel with user-space drivers and services
2. **Real-Time First**: Every design decision prioritizes deterministic behavior
3. **Distribution-Ready**: Built-in support for multi-node systems from the ground up
4. **Memory Safety**: Hardware-enforced memory protection between processes
5. **Flexible IPC**: Multiple communication mechanisms for different use cases

### Key Architectural Decisions
- **Fixed-priority preemptive scheduling** with optional time-slicing
- **Message-passing IPC** as primary communication (no shared memory between processes)
- **Two-tier memory system**: Regions (dynamic) and Partitions (fixed) for predictable allocation
- **Synchronous system calls** with optional asynchronous variants for events
- **4-character object naming** convention for kernel objects

## Codebase Structure

```
GX-Kernel-src/
├── include/
│   ├── gxkernel.h      # Main kernel API header
│   └── types.h         # System type definitions
├── kernel/
│   ├── core/           # Core kernel functionality
│   ├── ipc/            # Inter-process communication
│   ├── memory/         # Memory management (regions, partitions, MMU)
│   ├── sched/          # Task scheduling
│   └── time/           # Timer and time management
├── arch/               # Architecture-specific code
│   ├── arm/            # ARM Cortex-M support
│   ├── riscv/          # RISC-V support
│   └── x86/            # x86 host testing support
├── drivers/            # Device driver framework
├── tests/              # Test suites
└── tools/              # Build and debugging tools
```

## Subsystem Conventions

### Function Naming Prefixes
- `as_` - Asynchronous signaling operations
- `ev_` - Event management
- `k_` - Core kernel functionality
- `m_` - Interrupt vectors and memory mapping
- `pt_` - Memory partition operations
- `q_` - Queue management (fixed-size messages)
- `q_v` - Variable-size queue operations
- `rn_` - Memory region operations
- `sm_` - Semaphore operations
- `t_` - Task management
- `tm_` - Timer operations
- `mm_` - MMU operations
- `de_` - Device management

### Object Naming Convention
All kernel objects use 4-character names (char name[4]):
- Must be null-terminated if less than 4 chars
- Used for tasks, queues, semaphores, regions, partitions
- Enables fast comparison and compact storage

## Critical Implementation Details

### Memory Management

#### Regions (Dynamic Memory)
- First-fit allocation algorithm
- Minimum segment size determined by unit_size (power of 2, ≥16)
- Segments aligned on unit_size boundaries
- Used for variable-size allocations

#### Partitions (Fixed-Size Buffers)
- O(1) allocation/deallocation time
- Buffer size must be power of 2, minimum 4 bytes
- Preferred for real-time allocations
- Used by message queues when Q_PRIBUF flag set

#### MMU Integration
- Supports memory protection between tasks
- Write protection (MM_WPROTECT flag)
- Cache control (MM_NOCACHE flag)
- Access control (MM_NOACCESS flag)

### Task Management

#### Task States
- READY: Eligible to run
- RUNNING: Currently executing
- SUSPENDED: Explicitly suspended
- BLOCKED: Waiting on IPC or timer
- DELETED: Marked for cleanup

#### Task Attributes
- T_PREEMPT/T_NOPREEMPT: Preemption control
- T_TSLICE/T_NOTSLICE: Time-slicing enable
- T_ASR/T_NOASR: Asynchronous signal routine control
- T_FPU/T_NOFPU: Floating-point usage declaration
- T_ISR/T_NOISR: Interrupt enable control

#### Priority System
- Lower numbers = higher priority
- Priority 0 reserved for idle task
- Priority inheritance for priority inversion prevention
- Real-time tasks typically use priorities 1-127

### IPC Mechanisms

#### Message Queues
- Fixed-size: 4 ULONG array (16 bytes) per message
- Variable-size: Configurable maximum message length
- FIFO or priority ordering (Q_FIFO/Q_PRIOR flags)
- Broadcast capability for one-to-many communication
- Urgent messages bypass normal ordering

#### Events
- 32-bit event flags per task
- ANY/ALL waiting modes (EV_ANY/EV_ALL)
- Non-blocking option (EV_NOWAIT)
- Lightweight synchronization mechanism

#### Semaphores
- Counting semaphores with initial count
- Priority or FIFO waiting (SM_PRIOR/SM_FIFO)
- Timeout support on P operations
- Used for mutual exclusion and resource counting

### Real-Time Features

#### Timer Services
- Tick-based timing (system tick rate configurable)
- Absolute time: Date/time/tick representation
- Relative delays: After N ticks
- Periodic timers: Every N ticks
- One-shot and repeating modes

#### Scheduling
- Fixed-priority preemptive by default
- Optional round-robin within same priority (T_TSLICE)
- Idle task runs when no other tasks ready
- Interrupt handlers have implicit highest priority

### Multi-Node Support

#### Node Addressing
- Nodes identified by number (0 = local)
- Global objects visible across nodes (Q_GLOBAL, etc.)
- Local objects restricted to creating node
- Transparent remote object access

#### Distributed Features
- Remote procedure calls via message queues
- Global object namespace
- Node roster tracking (join/leave notifications)
- Sequence number management for ordering

## Error Handling Philosophy

### Error Code Ranges
- 0x00-0xFF: Service-specific errors
- 0x100-0x1FF: I/O subsystem errors
- 0xF00-0xFFF: Fatal/startup errors

### Error Propagation
- All system calls return ULONG error codes
- 0 = success, non-zero = specific error
- errno mechanism for POSIX compatibility
- Fatal errors trigger k_fatal() with optional global broadcast

## Testing Guidelines

### Unit Test Requirements
- Each IPC mechanism must have stress tests
- Timing tests validate real-time properties
- Priority inversion scenarios must be tested
- Memory allocation/deallocation cycles
- Boundary conditions for all inputs

### Integration Testing
- Multi-task scenarios with mixed IPC
- Task creation/deletion under load
- Timer accuracy under system stress
- Interrupt latency measurements

### Multi-Node Testing
- Message delivery across nodes
- Global object synchronization
- Node failure/recovery scenarios
- Network partition handling

## Common Development Tasks

### Adding a New System Call
1. Add function prototype to `gxkernel.h`
2. Define error codes if needed (follow existing ranges)
3. Implement in appropriate subsystem directory
4. Add system call number to syscall table
5. Create test cases in tests/
6. Update documentation

### Implementing a Device Driver
1. Create driver structure following `ioparms` interface
2. Implement six standard functions (init, open, close, read, write, cntrl)
3. Register in device table
4. Handle interrupts via event or ASR mechanism
5. Test with both polled and interrupt modes

### Porting to New Architecture
1. Implement context switching in arch/*/switch.S
2. Create interrupt entry/exit in arch/*/interrupt.S
3. Define architecture constants in arch/*/arch.h
4. Implement timer hardware interface
5. Create linker script for memory layout
6. Implement atomic operations if needed

## Performance Considerations

### Critical Paths
- Context switch: Minimize register save/restore
- Interrupt entry: Fast path for time-critical ISRs
- IPC operations: Lock-free where possible
- Scheduler: O(1) selection of highest priority task

### Memory Layout
- Kernel in low memory for efficient addressing
- Task stacks sized appropriately (T_TINYSTK error)
- DMA buffers in non-cached regions
- Critical data in fast memory (TCM if available)

## Security Considerations

### Current Model
- Hardware memory protection between tasks
- Privileged kernel mode operations
- Object ownership and access control
- Input validation on all system calls

### Future Enhancements Needed
- Capability-based security model
- Encrypted IPC for sensitive data
- Secure boot integration
- Rate limiting on resource allocation

## Areas Needing Improvement

### High Priority
1. **Power Management**: No CPU idle/sleep support yet
2. **Debugging Infrastructure**: Need kernel-aware debugger support
3. **Profiling**: No built-in performance monitoring
4. **SMP Support**: Currently single-processor only

### Medium Priority
1. **Priority Inheritance**: Basic implementation needs optimization
2. **Memory Allocator**: First-fit could be improved
3. **Driver Framework**: Needs standardized DMA support
4. **Documentation**: API reference needs examples

### Future Considerations
1. **Security Enhancements**: Capability-based model
2. **Virtualization**: Guest OS support
3. **Safety Certification**: DO-178C, ISO 26262 evidence
4. **Advanced Scheduling**: EDF, CBS for mixed criticality

## Coding Standards

### General Rules
- No dynamic allocation in kernel space
- All functions return error codes
- Validate all user inputs
- Use fixed-size types (ULONG, etc.)
- Comment rationale for non-obvious decisions

### Critical Sections
- Disable interrupts minimally
- No blocking operations in ISRs
- Document lock ordering to prevent deadlock
- Prefer lock-free algorithms where possible

### Architecture-Specific Code
- Isolate in arch/ directory
- Use consistent interface across architectures
- Document non-portable assumptions
- Test on multiple architectures regularly

## Build System Notes

### Standard Build
```bash
make clean
make core
```

### Debug Build
```bash
make DEBUG=1 core
```

### Cross-Compilation
```bash
make ARCH=arm CROSS_COMPILE=arm-none-eabi- core
```

## Integration with External Tools

### GDB Debugging
- Symbol information in core.elf
- Kernel-aware scripts in tools/gdb/
- Hardware breakpoint support

### Static Analysis
- Designed for MISRA-C compliance
- Coverity/PVS-Studio clean
- No undefined behavior

### Continuous Integration
- Build for all supported architectures
- Run test suite on QEMU
- Measure code coverage
- Check real-time properties

## Important Invariants

1. **Interrupts disabled** ⟺ **No blocking calls**
2. **Object created** ⟺ **Object in global table**
3. **Task running** ⟺ **Valid stack pointer**
4. **Message sent** ⟺ **Message delivered or timeout**
5. **Memory allocated** ⟺ **Memory tracked**

## Contact and Resources

- Architecture decisions: Document in docs/design/
- Bug reports: Use GitHub issues
- Performance data: Maintain in benchmarks/
- Porting guide: See docs/porting.md

---

*This document is a living guide. Update it when making architectural changes or discovering important implementation details that affect AI-assisted development.*