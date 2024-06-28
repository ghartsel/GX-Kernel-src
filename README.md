# Welcome to the *Core* microkernel

The Core microkernel is a supervisory space component that provides essential real-time services that meet the demands of today’s hard-deadline, embedded systems. It is the foundational component for implementing applications, device drivers, middleware, and a full-featured operating system.

The design emphasizes high run-time performance and reliability with consideration for robust system development and maintenance.

## Features

- *Preemptive Multitasking* scheduler for real-time responsiveness.

- Selectable *scheduling algorithm*: rate monotonic, earliest deadline first, round robin, timeslice

- *Multiple-instance* tasks.

- *Memory management* for protection and fault isolation.

- *Fault-tolerant* active fault detection and isolation.

- *C-language Interface* for high-level language support.

- *Layered architecture*:

    - Kernel
        - Task management
        - Resource mapping
        - IPC
        - device communication
        - IRQ handling
    - Service
        - UART
        - timer
    - Abstract
        - OS interface
        - application
        - driver
        - middleware    

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

## Develop applications with Core

See the [Core Programming Guide](http://gxkernel.s3-website-us-west-1.amazonaws.com/index.html) to learn how to design, build, configure, and deploy an application built on Core.

## License

The MIT License (MIT)

Copyright (c) 2024 Gene Hartsell

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

