# 🧱 FreeRTOS Block Breaker

> A portrait-oriented Breakout game on STM32F407G Discovery with FreeRTOS, pixel-level ball physics, and brick destruction

[![Platform](https://img.shields.io/badge/Platform-STM32F407-blue.svg)](https://www.st.com/)
[![RTOS](https://img.shields.io/badge/RTOS-FreeRTOS-green.svg)](https://www.freertos.org/)
[![Display](https://img.shields.io/badge/Display-16x2_LCD-orange.svg)](https://www.adafruit.com/)
[![Input](https://img.shields.io/badge/Input-Analog_Joystick-red.svg)](https://www.adafruit.com/)

---

## 📋 Overview

A portrait-oriented Block Breaker game on the STM32F407G Discovery board using FreeRTOS for real-time multitasking. Move a paddle with a joystick to bounce a ball and break bricks. Unlike traditional Breakout, the paddle moves vertically on the left side while bricks are on the right. Features pixel-level ball physics, brick HP system, and progressive difficulty.

**Built as a portfolio project to demonstrate FreeRTOS architecture and embedded game physics.**

### Key Features
- 🧱 **Portrait Breakout** - Paddle on left, bricks on right
- 🎱 **Pixel-level Ball Physics** - 80x16 virtual grid with smooth movement
- 💥 **Brick HP System** - 2 hits to break (cracked → destroyed)
- ⚡ **FreeRTOS Multitasking** - Three concurrent tasks
- 📈 **Progressive Difficulty** - Speed increases with bricks destroyed
- 🏆 **Level Progression** - Clear all bricks to advance
- 🎮 **State Machine** - READY → PLAYING → LIFE_LOST → GAME_OVER

---

## 🔧 Hardware Requirements

### Your Components
- **STM32F407G-DISC1** - Main board (168MHz Cortex-M4)
- **16x2 LCD with I2C backpack** - Display (address 0x27)
- **PS2 Analog Joystick** - Controller module
- **Jumper Wires** - For connections

### Pin Configuration

| Pin | Function | Connection |
|-----|----------|------------|
| PA2 | ADC1 CH2 | Joystick X-axis |
| PA3 | ADC1 CH3 | Joystick Y-axis |
| PB6 | I2C1 SCL | LCD SCL |
| PB7 | I2C1 SDA | LCD SDA |
| PD12-PD15 | LEDs | Status indicators |

---

## 🏗️ System Architecture

```
┌─────────────────────────────────────────┐
│        FreeRTOS Scheduler               │
├──────────┬──────────────┬───────────────┤
│ InputTask│   GameTask    │ DisplayTask   │
│ Priority 3│  Priority 2   │ Priority 1    │
│ (Highest)│               │ (Lowest)      │
├──────────┼──────────────┼───────────────┤
│ Joystick │  Block       │  LCD I2C      │
│ ADC      │  Breaker     │  Driver       │
│ PA2, PA3 │  Ball, Bricks│  16x2 at      │
│          │  Collision   │  0x27         │
└────┬─────┴──────┬───────┴───────┬───────┘
     │            │               │
     │ input_queue│ game_update_sem│ lcd_mutex
     └────────────┴───────────────┘
```

---

## 🎮 Game Rules

- 🕹️ Move joystick to control paddle up/down
- 🏓 Bounce ball to hit and break bricks
- 💥 Each brick takes 2 hits (cracked → destroyed)
- 🧱 Break all 8 bricks to advance to next level
- ❤️ Lose a life if ball gets past paddle
- 💀 3 lives total - game over when all lost
- ⚡ Game gets faster as bricks are destroyed

---

## 📊 Technical Specifications

### Playfield
- **Grid**: 80x16 pixels (16 cols x 5px, 2 rows x 8px)
- **Paddle**: 2 chars wide, 6px tall (columns 0-1)
- **Ball**: 2x2 pixel custom character
- **Bricks**: 8 total (4x2 grid, columns 12-15)

### Ball Physics
- Pixel-level tracking on virtual grid
- Bounces off walls, paddle, and bricks
- Passthrough on destroyed bricks
- Rendering priority: bricks → paddle → ball

### Speed Progression

| Event | Speed Change |
|-------|-------------|
| Start | 100ms per tick |
| Per brick | -5ms per tick |
| Per level | -10ms per tick |
| Minimum | 40ms per tick |

### FreeRTOS Tasks

| Task | Priority | Period | Description |
|------|----------|--------|-------------|
| InputTask | 3 (highest) | Fixed | Reads joystick ADC |
| GameTask | 2 | 100-40ms | Ball physics, collision, scoring |
| DisplayTask | 1 (lowest) | On signal | Renders LCD grid |

### Resource Usage
- **Flash**: ~5% of 1MB
- **RAM**: ~10KB of 128KB
- **FreeRTOS Heap**: 20KB
- **Custom LCD Characters**: 4 (indices 1-4)

---

## 🔄 Game States

```
  READY --(joystick)--> PLAYING --(ball lost)--> LIFE_LOST
    ^                       |                        |
    |                       | (all bricks clear)     | (lives > 0)
    |                       v                        |
    |                    VICTORY                     v
    |                       |                    PLAYING
    |                       | (lives = 0)           |
    |                       v                        |
    +----(auto-reset)---- GAME_OVER <----------------+
```

---

## 🚦 LED Indicators

| LED | Color | Meaning |
|-----|-------|---------|
| LD3 | Green | System ready |
| LD4 | Orange | Ball/brick activity |
| LD5 | Red | Life lost |
| LD6 | Blue | Level complete |

---

## 📁 Project Structure

```
Block_Breaker_FreeRTOS/
├── App/
│   ├── DisplayManager/    # LCD driver
│   ├── GameEngine/        # Ball physics, brick HP
│   └── InputManager/      # Joystick driver
├── Core/
│   ├── Inc/               # Headers + FreeRTOS config
│   └── Src/               # main.c + system files
├── FreeRTOS/              # FreeRTOS kernel
├── Drivers/               # STM32 HAL library
└── README.md
```

---

## 🚀 Getting Started

### Prerequisites
- STM32CubeIDE (v1.18.0+)
- ST-LINK programmer (built into Discovery board)
- Hardware components (LCD + Joystick)

### Quick Start
1. **Clone repository**
   ```bash
   git clone https://github.com/stalin-alexandar/Block_Breaker_FreeRTOS.git
   ```

2. **Open in STM32CubeIDE**
   - File → Import → Existing Projects
   - Select project folder

3. **Build and Flash**
   - Build (Ctrl+B) → Run

4. **Wire Hardware**
   - LCD: SCL→PB6, SDA→PB7
   - Joystick: X→PA2, Y→PA3

5. **Play!**
   - Move joystick to control the paddle

---

## 🎓 Skills Demonstrated

### Embedded Systems
- ✅ STM32F407 programming (Cortex-M4)
- ✅ FreeRTOS real-time scheduling
- ✅ I2C LCD communication
- ✅ ADC for analog input
- ✅ Custom character bitmaps
- ✅ Pixel-level game rendering

### Game Development
- ✅ State machine design
- ✅ Collision detection algorithms
- ✅ Ball physics simulation
- ✅ HP-based damage system
- ✅ Progressive difficulty curves

### Software Architecture
- ✅ Multi-tasking with priorities
- ✅ Producer-consumer pattern (queue)
- ✅ Mutual exclusion (mutex)
- ✅ Synchronization (semaphore)
- ✅ Rendering priority system

---

## 💼 For Employers

This project demonstrates:
- **Real-Time Systems**: FreeRTOS task scheduling with priorities
- **Game Physics**: Pixel-level ball movement and collision detection
- **Embedded Architecture**: Clean separation of concerns (Input/Game/Display)
- **State Machine Design**: Complex game flow management

**Interview Pitch**: "I built a Block Breaker game on STM32F407 using FreeRTOS with pixel-level ball physics, brick HP system, and a state machine for game flow. The system demonstrates real-time scheduling and embedded game development."

---

## 📝 License

MIT License - Free for educational and portfolio use

---

## 👤 Author

**Stalin Alexandar**
- GitHub: [@stalin-alexandar](https://github.com/stalin-alexandar)

---

**Built with ❤️ for embedded systems enthusiasts**
