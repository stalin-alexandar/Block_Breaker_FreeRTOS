/**
 * @file block_breaker.h
 * @brief Column Breaker — portrait/sideways breakout for 16×2 LCD
 *
 * Layout (rotated 90° from classic breakout):
 *   Paddle on LEFT (columns 0-1), moves UP/DOWN
 *   Bricks on RIGHT (columns 12-15), large full-cell blocks
 *   Ball travels HORIZONTALLY across the long dimension
 *
 * Pixel-smooth 2D motion: 80 × 16 pixel playfield with sub-cell animation
 * in both axes. The LCD's 16 columns become the main travel axis.
 */

#ifndef BLOCK_BREAKER_H
#define BLOCK_BREAKER_H

#include "main.h"
#include "joystick.h"
#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
   PLAYFIELD DIMENSIONS (pixels)
   ============================================================================ */

#define PLAYFIELD_WIDTH     80      // 16 cols × 5 pixels each
#define PLAYFIELD_HEIGHT    16      // 2 rows × 8 pixels each

/* ============================================================================
   BRICK CONFIGURATION — right side, 4 cols × 2 rows = 8 bricks
   ============================================================================ */

#define NUM_BRICKS          8
#define BRICK_COLS          4
#define BRICK_ROWS          2
#define BRICK_ZONE_COL      12      // Bricks occupy columns 12-15
#define BRICK_ZONE_PX       60      // 12 × 5 pixels

/* ============================================================================
   PADDLE CONFIGURATION — left side, vertical bar
   ============================================================================ */

#define PADDLE_WIDTH_PX     10      // 2 chars × 5 pixels
#define PADDLE_LENGTH       6       // Pixel height (out of 16)
#define PADDLE_PY_MAX       10      // Max top position = 16 - 6

/* ============================================================================
   BALL SPEED
   ============================================================================ */

#define INITIAL_SPEED_MS    100     // ms per game tick
#define MIN_SPEED_MS        40
#define SPEED_INCREASE_MS   5       // Faster per brick cleared
#define LEVEL_SPEED_BOOST   10      // Extra boost per level up

/* ============================================================================
   CUSTOM LCD CHARACTER SLOTS (8 available: 0-7)
   NOTE: Slot 0 is intentionally unused because character value 0 is the C
   string null terminator — LCD_Print() would stop printing at that position.
   ============================================================================ */

#define CHAR_BRICK_FULL     1       // Static — full 5×8 block
#define CHAR_BRICK_DMG      2       // Static — cracked 5×8 block
#define CHAR_PADDLE_TOP     3       // Dynamic — paddle segment in row 0
#define CHAR_PADDLE_BOT     4       // Dynamic — paddle segment in row 1
#define CHAR_BALL_DYN       5       // Dynamic — 2×2 ball at sub-pixel position

/* ============================================================================
   GAME STATE ENUM
   ============================================================================ */

typedef enum {
    GAME_STATE_READY = 0,       // Ball on paddle, waiting for launch
    GAME_STATE_PLAYING,         // Ball in play
    GAME_STATE_LIFE_LOST,       // Brief flash — auto-resets to READY
    GAME_STATE_GAME_OVER        // Final frame held — auto-resets
} GameStateEnum_t;

/* ============================================================================
   TYPE DEFINITIONS
   ============================================================================ */

typedef struct {
    int8_t px;          // Horizontal pixel position (0-79)
    int8_t py;          // Vertical pixel position (0-15)
    int8_t dx;          // Horizontal velocity (±2)
    int8_t dy;          // Vertical velocity (±1)
} Ball_t;

typedef struct {
    uint8_t py;         // Top pixel of paddle (0 to PADDLE_PY_MAX)
} Paddle_t;

typedef struct {
    uint8_t hp;         // 0=gone, 1=damaged, 2=fresh
} Brick_t;

typedef struct {
    GameStateEnum_t state;
    uint16_t score;
    uint8_t lives;
    uint8_t level;
    uint32_t speed_ms;
    uint32_t state_timer;
    uint32_t last_update;
} GameState_t;

/* ============================================================================
   PUBLIC FUNCTIONS
   ============================================================================ */

void Game_Init(void);
void Game_Update(Direction_t input);
void Game_Render(void);
bool Game_IsOver(void);
bool Game_IsVictory(void);
uint16_t Game_GetScore(void);
uint32_t Game_GetSpeed(void);
void Game_Reset(void);
GameState_t* Game_GetState(void);

#endif /* BLOCK_BREAKER_H */
