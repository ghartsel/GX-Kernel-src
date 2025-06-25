/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        timer_hw_stm32f4.c
* Type:        C Source
* Description: STM32F4 Hardware Timer Implementation
*
* This file provides hardware-specific timer implementation for STM32F4 
* microcontrollers. It uses:
* - SysTick timer for system tick generation (100 Hz default)
* - TIM2 (32-bit timer) for alarm functionality
* - NVIC for interrupt management
*
* Functions:
*   tm_hw_stm32f4_init        - Initialize STM32F4 timer hardware
*   tm_hw_stm32f4_get_ticks   - Get current tick count
*   tm_hw_stm32f4_set_alarm   - Set next alarm
*   tm_hw_stm32f4_enable_int  - Enable timer interrupts
*   tm_hw_stm32f4_disable_int - Disable timer interrupts
*   tm_hw_init                - Public hardware initialization
*
* Interrupt Handlers:
*   SysTick_Handler           - System tick interrupt handler
*   TIM2_IRQHandler           - Timer 2 interrupt handler (alarms)
*
**************************************END***************************************/

#include "../../gxkernel.h"
#include "timer.h"
#include "timer_hw_stm32f4.h"

/* STM32F4 Hardware Timer State */
static struct {
    ULONG           initialized;
    ULONG           tick_count;         /* System tick counter */
    ULONG           ticks_per_sec;      /* System tick rate */
    ULONG           sysclk_freq;        /* System clock frequency */
    ULONG           apb1_freq;          /* APB1 bus frequency */
    ULONG           alarm_active;       /* Alarm timer active flag */
    ULONG           alarm_target;       /* Target tick for alarm */
} stm32f4_timer_state = {0};

/* Forward declarations */
static ULONG stm32f4_get_sysclk_freq(void);
static ULONG stm32f4_systick_init(ULONG tick_rate);
static ULONG stm32f4_tim2_init(void);
static void stm32f4_tim2_start_alarm(ULONG target_ticks);
static void stm32f4_tim2_stop_alarm(void);

/******************************************************************************
* STM32F4 Hardware Timer Functions
******************************************************************************/

static ULONG tm_hw_stm32f4_init(void)
{
    ULONG error;
    
    if (stm32f4_timer_state.initialized) {
        return 0;
    }
    
    /* Initialize hardware state */
    stm32f4_timer_state.tick_count = 0;
    stm32f4_timer_state.ticks_per_sec = STM32F4_DEFAULT_TICK_RATE;
    stm32f4_timer_state.alarm_active = 0;
    stm32f4_timer_state.alarm_target = 0;
    
    /* Determine system clock frequency */
    stm32f4_timer_state.sysclk_freq = stm32f4_get_sysclk_freq();
    stm32f4_timer_state.apb1_freq = stm32f4_timer_state.sysclk_freq / 2; /* APB1 = SYSCLK/2 */
    
    /* Initialize SysTick timer for system tick */
    error = stm32f4_systick_init(stm32f4_timer_state.ticks_per_sec);
    if (error != 0) {
        return error;
    }
    
    /* Initialize TIM2 for alarm functionality */
    error = stm32f4_tim2_init();
    if (error != 0) {
        return error;
    }
    
    stm32f4_timer_state.initialized = 1;
    
    return 0;
}

static ULONG tm_hw_stm32f4_get_ticks(void)
{
    if (!stm32f4_timer_state.initialized) {
        return 0;
    }
    
    return stm32f4_timer_state.tick_count;
}

static ULONG tm_hw_stm32f4_set_alarm(ULONG target_ticks)
{
    ULONG current_ticks;
    ULONG ticks_to_wait;
    
    if (!stm32f4_timer_state.initialized) {
        return ERR_NOTINIT;
    }
    
    current_ticks = stm32f4_timer_state.tick_count;
    
    /* Stop any existing alarm */
    stm32f4_tim2_stop_alarm();
    
    if (target_ticks <= current_ticks) {
        /* Alarm is in the past or now - fire immediately by calling tm_tick */
        tm_tick();
        return 0;
    }
    
    ticks_to_wait = target_ticks - current_ticks;
    stm32f4_timer_state.alarm_target = target_ticks;
    
    /* Start TIM2 alarm */
    stm32f4_tim2_start_alarm(ticks_to_wait);
    
    stm32f4_timer_state.alarm_active = 1;
    
    return 0;
}

