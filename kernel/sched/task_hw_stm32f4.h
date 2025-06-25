/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        task_hw_stm32f4.h
* Type:        C Header
* Description: STM32F4 Task Hardware Register Definitions
*
* This header defines the STM32F4 ARM Cortex-M4 specific registers and constants
* needed for task context switching and stack management.
*
**************************************END***************************************/

#ifndef _TASK_HW_STM32F4_H_
#define _TASK_HW_STM32F4_H_

#include "../../types.h"

/* ARM Cortex-M4 System Control Block (SCB) */
#define STM32F4_SCB_BASE        0xE000ED00UL
#define STM32F4_SCB             ((STM32F4_SCB_TypeDef *)STM32F4_SCB_BASE)

typedef struct {
    volatile ULONG CPUID;       /* CPU ID Base Register */
    volatile ULONG ICSR;        /* Interrupt Control and State Register */
    volatile ULONG VTOR;        /* Vector Table Offset Register */
    volatile ULONG AIRCR;       /* Application Interrupt and Reset Control Register */
    volatile ULONG SCR;         /* System Control Register */
    volatile ULONG CCR;         /* Configuration Control Register */
    volatile ULONG SHPR[3];     /* System Handler Priority Register */
    volatile ULONG SHCSR;       /* System Handler Control and State Register */
    volatile ULONG CFSR;        /* Configurable Fault Status Register */
    volatile ULONG HFSR;        /* Hard Fault Status Register */
    volatile ULONG DFSR;        /* Debug Fault Status Register */
    volatile ULONG MMFAR;       /* Memory Management Fault Address Register */
    volatile ULONG BFAR;        /* Bus Fault Address Register */
    volatile ULONG AFSR;        /* Auxiliary Fault Status Register */
} STM32F4_SCB_TypeDef;

/* ICSR (Interrupt Control and State Register) bits */
#define STM32F4_SCB_ICSR_VECTACTIVE_Pos     0
#define STM32F4_SCB_ICSR_VECTACTIVE_Msk     (0x1FFUL << STM32F4_SCB_ICSR_VECTACTIVE_Pos)
#define STM32F4_SCB_ICSR_RETTOBASE_Pos      11
#define STM32F4_SCB_ICSR_RETTOBASE_Msk      (1UL << STM32F4_SCB_ICSR_RETTOBASE_Pos)
#define STM32F4_SCB_ICSR_VECTPENDING_Pos    12
#define STM32F4_SCB_ICSR_VECTPENDING_Msk    (0x1FFUL << STM32F4_SCB_ICSR_VECTPENDING_Pos)
#define STM32F4_SCB_ICSR_ISRPENDING_Pos     22
#define STM32F4_SCB_ICSR_ISRPENDING_Msk     (1UL << STM32F4_SCB_ICSR_ISRPENDING_Pos)
#define STM32F4_SCB_ICSR_PENDSTCLR_Pos      25
#define STM32F4_SCB_ICSR_PENDSTCLR_Msk      (1UL << STM32F4_SCB_ICSR_PENDSTCLR_Pos)
#define STM32F4_SCB_ICSR_PENDSTSET_Pos      26
#define STM32F4_SCB_ICSR_PENDSTSET_Msk      (1UL << STM32F4_SCB_ICSR_PENDSTSET_Pos)
#define STM32F4_SCB_ICSR_PENDSVCLR_Pos      27
#define STM32F4_SCB_ICSR_PENDSVCLR_Msk      (1UL << STM32F4_SCB_ICSR_PENDSVCLR_Pos)
#define STM32F4_SCB_ICSR_PENDSVSET_Pos      28
#define STM32F4_SCB_ICSR_PENDSVSET_Msk      (1UL << STM32F4_SCB_ICSR_PENDSVSET_Pos)
#define STM32F4_SCB_ICSR_NMIPENDSET_Pos     31
#define STM32F4_SCB_ICSR_NMIPENDSET_Msk     (1UL << STM32F4_SCB_ICSR_NMIPENDSET_Pos)

