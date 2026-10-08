# 🎮 Tetris Game — C++

A complete, classic **Tetris** that runs right in your terminal — written from scratch in modern **C++17** with clean, object-oriented design. No external libraries: just the standard library, ANSI escape codes for colour rendering, and raw terminal input (POSIX `termios` on Linux/macOS, `conio` on Windows).

## ✨ Features

- 🧱 All **7 classic tetrominoes** (I, J, L, O, S, T, Z) with rotation and simple wall kicks
- 🎲 Fair **7-bag randomiser**, like modern Tetris games
- 👉 **Next-piece preview** in the side panel
- 🏆 **Score, level and lines** counters — classic scoring (100/300/500/800 × level)
- 🚀 **Increasing speed** — the game speeds up every 10 lines
- ⏸️ Pause, soft drop and hard drop
- 🎨 Colourful block rendering with a flicker-free single-write frame buffer
- 🧪 Built-in `--selftest` mode that plays 300 pieces against the game logic and verifies it

## 🎬 The board

```
╔════════════════════╗
║                    ║   NEXT:
║                    ║     ██
║        ██          ║     ████
║      ██████        ║
║                    ║   SCORE: 1250
║                    ║   LEVEL: 2
║  ██                ║   LINES: 11
║  ████      ██      ║
║  ██████  ██████    ║
║████████  ████████  ║
║██████████████████  ║
╚════════════════════╝
```

## 🕹️ Controls

| Key | Action |
|---|---|
| `←` / `A` | Move left |
| `→` / `D` | Move right |
| `↓` / `S` | Soft drop (+1 point per cell) |
| `↑` / `W` | Rotate |
| `Space` | Hard drop (+2 points per cell) |
| `P` | Pause / resume |
| `Q` | Quit |

## 🔧 Build & Run

**Linux / macOS:**

```bash
g++ -std=c++17 -O2 -o tetris tetris.cpp
./tetris
```

**Verify the game logic (no terminal needed):**

```bash
./tetris --selftest
# Tetris self-test
#   [OK]   fresh game is not over
#   ...
# SELFTEST: PASS
```

**Windows:**

- *MinGW:* `g++ -std=c++17 -O2 -o tetris.exe tetris.cpp` then run `tetris.exe`
- *Visual Studio:* create an empty C++ console project, add `tetris.cpp`, and build. The code enables ANSI colours on the Windows console automatically.

## 🧠 How it's put together

- `Game` class — pure Tetris logic (board, collision, line clears, scoring, levels, 7-bag RNG). It knows nothing about the terminal, which is what makes the self-test possible.
- `render()` — builds each frame as one string (board + falling piece + side panel) and writes it in a single flush.
- `readKey()` — normalises arrow keys / WASD / space across POSIX terminals and Windows.

---

Built by **Muhammad Abdul Rafay** — [github.com/mabdulrafay7zip](https://github.com/mabdulrafay7zip)
