/**
 * @file stm32f4xx_hal_msp.c
 * @brief HAL MSP (Microcontroller Support Package) initialization
 */

#include "main.h"

/* ============================================================================
   HAL MSP INITIALIZATION
   ============================================================================ */

/**
 * @brief Initialize the Global MSP
 */
void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();

    /* System interrupt init */
    /* PendSV_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(PendSV_IRQn, 15, 0);
    /* SysTick_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(SysTick_IRQn, 15, 0);
}

/**
 * @brief I2C MSP Initialization
 * @param hi2c I2C handle pointer
 */
void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if(hi2c->Instance == I2C1)
    {
        __HAL_RCC_GPIOB_CLK_ENABLE();

        /**I2C1 GPIO Configuration
        PB8     ------> I2C1_SCL
        PB9     ------> I2C1_SDA
        */
        GPIO_InitStruct.Pin = I2C1_SCL_Pin | I2C1_SDA_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        /* Peripheral clock enable */
        __HAL_RCC_I2C1_CLK_ENABLE();
    }
}

/**
 * @brief I2C MSP De-Initialization
 * @param hi2c I2C handle pointer
 */
void HAL_I2C_MspDeInit(I2C_HandleTypeDef* hi2c)
{
    if(hi2c->Instance == I2C1)
    {
        /* Peripheral clock disable */
        __HAL_RCC_I2C1_CLK_DISABLE();

        /**I2C1 GPIO Configuration
        PB8     ------> I2C1_SCL
        PB9     ------> I2C1_SDA
        */
        HAL_GPIO_DeInit(GPIOB, I2C1_SCL_Pin | I2C1_SDA_Pin);
    }
}

/**
 * @brief ADC MSP Initialization
 * @param hadc ADC handle pointer
 */
void HAL_ADC_MspInit(ADC_HandleTypeDef* hadc)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if(hadc->Instance == ADC1)
    {
        /* Peripheral clock enable */
        __HAL_RCC_ADC1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /**ADC1 GPIO Configuration
        PA0     ------> ADC1_IN0 (Joystick X)
        PA1     ------> ADC1_IN1 (Joystick Y)
        */
        GPIO_InitStruct.Pin = JOYSTICK_X_Pin | JOYSTICK_Y_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

/**
 * @brief ADC MSP De-Initialization
 * @param hadc ADC handle pointer
 */
void HAL_ADC_MspDeInit(ADC_HandleTypeDef* hadc)
{
    if(hadc->Instance == ADC1)
    {
        /* Peripheral clock disable */
        __HAL_RCC_ADC1_CLK_DISABLE();

        /**ADC1 GPIO Configuration
        PA0     ------> ADC1_IN0
        PA1     ------> ADC1_IN1
        */
        HAL_GPIO_DeInit(GPIOA, JOYSTICK_X_Pin | JOYSTICK_Y_Pin);
    }
}

/**
 * @brief TIM_Base MSP Initialization
 * @param htim_base TIM_Base handle pointer
 */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* htim_base)
{
    if(htim_base->Instance == TIM6)
    {
        /* Peripheral clock enable */
        __HAL_RCC_TIM6_CLK_ENABLE();

        /* TIM6 interrupt Init */
        HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 15, 0);
        HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
    }
}

/**
 * @brief TIM_Base MSP De-Initialization
 * @param htim_base TIM_Base handle pointer
 */
void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef* htim_base)
{
    if(htim_base->Instance == TIM6)
    {
        /* Peripheral clock disable */
        __HAL_RCC_TIM6_CLK_DISABLE();

        /* TIM6 interrupt DeInit */
        HAL_NVIC_DisableIRQ(TIM6_DAC_IRQn);
    }
}