/* ARM Cortex-M4 Core Registers for Context Switching */
#define STM32F4_CONTROL_SPSEL_Pos           1
#define STM32F4_CONTROL_SPSEL_Msk           (1UL << STM32F4_CONTROL_SPSEL_Pos)
#define STM32F4_CONTROL_FPCA_Pos            2
#define STM32F4_CONTROL_FPCA_Msk            (1UL << STM32F4_CONTROL_FPCA_Pos)

/* Exception Return Values */
#define STM32F4_EXC_RETURN_THREAD_MSP       0xFFFFFFF9UL    /* Return to Thread mode, use MSP */
#define STM32F4_EXC_RETURN_THREAD_PSP       0xFFFFFFFDUL    /* Return to Thread mode, use PSP */
#define STM32F4_EXC_RETURN_HANDLER_MSP      0xFFFFFFF1UL    /* Return to Handler mode, use MSP */

/* Stack Frame Definitions */

/* Basic stack frame (without FPU) */
typedef struct {
    ULONG r0;       /* R0 register */
    ULONG r1;       /* R1 register */
    ULONG r2;       /* R2 register */
    ULONG r3;       /* R3 register */
    ULONG r12;      /* R12 register */
    ULONG lr;       /* Link Register (R14) */
    ULONG pc;       /* Program Counter (R15) */
    ULONG psr;      /* Program Status Register */
} STM32F4_BasicStackFrame_TypeDef;

/* Extended stack frame (with FPU) */
typedef struct {
    ULONG r0;       /* R0 register */
    ULONG r1;       /* R1 register */
    ULONG r2;       /* R2 register */
    ULONG r3;       /* R3 register */
    ULONG r12;      /* R12 register */
    ULONG lr;       /* Link Register (R14) */
    ULONG pc;       /* Program Counter (R15) */
    ULONG psr;      /* Program Status Register */
    ULONG s0;       /* FPU S0 register */
    ULONG s1;       /* FPU S1 register */
    ULONG s2;       /* FPU S2 register */
    ULONG s3;       /* FPU S3 register */
    ULONG s4;       /* FPU S4 register */
    ULONG s5;       /* FPU S5 register */
    ULONG s6;       /* FPU S6 register */
    ULONG s7;       /* FPU S7 register */
    ULONG s8;       /* FPU S8 register */
    ULONG s9;       /* FPU S9 register */
    ULONG s10;      /* FPU S10 register */
    ULONG s11;      /* FPU S11 register */
    ULONG s12;      /* FPU S12 register */
    ULONG s13;      /* FPU S13 register */
    ULONG s14;      /* FPU S14 register */
    ULONG s15;      /* FPU S15 register */
    ULONG fpscr;    /* FPU Status and Control Register */
    ULONG reserved; /* Reserved for alignment */
} STM32F4_ExtendedStackFrame_TypeDef;

/* Complete task context structure */
typedef struct {
    /* Software-saved registers (saved by PendSV handler) */
    ULONG r4;       /* R4 register */
    ULONG r5;       /* R5 register */
    ULONG r6;       /* R6 register */
    ULONG r7;       /* R7 register */
    ULONG r8;       /* R8 register */
    ULONG r9;       /* R9 register */
    ULONG r10;      /* R10 register */
    ULONG r11;      /* R11 register */
    
    /* FPU registers (if FPU enabled) */
    ULONG s16;      /* FPU S16 register */
    ULONG s17;      /* FPU S17 register */
    ULONG s18;      /* FPU S18 register */
    ULONG s19;      /* FPU S19 register */
    ULONG s20;      /* FPU S20 register */
    ULONG s21;      /* FPU S21 register */
    ULONG s22;      /* FPU S22 register */
    ULONG s23;      /* FPU S23 register */
    ULONG s24;      /* FPU S24 register */
    ULONG s25;      /* FPU S25 register */
    ULONG s26;      /* FPU S26 register */
    ULONG s27;      /* FPU S27 register */
    ULONG s28;      /* FPU S28 register */
    ULONG s29;      /* FPU S29 register */
    ULONG s30;      /* FPU S30 register */
    ULONG s31;      /* FPU S31 register */
    
    /* Exception return value */
    ULONG exc_return; /* Exception return value */
} STM32F4_TaskContext_TypeDef;

