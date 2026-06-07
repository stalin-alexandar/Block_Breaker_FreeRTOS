# Block Breaker FreeRTOS — Game Logic

> **Platform:** STM32F407VG Discovery + 16×2 LCD (PCF8574 I2C)  
> **Engine:** Column Breaker — portrait/sideways breakout variant  
> **RTOS:** FreeRTOS (3 tasks + binary semaphore + mutex + queue)

---

## 1. The LCD Playfield

The 16×2 LCD is treated as an **80 × 16 pixel** playfield:

```
   ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
   │  P  │  P  │     │     │     │     │     │     │     │     │     │     │  B  │  B  │  B  │  B  │← Row 0
   │  A  │  A  │  e  │  m  │  p  │  t  │  y  │  s  │  p  │  a  │  c  │  e  │  R  │  R  │  R  │  R  │  (col 0-15)
   │  D  │  D  │     │     │     │     │     │     │     │     │     │     │  I  │  I  │  I  │  I  │
   ├─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┼─────┤
   │  P  │  P  │     │     │     │     │     │     │     │     │     │     │  C  │  C  │  C  │  C  │← Row 1
   │  A  │  A  │  e  │  m  │  p  │  t  │  y  │  s  │  p  │  a  │  c  │  e  │  K  │  K  │  K  │  K  │  (col 0-15)
   │  D  │  D  │     │     │     │     │     │     │     │     │     │     │  S  │  S  │  S  │  S  │
   └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
   col:  0     1     2     3     4     5     6     7     8     9     10    11    12    13    14    15
   px:   0-4   5-9               ...                         55-59 60-64 65-69 70-74 75-79

         └──────┬──────┘                                    └──────────┬──────────┘
            PADDLE ZONE (px 0-9)                                  BRICK ZONE (px 60-79)
            2 chars × 5 px = 10 px wide                           4 chars × 5 px = 20 px wide
            6 px tall (out of 16 px total)
```

### Pixel-to-Character Mapping

Each LCD character cell is **5 pixels wide × 8 pixels tall**:

```
  character column = ball.px / 5       (0-15)
  character row    = ball.py / 8       (0 or 1)
  sub-pixel X      = ball.px % 5       (0-4, horizontal position within cell)
  sub-pixel Y      = ball.py % 8       (0-7, vertical position within cell)
```

---

## 2. Custom LCD Characters (CGRAM Slots 1-5)

| Slot | Define | Type | Pattern | Purpose |
|------|--------|------|---------|---------|
| **0** | *(unused)* | — | — | Intentionally empty — value 0 is C string null terminator |
| **1** | `CHAR_BRICK_FULL` | Static | `█████` | Solid 5×8 block (brick at full HP) |
| **2** | `CHAR_BRICK_DMG` | Static | `█ █ █` | Cracked pattern (brick at HP=1) |
| **3** | `CHAR_PADDLE_TOP` | Dynamic | Varies | Paddle segment in LCD row 0 |
| **4** | `CHAR_PADDLE_BOT` | Dynamic | Varies | Paddle segment in LCD row 1 |
| **5** | `CHAR_BALL_DYN` | Dynamic | `██` | 2×2 pixel dot at sub-pixel position |

### Slot 1: CHAR_BRICK_FULL (solid block)
```
Row 0: █████
Row 1: █████
Row 2: █████
Row 3: █████
Row 4: █████
Row 5: █████
Row 6: █████
Row 7: █████
```

### Slot 2: CHAR_BRICK_DMG (cracked)
```
Row 0: █████
Row 1: █   █
Row 2: █████
Row 3:  ███
Row 4: █████
Row 5: █   █
Row 6: █████
Row 7:      
```

### Slot 3-4: PADDLE (dynamic)
Generated each frame based on `paddle.py`. The 6-pixel-tall paddle is placed at absolute pixel rows `paddle.py` to `paddle.py + 5`:

```
Example: paddle.py = 2 (paddle at pixel rows 2-7)
┌─────────┬─────────┐
│  Row 0  │ PADDLE_ │  →  CGRAM shows pixels at rows 2-7
│  (px 0  │ _TOP    │     (top 2 rows empty, bottom 6 filled)
│   -7)   │         │
├─────────┼─────────┤
│  Row 1  │ PADDLE_ │  →  CGRAM shows pixels at rows 8-15
│  (px 8  │ _BOT    │     (rows 8-13 = partial, rows 14-15 = empty)
│   -15)  │         │
└─────────┴─────────┘
```

### Slot 5: BALL (dynamic)
2×2 pixel block generated each frame at sub-pixel position within its character cell:

