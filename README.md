# FreeRTOS Block Breaker

A Breakout-style brick breaker game on the STM32F407G Discovery board with a 16x2 LCD screen and a joystick.

---

## What Is This?

This is a Block Breaker game on a microcontroller. You move a paddle with a joystick to bounce a ball and break bricks. It's like the classic Atari game, but running on an STM32 chip with a tiny LCD screen.

The game uses FreeRTOS to handle input, game logic, and display all at the same time.

---

## How It Looks

```
+----------------+
|            ████|  <- Bricks (columns 12-15)
|            ████|
|                |
|*               |  <- Ball bouncing around
|*|              |  <- Paddle (columns 0-1)
+----------------+
     ^
  Joystick
```

The game is played in **portrait mode** - the paddle moves up and down on the left side, and the bricks are on the right side. The ball bounces between them.

---

## Game Rules

- Move the joystick to move the paddle up and down
- Bounce the ball to hit and break the bricks
- Each brick takes **2 hits** to break (it cracks on the first hit, breaks on the second)
- Break all 8 bricks to advance to the next level
- If the ball gets past your paddle, you lose a life
- You have **3 lives** - lose them all and it's game over
- The game gets faster as you break more bricks

---

## What You Need

| Item | What It's For |
|------|--------------|
| **STM32F407G-DISC1** board | The brain of the game |
| **16x2 LCD with I2C backpack** | The screen (connects to PB6/PB7) |
| **PS2 analog joystick** | The controller (connects to PA2/PA3) |
| **Jumper wires** | To connect everything |

### Wiring Diagram

```
LCD (I2C)              Joystick              STM32 Board
---------              --------              -----------
  SCL  ------------->  PB6
  SDA  ------------->  PB7
  VCC  ------------->  5V
  GND  ------------->  GND

  VRx  ------------->  PA2  (X axis)
  VRy  ------------->  PA3  (Y axis)
  VCC  ------------->  3.3V
  GND  ------------->  GND
```

---

## How to Build and Run

1. **Clone this repo:**
```bash
git clone https://github.com/stalin-alexandar/Block_Breaker_FreeRTOS.git
```

2. **Open in STM32CubeIDE:**
   - File -> Import -> Existing Projects into Workspace
   - Select the project folder

3. **Build:** Click Build (or Ctrl+B)

4. **Flash:** Click Run to upload to your board

5. **Play!** Move the joystick to control the paddle

---

## How It Works (Behind the Scenes)

### System Architecture

The game uses FreeRTOS to run three things at the same time:

```
+-------------------------------------+
|        FreeRTOS Scheduler            |
+----------+------------+------------+
| InputTask|  GameTask   | DisplayTask|
| Priority 3| Priority 2  | Priority 1 |
| (Highest)|             | (Lowest)   |
+----------+------------+------------+
| Joystick |  Block     |  LCD I2C   |
| ADC      |  Breaker   |  Driver    |
| PA2, PA3 |  Ball,     |  16x2 at   |
|          |  Bricks,   |  0x27      |
|          |  Collision |            |
+-----+----+-----+------+-----+------+
      |          |            |
      | input_queue | game_update_sem | lcd_mutex
      +----------+------------+-------+
```

### Task Descriptions

| Task | Priority | Period | What It Does |
|------|----------|--------|-------------|
| **InputTask** | 3 (highest) | Fixed | Reads joystick ADC values, applies deadzone filtering, sends direction via xQueueOverwrite for always-latest input |
| **GameTask** | 2 | 100-40 ms | Processes input, updates ball position/velocity, checks brick and paddle collisions, manages score/lives/levels |
| **DisplayTask** | 1 (lowest) | On signal | Waits on game_update_sem, renders bricks, paddle, then ball in priority order to the 16x2 LCD |

### Synchronization Primitives

| Primitive | Type | What It Does |
|-----------|------|-------------|
| input_queue | FreeRTOS Queue (overwrite) | Passes latest joystick direction from InputTask to GameTask |
| game_update_sem | Binary Semaphore | Signals DisplayTask that the game state has changed and needs redrawing |
| lcd_mutex | Mutex | Protects shared LCD I2C bus access from concurrent writes |

---

## Game Mechanics (Detailed)

### Playfield

The 16x2 LCD is treated as an **80x16 pixel virtual grid**:
- Each character cell = 5 pixels wide x 8 pixels tall
- 16 columns x 5 = 80 pixels horizontal
- 2 rows x 8 = 16 pixels vertical