static void tm_hw_stm32f4_enable_int(void)
{
    if (!stm32f4_timer_state.initialized) {
        return;
    }
    
    /* Enable SysTick interrupt */
    STM32F4_SYSTICK->CTRL |= STM32F4_SYSTICK_CTRL_TICKINT;
    
    /* Enable TIM2 interrupt in NVIC */
    STM32F4_NVIC_ENABLE_IRQ(STM32F4_IRQ_TIM2);
}

static void tm_hw_stm32f4_disable_int(void)
{
    if (!stm32f4_timer_state.initialized) {
        return;
    }
    
    /* Disable SysTick interrupt */
    STM32F4_SYSTICK->CTRL &= ~STM32F4_SYSTICK_CTRL_TICKINT;
    
    /* Disable TIM2 interrupt in NVIC */
    STM32F4_NVIC_DISABLE_IRQ(STM32F4_IRQ_TIM2);
    
    /* Stop alarm timer */
    stm32f4_tim2_stop_alarm();
    stm32f4_timer_state.alarm_active = 0;
}

/* Hardware operations structure for STM32F4 */
TM_HW_OPS tm_hw_stm32f4_ops = {
    .init = tm_hw_stm32f4_init,
    .get_ticks = tm_hw_stm32f4_get_ticks,
    .set_alarm = tm_hw_stm32f4_set_alarm,
    .enable_interrupt = tm_hw_stm32f4_enable_int,
    .disable_interrupt = tm_hw_stm32f4_disable_int
};

/******************************************************************************
* STM32F4 Hardware Initialization Functions
******************************************************************************/

static ULONG stm32f4_get_sysclk_freq(void)
{
    /* Simplified clock detection - in real implementation would read RCC registers */
    /* For now, assume default configuration */
    return STM32F4_DEFAULT_SYSCLK;
}

static ULONG stm32f4_systick_init(ULONG tick_rate)
{
    ULONG reload_value;
    
    if (tick_rate == 0 || tick_rate > 10000) {
        return ERR_BADPARAM;
    }
    
    /* Calculate reload value for desired tick rate */
    reload_value = (stm32f4_timer_state.sysclk_freq / tick_rate) - 1;
    
    if (reload_value > 0x00FFFFFF) {  /* SysTick is 24-bit */
        return ERR_BADPARAM;
    }
    
    /* Disable SysTick during configuration */
    STM32F4_SYSTICK->CTRL = 0;
    
    /* Set reload value */
    STM32F4_SYSTICK->LOAD = reload_value;
    
    /* Clear current value */
    STM32F4_SYSTICK->VAL = 0;
    
    /* Configure SysTick: processor clock, interrupt enabled, counter enabled */
    STM32F4_SYSTICK->CTRL = STM32F4_SYSTICK_CTRL_CLKSOURCE | 
                           STM32F4_SYSTICK_CTRL_TICKINT | 
                           STM32F4_SYSTICK_CTRL_ENABLE;
    
    /* Memory barrier to ensure configuration is complete */
    STM32F4_DSB();
    STM32F4_ISB();
    
    return 0;
}

static ULONG stm32f4_tim2_init(void)
{
    /* Enable TIM2 clock */
    STM32F4_ENABLE_TIM2_CLOCK();
    
    /* Reset TIM2 to default state */
    STM32F4_TIM2->CR1 = 0;
    STM32F4_TIM2->CR2 = 0;
    STM32F4_TIM2->DIER = 0;
    STM32F4_TIM2->SR = 0;
    STM32F4_TIM2->CNT = 0;
    
    /* Configure TIM2 prescaler for 1 MHz clock (1 µs resolution) */
    /* APB1 clock is typically 84 MHz, so prescaler = 83 gives 1 MHz */
    STM32F4_TIM2->PSC = STM32F4_TIMER_PRESCALER_1MHZ;
    
    /* Set maximum auto-reload value (32-bit timer) */
    STM32F4_TIM2->ARR = STM32F4_TIMER_MAX_COUNT;
    
    /* Configure for one-pulse mode initially (will be reconfigured per alarm) */
    STM32F4_TIM2->CR1 = STM32F4_TIM_CR1_OPM;  /* One pulse mode */
    
    /* Set interrupt priority */
    STM32F4_NVIC_SET_PRIORITY(STM32F4_IRQ_TIM2, STM32F4_IRQ_PRIORITY_TIMER);
    
    /* Enable TIM2 interrupt in NVIC */
    STM32F4_NVIC_ENABLE_IRQ(STM32F4_IRQ_TIM2);
    
    /* Memory barrier */
    STM32F4_DSB();
    
    return 0;
}