```
Pixel pattern: 0b11000 >> sub_x

sub_x=0:  ██░     (bit 4-3)
sub_x=1:  ░██░    (bit 3-2)
sub_x=2:  ░░██    (bit 2-1)
sub_x=3:  ░░██    (clamped, bit 1-0)

Placed at rows sub_y and sub_y+1 within the 8-row character cell
```

---

## 3. Game Objects

### Ball
```
Ball_t {
    int8_t px    // 0-79  horizontal position (pixels)
    int8_t py    // 0-15  vertical position (pixels)
    int8_t dx    // ±2    horizontal velocity
    int8_t dy    // ±1    vertical velocity
}
```

### Paddle
```
Paddle_t {
    uint8_t py   // 0-10  top pixel position (max = 16 - 6 = 10)
}
```
- Paddle is 6 pixels tall, sits in columns 0-1 (px 0-9, 2 chars wide)
- Moves UP/DOWN only

### Bricks (8 total)
```
Brick_t bricks[8]  // indexed as: brick_row * 4 + brick_col

Row 0: bricks[0] bricks[1] bricks[2] bricks[3]    ← col 12-15
Row 1: bricks[4] bricks[5] bricks[6] bricks[7]    ← col 12-15

Each brick: {
    uint8_t hp   // 2=fresh, 1=damaged, 0=destroyed
}
```

---

## 4. Game State Machine

```
                ┌──────────────────────────────────────────────┐
                │                                              │
                ▼                                              │
        ┌──────────────┐                                      │
   ┌───→│  GAME_STATE  │  (joystick moved)                    │
   │    │  _READY      │──────────────────────┐               │
   │    │              │                      │               │
   │    │ Ball on      │                      ▼               │
   │    │ paddle,      │              ┌──────────────┐        │
   │    │ waiting      │              │  GAME_STATE  │        │
   │    │ for launch   │              │  _PLAYING    │        │
   │    └──────────────┘              │              │        │
   │                                  │ Ball in      │        │
   │         (lives > 0)              │ play         │        │
   │    ┌──────────────┐              └──────┬───────┘        │
   │    │  GAME_STATE  │◄────────────────────┘                │
   │    │  _LIFE_LOST  │  ball missed paddle                  │
   │    │              │                                      │
   │    │ 200ms flash  │  (lives == 0)                        │
   │    └──────┬───────┘                                      │
   │           │              ┌──────────────┐                │
   │           └──────────────│  GAME_STATE  │                │
   │                          │  _GAME_OVER  │                │
   │                          │              │                │
   │                          │ 4s delay,    │                │
   │                          │ auto-reset   │────────────────┘
   │                          └──────────────┘
   │
   │  (all bricks destroyed)
   │  ┌──────────────────────┐
   │  │  Level Clear!        │
   │  │  - InitLevel()       │
   │  │  - ResetBall()       │
   │  │  - state = READY     │────────────────────────────────┘
   │  └──────────────────────┘
   │
   └───────────────────────────────────────────────────────────┘
```

---

## 5. Collision Detection — Visual Trace

### 5a. Ball Entering Brick Zone

```
Frame N:    px=58             [ball not in brick zone]
            ...col 11...|col 12|col 13|col 14|col 15|
                        |  ██  |  ██  |  ██  |  ██  |  (bricks hp>0)

Frame N+1:  px=60  cell_x=12  brick_col=0
            ...col 11...|col 12|col 13|col 14|col 15|
                        |  █░  |  ██  |  ██  |  ██  |  ← ball hits brick[0]
                        |      |      |      |      |
            CheckBrickCollision():
              cell_x = 60/5 = 12
              brick_col = 12 - 12 = 0
              bricks[0].hp--  (2→1 or 1→0)
              ball.dx = -ball.dx  (bounce back)
```

### 5b. Ball Passing Through Destroyed Bricks

```
All bricks in column 12 destroyed, column 13 still has bricks:

Frame N:    px=60  cell_x=12  brick_col=0  hp=0  → PASS THROUGH
            ...col 11...|col 12|col 13|col 14|col 15|
                        |      |  ██  |  ██  |  ██  |  (brick gone)

Frame N+1:  px=62  cell_x=12  brick_col=0  hp=0  → PASS THROUGH

Frame N+2:  px=64  cell_x=12  brick_col=0  hp=0  → PASS THROUGH

Frame N+3:  px=66  cell_x=13  brick_col=1  hp>0  → HIT!
            ...col 11...|col 12|col 13|col 14|col 15|
                        |      |  █░  |  ██  |  ██  |
```

