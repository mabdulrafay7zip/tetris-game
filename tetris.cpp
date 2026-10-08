// ============================================================================
//  TETRIS — classic falling-block game for the terminal
//  ----------------------------------------------------------------------------
//  A complete, self-contained Tetris written in modern C++ (C++17).
//  Rendering is done with ANSI escape codes and coloured blocks; input is
//  read in raw terminal mode (POSIX termios on Linux/macOS, conio on
//  Windows). No external libraries are required.
//
//  Build (Linux/macOS):  g++ -std=c++17 -O2 -o tetris tetris.cpp
//  Build (Windows):      g++ -std=c++17 -O2 -o tetris.exe tetris.cpp   (MinGW)
//                        or open in Visual Studio as a console project.
//  Run:                  ./tetris
//  Self test:            ./tetris --selftest
//
//  Built by Muhammad Abdul Rafay — https://github.com/mabdulrafay7zip
// ============================================================================

#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
#else
    #include <sys/select.h>
    #include <termios.h>
    #include <unistd.h>
#endif

// ============================================================================
//  Terminal handling — raw mode so key presses are seen instantly, one by one
// ============================================================================
#ifndef _WIN32
namespace {

termios g_originalTermios;
bool    g_rawModeActive = false;

// Put the terminal into raw mode (no echo, no line buffering).
void enableRawMode() {
    tcgetattr(STDIN_FILENO, &g_originalTermios);
    termios raw = g_originalTermios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN]  = 0;   // read() returns immediately
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    g_rawModeActive = true;
}

// Restore the terminal to its previous state.
void disableRawMode() {
    if (g_rawModeActive) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_originalTermios);
        g_rawModeActive = false;
    }
}

// True if at least one byte of input is waiting.
bool inputReady() {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);
    timeval tv{0, 0};
    return select(STDIN_FILENO + 1, &set, nullptr, nullptr, &tv) > 0;
}

} // namespace
#endif // !_WIN32

// ============================================================================
//  Board constants and tetromino shapes
//  ----------------------------------------------------------------------------
//  Each tetromino is stored as a 4x4 grid; filled cells carry the piece id
//  (1..7), which doubles as its colour index. Rotation is a 90-degree turn
//  of the 4x4 grid around its centre, plus simple wall-kick offsets.
// ============================================================================
constexpr int BOARD_W = 10;
constexpr int BOARD_H = 20;

using Shape = std::array<std::array<int, 4>, 4>;

// Build a Shape from four strings of four characters ('#' = filled cell).
static Shape makeShape(int id, const char* r0, const char* r1,
                       const char* r2, const char* r3) {
    const char* rows[4] = {r0, r1, r2, r3};
    Shape s{};
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x)
            s[y][x] = (rows[y][x] == '#') ? id : 0;
    return s;
}

// The seven classic tetrominoes, centred inside their 4x4 boxes.
static const std::array<Shape, 7> SHAPES = {{
    makeShape(1, "....", "####", "....", "...."),  // I
    makeShape(2, "....", ".#..", ".###", "...."),  // J
    makeShape(3, "....", "...#", ".###", "...."),  // L
    makeShape(4, "....", ".##.", ".##.", "...."),  // O
    makeShape(5, "....", "..##", ".##.", "...."),  // S
    makeShape(6, "....", "..#.", ".###", "...."),  // T
    makeShape(7, "....", ".##.", "..##", "...."),  // Z
}};

// ANSI colour code per piece id (index 0 unused).
static const char* PIECE_COLORS[8] = {
    "", "\033[1;36m", "\033[1;34m", "\033[1;33m",
    "\033[1;93m", "\033[1;32m", "\033[1;35m", "\033[1;31m"
};
static const char* COLOR_RESET = "\033[0m";

// Rotate a shape 90 degrees clockwise inside its 4x4 box.
static Shape rotatedCW(const Shape& s) {
    Shape r{};
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x)
            r[x][3 - y] = s[y][x];
    return r;
}

// ============================================================================
//  Game — pure Tetris logic (board state, movement, scoring, levels).
//  It knows nothing about the terminal, which keeps it easy to test.
// ============================================================================
class Game {
public:
    explicit Game(unsigned seed = 2463534242u) : rng_(seed) {
        board_.fill({});
        refillBag();
        curId_  = drawFromBag();
        nextId_ = drawFromBag();
        spawn();
    }

