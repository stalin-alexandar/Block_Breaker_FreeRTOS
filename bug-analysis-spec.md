# Block Breaker FreeRTOS — Bug Analysis Spec

> **Project:** Block_Breaker_FreeRTOS (STM32F407VG, 16×2 LCD + PCF8574 I2C)
> **Author:** Generated via AI-assisted interview (Buffy)
> **Date:** 2025-01-XX
> **Status:** Analysis complete — no code changes yet

---

## 1. How the Game Works (Brief)

- **Playfield:** 80 px wide (16 cols × 5 px) × 16 px tall (2 LCD rows × 8 px)
- **Paddle:** Left side (cols 0–1), 6 px tall, moves UP/DOWN via joystick
- **Bricks:** Right side (cols 12–15), 4 cols × 2 rows = 8 bricks, each 5×8 pixels
- **Ball:** 2×2 px dot, moves dx=±2, dy=±1 per tick
- **Custom LCD chars:** Slots 0–7 (0=BRICK_FULL, 1=BRICK_DMG, 2=PADDLE_TOP, 3=PADDLE_BOT, 4=BALL_DYN)

---

## 2. User-Reported Symptoms

| Symptom | Detail |
|---------|--------|
| **Bricks start invisible** | At game start, no bricks are visible on the LCD |
| **Brick appears on ball hit** | After the ball reaches/hits a brick's position, a solid block appears |
| **Paddle & ball always visible** | Display correctly at all times |
| **Game is playable** | Can play, lose lives, etc. — but visuals are wrong |
| **Hardware** | Standard 1602 LCD + PCF8574 I2C backpack |

---

## 3. Bug #1 (PRIMARY) — `CHAR_BRICK_FULL = 0` Causes `LCD_Print()` to Terminate Early

### Root Cause

`CHAR_BRICK_FULL` is defined as **0** in `block_breaker.h`:

```c
#define CHAR_BRICK_FULL     0   // <-- This is the null terminator character!
```

In C, character value `0` is the **null terminator** (`\0`) that terminates strings. The `LCD_Print()` function iterates with `while (*str)`, which stops at the first `\0`:

```c
void LCD_Print(const char *str) {
    while (*str) {        // Stops at first \0
        LCD_WriteData(*str++);
    }
}
```

### What Happens During Rendering

`Game_Render()` builds `line0` and `line1` buffers (`char[17]`), then calls `LCD_Print()`:

```
line0 before LCD_Print:
  [0]  = 2  (CHAR_PADDLE_TOP)
  [1]  = 2  (CHAR_PADDLE_TOP)
  [2]  = 32 (' ' space)
  ...
  [11] = 32 (' ' space)
  [12] = 0  (CHAR_BRICK_FULL)  ← C string null terminator!
  [13] = 0  (CHAR_BRICK_FULL)  ← C string null terminator!
  [14] = 0  (CHAR_BRICK_FULL)  ← C string null terminator!
  [15] = 0  (CHAR_BRICK_FULL)  ← C string null terminator!
  [16] = 0  (string \0 terminator)
```

When `LCD_Print(line0)` runs:
1. Prints chars at `line0[0]` through `line0[11]` (paddle + spaces) — all non-zero → OK
2. Reads `line0[12]` = `0` → **`while (*str)` evaluates to false, exits loop**
3. Characters at positions 12–15 are **never sent to the LCD**

**Result:** The first 12 columns (paddle + spaces) display correctly, but columns 12–15 (where bricks should be) retain whatever was previously in LCD DDRAM (old data from splash screen).

The same happens for `line1` (the second LCD row).

### Why the Ball Is Always Visible

`CHAR_BALL_DYN` = **4** (non-zero). So when `LCD_Print()` reaches the ball's position, the character value 4 ≠ 0, and printing continues. This is why the ball always displays correctly.

### Why Paddle Is Always Visible

`CHAR_PADDLE_TOP` = **2**, `CHAR_PADDLE_BOT` = **3** (both non-zero). Same logic applies.

### Why Bricks "Appear" After Ball Hits