### 5c. Ball Bouncing Off Paddle

```
Paddle at py=3 (pixel rows 3-8), ball approaching from right (dx < 0):

Frame M:    px=11, py=5   →  px < PADDLE_WIDTH_PX (10) && dx < 0
            ...col 0...|col 1...|col 2...
            | PADDLE  | PADDLE  |
            |  HERE   |  HERE   |  ← ball.py=5 within paddle.py..py+5
            |   ██    |   ██    |
            
            CheckPaddleCollision():
              ball.py(5) >= paddle.py(3) && ball.py(5) < paddle.py+6(9) → HIT!
              ball.dx = 2  (bounce right)
              hit_pos = 5-3 = 2
              2 <= 6/2=3 → ball.dy = -1 (go up)
```

### 5d. Ball Missing Paddle

```
Frame M:    px=11, py=0   →  px < 10 && dx < 0
            CheckPaddleCollision():
              ball.py(0) < paddle.py(3) → MISS!
              gs.lives--, gs.state = GAME_STATE_LIFE_LOST
```

---

## 6. Rendering Pipeline

```
Game_Render() is called each game tick:

  ┌─────────────────────────────────────────────┐
  │  1. Clear line buffers to spaces            │
  │     char line0[17], line1[17]               │
  │     memset(line0,' ',16), line0[16]='\0'    │
  │                                             │
  │  2. Update paddle CGRAM (if position        │
  │     changed since last frame)               │
  │     → Writes CHAR_PADDLE_TOP & _BOT         │
  │                                             │
  │  3. Update ball CGRAM (if position          │
  │     changed since last frame)               │
  │     → Writes CHAR_BALL_DYN                  │
  │                                             │
  │  4. Draw bricks into line buffers           │
  │     For each brick with hp>0:               │
  │       line0[12+col] = CHAR_BRICK_FULL/DMG   │
  │       line1[12+col] = CHAR_BRICK_FULL/DMG   │
  │                                             │
  │  5. Draw paddle into line buffers           │
  │     line0[0] = CHAR_PADDLE_TOP              │
  │     line0[1] = CHAR_PADDLE_TOP              │
  │     line1[0] = CHAR_PADDLE_BOT              │
  │     line1[1] = CHAR_PADDLE_BOT              │
  │                                             │
  │  6. Draw ball into line buffers             │
  │     cell_x = ball.px/5                      │
  │     cell_y = ball.py/8                      │
  │     lineY[cell_x] = CHAR_BALL_DYN           │
  │                                             │
  │  7. Write both rows to LCD                  │
  │     LCD_SetCursor(0,0); LCD_Print(line0)    │
  │     LCD_SetCursor(1,0); LCD_Print(line1)    │
  └─────────────────────────────────────────────┘
```

### Draw Order (Overlap Priority)

Painted in order: **bricks → paddle → ball** (last drawn = on top)

Since bricks are at columns 12-15 and paddle at columns 0-1, they never visually overlap. The ball is drawn last so it's always visible over everything.

---

## 7. Speed System

```
Game Speed (tick interval):

  INITIAL:  100ms

  Per brick destroyed:  -5ms
  Per level cleared:    -10ms

  Minimum:  40ms

Example progression:
  Level 1, 0 bricks destroyed:  100ms
  Level 1, 5 bricks destroyed:   75ms (100 - 5×5)
  Level 2, 0 bricks destroyed:   65ms (75 - 10)
  Level 2, 8 bricks destroyed:   25ms → clamped to 40ms
```

---

## 8. LED Indicators

| Event | LED | Behavior |
|-------|-----|----------|
| System running | **Green** (PD12) | On at boot, off on game over |
| Ball in play / brick hit | **Orange** (PD13) | On at launch, off on paddle hit, on again on brick destroy |
| Life lost | **Red** (PD14) | Flashes at 50% duty for 200ms, then stays on at game over |
| Level cleared | **Blue** (PD15) | On while transitioning levels |
| Error / Stack overflow | **Red** (PD14) | Blinks at 100ms intervals indefinitely |

---

## 9. FreeRTOS Task Architecture

