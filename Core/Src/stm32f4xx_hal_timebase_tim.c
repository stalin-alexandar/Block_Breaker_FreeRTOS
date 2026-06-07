/**
 * @file stm32f4xx_hal_timebase_tim.c
 * @brief HAL timebase override using TIM6 instead of SysTick.
 *
 * FreeRTOS takes ownership of SysTick (SysTick_Handler → xPortSysTickHandler),
 * so SysTick can no longer drive HAL_GetTick(). This file redirects the HAL
 * timebase to TIM6, ensuring HAL_Delay() and all HAL timeout mechanisms work
 * both before and after the FreeRTOS scheduler starts.
 */

#include "stm32f4xx_hal.h"

/* TIM6 handle used exclusively for HAL timebase */
TIM_HandleTypeDef htim6;

/**
 * @brief Override HAL_InitTick to configure TIM6 as the 1 ms timebase.
 *        Called by HAL_Init() instead of the default SysTick setup.
 * @param TickPriority Tick interrupt priority (passed from HAL_Init)
 * @retval HAL status
 */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    RCC_ClkInitTypeDef clkconfig;
    uint32_t uwTimclock;
    uint32_t uwPrescalerValue;
    uint32_t pFLatency;
    HAL_StatusTypeDef status;

    /* Enable TIM6 clock */
    __HAL_RCC_TIM6_CLK_ENABLE();

    /* Get clock configuration */
    HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);

    /* Compute TIM6 clock: APB1 timer clock */
    /* If APB1 prescaler != 1, timer clock = 2 × APB1 clock */
    if (clkconfig.APB1CLKDivider == RCC_HCLK_DIV1)
    {
        uwTimclock = HAL_RCC_GetPCLK1Freq();
    }
    else
    {
        uwTimclock = 2UL * HAL_RCC_GetPCLK1Freq();
    }

    /* Prescale to 10 kHz */
    uwPrescalerValue = (uint32_t)((uwTimclock / 10000U) - 1U);

    /* Configure TIM6 for 1 ms period (10 kHz / 10 = 1 kHz) */
    htim6.Instance = TIM6;
    htim6.Init.Prescaler = uwPrescalerValue;
    htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim6.Init.Period = (10U - 1U);  /* 10 kHz / 10 = 1 kHz = 1 ms */
    htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    status = HAL_TIM_Base_Init(&htim6);
    if (status == HAL_OK)
    {
        /* Start TIM6 with interrupt */
        status = HAL_TIM_Base_Start_IT(&htim6);
        if (status == HAL_OK)
        {
            /* Configure interrupt priority */
            HAL_NVIC_SetPriority(TIM6_DAC_IRQn, TickPriority, 0U);
            HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
        }
    }

    return status;
}

/**
 * @brief Suspend the HAL tick (disable TIM6 interrupt)
 */
void HAL_SuspendTick(void)
{
    __HAL_TIM_DISABLE_IT(&htim6, TIM_IT_UPDATE);
}

/**
 * @brief Resume the HAL tick (enable TIM6 interrupt)
 */
void HAL_ResumeTick(void)
{
    __HAL_TIM_ENABLE_IT(&htim6, TIM_IT_UPDATE);
}