When the ball hits a brick:
- Brick HP goes from 2 → 1
- Rendering changes from `CHAR_BRICK_FULL (0)` to `CHAR_BRICK_DMG (1)`
- `CHAR_BRICK_DMG (1)` is non-zero, so `LCD_Print()` no longer stops at that position

**But:** Only the hit brick's column changes from 0→1. If the first brick at column 12 is hit (value becomes 1), `LCD_Print()` passes that column, but then hits the next brick at column 13 which still has value `0` (unless it was also hit) → stops again. This means bricks only become visible one column at a time as the ball hits them.

### Severity

**HIGH** — This is the primary bug causing all bricks to be invisible at game start. The fix is to reassign `CHAR_BRICK_FULL` to a non-zero slot (e.g., slot 5) and reassign other custom character slots accordingly.

---

## 4. Bug #2 (SECONDARY) — Off-by-One in Brick Collision Zone Entry

### Root Cause

In `block_breaker.c`, `Game_Update()`, the brick collision check uses `ball.px + 1 >= BRICK_ZONE_PX`:

```c
else if (ball.px + 1 >= BRICK_ZONE_PX && ball.dx > 0)
{
    CheckBrickCollision();
}
```

Where:
```c
#define BRICK_ZONE_PX   60   // Column 12 × 5 pixels
```

`ball.px + 1 >= 60` means the check fires when `ball.px >= 59` — **1 pixel before** the actual brick zone (which starts at pixel 60).

### What Goes Wrong

At `ball.px = 59`, the ball is in **column 11** (pixels 55–59), which is **before** any bricks exist. Inside `CheckBrickCollision()`:

```c
int cell_x = ball.px / 5;   // 59 / 5 = 11
if (cell_x < BRICK_ZONE_COL) return;  // 11 < 12 → would return!
```

Wait — there IS a guard: `if (cell_x < BRICK_ZONE_COL) return;` at the top of `CheckBrickCollision()`. At px=59, cell_x=11 < 12, so it returns early!

So actually, the `+1` only matters when `ball.px = 59` → the condition fires but the function returns early because `cell_x = 11 < 12`.

BUT — at `ball.px = 60`: `cell_x = 60 / 5 = 12`, `brick_col = 12 - 12 = 0`. So the ball always hits `brick_col = 0` (first brick column) regardless of its actual horizontal trajectory.

### Why the Ball Can't Reach Bricks at Columns 13–15

1. Ball enters brick zone moving right (dx > 0)
2. At `px = 60`, `cell_x = 12`, `brick_col = 0` → hits `bricks[0]` or `bricks[4]`
3. Brick HP decrements (2→1 or 1→0)
4. `ball.dx = -ball.dx` (bounces left)
5. Ball moves left, exits brick zone

On the next approach:
6. Same entry at `px = 60`, same `brick_col = 0`
7. If brick already destroyed (hp=0): `else` clause bounces ball back (`!AreAllBricksDestroyed()` is true)
8. Ball **never advances past column 0** (the first brick column)

### Impact on Bug #1's Symptom

Because the ball always hits brick_col 0 first, only bricks at column 12 get their HP reduced (from 2→1 or 1→0). Bricks at columns 13–15 retain `CHAR_BRICK_FULL (0)` and remain invisible. Only if the ball somehow destroys all bricks in column 12 of both rows and `AreAllBricksDestroyed()` returns true would the ball pass through... but `AreAllBricksDestroyed()` checks ALL 8 bricks, and columns 13–15 are undamaged, so it returns false.

**Net effect:** The ball gets stuck bouncing off column 12 and can never reach columns 13–15.

### Severity

**MEDIUM** — Makes the game unwinnable for most levels. The fix should change `ball.px + 1 >= BRICK_ZONE_PX` to `ball.px >= BRICK_ZONE_PX` and ensure the ball can enter different brick columns based on its actual position.

---

## 5. Bug #3 (MINOR) — `GameState_t` Struct Has Unpopulated Duplicate Fields

### Root Cause

`GameState_t` in `block_breaker.h` contains fields that shadow global variables but are never populated:

