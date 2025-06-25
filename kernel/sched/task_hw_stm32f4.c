/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        task_hw_stm32f4.c
* Type:        C Source
* Description: STM32F4 Hardware Task Implementation
*
* This file provides hardware-specific task implementation for STM32F4 
* microcontrollers. It uses:
* - PendSV interrupt for context switching
* - Process Stack Pointer (PSP) for task stacks
* - Main Stack Pointer (MSP) for kernel/interrupt stack
* - ARM Cortex-M4 context switching with optional FPU support
*
* Functions:
*   t_hw_stm32f4_init           - Initialize STM32F4 task hardware
*   t_hw_stm32f4_create_context - Create task context
*   t_hw_stm32f4_switch_context - Switch between task contexts
*   t_hw_stm32f4_delete_context - Delete task context
*   t_hw_stm32f4_enable_int     - Enable interrupts
*   t_hw_stm32f4_disable_int    - Disable interrupts
*   t_hw_init                   - Public hardware initialization
*
* Interrupt Handlers:
*   PendSV_Handler              - Context switch interrupt handler
*
**************************************END***************************************/

#include "../../gxkernel.h"
#include "task.h"
#include "task_hw_stm32f4.h"

/* STM32F4 Hardware Task State */
static struct {
    ULONG                       initialized;
    STM32F4_TaskHwContext_TypeDef *current_task;    /* Current running task */
    STM32F4_TaskHwContext_TypeDef *next_task;       /* Next task to run */
    ULONG                       context_switches;   /* Context switch counter */
    ULONG                       interrupt_nesting;  /* Interrupt nesting level */
    ULONG                       fpu_enabled;        /* Global FPU enable flag */
} stm32f4_task_state = {0};

/* Forward declarations */
static ULONG stm32f4_init_context_switching(void);
static void *stm32f4_setup_initial_stack(void *stack_top, void (*entry)(ULONG args[]), ULONG args[4], ULONG fpu_enabled);
static void stm32f4_trigger_context_switch(void);

/******************************************************************************
* STM32F4 Hardware Task Functions
******************************************************************************/

static ULONG t_hw_stm32f4_init(void)
{
    ULONG error;
    
    if (stm32f4_task_state.initialized) {
        return 0;
    }
    
    /* Initialize hardware state */
    stm32f4_task_state.current_task = NULL;
    stm32f4_task_state.next_task = NULL;
    stm32f4_task_state.context_switches = 0;
    stm32f4_task_state.interrupt_nesting = 0;
    stm32f4_task_state.fpu_enabled = 1;  /* Enable FPU by default */
    
    /* Initialize context switching hardware */
    error = stm32f4_init_context_switching();
    if (error != 0) {
        return error;
    }
    
    /* Set PSP to use task stacks, MSP for kernel/interrupts */
    stm32f4_set_control(stm32f4_get_control() | STM32F4_CONTROL_SPSEL_Msk);
    
    stm32f4_task_state.initialized = 1;
    
    return 0;
}

static ULONG t_hw_stm32f4_create_context(void *tcb, void *entry, void *stack, ULONG size)
{
    T_TCB *task = (T_TCB *)tcb;
    STM32F4_TaskHwContext_TypeDef *hw_ctx;
    void *initial_sp;
    ULONG fpu_enabled;
    
    if (task == NULL || entry == NULL || stack == NULL) {
        return ERR_BADPARAM;
    }
    
    if (size < STM32F4_MIN_STACK_SIZE) {
        return ERR_TINYSTK;
    }
    
    /* Check if task uses FPU */
    fpu_enabled = (task->flags & T_FPU) ? 1 : 0;
    
    /* Allocate hardware context structure */
    hw_ctx = malloc(sizeof(STM32F4_TaskHwContext_TypeDef));
    if (hw_ctx == NULL) {
        return ERR_NOMEM;
    }
    
    /* Initialize hardware context */
    hw_ctx->stack_top = (char *)stack + size;  /* Stack grows downward */
    hw_ctx->stack_size = size;
    hw_ctx->fpu_enabled = fpu_enabled;
    
    /* Set up initial stack frame */
    initial_sp = stm32f4_setup_initial_stack(hw_ctx->stack_top, 
                                             (void (*)(ULONG[]))entry, 
                                             task->args, 
                                             fpu_enabled);
    
    hw_ctx->stack_ptr = initial_sp;
    hw_ctx->context = (STM32F4_TaskContext_TypeDef *)initial_sp;
    
    /* Store hardware context in TCB */
    task->hw_context = hw_ctx;
    task->context_size = sizeof(STM32F4_TaskHwContext_TypeDef);
    
    return 0;
}