```
Column:  0  1  |  2  3  4  ...  11  | 12 13 14 15
         PADDLE|    GAME AREA       |  BRICKS
         (6px |                    |  (4x2 grid)
          tall)|                    |
```

### Game Objects

| Object | Size | Position | What It Does |
|--------|------|----------|-------------|
| **Paddle** | 2 chars wide x 6 px tall | Columns 0-1 | Moves vertically, controlled by joystick Y-axis |
| **Ball** | 2x2 pixels (custom char) | Anywhere on grid | Bounces off walls, paddle, and bricks |
| **Bricks** | 1 char each (4x2 grid) | Columns 12-15 | HP: 2=full, 1=damaged, 0=destroyed |

### Collision Detection

- **Ball <-> Wall:** Bounces off top and bottom edges, wraps or bounces on left/right
- **Ball <-> Paddle:** Bounces when ball reaches columns 0-1; life lost if ball exits left
- **Ball <-> Brick:** Damages brick (HP-1), bounces ball; destroyed bricks are removed
- **Passthrough:** Ball passes through destroyed bricks without bouncing

### Speed Progression

| Event | Speed Change |
|-------|-------------|
| Starting speed | 100 ms per tick |
| Per brick destroyed | -5 ms per tick |
| Per level cleared | -10 ms per tick |
| Minimum speed | 40 ms per tick |

### State Machine

```
  READY --(joystick)--> PLAYING --(ball lost)--> LIFE_LOST
    ^                       |                        |
    |                       | (all bricks destroyed) | (lives > 0)
    |                       v                        |
    |                    VICTORY                     v
    |                       |                    PLAYING
    |                       | (lives = 0)           |
    |                       v                        |
    +----(auto-reset)---- GAME_OVER <----------------+
```

**States:**
- **READY:** Ball sits on paddle, waiting for player input
- **PLAYING:** Game is active, ball is moving
- **LIFE_LOST:** Ball got past paddle, short pause before respawn
- **GAME_OVER:** All lives lost, auto-reset after 3 seconds
- **VICTORY:** All bricks cleared, advance to next level

### Custom LCD Characters

| Index | Character | What It Looks Like |
|-------|-----------|-------------------|
| 1 | Brick Full | Solid block (HP = 2) |
| 2 | Brick Damaged | Partial block (HP = 1) |
| 3 | Paddle | Vertical bar |
| 4 | Ball | 2x2 pixel dot |

> **Important:** Custom character indices must be 1-7. Using index 0 causes C-string functions to treat it as a null terminator, making objects invisible.

### Onboard LED Indicators

| LED | Pin | What It Means |
|-----|-----|---------------|
| LD3 | PD13 (Green) | System power / ready |
| LD4 | PD12 (Orange) | Ball / brick activity |
| LD5 | PD14 (Red) | Life lost |
| LD6 | PD15 (Blue) | Level complete |

---

## Technical Specifications

### FreeRTOS Configuration

| Parameter | Value |
|-----------|-------|
| CPU Clock | 168 MHz |
| Tick Rate | 1000 Hz |
| Total Heap | 20 KB |
| Max Priorities | 5 |
| Preemption | Enabled |
| Stack Overflow Check | Mode 2 |
| Software Timers | Enabled |
| Mutexes | Enabled |

### Resource Usage

- **RAM:** ~10 KB (of 128 KB available)
- **Flash:** ~5% of 1 MB
- **Tasks:** 3 application + idle + timer
- **Synchronization:** 1 queue (overwrite), 1 binary semaphore, 1 mutex
- **Custom LCD Characters:** 4 (indices 1-4)

### Joystick Configuration

| Parameter | Value |
|-----------|-------|
| ADC Channels | X=CH2 (PA2), Y=CH3 (PA3) |
| Resolution | 12-bit (0-4095) |
| Center Value | 2048 |
| Deadzone | +/-200 from center |
| Left/Down Threshold | < 1800 |
| Right/Up Threshold | > 2300 |

### LCD Configuration

| Parameter | Value |
|-----------|-------|
| Type | 16x2 Character LCD |
| Interface | I2C via PCF8574 expander |
| I2C Address | 0x27 |
| I2C Pins | SCL=PB6, SDA=PB7 |
| Mode | 4-b
