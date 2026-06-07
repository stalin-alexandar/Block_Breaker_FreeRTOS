/**
 * @file stm32f4xx_it.c
 * @brief Interrupt handlers implementation
 */

#include "main.h"
#include "stm32f4xx_it.h"
#include "FreeRTOS.h"
#include "task.h"

/* ============================================================================
   EXTERNAL VARIABLES
   ============================================================================ */

extern TIM_HandleTypeDef htim6;

/* ============================================================================
   CORTEX-M4 PROCESSOR INTERRUPTS
   ============================================================================ */

/**
 * @brief Non Maskable Interrupt
 */
void NMI_Handler(void)
{
    while (1)
    {
    }
}

/**
 * @brief Hard Fault Interrupt
 */
void HardFault_Handler(void)
{
    while (1)
    {
    }
}

/**
 * @brief Memory Management Fault
 */
void MemManage_Handler(void)
{
    while (1)
    {
    }
}

/**
 * @brief Pre-fetch fault, memory access fault
 */
void BusFault_Handler(void)
{
    while (1)
    {
    }
}

/**
 * @brief Undefined instruction or illegal state
 */
void UsageFault_Handler(void)
{
    while (1)
    {
    }
}

/**
 * @brief Debug Monitor
 */
void DebugMon_Handler(void)
{
}

/* ============================================================================
   STM32F4xx PERIPHERAL INTERRUPTS
   ============================================================================ */

/**
 * @brief TIM6 global interrupt (used for HAL timebase)
 */
void TIM6_DAC_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim6);
}

/**
 * @brief Period elapsed callback in non blocking mode
 * @param htim TIM handle
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)
    {
        HAL_IncTick();
    }
}