static void t_hw_stm32f4_switch_context(void *old_tcb, void *new_tcb)
{
    T_TCB *old_task = (T_TCB *)old_tcb;
    T_TCB *new_task = (T_TCB *)new_tcb;
    STM32F4_TaskHwContext_TypeDef *old_ctx = NULL;
    STM32F4_TaskHwContext_TypeDef *new_ctx = NULL;
    
    if (new_task == NULL || new_task->hw_context == NULL) {
        return;
    }
    
    stm32f4_task_state.context_switches++;
    
    /* Get hardware contexts */
    if (old_task != NULL && old_task->hw_context != NULL) {
        old_ctx = (STM32F4_TaskHwContext_TypeDef *)old_task->hw_context;
    }
    
    new_ctx = (STM32F4_TaskHwContext_TypeDef *)new_task->hw_context;
    
    /* Update global pointers for PendSV handler */
    stm32f4_task_state.current_task = old_ctx;
    stm32f4_task_state.next_task = new_ctx;
    
    /* Trigger context switch via PendSV */
    stm32f4_trigger_context_switch();
}

static void t_hw_stm32f4_delete_context(void *tcb)
{
    T_TCB *task = (T_TCB *)tcb;
    STM32F4_TaskHwContext_TypeDef *hw_ctx;
    
    if (task == NULL || task->hw_context == NULL) {
        return;
    }
    
    hw_ctx = (STM32F4_TaskHwContext_TypeDef *)task->hw_context;
    
    /* Clear global pointers if they point to this context */
    if (stm32f4_task_state.current_task == hw_ctx) {
        stm32f4_task_state.current_task = NULL;
    }
    
    if (stm32f4_task_state.next_task == hw_ctx) {
        stm32f4_task_state.next_task = NULL;
    }
    
    /* Free hardware context */
    free(hw_ctx);
    task->hw_context = NULL;
    task->context_size = 0;
}

static void t_hw_stm32f4_enable_int(void)
{
    STM32F4_ENABLE_INTERRUPTS();
}

static void t_hw_stm32f4_disable_int(void)
{
    STM32F4_DISABLE_INTERRUPTS();
}

static ULONG t_hw_stm32f4_get_current_sp(void)
{
    /* Return current Process Stack Pointer */
    return stm32f4_get_psp();
}

/* Hardware operations structure for STM32F4 */
T_HW_OPS t_hw_stm32f4_ops = {
    .init = t_hw_stm32f4_init,
    .create_context = t_hw_stm32f4_create_context,
    .switch_context = t_hw_stm32f4_switch_context,
    .delete_context = t_hw_stm32f4_delete_context,
    .enable_interrupts = t_hw_stm32f4_enable_int,
    .disable_interrupts = t_hw_stm32f4_disable_int,
    .get_current_sp = t_hw_stm32f4_get_current_sp
};

/******************************************************************************
* STM32F4 Hardware Initialization Functions
******************************************************************************/

static ULONG stm32f4_init_context_switching(void)
{
    /* Set PendSV to lowest priority (15) */
    STM32F4_SCB->SHPR[2] |= (STM32F4_PENDSV_PRIORITY << 16);
    
    /* Ensure memory barriers */
    STM32F4_DSB();
    STM32F4_ISB();
    
    return 0;
}

