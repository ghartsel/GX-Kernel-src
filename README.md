# Welcome to the *Core* microkernel

The Core microkernel is a supervisory space component that provides essential real-time services that meet the demands of today’s hard-deadline, embedded systems. It is the foundational component for implementing applications, device drivers, middleware, and a full-featured operating system.

The design emphasizes high run-time performance and reliability with consideration for robust system development and maintenance.

## Features

This microkernel, designed for microcontrollers, is a mature, well-structured microkernel featuring an API with comprehensive functionality.

### Comprehensive Task Management

The interface provides excellent task control with:
- Task creation/deletion (`t_create`, `t_delete`)
- Task control (`t_start`, `t_suspend`, `t_resume`, `t_restart`)
- Priority management (`t_setpri`)
- Register access (`t_getreg`, `t_setreg`)
- Mode control with preemption and time-slicing options

### Rich IPC Mechanisms

Multiple IPC primitives are well-represented:
- Message Queues: Both fixed-size (`q_*`) and variable-size (`q_v*`) with broadcast, urgent, and async variants
- Events: Synchronous event passing (`ev_send`, `ev_receive`)
- Semaphores: Full implementation with timeout support (`sm_create`, `sm_p`, `sm_v`)
- Asynchronous signals: ASR support (`as_catch`, `as_send`, `as_return`)

### Memory Management

Two-tier memory system appropriate for embedded systems:
- Regions (`rn_*`): Dynamic memory allocation with segments
- Partitions (`pt_*`): Fixed-size buffer pools for deterministic allocation
- MMU support (`mm_*`): Virtual-to-physical mapping, protection, and access control

### Real-Time Features

Strong real-time support evident in:
- Comprehensive timer services (`tm_*`) with absolute and relative timing
- Priority-based queueing options
- Timeout parameters on blocking calls
- Time-slicing control

### Multi-Node Support

The interface shows distributed system capabilities:
- Global vs. local object creation flags
- Node specifiers in identification functions
- Roster change notifications
- Multi-processor configuration support

### Device Management

Basic but complete I/O subsystem (`de_*`) with standard operations:
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
