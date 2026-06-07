/**
 * @file main.h
 * @brief Main application header for Block Breaker Game
 */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdbool.h>

/* Error handler */
void Error_Handler(void);

/* System clock configuration */
void SystemClock_Config(void);

/* ============================================================================
   GPIO PIN DEFINITIONS
   ============================================================================ */

/* I2C1 for LCD Display (PCF8574 Adapter) */
#define I2C1_SCL_Pin            GPIO_PIN_8      // PB8 - I2C1_SCL
#define I2C1_SCL_GPIO_Port      GPIOB
#define I2C1_SDA_Pin            GPIO_PIN_9      // PB9 - I2C1_SDA
#define I2C1_SDA_GPIO_Port      GPIOB

/* ADC1 for Joystick */
#define JOYSTICK_X_Pin          GPIO_PIN_0      // PA0 - ADC1_IN0
#define JOYSTICK_X_GPIO_Port    GPIOA
#define JOYSTICK_Y_Pin          GPIO_PIN_1      // PA1 - ADC1_IN1
#define JOYSTICK_Y_GPIO_Port    GPIOA

/* Optional: Joystick Button */
#define JOYSTICK_BTN_Pin        GPIO_PIN_4      // PA4
#define JOYSTICK_BTN_GPIO_Port  GPIOA

/* Status LEDs (Onboard STM32F407G Discovery) */
#define LED_GREEN_Pin           GPIO_PIN_12     // PD12
#define LED_GREEN_GPIO_Port     GPIOD
#define LED_ORANGE_Pin          GPIO_PIN_13     // PD13
#define LED_ORANGE_GPIO_Port    GPIOD
#define LED_RED_Pin             GPIO_PIN_14     // PD14
#define LED_RED_GPIO_Port       GPIOD
#define LED_BLUE_Pin            GPIO_PIN_15     // PD15
#define LED_BLUE_GPIO_Port      GPIOD

/* ============================================================================
   PERIPHERAL HANDLES
   ============================================================================ */

extern I2C_HandleTypeDef hi2c1;     // LCD I2C communication
extern ADC_HandleTypeDef hadc1;     // Joystick ADC

/* ============================================================================
   SYSTEM CONSTANTS
   ============================================================================ */

/* I2C Configuration */
#define I2C_CLOCK_SPEED         100000  // 100kHz standard mode

/* ADC Configuration */
#define ADC_RESOLUTION          ADC_RESOLUTION_12B
#define ADC_SAMPLING_TIME       ADC_SAMPLETIME_84CYCLES

/* ============================================================================
   FREERTOS TASK CONFIGURATION
   ============================================================================ */

/* Task priorities */
#define INPUT_TASK_PRIORITY     3       // High priority
#define GAME_TASK_PRIORITY      2       // Normal priority
#define DISPLAY_TASK_PRIORITY   1       // Low priority

/* Task stack sizes (in words) */
#define INPUT_TASK_STACK_SIZE   256
#define GAME_TASK_STACK_SIZE    512
#define DISPLAY_TASK_STACK_SIZE 512

/* Task periods (in milliseconds) */
#define INPUT_TASK_PERIOD_MS    50      // 20 Hz
#define DISPLAY_TASK_PERIOD_MS  100     // 10 Hz

/* Queue sizes */
#define INPUT_QUEUE_SIZE        1  // Only latest input matters (xQueueOverwrite)

/* ============================================================================
   FUNCTION PROTOTYPES
   ============================================================================ */

/* Initialization */
void SystemClock_Config(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