static void *stm32f4_setup_initial_stack(void *stack_top, void (*entry)(ULONG args[]), ULONG args[4], ULONG fpu_enabled)
{
    STM32F4_BasicStackFrame_TypeDef *basic_frame;
    STM32F4_TaskContext_TypeDef *context;
    char *sp;
    
    /* Stack grows downward, ensure 8-byte alignment */
    sp = (char *)stack_top;
    sp = (char *)((ULONG)sp & ~(STM32F4_STACK_ALIGNMENT - 1));
    
    /* Reserve space for hardware-saved registers (basic frame) */
    sp -= sizeof(STM32F4_BasicStackFrame_TypeDef);
    basic_frame = (STM32F4_BasicStackFrame_TypeDef *)sp;
    
    /* Initialize hardware-saved registers */
    basic_frame->r0 = args[0];          /* First argument */
    basic_frame->r1 = args[1];          /* Second argument */
    basic_frame->r2 = args[2];          /* Third argument */
    basic_frame->r3 = args[3];          /* Fourth argument */
    basic_frame->r12 = 0;               /* R12 */
    basic_frame->lr = 0;                /* Link register (return address) */
    basic_frame->pc = (ULONG)entry;     /* Program counter (entry point) */
    basic_frame->psr = STM32F4_INITIAL_PSR; /* Program status register */
    
    /* Reserve space for software-saved context */
    sp -= sizeof(STM32F4_TaskContext_TypeDef);
    context = (STM32F4_TaskContext_TypeDef *)sp;
    
    /* Initialize software-saved registers */
    context->r4 = 0;
    context->r5 = 0;
    context->r6 = 0;
    context->r7 = 0;
    context->r8 = 0;
    context->r9 = 0;
    context->r10 = 0;
    context->r11 = 0;
    
    /* Initialize FPU registers if FPU is enabled for this task */
    if (fpu_enabled) {
        context->s16 = 0;
        context->s17 = 0;
        context->s18 = 0;
        context->s19 = 0;
        context->s20 = 0;
        context->s21 = 0;
        context->s22 = 0;
        context->s23 = 0;
        context->s24 = 0;
        context->s25 = 0;
        context->s26 = 0;
        context->s27 = 0;
        context->s28 = 0;
        context->s29 = 0;
        context->s30 = 0;
        context->s31 = 0;
        
        /* Set exception return value for FPU context */
        context->exc_return = STM32F4_EXC_RETURN_THREAD_PSP;
    } else {
        /* Set exception return value for basic context */
        context->exc_return = STM32F4_EXC_RETURN_THREAD_PSP;
    }
    
    return sp;
}

static void stm32f4_trigger_context_switch(void)
{
    /* Trigger PendSV interrupt for context switch */
    STM32F4_TRIGGER_PENDSV();
    
    /* Memory barrier to ensure PendSV is triggered */
    STM32F4_DSB();
    STM32F4_ISB();
}

/******************************************************************************
* Public Hardware Initialization
******************************************************************************/

ULONG t_hw_init(void)
{
    T_STATE *state = T_GET_STATE();
    
    /* Set up STM32F4 hardware operations */
    state->hw_ops = &t_hw_stm32f4_ops;
    
    /* Initialize hardware */
    if (state->hw_ops->init) {
        return state->hw_ops->init();
    }
    
    return 0;
}

/******************************************************************************
* STM32F4 Interrupt Handlers
******************************************************************************/

void PendSV_Handler(void)
{
    STM32F4_TaskHwContext_TypeDef *current_ctx = stm32f4_task_state.current_task;
    STM32F4_TaskHwContext_TypeDef *next_ctx = stm32f4_task_state.next_task;
    
    /* This would normally be implemented in assembly for optimal performance */
    /* For C implementation, we simulate the context switch */
    
    if (current_ctx != NULL) {
        /* Save current PSP to current task context */
        current_ctx->stack_ptr = (void *)stm32f4_get_psp();
    }
    
    if (next_ctx != NULL) {
        /* Load PSP from next task context */
        stm32f4_set_psp((ULONG)next_ctx->stack_ptr);
        
        /* Update current task pointer */
        stm32f4_task_state.current_task = next_ctx;
        stm32f4_task_state.next_task = NULL;
    }
    
    /* Clear PendSV flag */
    STM32F4_CLEAR_PENDSV();
}

