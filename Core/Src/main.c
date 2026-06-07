/**
 * @file main.c
 * @brief Block Breaker Game — FreeRTOS multi-task architecture
 *
 * Task Architecture:
 *   InputTask   (50ms, high pri)  → Reads joystick → Overwrites queue (latest always)
 *   GameTask    (variable speed)  → Consumes input → Updates game logic → Signals display
 *   DisplayTask (on signal)       → Acquires LCD mutex → Renders game
 *
 * Gamer features driven from here:
 *   - Game over timeout (auto-reset after 4s)
 *   - Game over LED cleanup on reset
 */

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "lcd_display.h"
#include "joystick.h"
#include "block_breaker.h"

/* ============================================================================
   PERIPHERAL HANDLES
   ============================================================================ */

I2C_HandleTypeDef hi2c1;
ADC_HandleTypeDef hadc1;

/* ============================================================================
   FREERTOS HANDLES
   ============================================================================ */

QueueHandle_t input_queue = NULL;
SemaphoreHandle_t lcd_mutex = NULL;
SemaphoreHandle_t game_update_sem = NULL;

TaskHandle_t input_task_handle = NULL;
TaskHandle_t game_task_handle = NULL;
TaskHandle_t display_task_handle = NULL;

/* ============================================================================
   TASK PROTOTYPES
   ============================================================================ */

void InputTask(void *pvParameters);
void GameTask(void *pvParameters);
void DisplayTask(void *pvParameters);

/* ============================================================================
   PERIPHERAL INITIALIZATION PROTOTYPES
   ============================================================================ */

static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_ADC1_Init(void);

/* ============================================================================
   MAIN FUNCTION
   ============================================================================ */

int main(void)
{
    /* Initialize HAL */
    HAL_Init();

    /* Configure system clock to 168MHz */
    SystemClock_Config();

    /* Initialize peripherals */
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_ADC1_Init();

    /* Initialize application modules */
    LCD_Init();
    Joystick_Init();

    /* Splash screen */
    LCD_Clear();
    LCD_SetCursor(0, 0);
    LCD_Print(" BLOCK BREAKER ");
    LCD_SetCursor(1, 0);
    LCD_Print("  Loading...   ");
    HAL_Delay(1500);

    /* Initialize game (uploads custom chars, shows game splash) */
    Game_Init();
    HAL_Delay(1500);

    /* Create FreeRTOS sync objects */
    input_queue = xQueueCreate(INPUT_QUEUE_SIZE, sizeof(Direction_t));
    lcd_mutex = xSemaphoreCreateMutex();
    game_update_sem = xSemaphoreCreateBinary();

    /* Create FreeRTOS tasks */
    xTaskCreate(InputTask, "Input", INPUT_TASK_STACK_SIZE,
                NULL, INPUT_TASK_PRIORITY, &input_task_handle);

    xTaskCreate(GameTask, "Game", GAME_TASK_STACK_SIZE,
                NULL, GAME_TASK_PRIORITY, &game_task_handle);

    xTaskCreate(DisplayTask, "Display", DISPLAY_TASK_STACK_SIZE,
                NULL, DISPLAY_TASK_PRIORITY, &display_task_handle);

    /* Start FreeRTOS scheduler — never returns */
    vTaskStartScheduler();

    /* Should never reach here */
    while (1) { }
}

/* ============================================================================
   FREERTOS TASKS
   ============================================================================ */

/**
 * @brief Input task — reads joystick continuously, always stores latest input
 *
 * Uses xQueueOverwrite so the game task always gets the most recent direction.
 * This gives smooth paddle control (hold-to-move, not edge-triggered).
 */