    // ---- player actions ----------------------------------------------------
    bool moveLeft()  { return tryMove(-1, 0); }
    bool moveRight() { return tryMove( 1, 0); }

    // One soft-drop step; locks the piece if it cannot move down.
    bool softDrop() {
        if (tryMove(0, 1)) { score_ += 1; return true; }
        lockPiece();
        return false;
    }

    // Drop instantly to the bottom (+2 points per cell), then lock.
    void hardDrop() {
        int cells = 0;
        while (canPlace(curShape_, curX_, curY_ + 1)) { ++curY_; ++cells; }
        score_ += 2 * cells;
        lockPiece();
    }

    // Rotate clockwise, with simple wall kicks if the plain rotation fails.
    bool rotate() {
        Shape r = rotatedCW(curShape_);
        const int kicks[5] = {0, -1, 1, -2, 2};
        for (int k : kicks) {
            if (canPlace(r, curX_ + k, curY_)) {
                curShape_ = r;
                curX_ += k;
                return true;
            }
        }
        return false;
    }

    // One gravity step; locks the piece when it lands.
    void gravityStep() {
        if (!tryMove(0, 1)) lockPiece();
    }

    // ---- state accessors ---------------------------------------------------
    int  score()        const { return score_; }
    int  lines()        const { return lines_; }
    int  level()        const { return level_; }
    bool gameOver()     const { return gameOver_; }
    int  piecesLocked() const { return piecesLocked_; }
    int  cellAt(int x, int y) const { return board_[y][x]; }
    const Shape& currentShape() const { return curShape_; }
    int  currentX() const { return curX_; }
    int  currentY() const { return curY_; }
    int  currentId() const { return curId_; }
    int  nextId()    const { return nextId_; }

    // Milliseconds between gravity steps at the current level.
    int dropIntervalMs() const {
        int ms = 800 - (level_ - 1) * 70;
        return ms < 60 ? 60 : ms;
    }

    // ---- debug hooks (used by --selftest) ----------------------------------
    void debugSetCell(int x, int y, int v) { board_[y][x] = v; }
    int  debugClearLines() { return clearLines(); }

private:
    // True if shape `s` can sit at board offset (px, py) without colliding.
    bool canPlace(const Shape& s, int px, int py) const {
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x) {
                if (!s[y][x]) continue;
                int bx = px + x, by = py + y;
                if (bx < 0 || bx >= BOARD_W || by >= BOARD_H) return false;
                if (by >= 0 && board_[by][bx]) return false;
            }
        return true;
    }

    bool tryMove(int dx, int dy) {
        if (canPlace(curShape_, curX_ + dx, curY_ + dy)) {
            curX_ += dx;
            curY_ += dy;
            return true;
        }
        return false;
    }

    // Freeze the current piece into the board, score any cleared lines,
    // then bring in the next piece.
    void lockPiece() {
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x) {
                if (!curShape_[y][x]) continue;
                int bx = curX_ + x, by = curY_ + y;
                if (by < 0) { gameOver_ = true; return; }  // locked above sky
                board_[by][bx] = curShape_[y][x];
            }
        ++piecesLocked_;

        int cleared = clearLines();
        if (cleared > 0) {
            static const int POINTS[5] = {0, 100, 300, 500, 800};
            score_ += POINTS[cleared] * level_;
            lines_ += cleared;
            level_  = lines_ / 10 + 1;      // level up every 10 lines
        }
        spawn();
    }

    // Remove full rows, drop everything above them. Returns rows removed.
    int clearLines() {
        int cleared = 0;
        for (int y = BOARD_H - 1; y >= 0; --y) {
            bool full = true;
            for (int x = 0; x < BOARD_W; ++x)
                if (!board_[y][x]) { full = false; break; }
            if (!full) continue;
            ++cleared;
            for (int yy = y; yy > 0; --yy) board_[yy] = board_[yy - 1];
            board_[0].fill(0);
            ++y;  // re-check the same row index after the shift
        }
        return cleared;
    }

    // Place the queued piece at the top; promote the "next" piece.
    void spawn() {
        curId_    = nextId_;
        nextId_   = drawFromBag();
        curShape_ = SHAPES[curId_ - 1];
        curX_ = 3;
        curY_ = 0;
        if (!canPlace(curShape_, curX_, curY_)) gameOver_ = true;
    }

    // 7-bag randomiser: fair distribution, like modern Tetris games.
    void refillBag() {
        bag_ = {1, 2, 3, 4, 5, 6, 7};
        for (int i = 6; i > 0; --i) {
            std::uniform_int_distribution<int> d(0, i);
            std::swap(bag_[i], bag_[d(rng_)]);
        }
        bagPos_ = 0;
    }
    int drawFromBag() {
        if (bagPos_ >= 7) refillBag();
        return bag_[bagPos_++];
    }

    std::array<std::array<int, BOARD_W>, BOARD_H> board_{};
    Shape curShape_{};
    int   curId_ = 1, nextId_ = 1;
    int   curX_ = 3, curY_ = 0;
    int   score_ = 0, lines_ = 0, level_ = 1;
    int   piecesLocked_ = 0;
    bool  gameOver_ = false;
    std::mt19937 rng_;
    std::array<int, 7> bag_{};
    int   bagPos_ = 0;
};