/* Task-specific context including stack pointer */
typedef struct {
    STM32F4_TaskContext_TypeDef *context;   /* Pointer to saved context */
    void                        *stack_top; /* Top of task stack */
    void                        *stack_ptr; /* Current stack pointer */
    ULONG                       stack_size; /* Size of task stack */
    ULONG                       fpu_enabled; /* FPU usage flag */
} STM32F4_TaskHwContext_TypeDef;

/* Stack alignment and sizes */
#define STM32F4_STACK_ALIGNMENT     8       /* 8-byte stack alignment required */
#define STM32F4_MIN_STACK_SIZE      512     /* Minimum stack size */
#define STM32F4_CONTEXT_SIZE        sizeof(STM32F4_TaskContext_TypeDef)
#define STM32F4_BASIC_FRAME_SIZE    sizeof(STM32F4_BasicStackFrame_TypeDef)
#define STM32F4_EXTENDED_FRAME_SIZE sizeof(STM32F4_ExtendedStackFrame_TypeDef)

/* Initial PSR value for new tasks */
#define STM32F4_INITIAL_PSR         0x01000000UL    /* Thumb bit set */

/* Memory barrier macros for ARM Cortex-M4 */
#define STM32F4_DSB()   __asm volatile ("dsb" ::: "memory")
#define STM32F4_ISB()   __asm volatile ("isb" ::: "memory")
#define STM32F4_DMB()   __asm volatile ("dmb" ::: "memory")

/* Interrupt control macros */
#define STM32F4_DISABLE_INTERRUPTS() __asm volatile ("cpsid i" ::: "memory")
#define STM32F4_ENABLE_INTERRUPTS()  __asm volatile ("cpsie i" ::: "memory")

/* PendSV interrupt priority (lowest priority) */
#define STM32F4_PENDSV_PRIORITY     0xFF

/* Function-like macros for context switching */
#define STM32F4_TRIGGER_PENDSV()    (STM32F4_SCB->ICSR |= STM32F4_SCB_ICSR_PENDSVSET_Msk)
#define STM32F4_CLEAR_PENDSV()      (STM32F4_SCB->ICSR |= STM32F4_SCB_ICSR_PENDSVCLR_Msk)

/* Inline assembly functions for register access */
static inline ULONG stm32f4_get_psp(void)
{
    ULONG psp;
    __asm volatile ("mrs %0, psp" : "=r" (psp));
    return psp;
}

static inline void stm32f4_set_psp(ULONG psp)
{
    __asm volatile ("msr psp, %0" :: "r" (psp) : "memory");
}

static inline ULONG stm32f4_get_msp(void)
{
    ULONG msp;
    __asm volatile ("mrs %0, msp" : "=r" (msp));
    return msp;
}

static inline void stm32f4_set_msp(ULONG msp)
{
    __asm volatile ("msr msp, %0" :: "r" (msp) : "memory");
}

static inline ULONG stm32f4_get_control(void)
{
    ULONG control;
    __asm volatile ("mrs %0, control" : "=r" (control));
    return control;
}

static inline void stm32f4_set_control(ULONG control)
{
    __asm volatile ("msr control, %0" :: "r" (control) : "memory");
    STM32F4_ISB();
}

static inline ULONG stm32f4_get_primask(void)
{
    ULONG primask;
    __asm volatile ("mrs %0, primask" : "=r" (primask));
    return primask;
}

static inline void stm32f4_set_primask(ULONG primask)
{
    __asm volatile ("msr primask, %0" :: "r" (primask) : "memory");
}

#endif /* _TASK_HW_STM32F4_H_ */