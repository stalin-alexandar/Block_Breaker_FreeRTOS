/**
 * @file lcd_display.c
 * @brief LCD display driver implementation
 */

#include "lcd_display.h"
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* ============================================================================
   PRIVATE VARIABLES
   ============================================================================ */

static bool initialized = false;
static uint8_t backlight_state = 1;  // Backlight on by default

/* ============================================================================
   PRIVATE FUNCTION PROTOTYPES
   ============================================================================ */

static void LCD_WriteCommand(uint8_t cmd);
static void LCD_WriteData(uint8_t data);
static void LCD_WriteNibble(uint8_t nibble, uint8_t rs);
static void LCD_WriteByte(uint8_t data, uint8_t rs);
static void LCD_EnablePulse(uint8_t data);
static void LCD_SendToExpander(uint8_t data);
static void LCD_Delay_us(uint32_t us);

/* ============================================================================
   PUBLIC FUNCTIONS
   ============================================================================ */

bool LCD_Init(void) {
    if (initialized) {
        return true;
    }

    // Wait for LCD power-up (>40ms after VCC rises to 4.5V)
    HAL_Delay(50);

    // Initialize in 4-bit mode (special sequence)
    // Send 0x03 three times (8-bit mode command in 4-bit interface)
    LCD_WriteNibble(0x03, 0);
    HAL_Delay(5);
    LCD_WriteNibble(0x03, 0);
    LCD_Delay_us(150);
    LCD_WriteNibble(0x03, 0);
    LCD_Delay_us(150);

    // Switch to 4-bit mode
    LCD_WriteNibble(0x02, 0);
    LCD_Delay_us(150);

    // Function set: 4-bit mode, 2 lines, 5x8 font
    LCD_WriteCommand(0x28);  // 0x28 = 4-bit, 2-line, 5x8

    // Display off
    LCD_WriteCommand(LCD_CMD_DISPLAY_OFF);

    // Clear display
    LCD_WriteCommand(LCD_CMD_CLEAR);
    HAL_Delay(2);  // Clear needs 1.52ms

    // Entry mode: increment cursor, no shift
    LCD_WriteCommand(LCD_CMD_ENTRY_MODE);

    // Display on, cursor off, blink off
    LCD_WriteCommand(LCD_CMD_DISPLAY_ON);

    // Turn on backlight
    backlight_state = 1;
    LCD_SendToExpander(0x00);  // Send dummy byte to set backlight

    initialized = true;
    return true;
}

void LCD_Clear(void) {
    if (!initialized) return;

    LCD_WriteCommand(LCD_CMD_CLEAR);
    HAL_Delay(2);
}

void LCD_SetCursor(uint8_t row, uint8_t col) {
    if (!initialized) return;

    uint8_t row_offsets[] = {0x00, 0x40, 0x14, 0x54};

    if (row >= LCD_ROWS) row = 0;
    if (col >= LCD_COLS) col = 0;

    LCD_WriteCommand(LCD_CMD_SET_DDRAM | (col + row_offsets[row]));
}

void LCD_Print(const char *str) {
    if (!initialized || str == NULL) return;

    while (*str) {
        LCD_WriteData(*str++);
    }
}

void LCD_Printf(const char *format, ...) {
    if (!initialized || format == NULL) return;

    char buffer[LCD_COLS + 1];
    va_list args;

    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    LCD_Print(buffer);
}

void LCD_PrintChar(char c) {
    if (!initialized) return;

    LCD_WriteData(c);
}

void LCD_DisplayMessage(uint8_t row, const char *message) {
    if (!initialized || message == NULL) return;

    LCD_SetCursor(row, 0);

    // Pad with spaces to clear line
    char buffer[LCD_COLS + 1];
    snprintf(buffer, sizeof(buffer), "%-*s", LCD_COLS, message);
    LCD_Print(buffer);
}

void LCD_SetDisplay(bool on) {
    if (!initialized) return;

    if (on) {
        LCD_WriteCommand(LCD_CMD_DISPLAY_ON);
    } else {
        LCD_WriteCommand(LCD_CMD_DISPLAY_OFF);
    }
}

