/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        timer_hw_stm32f4.h
* Type:        C Header
* Description: STM32F4 Hardware Timer Register Definitions
*
* This header defines the STM32F4 specific timer registers and constants
* needed for hardware timer implementation.
*
**************************************END***************************************/

#ifndef _TIMER_HW_STM32F4_H_
#define _TIMER_HW_STM32F4_H_

#include "../../types.h"

/* STM32F4 System Configuration */
#define STM32F4_HSI_FREQ        16000000UL  /* 16 MHz internal oscillator */
#define STM32F4_HSE_FREQ        8000000UL   /* 8 MHz external crystal (typical) */
#define STM32F4_MAX_SYSCLK      168000000UL /* Maximum system clock */
#define STM32F4_DEFAULT_SYSCLK  84000000UL  /* Conservative default */

/* STM32F4 Memory Map Base Addresses */
#define STM32F4_PERIPH_BASE     0x40000000UL
#define STM32F4_APB1_BASE       (STM32F4_PERIPH_BASE + 0x00000)
#define STM32F4_APB2_BASE       (STM32F4_PERIPH_BASE + 0x10000)
#define STM32F4_AHB1_BASE       (STM32F4_PERIPH_BASE + 0x20000)

/* SysTick Timer (Cortex-M4 Core) */
#define STM32F4_SYSTICK_BASE    0xE000E010UL
#define STM32F4_SYSTICK         ((STM32F4_SysTick_TypeDef *)STM32F4_SYSTICK_BASE)

typedef struct {
    volatile ULONG CTRL;    /* SysTick Control and Status Register */
    volatile ULONG LOAD;    /* SysTick Reload Value Register */
    volatile ULONG VAL;     /* SysTick Current Value Register */
    volatile ULONG CALIB;   /* SysTick Calibration Register */
} STM32F4_SysTick_TypeDef;

/* SysTick Control Register bits */
#define STM32F4_SYSTICK_CTRL_ENABLE     (1UL << 0)  /* Counter enable */
#define STM32F4_SYSTICK_CTRL_TICKINT    (1UL << 1)  /* Exception request enable */
#define STM32F4_SYSTICK_CTRL_CLKSOURCE  (1UL << 2)  /* Clock source (0=external, 1=processor) */
#define STM32F4_SYSTICK_CTRL_COUNTFLAG  (1UL << 16) /* Count flag */

/* Timer 2 (32-bit general purpose timer for alarms) */
#define STM32F4_TIM2_BASE       (STM32F4_APB1_BASE + 0x0000)
#define STM32F4_TIM2            ((STM32F4_TIM_TypeDef *)STM32F4_TIM2_BASE)

typedef struct {
    volatile ULONG CR1;     /* Control register 1 */
    volatile ULONG CR2;     /* Control register 2 */
    volatile ULONG SMCR;    /* Slave mode control register */
    volatile ULONG DIER;    /* DMA/interrupt enable register */
    volatile ULONG SR;      /* Status register */
    volatile ULONG EGR;     /* Event generation register */
    volatile ULONG CCMR1;   /* Capture/compare mode register 1 */
    volatile ULONG CCMR2;   /* Capture/compare mode register 2 */
    volatile ULONG CCER;    /* Capture/compare enable register */
    volatile ULONG CNT;     /* Counter */
    volatile ULONG PSC;     /* Prescaler */
    volatile ULONG ARR;     /* Auto-reload register */
    volatile ULONG RESERVED1;
    volatile ULONG CCR1;    /* Capture/compare register 1 */
    volatile ULONG CCR2;    /* Capture/compare register 2 */
    volatile ULONG CCR3;    /* Capture/compare register 3 */
    volatile ULONG CCR4;    /* Capture/compare register 4 */
    volatile ULONG RESERVED2;
    volatile ULONG DCR;     /* DMA control register */
    volatile ULONG DMAR;    /* DMA address for full transfer */
} STM32F4_TIM_TypeDef;

/* Timer Control Register 1 (CR1) bits */
#define STM32F4_TIM_CR1_CEN     (1UL << 0)  /* Counter enable */
#define STM32F4_TIM_CR1_UDIS    (1UL << 1)  /* Update disable */
#define STM32F4_TIM_CR1_URS     (1UL << 2)  /* Update request source */
#define STM32F4_TIM_CR1_OPM     (1UL << 3)  /* One pulse mode */
#define STM32F4_TIM_CR1_DIR     (1UL << 4)  /* Direction */
#define STM32F4_TIM_CR1_ARPE    (1UL << 7)  /* Auto-reload preload enable */

