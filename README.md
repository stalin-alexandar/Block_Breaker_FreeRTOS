# FreeRTOS Block Breaker — STM32F407G Discovery

A portrait-oriented Block Breaker (Breakout) game running on the STM32F407G Discovery board, built with FreeRTOS for real-time multitasking. The game renders on a 16×2 character LCD via I2C and accepts input from an analog joystick via ADC.

---

## Overview

This project implements a complete Block Breaker game on bare-metal STM32 hardware using FreeRTOS. Unlike traditional Breakout games, this version uses a **portrait orientation** — the paddle moves vertically on the left side of the screen while bricks are arranged on the right. The architecture splits work across three concurrent tasks for input, game logic, and display rendering.

---

## Key Features

- **FreeRTOS Multitasking** — three tasks with distinct priorities for input, game logic, and display
- **Portrait Breakout** — paddle moves vertically on columns 0–1, bricks on columns 12–15
- **16×2 LCD Display** — renders the game using custom 5×8 pixel character bitmaps via I2C (PCF8574)
- **Pixel-level Ball Physics** — ball moves across an 80×16 pixel virtual grid with sub-pixel tracking
- **Analog Joystick Input** — reads X/Y axes via ADC with deadzone filtering
- **Dynamic Difficulty** — game speed increases as bricks are destroyed and levels are cleared
- **State Machine** — clean transitions between READY, PLAYING, LIFE_LOST, and GAME_OVER states
- **Onboard LED Status** — four LEDs indicate system, ball, life, and level status
- **Level Progression** — destroy all bricks to advance to the next level

---

## System Architecture

```
┌─────────────────────────────────────────────────────┐
│                    FreeRTOS Scheduler                │
├──────────────┬──────────────────┬───────────────────┤
│  InputTask   │    GameTask      │   DisplayTask     │
│  Priority 3  │   Priority 2     │   Priority 1      │
│  (Highest)   │                  │   (Lowest)        │
├──────────────┼──────────────────┼───────────────────┤
│ Joystick ADC │  Block Breaker   │  LCD I2C Driver   │
│  PA2 (X)     │  Ball physics,   │  16×2 character   │
│  PA3 (Y)     │  collision, brick│  display at 0x27  │
│              │  HP, scoring     │                   │
└──────┬───────┴────────┬─────────┴─────────┬─────────┘
       │                │                   │
       │   input_queue  │  game_update_sem  │ lcd_mutex
       └────────────────┴───────────────────┘
```

### Task Descriptions

| Task | Priority | Period | Description |
|------|----------|--------|-------------|
| **InputTask** | 3 (highest) | Fixed | Reads joystick ADC values, applies deadzone filtering, sends direction via `xQueueOverwrite` for always-latest input |
| **GameTask** | 2 | 100→40 ms | Processes input, updates ball position/velocity, checks brick and paddle collisions, manages score/lives/levels |
| **DisplayTask** | 1 (lowest) | On signal | Waits on `game_update_sem`, renders bricks → paddle → ball in priority order to the 16×2 LCD |

### Synchronization Primitives

| Primitive | Type | Purpose |
|-----------|------|--------|
| `input_queue` | FreeRTOS Queue (overwrite) | Passes latest joystick direction from InputTask to GameTask |
| `game_update_sem` | Binary Semaphore | Signals DisplayTask that the game state has changed and needs redrawing |
| `lcd_mutex` | Mutex | Protects shared LCD I2C bus access from concurrent writes |

---

## Game Mechanics

### Playfield

The 16×2 LCD is treated as an **80×16 pixel virtual grid**:
- Each character cell = 5 pixels wide × 8 pixels tall
- 16 columns × 5 = 80 pixels horizontal
- 2 rows × 8 = 16 pixels vertical

```
Column:  0  1  │  2  3  4  ...  11  │ 12 13 14 15
         PADDLE│    GAME AREA       │  BRICKS
         (6px │                    │  (4×2 grid)
          tall)│                    │
```

### Game Objects

| Object | Size | Position | Description |
|--------|------|----------|-------------|
| **Paddle** | 2 chars wide × 6 px tall | Columns 0–1 | Moves vertically, controlled by joystick Y-axis |
| **Ball** | 2×2 pixels (custom char) | Anywhere on grid | Bounces off walls, paddle, and bricks |
| **Bricks** | 1 char each (4×2 grid) | Columns 12–15 | HP: 2=full, 1=damaged, 0=destroyed |

### Collision Detection

