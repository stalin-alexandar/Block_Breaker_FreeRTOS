/**
 * @file block_breaker.c
 * @brief Column Breaker — portrait/sideways breakout for 16×2 LCD
 *
 * COLUMN-BREAKER layout:
 *   Paddle on LEFT (col 0-1), moves UP/DOWN via joystick
 *   Bricks on RIGHT (col 12-15), large full-cell 5×8 blocks
 *   Ball travels HORIZONTALLY across 16 columns (~80 pixels with sub_x)
 *   Both LCD rows form a 16-pixel-tall vertical corridor
 *
 * Controls: UP/DOWN = paddle, any direction = launch
 * LED: orange on brick hit, red on life lost, blue on level clear
 *
 * Pixel-smooth 2D motion: ball has px (0-79) and py (0-16).
 * Ball char is dynamically generated each frame at sub-pixel position.
 * Paddle char regenerated when vertical position changes.
 */

#include "block_breaker.h"
#include "lcd_display.h"

#include <stdlib.h>
#include <string.h>

/* ============================================================================
   STATIC LCD CHARACTERS (slots 1-2, uploaded once)
   Note: Slot 0 is unused (null terminator issue)
   ============================================================================ */

/* [1] CHAR_BRICK_FULL — full 5×8 solid block */
static const uint8_t char_brick_full[8] = {
    0b11111,
    0b11111,
    0b11111,
    0b11111,
    0b11111,
    0b11111,
    0b11111,
    0b11111
};

/* [2] CHAR_BRICK_DMG — cracked, broken-line pattern */
static const uint8_t char_brick_dmg[8] = {
    0b11111,
    0b10001,
    0b11111,
    0b01110,
    0b11111,
    0b10001,
    0b11111,
    0b00000
};

/* ============================================================================
   PRIVATE VARIABLES
   ============================================================================ */

static Ball_t ball;
static Paddle_t paddle;
static Brick_t bricks[NUM_BRICKS];
static GameState_t gs;
static bool initialized = false;

/* Cached values to avoid redundant CGRAM writes (LCD over I2C is slow) */
static int8_t last_paddle_py = -1;
static int8_t last_ball_px = -1;
static int8_t last_ball_py = -1;
static uint8_t flash_cycle = 0;

/* ============================================================================
   PRIVATE FUNCTION PROTOTYPES
   ============================================================================ */

static void InitGame(void);
static void LaunchBall(void);
static void CheckPaddleCollision(void);
static void CheckBrickCollision(void);
static void CheckWallBounce(void);
static bool AreAllBricksDestroyed(void);
static void InitLevel(void);
static void ResetBall(void);
static void UpdatePaddleCGRAM(void);
static void UpdateBallCGRAM(void);

/* ============================================================================
   PUBLIC FUNCTIONS
   ============================================================================ */

void Game_Init(void)
{
    // Upload static custom characters (slots 1-2)
    LCD_CreateChar(CHAR_BRICK_FULL, (uint8_t*)char_brick_full);
    LCD_CreateChar(CHAR_BRICK_DMG,  (uint8_t*)char_brick_dmg);

    // Pre-initialize paddle and ball CGRAM slots
    LCD_CreateChar(CHAR_PADDLE_TOP, (uint8_t*)char_brick_full); // temp fill
    LCD_CreateChar(CHAR_PADDLE_BOT, (uint8_t*)char_brick_full); // temp fill
    LCD_CreateChar(CHAR_BALL_DYN,   (uint8_t*)char_brick_full); // temp fill

    // Seed RNG
    srand(HAL_GetTick());

    // Quick clear
    LCD_Clear();

    InitGame();
    initialized = true;
}