void LCD_SetBacklight(bool on) {
    backlight_state = on ? 1 : 0;
    LCD_SendToExpander(0x00);  // Send dummy byte to update backlight
}

/* ============================================================================
   PRIVATE FUNCTIONS
   ============================================================================ */

/**
 * @brief Send byte to PCF8574 I2C expander
 * @param data Byte to send (8 bits control LCD pins)
 */
static void LCD_SendToExpander(uint8_t data) {
    // Add backlight bit to data
    if (backlight_state) {
        data |= (1 << PCF8574_BL_BIT);
    }

    // Send via I2C
    HAL_I2C_Master_Transmit(&hi2c1, PCF8574_I2C_ADDRESS << 1,
                            &data, 1, PCF8574_I2C_TIMEOUT);
}

/**
 * @brief Generate enable pulse for LCD
 * @param data Data byte with EN bit to be pulsed
 */
static void LCD_EnablePulse(uint8_t data) {
    // EN high
    LCD_SendToExpander(data | (1 << PCF8574_EN_BIT));
    LCD_Delay_us(1);  // >450ns pulse width

    // EN low
    LCD_SendToExpander(data & ~(1 << PCF8574_EN_BIT));
    LCD_Delay_us(50);  // Command execution time
}

/**
 * @brief Write 4-bit nibble to LCD
 * @param nibble 4-bit data (lower nibble used)
 * @param rs Register select (0=command, 1=data)
 */
static void LCD_WriteNibble(uint8_t nibble, uint8_t rs) {
    uint8_t data = 0;

    // Set RS bit
    if (rs) {
        data |= (1 << PCF8574_RS_BIT);
    }

    // RW = 0 (write mode)
    data &= ~(1 << PCF8574_RW_BIT);

    // Map nibble to D4-D7 bits
    if (nibble & 0x01) data |= (1 << PCF8574_D4_BIT);
    if (nibble & 0x02) data |= (1 << PCF8574_D5_BIT);
    if (nibble & 0x04) data |= (1 << PCF8574_D6_BIT);
    if (nibble & 0x08) data |= (1 << PCF8574_D7_BIT);

    // Send with enable pulse
    LCD_EnablePulse(data);
}

/**
 * @brief Write 8-bit byte to LCD (as two 4-bit nibbles)
 * @param data 8-bit data
 * @param rs Register select (0=command, 1=data)
 */
static void LCD_WriteByte(uint8_t data, uint8_t rs) {
    // Send upper nibble (bits 7-4)
    LCD_WriteNibble((data >> 4) & 0x0F, rs);

    // Send lower nibble (bits 3-0)
    LCD_WriteNibble(data & 0x0F, rs);
}

static void LCD_WriteCommand(uint8_t cmd) {
    LCD_WriteByte(cmd, 0);  // RS = 0 for command
}

static void LCD_WriteData(uint8_t data) {
    LCD_WriteByte(data, 1);  // RS = 1 for data
}

static void LCD_Delay_us(uint32_t us) {
    // Simple delay using DWT cycle counter or busy loop
    // For STM32F407 @ 168MHz: 168 cycles = 1us
    uint32_t cycles = us * 168;

    // Use DWT if available, otherwise busy loop
    volatile uint32_t count = cycles / 4;  // Approximate
    while (count--) {
        __NOP();
    }
}

void LCD_CreateChar(uint8_t location, uint8_t charmap[8]) {
    if (!initialized || location > 7) return;

    // Set CGRAM address (0x40 + location * 8)
    LCD_WriteCommand(0x40 | (location << 3));

    // Write 8 bytes of character data
    for (uint8_t i = 0; i < 8; i++) {
        LCD_WriteData(charmap[i]);
    }

    // Return to DDRAM mode
    LCD_WriteCommand(LCD_CMD_SET_DDRAM);
    LCD_Delay_us(50);
}
