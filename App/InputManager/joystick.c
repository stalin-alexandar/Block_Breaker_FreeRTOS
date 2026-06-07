/**
 * @file joystick.c
 * @brief PS2 Joystick driver implementation
 */

#include "joystick.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdlib.h>

/* ============================================================================
   PRIVATE VARIABLES
   ============================================================================ */

static bool initialized = false;
static JoystickState_t current_state = {0};
static uint32_t last_direction_change = 0;

/* ============================================================================
   PRIVATE FUNCTION PROTOTYPES
   ============================================================================ */

static uint16_t ADC_ReadChannel(uint32_t channel);
static Direction_t CalculateDirection(uint16_t x, uint16_t y);

/* ============================================================================
   PUBLIC FUNCTIONS
   ============================================================================ */

bool Joystick_Init(void) {
    if (initialized) {
        return true;
    }

    // ADC should be initialized by main.c (MX_ADC1_Init)
    // Just verify it's ready
    if (hadc1.Instance == NULL) {
        return false;
    }

    // Initialize state
    current_state.x_raw = JOYSTICK_CENTER_VALUE;
    current_state.y_raw = JOYSTICK_CENTER_VALUE;
    current_state.direction = DIR_NONE;
    current_state.button_pressed = false;

    initialized = true;
    return true;
}

void Joystick_Read(JoystickState_t *state) {
    if (!initialized || state == NULL) {
        return;
    }

    // Read X-axis (PA0 - ADC1_IN0)
    current_state.x_raw = ADC_ReadChannel(ADC_CHANNEL_0);

    // Read Y-axis (PA1 - ADC1_IN1)
    current_state.y_raw = ADC_ReadChannel(ADC_CHANNEL_1);

    // Calculate direction
    current_state.direction = CalculateDirection(current_state.x_raw, current_state.y_raw);

    // Copy to output
    *state = current_state;
}

Direction_t Joystick_GetDirection(void) {
    if (!initialized) {
        return DIR_NONE;
    }

    // Read current values
    uint16_t x = ADC_ReadChannel(ADC_CHANNEL_0);
    uint16_t y = ADC_ReadChannel(ADC_CHANNEL_1);

    // Calculate and return direction
    Direction_t dir = CalculateDirection(x, y);

    // Debouncing: ignore rapid changes within 100ms
    uint32_t current_time = HAL_GetTick();
    if (dir != DIR_NONE && dir != current_state.direction) {
        if (current_time - last_direction_change < 100) {
            return current_state.direction; // Keep previous direction
        }
        last_direction_change = current_time;
    }

    current_state.direction = dir;
    return dir;
}

uint16_t Joystick_GetX(void) {
    if (!initialized) {
        return JOYSTICK_CENTER_VALUE;
    }
    return ADC_ReadChannel(ADC_CHANNEL_0);
}

uint16_t Joystick_GetY(void) {
    if (!initialized) {
        return JOYSTICK_CENTER_VALUE;
    }
    return ADC_ReadChannel(ADC_CHANNEL_1);
}

/* ============================================================================
   PRIVATE FUNCTIONS
   ============================================================================ */

/**
 * @brief Read ADC channel value
 * @param channel ADC channel to read
 * @return ADC value (0-4095)
 */
static uint16_t ADC_ReadChannel(uint32_t channel) {
    ADC_ChannelConfTypeDef sConfig = {0};

    // Configure channel
    sConfig.Channel = channel;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;

    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        return JOYSTICK_CENTER_VALUE;
    }

    // Start conversion
    HAL_ADC_Start(&hadc1);

    // Wait for conversion to complete
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
        return (uint16_t)HAL_ADC_GetValue(&hadc1);
    }

    return JOYSTICK_CENTER_VALUE;
}

/**
 * @brief Calculate direction from X/Y values
 * @param x X-axis ADC value
 * @param y Y-axis ADC value
 * @return Direction enum
 */
static Direction_t CalculateDirection(uint16_t x, uint16_t y) {
    // Check if in deadzone (center position)
    int16_t x_offset = (int16_t)x - JOYSTICK_CENTER_VALUE;
    int16_t y_offset = (int16_t)y - JOYSTICK_CENTER_VALUE;

    if (abs(x_offset) < JOYSTICK_DEADZONE && abs(y_offset) < JOYSTICK_DEADZONE) {
        return DIR_NONE;
    }

    // Determine primary direction (prioritize larger offset)
    if (abs(x_offset) > abs(y_offset)) {
        // Horizontal movement dominant
        if (x < JOYSTICK_THRESHOLD_LOW) {
            return DIR_LEFT;   // Corrected back to DIR_LEFT
        } else if (x > JOYSTICK_THRESHOLD_HIGH) {
            return DIR_RIGHT;  // Corrected back to DIR_RIGHT
        }
    } else {
        // Vertical movement dominant
        if (y < JOYSTICK_THRESHOLD_LOW) {
            return DIR_DOWN;   // Corrected back to DIR_DOWN
        } else if (y > JOYSTICK_THRESHOLD_HIGH) {
            return DIR_UP;     // Corrected back to DIR_UP
        }
    }

    return DIR_NONE;
}