```
                    ┌──────────────┐
                    │   main()     │
                    │  (one-time)  │
                    └──────┬───────┘
                           │ vTaskStartScheduler()
                           ▼
     ┌─────────────────────────────────┐
     │         3 Concurrent Tasks      │
     │                                 │
     │  ┌──────────────────────┐       │
     │  │    InputTask (pri 3)  │ 50ms │
     │  │  Reads joystick ADC   │      │
     │  │  xQueueOverwrite →    │──────┼──→ input_queue
     │  └──────────────────────┘      │
     │                                 │
     │  ┌──────────────────────┐       │
     │  │   GameTask (pri 2)   │       │
     │  │  xQueueReceive ←     │←──────┼── input_queue
     │  │  Game_Update()       │      │
     │  │  xSemaphoreGive →    │──────┼──→ game_update_sem
     │  │  vTaskDelay(speed)   │      │
     │  └──────────────────────┘      │
     │                                 │
     │  ┌──────────────────────┐       │
     │  │ DisplayTask (pri 1)  │       │
     │  │  xSemaphoreTake ←    │←──────┼── game_update_sem
     │  │  xSemaphoreTake ←    │←──────┼── lcd_mutex (protects LCD I2C)
     │  │  Game_Render()       │      │
     │  └──────────────────────┘      │
     └─────────────────────────────────┘
```

### Synchronization

- **input_queue** (size=1 queue, `xQueueOverwrite`): Always holds the latest joystick direction. GameTask reads it non-blocking (timeout=0).
- **game_update_sem** (binary semaphore): GameTask signals DisplayTask to render after each update. If DisplayTask is slow, excess signals are dropped (binary semaphore doesn't count).
- **lcd_mutex** (mutex): Protects LCD I2C bus from concurrent writes (e.g., splash screen and Game_Render).

---

## 10. Level Progression

```
Level 1:  All 8 bricks at HP=2
          ┌─────┬─────┬─────┬─────┐
    Row 0 │ HP2 │ HP2 │ HP2 │ HP2 │
          ├─────┼─────┼─────┼─────┤
    Row 1 │ HP2 │ HP2 │ HP2 │ HP2 │
          └─────┴─────┴─────┴─────┘

Level 2:  1 brick pre-damaged to HP=1
          ┌─────┬─────┬─────┬─────┐
    Row 0 │ HP2 │ HP1 │ HP2 │ HP2 │  (damaged is random)
          ├─────┼─────┼─────┼─────┤
    Row 1 │ HP2 │ HP2 │ HP2 │ HP2 │
          └─────┴─────┴─────┴─────┘

Level 3:  2 bricks pre-damaged
          ...and so on
          More pre-damaged bricks each level = faster progression
```

### Level Clear Sequence
1. Ball destroys last brick → `AreAllBricksDestroyed()` returns true
2. Blue LED turns on
3. `gs.level++`
4. Speed gets a one-time boost: `gs.speed_ms -= LEVEL_SPEED_BOOST`
5. `InitLevel()` — resets all bricks to HP=2, pre-damages some
6. `ResetBall()` — ball placed back on paddle
7. `gs.state = GAME_STATE_READY` — waits for launch input

---

## 11. Game Over & Reset

```
Game Over sequence (no lives remaining):
  1. Ball misses paddle → lives == 0 → GAME_STATE_GAME_OVER
  2. Display stops updating (shows final frame)
  3. GameTask sees Game_IsOver() == true → vTaskDelay(4000ms)
  4. All LEDs turned off
  5. Game_Reset() → calls InitGame() → fresh state
  6. input_queue cleared (no stale inputs)
  7. Fresh render signaled
  8. Game loop continues from READY state
```

---

## 12. Key Constants Summary

| Constant | Value | Meaning |
|----------|-------|---------|
| `PLAYFIELD_WIDTH` | 80 | Total pixels horizontally (16 cols × 5px) |
| `PLAYFIELD_HEIGHT` | 16 | Total pixels vertically (2 rows × 8px) |
| `PADDLE_WIDTH_PX` | 10 | Paddle width (2 chars × 5px) |
| `PADDLE_LENGTH` | 6 | Paddle height in pixels |
| `PADDLE_PY_MAX` | 10 | Max paddle top position |
| `BRICK_ZONE_COL` | 12 | First brick LCD column |
| `BRICK_ZONE_PX` | 60 | First brick pixel position (12×5) |
| `NUM_BRICKS` | 8 | Total bricks (4 cols × 2 rows) |
| `INITIAL_SPEED_MS` | 100 | Starting game tick interval |
| `MIN_SPEED_MS` | 40 | Minimum game tick interval |
| `SPEED_INCREASE_MS` | 5 | Speed-up per brick destroyed |
| `LEVEL_SPEED_BOOST` | 10 | Speed-up per level cleared |
| `INPUT_TASK_PERIOD_MS` | 50 | Joystick polling interval |