// ============================================================================
//  Renderer — draws the board, the active piece, and the side panel.
//  The whole frame is built in one string and printed with a single write
//  after homing the cursor, which avoids flicker.
// ============================================================================
static std::string cellGlyph(int id) {
    if (id == 0) return "  ";
    return std::string(PIECE_COLORS[id]) + "██" + COLOR_RESET;
}

static void render(const Game& g, bool paused) {
    // Merge the falling piece into a view of the board for drawing.
    int view[BOARD_H][BOARD_W] = {};
    for (int y = 0; y < BOARD_H; ++y)
        for (int x = 0; x < BOARD_W; ++x)
            view[y][x] = g.cellAt(x, y);
    const Shape& s = g.currentShape();
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) {
            if (!s[y][x]) continue;
            int bx = g.currentX() + x, by = g.currentY() + y;
            if (bx >= 0 && bx < BOARD_W && by >= 0 && by < BOARD_H)
                view[by][bx] = s[y][x];
        }

    // Side panel lines, aligned with the board rows.
    std::vector<std::string> panel(BOARD_H, "");
    panel[1] = "  NEXT:";
    const Shape& ns = SHAPES[g.nextId() - 1];
    for (int y = 0; y < 4; ++y) {
        std::string row = "  ";
        for (int x = 0; x < 4; ++x) row += cellGlyph(ns[y][x]);
        panel[2 + y] = row;
    }
    panel[8]  = "  SCORE: " + std::to_string(g.score());
    panel[9]  = "  LEVEL: " + std::to_string(g.level());
    panel[10] = "  LINES: " + std::to_string(g.lines());
    panel[13] = "  CONTROLS";
    panel[14] = "  <-/-> or A/D : move";
    panel[15] = "  Up / W       : rotate";
    panel[16] = "  Down / S     : soft drop";
    panel[17] = "  SPACE        : hard drop";
    panel[18] = "  P pause      Q quit";

    std::string out = "\033[H";
    out += "  T E T R I S\r\n";
    out += "╔════════════════════╗\r\n";
    for (int y = 0; y < BOARD_H; ++y) {
        out += "║";
        for (int x = 0; x < BOARD_W; ++x) out += cellGlyph(view[y][x]);
        out += "║";
        out += panel[y];
        out += "\r\n";
    }
    out += "╚════════════════════╝\r\n";
    if (paused) out += "\r\n        *** PAUSED ***\r\n";
    std::cout << out << std::flush;
}

// Final screen shown when the stack reaches the top.
static void renderGameOver(const Game& g) {
    std::cout << "\033[2J\033[H"
              << "\r\n"
              << "   ╔══════════════════════╗\r\n"
              << "   ║      GAME  OVER      ║\r\n"
              << "   ╚══════════════════════╝\r\n\r\n"
              << "   Final score : " << g.score() << "\r\n"
              << "   Lines       : " << g.lines() << "\r\n"
              << "   Level       : " << g.level() << "\r\n"
              << "   Pieces      : " << g.piecesLocked() << "\r\n\r\n"
              << "   Thanks for playing!\r\n";
}