/* Timer DMA/Interrupt Enable Register (DIER) bits */
#define STM32F4_TIM_DIER_UIE    (1UL << 0)  /* Update interrupt enable */
#define STM32F4_TIM_DIER_CC1IE  (1UL << 1)  /* Capture/Compare 1 interrupt enable */
#define STM32F4_TIM_DIER_CC2IE  (1UL << 2)  /* Capture/Compare 2 interrupt enable */
#define STM32F4_TIM_DIER_CC3IE  (1UL << 3)  /* Capture/Compare 3 interrupt enable */
#define STM32F4_TIM_DIER_CC4IE  (1UL << 4)  /* Capture/Compare 4 interrupt enable */

/* Timer Status Register (SR) bits */
#define STM32F4_TIM_SR_UIF      (1UL << 0)  /* Update interrupt flag */
#define STM32F4_TIM_SR_CC1IF    (1UL << 1)  /* Capture/Compare 1 interrupt flag */
#define STM32F4_TIM_SR_CC2IF    (1UL << 2)  /* Capture/Compare 2 interrupt flag */
#define STM32F4_TIM_SR_CC3IF    (1UL << 3)  /* Capture/Compare 3 interrupt flag */
#define STM32F4_TIM_SR_CC4IF    (1UL << 4)  /* Capture/Compare 4 interrupt flag */

/* RCC (Reset and Clock Control) */
#define STM32F4_RCC_BASE        (STM32F4_AHB1_BASE + 0x3800)
#define STM32F4_RCC             ((STM32F4_RCC_TypeDef *)STM32F4_RCC_BASE)

typedef struct {
    volatile ULONG CR;          /* Clock control register */
    volatile ULONG PLLCFGR;     /* PLL configuration register */
    volatile ULONG CFGR;        /* Clock configuration register */
    volatile ULONG CIR;         /* Clock interrupt register */
    volatile ULONG AHB1RSTR;    /* AHB1 peripheral reset register */
    volatile ULONG AHB2RSTR;    /* AHB2 peripheral reset register */
    volatile ULONG AHB3RSTR;    /* AHB3 peripheral reset register */
    volatile ULONG RESERVED0;
    volatile ULONG APB1RSTR;    /* APB1 peripheral reset register */
    volatile ULONG APB2RSTR;    /* APB2 peripheral reset register */
    volatile ULONG RESERVED1[2];
    volatile ULONG AHB1ENR;     /* AHB1 peripheral clock enable register */
    volatile ULONG AHB2ENR;     /* AHB2 peripheral clock enable register */
    volatile ULONG AHB3ENR;     /* AHB3 peripheral clock enable register */
    volatile ULONG RESERVED2;
    volatile ULONG APB1ENR;     /* APB1 peripheral clock enable register */
    volatile ULONG APB2ENR;     /* APB2 peripheral clock enable register */
    volatile ULONG RESERVED3[2];
    volatile ULONG AHB1LPENR;   /* AHB1 peripheral clock enable in low power mode register */
    volatile ULONG AHB2LPENR;   /* AHB2 peripheral clock enable in low power mode register */
    volatile ULONG AHB3LPENR;   /* AHB3 peripheral clock enable in low power mode register */
    volatile ULONG RESERVED4;
    volatile ULONG APB1LPENR;   /* APB1 peripheral clock enable in low power mode register */
    volatile ULONG APB2LPENR;   /* APB2 peripheral clock enable in low power mode register */
    volatile ULONG RESERVED5[2];
    volatile ULONG BDCR;        /* Backup domain control register */
    volatile ULONG CSR;         /* Clock control & status register */
    volatile ULONG RESERVED6[2];
    volatile ULONG SSCGR;       /* Spread spectrum clock generation register */
    volatile ULONG PLLI2SCFGR;  /* PLLI2S configuration register */
} STM32F4_RCC_TypeDef;