void Game_Update(Direction_t input)
{
    if (!initialized) return;

    uint32_t now = HAL_GetTick();

    switch (gs.state)
    {
    case GAME_STATE_READY:
        /* ── Ball on paddle (left side), waiting for launch ── */
        ResetBall();

        // Move paddle vertically with joystick UP/DOWN
        if (input == DIR_UP && paddle.py > 0)
            paddle.py--;
        else if (input == DIR_DOWN && paddle.py < PADDLE_PY_MAX)
            paddle.py++;

        // Launch on any joystick movement
        if (input != DIR_NONE)
            LaunchBall();
        break;

    case GAME_STATE_PLAYING:
        /* ── COLUMN BREAKER: ball travels horizontally up to 80 pixels ── */

        // 1. Move paddle with joystick
        if (input == DIR_UP && paddle.py > 0)
            paddle.py--;
        else if (input == DIR_DOWN && paddle.py < PADDLE_PY_MAX)
            paddle.py++;

        // 2. Move ball in both axes
        ball.px += ball.dx;
        ball.py += ball.dy;

        // 3. Check vertical walls (top/bottom bounce)
        CheckWallBounce();

        // 4. Check paddle zone (left side)
        if (ball.px < 0)
        {
            ball.px = 0;
            CheckPaddleCollision();
        }
        else if (ball.px < PADDLE_WIDTH_PX && ball.dx < 0)
        {
            CheckPaddleCollision();
        }

        // 5. Check brick zone (right side)
        if (ball.px >= PLAYFIELD_WIDTH - 1)
        {
            ball.px = PLAYFIELD_WIDTH - 1;
            // Right wall — bounce back
            ball.dx = -ball.dx;
        }
        else if (ball.px >= BRICK_ZONE_PX && ball.dx > 0)
        {
            CheckBrickCollision();
        }
        break;

    case GAME_STATE_LIFE_LOST:
        /* ── Instant reset — 200ms red LED flash ── */
        flash_cycle++;
        if (flash_cycle & 1)
            HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
        else
            HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);

        if (now - gs.state_timer > 200)
        {
            HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);
            if (gs.lives > 0)
            {
                ResetBall();
                gs.state = GAME_STATE_READY;
            }
            else
            {
                gs.state = GAME_STATE_GAME_OVER;
                gs.state_timer = now;
                HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
                HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
            }
        }
        break;

    case GAME_STATE_GAME_OVER:
        /* ── Final board displayed — main.c handles 4s delay + reset ── */
        break;
    }

    gs.last_update = now;
}

void Game_Render(void)
{
    if (!initialized) return;

    char line0[17];
    char line1[17];
    memset(line0, ' ', 16);
    memset(line1, ' ', 16);
    line0[16] = '\0';
    line1[16] = '\0';

    switch (gs.state)
    {
    case GAME_STATE_READY:
    case GAME_STATE_PLAYING:
    case GAME_STATE_LIFE_LOST:
    case GAME_STATE_GAME_OVER:
    {
        // ── Update paddle CGRAM if position changed ──
        if (paddle.py != last_paddle_py)
        {
            UpdatePaddleCGRAM();
            last_paddle_py = paddle.py;
        }

        // ── Update ball CGRAM if position changed ──
        if (ball.px != last_ball_px || ball.py != last_ball_py)
        {
            UpdateBallCGRAM();
            last_ball_px = ball.px;
            last_ball_py = ball.py;
        }

        // ── Draw bricks (right side, columns 12-15) ──
        for (uint8_t col = 0; col < BRICK_COLS; col++)
        {
            for (uint8_t row = 0; row < BRICK_ROWS; row++)
            {
                uint8_t idx = row * BRICK_COLS + col;
                if (bricks[idx].hp > 0)
                {
                    char ch = (bricks[idx].hp >= 2) ? CHAR_BRICK_FULL : CHAR_BRICK_DMG;
                    uint8_t x = BRICK_ZONE_COL + col;
                    if (row == 0)
                        line0[x] = ch;
                    else
                        line1[x] = ch;
                }
            }
        }

        // ── Draw paddle (left side, columns 0-1) ──
        line0[0] = CHAR_PADDLE_TOP;
        line0[1] = CHAR_PADDLE_TOP;
        line1[0] = CHAR_PADDLE_BOT;
        line1[1] = CHAR_PADDLE_BOT;

        // ── Draw ball ──
        if (gs.state == GAME_STATE_READY || gs.state == GAME_STATE_PLAYING)
        {
            int cell_x = ball.px / 5;
            int cell_y = ball.py / 8;

            // Clamp to valid range
            if (cell_x < 0) cell_x = 0;
            if (cell_x >= 16) cell_x = 15;
            if (cell_y < 0) cell_y = 0;
            if (cell_y > 1) cell_y = 1;

            if (cell_y == 0)
                line0[cell_x] = CHAR_BALL_DYN;
            else
                line1[cell_x] = CHAR_BALL_DYN;
        }
        break;
    }
    }

    // Write both rows to LCD
    LCD_SetCursor(0, 0);
    LCD_Print(line0);
    LCD_SetCursor(1, 0);
    LCD_Print(line1);
}

bool Game_IsOver(void)
{
    return (gs.state == GAME_STATE_GAME_OVER);
}

bool Game_IsVictory(void)
{
    return (gs.state == GAME_STATE_GAME_OVER && gs.lives > 0);
}

uint16_t Game_GetScore(void)
{
    return gs.score;
}