static void stm32f4_tim2_start_alarm(ULONG ticks_to_wait)
{
    ULONG microseconds;
    
    /* Stop TIM2 if running */
    STM32F4_TIM2->CR1 &= ~STM32F4_TIM_CR1_CEN;
    
    /* Clear any pending interrupt */
    STM32F4_TIM2->SR = 0;
    
    /* Convert ticks to microseconds (1 tick = 10ms at 100Hz = 10000 µs) */
    microseconds = (ticks_to_wait * 1000000UL) / stm32f4_timer_state.ticks_per_sec;
    
    /* Set alarm time (TIM2 runs at 1 MHz, so count = microseconds) */
    if (microseconds > STM32F4_TIMER_MAX_COUNT) {
        microseconds = STM32F4_TIMER_MAX_COUNT;
    }
    
    STM32F4_TIM2->ARR = microseconds;
    STM32F4_TIM2->CNT = 0;
    
    /* Enable update interrupt */
    STM32F4_TIM2->DIER = STM32F4_TIM_DIER_UIE;
    
    /* Configure for one-pulse mode and start */
    STM32F4_TIM2->CR1 = STM32F4_TIM_CR1_OPM | STM32F4_TIM_CR1_CEN;
    
    /* Memory barrier */
    STM32F4_DSB();
}

static void stm32f4_tim2_stop_alarm(void)
{
    /* Stop TIM2 */
    STM32F4_TIM2->CR1 &= ~STM32F4_TIM_CR1_CEN;
    
    /* Disable interrupt */
    STM32F4_TIM2->DIER = 0;
    
    /* Clear any pending interrupt */
    STM32F4_TIM2->SR = 0;
    
    /* Memory barrier */
    STM32F4_DSB();
}

/******************************************************************************
* Public Hardware Initialization
******************************************************************************/

ULONG tm_hw_init(void)
{
    TM_STATE *state = TM_GET_STATE();
    
    /* Set up STM32F4 hardware operations */
    state->hw_ops = &tm_hw_stm32f4_ops;
    
    /* Initialize hardware */
    if (state->hw_ops->init) {
        return state->hw_ops->init();
    }
    
    return 0;
}

/******************************************************************************
* STM32F4 Interrupt Handlers
******************************************************************************/

void SysTick_Handler(void)
{
    /* Increment system tick counter */
    stm32f4_timer_state.tick_count++;
    
    /* Call kernel timer tick processing */
    tm_tick();
    
    /* Note: SysTick COUNTFLAG is automatically cleared by reading CTRL */
}

void TIM2_IRQHandler(void)
{
    /* Check if this is an update interrupt */
    if (STM32F4_TIM2->SR & STM32F4_TIM_SR_UIF) {
        /* Clear interrupt flag */
        STM32F4_TIM2->SR &= ~STM32F4_TIM_SR_UIF;
        
        /* Mark alarm as inactive */
        stm32f4_timer_state.alarm_active = 0;
        
        /* Call kernel timer tick processing for alarm */
        tm_tick();
    }
}

/******************************************************************************
* Utility Functions for Debugging/Configuration
******************************************************************************/

/* Get STM32F4 timer hardware state */
void tm_hw_stm32f4_get_state(ULONG *initialized, ULONG *tick_count, 
                             ULONG *ticks_per_sec, ULONG *sysclk_freq)
{
    if (initialized) *initialized = stm32f4_timer_state.initialized;
    if (tick_count) *tick_count = stm32f4_timer_state.tick_count;
    if (ticks_per_sec) *ticks_per_sec = stm32f4_timer_state.ticks_per_sec;
    if (sysclk_freq) *sysclk_freq = stm32f4_timer_state.sysclk_freq;
}

/* Set tick rate (must be called before initialization) */
ULONG tm_hw_stm32f4_set_tick_rate(ULONG tick_rate)
{
    if (stm32f4_timer_state.initialized) {
        return ERR_ALREADY_INIT;
    }
    
    if (tick_rate == 0 || tick_rate > 10000) {
        return ERR_BADPARAM;
    }
    
    stm32f4_timer_state.ticks_per_sec = tick_rate;
    return 0;
}

/* Check if alarm is active */
ULONG tm_hw_stm32f4_is_alarm_active(void)
{
    return stm32f4_timer_state.alarm_active;
}

/* Get current SysTick counter value (for high-resolution timing) */
ULONG tm_hw_stm32f4_get_systick_count(void)
{
    if (!stm32f4_timer_state.initialized) {
        return 0;
    }
    
    return STM32F4_SYSTICK->VAL;
}

/* Force a system tick (for testing) */
void tm_hw_stm32f4_force_tick(void)
{
    SysTick_Handler();
}