/* RCC APB1 peripheral clock enable register bits */
#define STM32F4_RCC_APB1ENR_TIM2EN  (1UL << 0)  /* Timer 2 clock enable */
#define STM32F4_RCC_APB1ENR_TIM3EN  (1UL << 1)  /* Timer 3 clock enable */
#define STM32F4_RCC_APB1ENR_TIM4EN  (1UL << 2)  /* Timer 4 clock enable */
#define STM32F4_RCC_APB1ENR_TIM5EN  (1UL << 3)  /* Timer 5 clock enable */

/* NVIC (Nested Vector Interrupt Controller) */
#define STM32F4_NVIC_BASE       0xE000E100UL
#define STM32F4_NVIC            ((STM32F4_NVIC_TypeDef *)STM32F4_NVIC_BASE)

typedef struct {
    volatile ULONG ISER[8];     /* Interrupt Set Enable Register */
    volatile ULONG RESERVED0[24];
    volatile ULONG ICER[8];     /* Interrupt Clear Enable Register */
    volatile ULONG RESERVED1[24];
    volatile ULONG ISPR[8];     /* Interrupt Set Pending Register */
    volatile ULONG RESERVED2[24];
    volatile ULONG ICPR[8];     /* Interrupt Clear Pending Register */
    volatile ULONG RESERVED3[24];
    volatile ULONG IABR[8];     /* Interrupt Active bit Register */
    volatile ULONG RESERVED4[56];
    volatile UCHAR IP[240];     /* Interrupt Priority Register */
    volatile ULONG RESERVED5[644];
    volatile ULONG STIR;        /* Software Trigger Interrupt Register */
} STM32F4_NVIC_TypeDef;

/* STM32F4 Interrupt numbers */
#define STM32F4_IRQ_SYSTICK     -1  /* SysTick interrupt */
#define STM32F4_IRQ_TIM2        28  /* Timer 2 global interrupt */
#define STM32F4_IRQ_TIM3        29  /* Timer 3 global interrupt */
#define STM32F4_IRQ_TIM4        30  /* Timer 4 global interrupt */
#define STM32F4_IRQ_TIM5        50  /* Timer 5 global interrupt */

/* Timer Configuration Constants */
#define STM32F4_TIMER_PRESCALER_1MHZ    83  /* For 84MHz APB1, gives 1MHz timer clock */
#define STM32F4_TIMER_MAX_COUNT         0xFFFFFFFFUL  /* 32-bit timer maximum */

/* System Configuration */
#define STM32F4_DEFAULT_TICK_RATE       100     /* 100 Hz system tick */
#define STM32F4_SYSTICK_RELOAD_1KHZ     83999   /* For 1 kHz tick at 84 MHz */
#define STM32F4_SYSTICK_RELOAD_100HZ    839999  /* For 100 Hz tick at 84 MHz */

/* Function-like macros for register access */
#define STM32F4_ENABLE_TIM2_CLOCK()     (STM32F4_RCC->APB1ENR |= STM32F4_RCC_APB1ENR_TIM2EN)
#define STM32F4_DISABLE_TIM2_CLOCK()    (STM32F4_RCC->APB1ENR &= ~STM32F4_RCC_APB1ENR_TIM2EN)

#define STM32F4_NVIC_ENABLE_IRQ(irq)    (STM32F4_NVIC->ISER[(irq) >> 5] = (1UL << ((irq) & 0x1F)))
#define STM32F4_NVIC_DISABLE_IRQ(irq)   (STM32F4_NVIC->ICER[(irq) >> 5] = (1UL << ((irq) & 0x1F)))
#define STM32F4_NVIC_SET_PRIORITY(irq, priority) (STM32F4_NVIC->IP[irq] = (priority) << 4)

/* Memory barrier macros for Cortex-M4 */
#define STM32F4_DSB()   __asm volatile ("dsb" ::: "memory")
#define STM32F4_ISB()   __asm volatile ("isb" ::: "memory")
#define STM32F4_DMB()   __asm volatile ("dmb" ::: "memory")

/* Interrupt priority levels (0 = highest, 15 = lowest) */
#define STM32F4_IRQ_PRIORITY_SYSTICK    0   /* Highest priority for system tick */
#define STM32F4_IRQ_PRIORITY_TIMER      1   /* High priority for timer alarms */
#define STM32F4_IRQ_PRIORITY_DEFAULT    8   /* Default priority */

#endif /* _TIMER_HW_STM32F4_H_ */