uint32_t Game_GetSpeed(void)
{
    return gs.speed_ms;
}

void Game_Reset(void)
{
    InitGame();
}

GameState_t* Game_GetState(void)
{
    return &gs;
}

/* ============================================================================
   PRIVATE FUNCTIONS
   ============================================================================ */

static void InitGame(void)
{
    paddle.py = (PLAYFIELD_HEIGHT - PADDLE_LENGTH) / 2; // Center vertically

    ResetBall();

    for (uint8_t i = 0; i < NUM_BRICKS; i++)
        bricks[i].hp = 2;

    gs.score = 0;
    gs.lives = 3;
    gs.level = 1;
    gs.speed_ms = INITIAL_SPEED_MS;
    gs.state = GAME_STATE_READY;
    gs.state_timer = HAL_GetTick();
    gs.last_update = HAL_GetTick();
    flash_cycle = 0;

    last_paddle_py = -1;
    last_ball_px = -1;
    last_ball_py = -1;

    HAL_GPIO_WritePin(LED_RED_GPIO_Port,    LED_RED_Pin,    GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_ORANGE_GPIO_Port, LED_ORANGE_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_BLUE_GPIO_Port,   LED_BLUE_Pin,   GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_GREEN_GPIO_Port,  LED_GREEN_Pin,  GPIO_PIN_SET);
}

static void InitLevel(void)
{
    for (uint8_t i = 0; i < NUM_BRICKS; i++)
        bricks[i].hp = 2;

    // Some bricks pre-damaged on higher levels
    if (gs.level >= 2)
    {
        uint8_t damaged = (gs.level - 1);
        if (damaged > NUM_BRICKS) damaged = NUM_BRICKS;
        for (uint8_t i = 0; i < damaged; i++)
        {
            uint8_t idx = rand() % NUM_BRICKS;
            if (bricks[idx].hp > 1)
                bricks[idx].hp = 1;
        }
    }
}

static void LaunchBall(void)
{
    ball.dx = 2;              // Move right toward bricks
    ball.dy = (rand() % 2 == 0) ? -1 : 1;
    ball.px = PADDLE_WIDTH_PX; // Just past paddle
    ball.py = paddle.py + PADDLE_LENGTH / 2;
    gs.state = GAME_STATE_PLAYING;

    HAL_GPIO_WritePin(LED_ORANGE_GPIO_Port, LED_ORANGE_Pin, GPIO_PIN_SET);
}

static void ResetBall(void)
{
    ball.px = PADDLE_WIDTH_PX + 2;  // Slightly right of paddle
    ball.py = paddle.py + PADDLE_LENGTH / 2;
    ball.dx = 0;
    ball.dy = 0;
}

static void CheckWallBounce(void)
{
    // Top wall
    if (ball.py < 0)
    {
        ball.py = 0;
        ball.dy = -ball.dy;
    }
    // Bottom wall
    if (ball.py >= PLAYFIELD_HEIGHT)
    {
        ball.py = PLAYFIELD_HEIGHT - 1;
        ball.dy = -ball.dy;
    }
}

static void CheckPaddleCollision(void)
{
    // Check if ball's vertical position overlaps paddle
    if (ball.py >= paddle.py && ball.py < paddle.py + PADDLE_LENGTH)
    {
        // Hit paddle — bounce right
        ball.dx = 2;  // Always move right

        // Angle based on where on paddle ball hit
        int8_t hit_pos = ball.py - paddle.py;  // 0 to PADDLE_LENGTH-1
        if (hit_pos <= PADDLE_LENGTH / 2)
            ball.dy = -1;  // Hit upper half → go upward
        else
            ball.dy = 1;   // Hit lower half → go downward

        HAL_GPIO_WritePin(LED_ORANGE_GPIO_Port, LED_ORANGE_Pin, GPIO_PIN_RESET);
    }
    else
    {
        // Missed paddle — life lost
        gs.lives--;

        if (gs.lives > 0)
        {
            gs.state = GAME_STATE_LIFE_LOST;
            gs.state_timer = HAL_GetTick();
        }
        else
        {
            gs.state = GAME_STATE_GAME_OVER;
            gs.state_timer = HAL_GetTick();
            HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
        }
    }
}

