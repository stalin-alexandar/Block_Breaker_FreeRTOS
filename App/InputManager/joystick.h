/**
 * @file joystick.h
 * @brief PS2 Joystick driver for 2-axis analog input
 */

#ifndef JOYSTICK_H
#define JOYSTICK_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
   JOYSTICK CONFIGURATION
   ============================================================================ */

#define JOYSTICK_CENTER_VALUE       2048    // 12-bit ADC center (0-4095)
#define JOYSTICK_DEADZONE           200     // Deadzone around center
#define JOYSTICK_THRESHOLD_LOW      1800    // Below this = LEFT/DOWN
#define JOYSTICK_THRESHOLD_HIGH     2300    // Above this = RIGHT/UP

/* ============================================================================
   TYPE DEFINITIONS
   ============================================================================ */

typedef enum {
    DIR_NONE = 0,
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
} Direction_t;

typedef struct {
    uint16_t x_raw;         // ADC raw value (0-4095)
    uint16_t y_raw;         // ADC raw value (0-4095)
    Direction_t direction;  // Current direction
    bool button_pressed;    // Optional: joystick button state
} JoystickState_t;

/* ============================================================================
   EXTERNAL REFERENCES
   ============================================================================ */

extern ADC_HandleTypeDef hadc1;

/* ============================================================================
   PUBLIC FUNCTIONS
   ============================================================================ */

/**
 * @brief Initialize joystick ADC
 * @return true if successful
 */
bool Joystick_Init(void);

/**
 * @brief Read joystick state
 * @param state Pointer to joystick state structure
 */
void Joystick_Read(JoystickState_t *state);

/**
 * @brief Get current direction from joystick
 * @return Current direction (DIR_NONE if in deadzone)
 */
Direction_t Joystick_GetDirection(void);

/**
 * @brief Get raw X-axis value
 * @return ADC value (0-4095)
 */
uint16_t Joystick_GetX(void);

/**
 * @brief Get raw Y-axis value
 * @return ADC value (0-4095)
 */
uint16_t Joystick_GetY(void);

#endif /* JOYSTICK_H */