- **Ball ↔ Wall**: Bounces off top and bottom edges, wraps or bounces on left/right
- **Ball ↔ Paddle**: Bounces when ball reaches columns 0–1; life lost if ball exits left
- **Ball ↔ Brick**: Damages brick (HP−1), bounces ball; destroyed bricks are removed
- **Passthrough**: Ball passes through destroyed bricks without bouncing

### Speed Progression

| Event | Speed Change |
|-------|-------------|
| Starting speed | 100 ms per tick |
| Per brick destroyed | −5 ms per tick |
| Per level cleared | −10 ms per tick |
| Minimum speed | 40 ms per tick |

### State Machine

```
  READY ──(joystick)──▶ PLAYING ──(ball lost)──▶ LIFE_LOST
    ▲                       │                        │
    │                       │ (all bricks destroyed) │ (lives > 0)
    │                       ▼                        │
    │                    VICTORY                     ▼
    │                       │                    PLAYING
    │                       │ (lives = 0)           │
    │                       ▼                        │
    └────(auto-reset)──── GAME_OVER ◀────────────────┘
```

### Custom LCD Characters

| Index | Character | Description |
|-------|-----------|-------------|
| 1 | Brick Full | Solid block (HP = 2) |
| 2 | Brick Damaged | Partial block (HP = 1) |
| 3 | Paddle | Vertical bar |
| 4 | Ball | 2×2 pixel dot |

> **Important**: Custom character indices must be 1–7. Using index 0 causes C-string functions to treat it as a null terminator, making objects invisible.

### Onboard LED Indicators

| LED | Pin | Purpose |
|-----|-----|--------|
| LD3 (Green) | PD13 | System power / ready |
| LD4 (Orange) | PD12 | Ball / brick activity |
| LD5 (Red) | PD14 | Life lost |
| LD6 (Blue) | PD15 | Level complete |

---

## Hardware Requirements

- **MCU**: STM32F407G-DISC1 Discovery Board
- **Display**: 16×2 Character LCD with PCF8574 I2C backpack (address `0x27`)
- **Input**: PS2 analog joystick module
- **Connections**:

| Signal | Pin | Connection |
|--------|-----|------------|
| LCD SCL | PB6 | I2C1 Clock → LCD SCL |
| LCD SDA | PB7 | I2C1 Data → LCD SDA |
| Joystick X | PA2 | ADC1 Channel 2 |
| Joystick Y | PA3 | ADC1 Channel 3 |
| LED LD3 | PD13 | Green (system ready) |
| LED LD4 | PD12 | Orange (ball activity) |
| LED LD5 | PD14 | Red (life lost) |
| LED LD6 | PD15 | Blue (level complete) |

---

## Project Structure

```
Block_Breaker_FreeRTOS/
├── App/
│   ├── DisplayManager/
│   │   ├── lcd_display.c       # I2C LCD driver (4-bit mode via PCF8574)
│   │   └── lcd_display.h
│   ├── GameEngine/
│   │   ├── block_breaker.c     # Ball physics, collision, brick HP, scoring
│   │   └── block_breaker.h
│   └── InputManager/
│       ├── joystick.c          # ADC joystick driver with deadzone
│       └── joystick.h
├── Core/
│   ├── Inc/
│   │   ├── main.h
│   │   ├── FreeRTOSConfig.h    # RTOS configuration (168 MHz, 20KB heap)
│   │   ├── stm32f4xx_hal_conf.h
│   │   └── stm32f4xx_it.h
│   └── Src/
│       ├── main.c              # Entry point, task creation, peripherals
│       ├── stm32f4xx_hal_msp.c
│       ├── stm32f4xx_it.c
│       └── system_stm32f4xx.c
├── FreeRTOS/                   # FreeRTOS kernel source
├── Drivers/                    # STM32 HAL and CMSIS drivers
├── game_logic.md               # Detailed game logic documentation
├── bug-analysis-spec.md        # Bug analysis and fix documentation
├── STM32F407VGTX_FLASH.ld     # Linker script
├── .gitignore
└── README.md                   # This file
```

---

## Getting Started

### Prerequisites

- **STM32CubeIDE** 1.18.0 or later
- **ST-LINK** programmer/debugger (onboard on Discovery board)
- **Hardware**: 16×2 I2C LCD + PS2 joystick module

### Build and Flash

1. Clone the repository:
```bash
git clone https://github.com/stalin-alexandar/Block_Breaker_FreeRTOS.git
cd Block_Breaker_FreeRTOS
```

2. Open in STM32CubeIDE:
   - File → Import 