// ============================================================================
//  Input — normalised key codes across platforms
// ============================================================================
enum class Key { None, Left, Right, Down, Rotate, HardDrop, Pause, Quit };

static Key readKey() {
#ifdef _WIN32
    if (!_kbhit()) return Key::None;
    int c = _getch();
    if (c == 224 || c == 0) {                 // arrow keys send a prefix first
        switch (_getch()) {
            case 75: return Key::Left;
            case 77: return Key::Right;
            case 80: return Key::Down;
            case 72: return Key::Rotate;
            default: return Key::None;
        }
    }
    switch (c) {
        case ' ': return Key::HardDrop;
        case 'p': case 'P': return Key::Pause;
        case 'q': case 'Q': case 27: return Key::Quit;
        case 'a': case 'A': return Key::Left;
        case 'd': case 'D': return Key::Right;
        case 's': case 'S': return Key::Down;
        case 'w': case 'W': return Key::Rotate;
        default: return Key::None;
    }
#else
    if (!inputReady()) return Key::None;
    char c = 0;
    if (read(STDIN_FILENO, &c, 1) <= 0) return Key::None;
    if (c == '\033') {                        // possible escape sequence
        char seq[2] = {0, 0};
        if (!inputReady()) return Key::Quit;  // bare ESC key
        if (read(STDIN_FILENO, &seq[0], 1) <= 0) return Key::None;
        if (seq[0] != '[') return Key::None;
        if (read(STDIN_FILENO, &seq[1], 1) <= 0) return Key::None;
        switch (seq[1]) {
            case 'A': return Key::Rotate;
            case 'B': return Key::Down;
            case 'C': return Key::Right;
            case 'D': return Key::Left;
            default:  return Key::None;
        }
    }
    switch (c) {
        case ' ': return Key::HardDrop;
        case 'p': case 'P': return Key::Pause;
        case 'q': case 'Q': return Key::Quit;
        case 'a': case 'A': return Key::Left;
        case 'd': case 'D': return Key::Right;
        case 's': case 'S': return Key::Down;
        case 'w': case 'W': return Key::Rotate;
        default: return Key::None;
    }
#endif
}