static void CheckBrickCollision(void)
{
    // Determine which brick the ball hit
    int cell_x = ball.px / 5;
    if (cell_x < BRICK_ZONE_COL) return;

    int brick_col = cell_x - BRICK_ZONE_COL;
    int brick_row = ball.py / 8;

    // Clamp to valid range
    if (brick_col < 0) brick_col = 0;
    if (brick_col >= BRICK_COLS) brick_col = BRICK_COLS - 1;
    if (brick_row < 0) brick_row = 0;
    if (brick_row >= BRICK_ROWS) brick_row = BRICK_ROWS - 1;
    int brick_idx = brick_row * BRICK_COLS + brick_col;
    if (brick_idx >= NUM_BRICKS) brick_idx = NUM_BRICKS - 1;

    if (bricks[brick_idx].hp > 0)
    {
        bricks[brick_idx].hp--;

        if (bricks[brick_idx].hp == 0)
        {
            gs.score++;
            ball.dx = -ball.dx;  // Bounce ball back left

            // Speed up slightly
            if (gs.speed_ms > SPEED_INCREASE_MS)
                gs.speed_ms -= SPEED_INCREASE_MS;
            if (gs.speed_ms < MIN_SPEED_MS)
                gs.speed_ms = MIN_SPEED_MS;

            HAL_GPIO_WritePin(LED_ORANGE_GPIO_Port, LED_ORANGE_Pin, GPIO_PIN_SET);
        }
        else
        {
            // Brick damaged but not destroyed — bounce back
            ball.dx = -ball.dx;
        }

        // Check level clear
        if (AreAllBricksDestroyed())
        {
            HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET);
            gs.level++;
            if (gs.speed_ms > LEVEL_SPEED_BOOST)
                gs.speed_ms -= LEVEL_SPEED_BOOST;
            if (gs.speed_ms < MIN_SPEED_MS)
                gs.speed_ms = MIN_SPEED_MS;
            InitLevel();
            ResetBall();
            gs.state = GAME_STATE_READY;
        }
    }
    // else: empty brick cell — ball passes through without bouncing
    // This allows the ball to reach bricks in columns 13-15 after
    // destroying bricks in columns 12
}

static bool AreAllBricksDestroyed(void)
{
    for (uint8_t i = 0; i < NUM_BRICKS; i++)
    {
        if (bricks[i].hp > 0)
            return false;
    }
    return true;
}

/**
 * @brief Generate paddle CGRAM chars based on paddle vertical position
 *
 * The paddle is a vertical bar, 2 chars wide (columns 0-1),
 * painted across both LCD rows. This function generates one
 * custom char for row 0 (pixel rows 0-7) and one for row 1
 * (pixel rows 8-15).
 *
 * Paddle occupies pixel rows: paddle.py to paddle.py + 5
 */
static void UpdatePaddleCGRAM(void)
{
    uint8_t top[8], bot[8];

    for (int i = 0; i < 8; i++)
    {
        int py_row0 = i;                  // Pixel row within the LCD row 0 (0-7)
        int abs_row = py_row0;            // Absolute pixel row (0-7)

        // Paddle fills the full 5-pixel width where active
        if (abs_row >= paddle.py && abs_row < paddle.py + PADDLE_LENGTH)
            top[i] = 0b11111;
        else
            top[i] = 0b00000;
    }

    for (int i = 0; i < 8; i++)
    {
        int py_row1 = i;                  // Pixel row within the LCD row 1 (0-7)
        int abs_row = py_row1 + 8;        // Absolute pixel row (8-15)

        if (abs_row >= paddle.py && abs_row < paddle.py + PADDLE_LENGTH)
            bot[i] = 0b11111;
        else
            bot[i] = 0b00000;
    }

    LCD_CreateChar(CHAR_PADDLE_TOP, top);
    LCD_CreateChar(CHAR_PADDLE_BOT, bot);
}

/**
 * @brief Generate ball CGRAM char at current sub-pixel position
 *
 * The ball is a 2×2 pixel dot placed at (sub_x, sub_y) within
 * its 5×8 character cell. sub_x = px % 5, sub_y = py % 8.
 * The 2×2 block is positioned so it never crosses cell boundaries.
 */
static void UpdateBallCGRAM(void)
{
    uint8_t cgram[8];
    memset(cgram, 0, 8);

    int sub_x = ball.px % 5;   // 0-4
    int sub_y = ball.py % 8;   // 0-7

    // Clamp to keep 2×2 block within the cell
    if (sub_x > 3) sub_x = 3;
    if (sub_y > 6) sub_y = 6;

    // Place 2×2 pixel block at (sub_x, sub_y)
    // Bit pattern: 0b11000 >> sub_x shifts the 2 pixels right
    uint8_t pixel_pattern = (0b11000 >> sub_x);
    cgram[sub_y]     |= pixel_pattern;
    cgram[sub_y + 1] |= pixel_pattern;

    LCD_CreateChar(CHAR_BALL_DYN, cgram);
}
