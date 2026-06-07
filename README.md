# FreeRTOS Block Breaker

A Breakout-style brick breaker game on the STM32F407G Discovery board with a 16×2 LCD screen and a joystick.

---

## What Is This?

This is a Block Breaker game on a microcontroller. You move a paddle with a joystick to bounce a ball and break bricks. It's like the classic Atari game, but running on an STM32 chip with a tiny LCD screen.

The game uses FreeRTOS to handle input, game logic, and display all at the same time.

---

## How It Looks

```
┌────────────────┐
│            ████│  ← Bricks (columns 12-15)
│            ████│
│                │
│⬤              │  ← Ball bouncing around
│█│             │  ← Paddle (columns 0-1)
└────────────────┘
      ↑
   Joystick
```

The game is played in **portrait mode** — the paddle moves up and down on the left side, and the bricks are on the right side. The ball bounces between them.

---

## Game Rules

- Move the joystick to move the paddle up and down
- Bounce the ball to hit and break the bricks
- Each brick takes **2 hits** to break (it cracks on the first hit, breaks on the second)
- Break all 8 bricks to advance to the next level
- If the ball gets past your paddle, you lose a life
- You have **3 lives** — lose them all and it's game over
- The game gets faster as you break more bricks

---

## What You Need

| Item | What It's For |
|------|--------------|
| **STM32F407G-DISC1** board | The brain of the game |
| **16×2 LCD with I2C backpack** | The screen (connects to PB6/PB7) |
| **PS2 analog joystick** | The controller (connects to PA2/PA3) |
| **Jumper wires** | To connect everything |

### Wiring Diagram

```
LCD (I2C)              Joystick              STM32 Board
─────────              ────────              ───────────
  SCL  ──────────────→  PB6
  SDA  ──────────────→  PB7
  VCC  ──────────────→  5V
  GND  ──────────────→  GND

  VRx  ──────────────→  PA2  (X axis)
  VRy  ──────────────→  PA3  (Y axis)
  VCC  ──────────────→  3.3V
  GND  ──────────────→  GND
```

---

## How to Build and Run

1. **Clone this repo:**
```bash
git clone https://github.com/stalin-alexandar/Block_Breaker_FreeRTOS.git
```

2. **Open in STM32CubeIDE:**
   - File → Import → Existing Projects into Workspace
   - Select the project folder

3. **Build:** Click Build (or `Ctrl+B`)

4. **Flash:** Click Run to upload to your board

5. **Play!** Move the joystick to control the paddle

---

## How It Works (Behind the Scenes)

The game uses **FreeRTOS** to run three things at the same time:

| Task | Job | How Often |
|------|-----|-----------|
| **InputTask** | Reads the joystick | Continuously |
| **GameTask** | Moves the ball, checks collisions, updates score | Every 40–100 milliseconds |
| **DisplayTask** | Draws the game to the LCD | Only when something changes |

These three tasks talk to each other using:
- A **queue** to pass joystick commands
- A **semaphore** to tell the display "hey, redraw!"
- A **mutex** to make sure only one task uses the LCD at a time

### The Screen

The 16×2 LCD has 32 character cells, but each character can show a 5×8 pixel pattern. So the actual play area is **80 pixels wide × 16 pixels tall**.

The game uses custom characters for:
- **Bricks** — solid blocks (full or cracked)
- **Paddle** — a vertical bar
- **Ball** — a small dot that bounces around

### Speed Progression

The game starts at a comfortable speed and gets faster:

| Event | What Happens |
|-------|-------------|
| Game starts | Ball moves every 100ms |
| Break a brick | Gets 5ms faster |
| Clear a level | Gets 10ms faster |
| Minimum speed | 40ms per tick (very fast!) |

### Game States

```
   READY (press joystick to start)
     ↓
   PLAYING (move the paddle!)
     ↓
   Life lost? → Still have lives? → Keep playing
     ↓
   No lives left → GAME OVER → Auto-reset after 3 seconds
     ↓
   All bricks broken → Next level!
```

---

## LED Indicators

The four onboard LEDs show what's happening:

| LED | Color | What It Means |
|-----|-------|---------------|
| LD3 | Green | System is on and ready |
| LD4 | Orange | Ball is moving / bricks are being hit |
| LD5 | Red | You lost a life |
| LD6 | Blue | You cleared a level |

---

## Project Structure

```
Block_Breaker_FreeRTOS/
├── App/
│   ├── DisplayManager/    # LCD drawing code
│   ├── GameEngine/        # Ball physics, brick collision, scoring
│   └── InputManager/      # Joystick reading code
├── Core/
│   ├── Inc/               # Header files and FreeRTOS config
│   └── Src/               # main.c and system files
├── FreeRTOS/              # FreeRTOS operating system
├── Drivers/               # STM32 HAL library
└── Startup/               # Boot assembly code
```

---

## Tips

- The LCD needs an I2C backpack (PCF8574 chip). Most "I2C LCD" modules have this built in.
- The I2C address is usually `0x27`. If your LCD doesn't show anything, try `0x3F`.
- The joystick uses analog input — make sure it's connected to PA2 (X) and PA3 (Y).
- If the ball seems stuck, check that your joystick isn't sending constant input.

---

## Author

**Stalin Alexandar** — [@stalin-alexandar](https://github.com/stalin-alexandar)