```c
typedef struct {
    GameStateEnum_t state;
    Ball_t ball;            // ← DUPLICATE of global `ball`
    Paddle_t paddle;         // ← DUPLICATE of global `paddle`
    Brick_t bricks[8];       // ← DUPLICATE of global `bricks`
    uint16_t score;
    uint8_t lives;
    uint8_t level;
    uint32_t speed_ms;
    uint32_t state_timer;
    uint32_t last_update;
} GameState_t;
```

The game engine uses **global** `ball`, `paddle`, `bricks` variables. The `gs` struct's `ball`, `paddle`, and `bricks` fields are **never written to** — they remain zero-initialized. `Game_GetState()` returns `&gs`, so callers get stale/zero data for ball, paddle, and bricks.

### Impact

Currently none — `Game_GetState()` is not called in the codebase (it was likely intended for debug/monitoring use). But if used in the future, it would return incorrect data.

### Severity

**LOW** — No current functional impact, but a design smell.

---

## 6. Additional Observations

### Edge Case: Ball at LCD Row Boundary (py = 7–8)

When `ball.py = 7` (last pixel of LCD row 0), the ball CGRAM clamping `if (sub_y > 6) sub_y = 6` shifts the 2×2 ball up by 1 pixel to keep it within the character cell. This means the ball's visual position is 1 pixel higher than its logical position at this boundary. Purely cosmetic — collision detection uses `ball.py` directly.

### Edge Case: Ball at Vertical Walls

`CheckWallBounce()` sets `ball.py = 0` when `< 0` and `ball.py = 15` when `>= 16`. Then `ball.dy = -ball.dy`. This correctly reflects the ball at the top and bottom edges.

### Edge Case: Life Lost → READY Transition

In `GAME_STATE_LIFE_LOST`, the 200ms red LED flash timer uses `gs.state_timer` (set at life-loss time) and `flash_cycle` (increments every update call). After 200ms, if `gs.lives > 0`, it calls `ResetBall()` and transitions to READY. This is correct.

### Rendering Order

The `Game_Render()` function in `GAME_STATE_PLAYING` draws:
1. Paddle CGRAM (if position changed)
2. Ball CGRAM (if position changed — **after** paddle CGRAM, so ball overwrites paddle if they overlap)
3. Bricks (into line buffers)
4. Paddle (into line buffers — **after** bricks, overwriting bricks at cols 0–1... but bricks are at cols 12–15, paddle at cols 0–1, so no conflict)
5. Ball (into line buffers — **after** paddle)
6. LCD write (both rows)

This overwrite order is correct — paddle is drawn after bricks (no actual overlap since they're on opposite sides), and ball is drawn last so it appears on top.

---

## 7. Summary of All Bugs Found

| # | Bug | Location | Severity | Description |
|---|-----|----------|----------|-------------|
| **1** | Null-terminator brick char | `block_breaker.h:CHAR_BRICK_FULL = 0` | **HIGH** | `LCD_Print()` stops at first brick (value 0 = `\0`), bricks invisible |
| **2** | Off-by-one brick zone entry | `block_breaker.c`:`ball.px + 1 >= BRICK_ZONE_PX` | **MEDIUM** | Ball always hits brick_col=0, can't reach cols 13–15 |
| **3** | Unpopulated struct fields | `GameState_t` in `block_breaker.h` | **LOW** | `gs.ball/paddle/bricks` never written, stale data via `GetState()` |

---

## 8. Next Steps

1. **Fix Bug #1:** Reassign custom character slots so `CHAR_BRICK_FULL` is non-zero (e.g., slot 0→5, shift others accordingly or swap roles)
2. **Fix Bug #2:** Change `ball.px + 1 >= BRICK_ZONE_PX` to `ball.px >= BRICK_ZONE_PX` and ensure proper brick-column mapping
3. **Fix Bug #3 (optional):** Either remove the duplicate fields from `GameState_t` or populate them from the globals in `Game_GetState()`
4. **Validate:** Test on hardware to verify bricks render correctly and the ball can reach all brick columns
