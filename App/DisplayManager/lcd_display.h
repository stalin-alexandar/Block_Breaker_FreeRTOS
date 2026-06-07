/**
 * @file lcd_display.h
 * @brief LCD display driver for 16x2 or 20x4 character display
 */

#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
   LCD CONFIGURATION
   ============================================================================ */

#define LCD_ROWS                2       // 2 or 4
#define LCD_COLS                16      // 16 or 20

/* PCF8574 I2C LCD Adapter Configuration */
#define PCF8574_I2C_ADDRESS     0x27    // Default address (0x27 or 0x3F)
#define PCF8574_I2C_TIMEOUT     100     // I2C timeout in ms

/* PCF8574 Pin Mapping (typical adapter layout) */
#define PCF8574_RS_BIT          0       // Register Select
#define PCF8574_RW_BIT          1       // Read/Write
#define PCF8574_EN_BIT          2       // Enable
#define PCF8574_BL_BIT          3       // Backlight
#define PCF8574_D4_BIT          4       // Data bit 4
#define PCF8574_D5_BIT          5       // Data bit 5
#define PCF8574_D6_BIT          6       // Data bit 6
#define PCF8574_D7_BIT          7       // Data bit 7

/* LCD Commands */
#define LCD_CMD_CLEAR           0x01
#define LCD_CMD_HOME            0x02
#define LCD_CMD_ENTRY_MODE      0x06
#define LCD_CMD_DISPLAY_ON      0x0C
#define LCD_CMD_DISPLAY_OFF     0x08
#define LCD_CMD_CURSOR_ON       0x0E
#define LCD_CMD_CURSOR_BLINK    0x0F
#define LCD_CMD_FUNCTION_SET    0x38    // 8-bit, 2 lines, 5x8 font
#define LCD_CMD_SET_DDRAM       0x80

/* I2C Handle (external reference) */
extern I2C_HandleTypeDef hi2c1;

/* ============================================================================
   PUBLIC FUNCTIONS
   ============================================================================ */

/**
 * @brief Initialize LCD display
 * @return true if successful
 */
bool LCD_Init(void);

/**
 * @brief Clear LCD display
 */
void LCD_Clear(void);

/**
 * @brief Set cursor position
 * @param row Row (0 to LCD_ROWS-1)
 * @param col Column (0 to LCD_COLS-1)
 */
void LCD_SetCursor(uint8_t row, uint8_t col);

/**
 * @brief Print string to LCD
 * @param str String to print
 */
void LCD_Print(const char *str);

/**
 * @brief Print formatted string to LCD
 * @param format Format string (printf style)
 * @param ... Variable arguments
 */
void LCD_Printf(const char *format, ...);

/**
 * @brief Print character to LCD
 * @param c Character to print
 */
void LCD_PrintChar(char c);

/**
 * @brief Display custom message
 * @param row Row number
 * @param message Message to display
 */
void LCD_DisplayMessage(uint8_t row, const char *message);

/**
 * @brief Turn display on/off
 * @param on true to turn on, false to turn off
 */
void LCD_SetDisplay(bool on);

/**
 * @brief Turn backlight on/off
 * @param on true to turn on, false to turn off
 */
void LCD_SetBacklight(bool on);

/**
 * @brief Create custom character in CGRAM
 * @param location Character code (0-7)
 * @param charmap 8-byte array defining 5x8 pixel pattern
 */
void LCD_CreateChar(uint8_t location, uint8_t charmap[8]);

#endif /* LCD_DISPLAY_H */