// ============================================================================
//  Self test — exercises the game logic without a terminal: simulates drops
//  on an initially empty board, checks line clearing and scoring, and
//  reports PASS/FAIL per check. Run with: ./tetris --selftest
// ============================================================================
static int runSelfTest() {
    int failures = 0;
    auto check = [&failures](const char* name, bool ok) {
        std::cout << (ok ? "  [OK]   " : "  [FAIL] ") << name << '\n';
        if (!ok) ++failures;
    };

    std::cout << "Tetris self-test\n";

    // 1) A fresh game spawns a live piece and is not over.
    Game g(12345);
    check("fresh game is not over", !g.gameOver());
    check("fresh game starts at level 1 with score 0",
          g.level() == 1 && g.score() == 0);

    // 2) Rotation works at spawn on an empty board.
    check("piece rotates at spawn", g.rotate());

    // 3) Simulated play with a greedy one-piece lookahead: for every
    //    rotation and target column, drop the piece on a COPY of the game
    //    and keep the move with the best board rating — low aggregate
    //    height, few buried holes, a flat surface, and lines cleared.
    //    (Game is a plain value type, so copies are cheap and safe.)
    auto badness = [](const Game& gg, int baseLines) {
        int heights[BOARD_W] = {};
        int holes = 0;
        for (int x = 0; x < BOARD_W; ++x) {
            bool seenBlock = false;
            for (int y = 0; y < BOARD_H; ++y) {
                if (gg.cellAt(x, y)) {
                    if (!seenBlock) { heights[x] = BOARD_H - y; seenBlock = true; }
                } else if (seenBlock) {
                    ++holes;
                }
            }
        }
        int bad = 0;
        for (int x = 0; x < BOARD_W; ++x) {
            bad += heights[x];                       // aggregate height
            if (x + 1 < BOARD_W) {
                int d = heights[x] - heights[x + 1]; // bumpiness
                bad += 2 * (d < 0 ? -d : d);
            }
        }
        bad += 20 * holes;                           // holes are the worst
        bad -= 40 * (gg.lines() - baseLines);        // reward line clears
        return bad;
    };
    for (int i = 0; i < 300 && !g.gameOver(); ++i) {
        int bestVal = -1, bestRot = 0, bestX = g.currentX();
        for (int r = 0; r < 4; ++r) {
            for (int tx = -2; tx < BOARD_W; ++tx) {
                Game sim = g;
                int applied = 0;
                while (applied < r && sim.rotate()) ++applied;
                if (applied != r) continue;
                while (sim.currentX() > tx && sim.moveLeft())  {}
                while (sim.currentX() < tx && sim.moveRight()) {}
                if (sim.currentX() != tx) continue;  // unreachable column
                sim.hardDrop();
                int val = badness(sim, g.lines());
                if (bestVal < 0 || val < bestVal) {
                    bestVal = val; bestRot = r; bestX = tx;
                }
            }
        }
        for (int k = 0; k < bestRot; ++k) g.rotate();
        while (g.currentX() > bestX && g.moveLeft())  {}
        while (g.currentX() < bestX && g.moveRight()) {}
        g.hardDrop();
    }
    check("greedy simulation locks 300 pieces without topping out",
          g.piecesLocked() == 300 && !g.gameOver());
    check("greedy simulation clears lines", g.lines() > 0);
    check("score grows as pieces lock", g.score() > 0);

    // 4) Line clearing: fill the bottom two rows by hand, clear them.
    Game g2(777);
    for (int x = 0; x < BOARD_W; ++x) {
        g2.debugSetCell(x, BOARD_H - 1, 3);
        g2.debugSetCell(x, BOARD_H - 2, 5);
    }
    int cleared = g2.debugClearLines();
    check("two full rows are cleared", cleared == 2);
    check("board is empty after the clear",
          g2.cellAt(0, BOARD_H - 1) == 0 && g2.cellAt(9, BOARD_H - 2) == 0);

    // 5) A single dropped piece rests on the floor of an empty board.
    Game g3(99);
    g3.hardDrop();
    bool floorFilled = false;
    for (int x = 0; x < BOARD_W; ++x)
        if (g3.cellAt(x, BOARD_H - 1) != 0) floorFilled = true;
    check("hard drop lands a piece on the floor", floorFilled);

    if (failures == 0) {
        std::cout << "SELFTEST: PASS\n";
        return 0;
    }
    std::cout << "SELFTEST: FAIL (" << failures << " check(s) failed)\n";
    return 1;
}

// ============================================================================
//  main — game loop with a fixed input poll and level-based gravity
// ============================================================================
int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--selftest") == 0)
        return runSelfTest();

#ifdef _WIN32
    // Enable ANSI escape sequences on the Windows console.
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(hOut, &mode))
        SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#else
    enableRawMode();
#endif

    std::cout << "\033[2J\033[?25l";  // clear screen, hide cursor

    Game game(static_cast<unsigned>(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    bool paused  = false;
    bool running = true;
    auto lastFall = std::chrono::steady_clock::now();

    render(game, paused);
    while (running && !game.gameOver()) {
        Key key = readKey();
        switch (key) {
            case Key::Quit:  running = false; break;
            case Key::Pause: paused = !paused; break;
            case Key::Left:  if (!paused) game.moveLeft();  break;
            case Key::Right: if (!paused) game.moveRight(); break;
            case Key::Down:  if (!paused) game.softDrop();  break;
            case Key::Rotate: if (!paused) game.rotate();   break;
            case Key::HardDrop:
                if (!paused) {
                    game.hardDrop();
                    lastFall = std::chrono::steady_clock::now();
                }
                break;
            case Key::None: break;
        }

        // Gravity: one step every dropIntervalMs() at the current level.
        auto now = std::chrono::steady_clock::now();
        if (!paused &&
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now - lastFall).count() >= game.dropIntervalMs()) {
            game.gravityStep();
            lastFall = now;
        }

        render(game, paused);
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }

    std::cout << "\033[?25h";  // show cursor again
#ifndef _WIN32
    disableRawMode();
#endif
    if (game.gameOver()) renderGameOver(game);
    return 0;
}
