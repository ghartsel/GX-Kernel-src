# Welcome to the *Core* microkernel

The Core microkernel is a supervisory space component that provides essential real-time services that meet the demands of today’s hard-deadline, embedded systems. It is the foundational component for implementing applications, device drivers, middleware, and a full-featured operating system.

The microkernel is intended for for industrial control systems, automotive applications, or other embedded real-time scenarios, and the design prioritizes reliability, determinism, and distributed operation.

## Features

This microkernel, designed for microcontrollers, is a mature, well-structured microkernel that features an API with comprehensive functionality. It provides a solid core for abstracting middleware and application-specific logic.

### Comprehensive Task Management

The interface provides task control with:

- Task creation/deletion (`t_create`, `t_delete`)
- Task control (`t_start`, `t_suspend`, `t_resume`, `t_restart`)
- Priority management (`t_setpri`)
- Register access (`t_getreg`, `t_setreg`)
- Mode control with preemption and time-slicing options

### Rich IPC Mechanisms

Multiple IPC primitives:

- Message Queues: Both fixed-size and variable-size (`q_v*`) with broadcast, urgent, and async variants
- Events: Synchronous event passing (`ev_send`, `ev_receive`)
- Semaphores: Full implementation with timeout support (`sm_create`, `sm_p`, `sm_v`)
- Asynchronous signals: ASR support (`as_catch`, `as_send`, `as_return`)

### Memory Management

Two-tier memory system appropriate for embedded systems:

- Regions: Dynamic memory allocation with segments
- Partitions: Fixed-size buffer pools for deterministic allocation
- MMU support: Virtual-to-physical mapping, protection, and access control

### Real-Time Features

Strong real-time support:

- Comprehensive timer services with absolute and relative timing
- Priority-based queueing options
- Timeout parameters on blocking calls
- Time-slicing control

### Multi-Node Support

Distributed system capabilities:

- Global vs. local object creation flags
- Node specifiers in identification functions
- Roster change notifications
- Multi-processor configuration support

### Device Management

Basic, complete I/O subsystem with standard operations:

- init, open, close, read, write, control

## Set up

```
git clone git@github.com:ghartsel/Core.git
make setup
```

## Build and test Core (locally)

```
make clean
make core
cd bin
./core
```

## License

MIT License - see [LICENSE](LICENSE) file for details.