void InputTask(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(INPUT_TASK_PERIOD_MS);

    while (1)
    {
        Direction_t dir = Joystick_GetDirection();

        // Always overwrite with latest input — smooth continuous control
        xQueueOverwrite(input_queue, &dir);

        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

/**
 * @brief Game task — consumes latest input, updates game logic, signals display
 *
 * Speed varies based on game progression (more bricks destroyed = faster).
 * When game over, shows results for 4 seconds then auto-resets.
 */
void GameTask(void *pvParameters)
{
    Direction_t input_dir = DIR_NONE;

    // Give initial render signal (don't wait for first input)
    xSemaphoreGive(game_update_sem);

    while (1)
    {
        // Consume latest joystick input (non-blocking)
        xQueueReceive(input_queue, &input_dir, 0);

        // Update game state
        Game_Update(input_dir);

        // Signal display to render
        xSemaphoreGive(game_update_sem);

        // Handle game over
        if (Game_IsOver())
        {
            // Let display show final screen for 4 seconds
            vTaskDelay(pdMS_TO_TICKS(4000));

            // Turn off game-over LEDs before reset
            HAL_GPIO_WritePin(LED_RED_GPIO_Port,  LED_RED_Pin,  GPIO_PIN_RESET);
            HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(LED_ORANGE_GPIO_Port, LED_ORANGE_Pin, GPIO_PIN_RESET);

            // Reset game
            Game_Reset();

            // Clear stale input
            xQueueReset(input_queue);
            input_dir = DIR_NONE;

            // Signal display to show the fresh start screen
            xSemaphoreGive(game_update_sem);
        }

        // Dynamic speed based on game progression
        vTaskDelay(pdMS_TO_TICKS(Game_GetSpeed()));
    }
}

/**
 * @brief Display task — renders game to LCD when signaled
 *
 * Acquires LCD mutex to prevent conflicts with splash boot-up writes.
 */
void DisplayTask(void *pvParameters)
{
    while (1)
    {
        // Wait for game update signal (200ms timeout prevents permanent block)
        if (xSemaphoreTake(game_update_sem, pdMS_TO_TICKS(200)) == pdTRUE)
        {
            // Acquire LCD mutex
            if (xSemaphoreTake(lcd_mutex, portMAX_DELAY) == pdTRUE)
            {
                Game_Render();
                xSemaphoreGive(lcd_mutex);
            }
        }
    }
}

/* ============================================================================
   SYSTEM CLOCK CONFIGURATION
   ============================================================================ */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* Configure the main internal regulator output voltage */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* Initializes the RCC Oscillators */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 168;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /* Initializes the CPU, AHB and APB buses clocks */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ============================================================================
   PERIPHERAL INITIALIZATION
   ============================================================================ */

/**
 * @brief GPIO Initialization — LEDs are now also controlled by game engine
 */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* Configure GPIO pins for status LEDs */
    GPIO_InitStruct.Pin = LED_GREEN_Pin | LED_ORANGE_Pin | LED_RED_Pin | LED_BLUE_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* Green LED on = system running */
    HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
    /* All others off */
    HAL_GPIO_WritePin(LED_ORANGE_GPIO_Port, LED_ORANGE_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET);
}

/**
 * @brief I2C1 Initialization (for LCD)
 */
static void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief ADC1 Initialization (for Joystick)
 */
static void MX_ADC1_Init(void)
{
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;

    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ============================================================================
   ERROR HANDLER
   ============================================================================ */

/**
 * @brief Error Handler — flashes red LED rapidly
 */
void Error_Handler(void)
{
    __disable_irq();
    /* Turn off all other LEDs */
    HAL_GPIO_WritePin(LED_GREEN_GPIO_Port,  LED_GREEN_Pin,  GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_ORANGE_GPIO_Port, LED_ORANGE_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_BLUE_GPIO_Port,   LED_BLUE_Pin,   GPIO_PIN_RESET);

    while (1)
    {
        HAL_GPIO_TogglePin(LED_RED_GPIO_Port, LED_RED_Pin);
        HAL_Delay(100);
    }
}

/* ============================================================================
   FREERTOS HOOK FUNCTIONS
   ============================================================================ */

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    Error_Handler();
}

void vApplicationMallocFailedHook(void)
{
    Error_Handler();
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
    Error_Handler();
}
#endif