/******************************************************************************
* Assembly Context Switch Implementation (would be in separate .s file)
******************************************************************************/

/*
 * The actual PendSV handler would be implemented in assembly for maximum
 * efficiency. Here's what it would look like:
 *
 * PendSV_Handler:
 *     ; Disable interrupts
 *     cpsid   i
 *     
 *     ; Check if we have a current task
 *     ldr     r0, =stm32f4_task_state
 *     ldr     r1, [r0, #0]    ; current_task
 *     cbz     r1, load_next   ; If no current task, just load next
 *     
 *     ; Save current task context
 *     mrs     r2, psp         ; Get current PSP
 *     stmdb   r2!, {r4-r11}   ; Save R4-R11 on task stack
 *     str     r2, [r1, #8]    ; Save new PSP to current_task->stack_ptr
 *     
 * load_next:
 *     ; Load next task context
 *     ldr     r1, [r0, #4]    ; next_task
 *     cbz     r1, exit        ; If no next task, exit
 *     
 *     ldr     r2, [r1, #8]    ; Load next_task->stack_ptr
 *     ldmia   r2!, {r4-r11}   ; Restore R4-R11 from task stack
 *     msr     psp, r2         ; Set PSP to new task stack
 *     
 *     ; Update current_task = next_task, next_task = NULL
 *     str     r1, [r0, #0]    ; current_task = next_task
 *     movs    r1, #0
 *     str     r1, [r0, #4]    ; next_task = NULL
 *     
 * exit:
 *     ; Enable interrupts and return
 *     cpsie   i
 *     bx      lr              ; Return from exception
 */

/******************************************************************************
* Utility Functions for Debugging/Configuration
******************************************************************************/

/* Get STM32F4 task hardware state */
void t_hw_stm32f4_get_state(ULONG *initialized, ULONG *context_switches, 
                            ULONG *interrupt_nesting, ULONG *fpu_enabled)
{
    if (initialized) *initialized = stm32f4_task_state.initialized;
    if (context_switches) *context_switches = stm32f4_task_state.context_switches;
    if (interrupt_nesting) *interrupt_nesting = stm32f4_task_state.interrupt_nesting;
    if (fpu_enabled) *fpu_enabled = stm32f4_task_state.fpu_enabled;
}

/* Enable/disable FPU for new tasks */
ULONG t_hw_stm32f4_set_fpu_enabled(ULONG enabled)
{
    if (stm32f4_task_state.initialized) {
        return ERR_ALREADY_INIT;
    }
    
    stm32f4_task_state.fpu_enabled = enabled ? 1 : 0;
    return 0;
}

/* Get current task hardware context */
void *t_hw_stm32f4_get_current_context(void)
{
    return stm32f4_task_state.current_task;
}

/* Force a context switch (for testing) */
void t_hw_stm32f4_force_context_switch(void)
{
    stm32f4_trigger_context_switch();
}

/* Check if we're in interrupt context */
ULONG t_hw_stm32f4_in_interrupt(void)
{
    return (STM32F4_SCB->ICSR & STM32F4_SCB_ICSR_VECTACTIVE_Msk) != 0;
}

/* Get stack usage for current task */
ULONG t_hw_stm32f4_get_stack_usage(void)
{
    STM32F4_TaskHwContext_TypeDef *current = stm32f4_task_state.current_task;
    ULONG current_sp;
    
    if (current == NULL) {
        return 0;
    }
    
    current_sp = stm32f4_get_psp();
    
    if (current_sp >= (ULONG)current->stack_top) {
        return 0;  /* Stack overflow */
    }
    
    return (ULONG)current->stack_top - current_sp;
}