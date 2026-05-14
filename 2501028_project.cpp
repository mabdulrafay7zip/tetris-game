// PROJECT INFORMATION
// Name: Muhammad Abdul Rafay
// Student ID: 2501028
// PF PROJECT

// Library Inclusion Section
#include <SFML/Graphics.hpp>
#include <time.h>
#include <string>
#include <fstream>

using namespace sf;
using namespace std;

// Global Constants And Variables

const int M = 20;
const int N = 10;
int field[M][N] = { 0 };

// Structure of Block
struct Point { int x, y; } a[4], b[4], shadow[4], nextP[4];

// Tetromino shapes data
int figures[7][4] = {
	1,3,5,7, 2,4,5,7, 3,5,4,6, 3,5,4,7, 2,3,5,7, 3,5,7,6, 2,3,4,5
};
// Game States
enum GameState { MENU, PLAYING, PAUSED, HELP, HIGHSCORE, GAME_OVER };
GameState state = MENU;

// Global high scores array 
int globalHighScores[10] = { 0 };

// Load scores using Array 
void loadScores(int scores[10]) {
	for (int i = 0; i < 10; i++) scores[i] = 0;
	ifstream f("highscores.txt");
	if (f.is_open()) {
		int val, count = 0;
		while (f >> val && count < 10) {
			scores[count++] = val;
		}
		f.close();
	}
}

// Save score using Manual Sorting 
void saveScore(int s) {
	int scores[11]; // 10 existing + 1 new
	int existingScores[10];
	loadScores(existingScores);

	for (int i = 0; i < 10; i++) scores[i] = existingScores[i];
	scores[10] = s;

	// Manual Bubble Sort (Descending order)
	for (int i = 0; i < 11; i++) {
		for (int j = i + 1; j < 11; j++) {
			if (scores[j] > scores[i]) {
				int temp = scores[i];
				scores[i] = scores[j];
				scores[j] = temp;
			}
		}
	}

	// File Handling (High Score)
	ofstream f("highscores.txt");
	for (int i = 0; i < 10; i++) f << scores[i] << endl;
	f.close();
}

// Collision Check Function 
bool check(Point p[4]) {
	for (int i = 0; i < 4; i++)
		if (p[i].x < 0 || p[i].x >= N || p[i].y >= M) return 0;
		else if (field[p[i].y][p[i].x]) return 0;
	return 1;
};

// Main Function Start 
int main() {
	srand(time(0));

	// Load high scores immediately at startup 
	loadScores(globalHighScores);

	// Windows and Graphics
	RenderWindow window(VideoMode(560, 520), "Tetris Game - PF Project");

	Texture t1, t2, t3;
	t1.loadFromFile("tiles.png");
	t2.loadFromFile("background.png");
	t3.loadFromFile("frame.png");

	Sprite s(t1), background(t2), frame(t3);

	float scaleX = (float)window.getSize().x / t2.getSize().x;
	float scaleY = (float)window.getSize().y / t2.getSize().y;
	background.setScale(scaleX, scaleY);

	Font font;
	font.loadFromFile("monogram.ttf");

	// Menu And UI Text Setup
	string menuStrings[] = { "1. Start a new game", "2. See high scores", "3. Help", "4. Exit", "5. Continue" };
	Text menuTexts[5];
	for (int i = 0; i < 5; i++) {
		menuTexts[i].setFont(font);
		menuTexts[i].setString(menuStrings[i]);
		menuTexts[i].setCharacterSize(45);
		menuTexts[i].setPosition(140, 180 + (i * 50));
	}

	RectangleShape selector(Vector2f(10, 10));
	selector.setFillColor(Color::Cyan);

	Text pauseBtn("[ PAUSE ]", font, 30);
	FloatRect pb = pauseBtn.getLocalBounds();
	pauseBtn.setOrigin(pb.left + pb.width / 2.0f, pb.top + pb.height / 2.0f);
	pauseBtn.setPosition(455, 35);
	pauseBtn.setFillColor(Color::White);

	int dx = 0; bool rotate = 0; int colorNum = 1, nextColor = 1;
	float timer = 0, delay = 0.3;
	int score = 0, level = 1;

	int n = rand() % 4, nextN = rand() % 4;
	Clock clock, struggleTimer;

	int blockedRowsCount = 0;
	int checkInterval = 300;
	float speedFactor = 1.0f;
	// Spawn Function
	auto spawn = [&]() {
		n = nextN; colorNum = nextColor;
		for (int i = 0; i < 4; i++) {
			a[i].x = figures[n][i] % 2 + N / 2 - 1;
			a[i].y = figures[n][i] / 2;
		}

		nextN = (level == 1) ? rand() % 4 : rand() % 7;
		nextColor = 1 + rand() % 7;
		for (int i = 0; i < 4; i++) {
			nextP[i].x = figures[nextN][i] % 2;
			nextP[i].y = figures[nextN][i] / 2;
		}
		};

	spawn();

	while (window.isOpen()) {
		float time = clock.getElapsedTime().asSeconds();
		clock.restart();
		Vector2f mPos = window.mapPixelToCoords(Mouse::getPosition(window));


		if (pauseBtn.getGlobalBounds().contains(mPos)) {
			pauseBtn.setFillColor(Color::Yellow);
			pauseBtn.setScale(1.15f, 1.15f);
		}
		else {
			pauseBtn.setFillColor(Color::White);
			pauseBtn.setScale(1.0f, 1.0f);
		}

		Event e;
		while (window.pollEvent(e)) {
			if (e.type == Event::Closed) window.close();
			// Game Logic Section
			if (e.type == Event::MouseButtonPressed && e.mouseButton.button == Mouse::Left) {
				if (state == PLAYING && pauseBtn.getGlobalBounds().contains(mPos)) state = PAUSED;
				if (state == MENU || state == PAUSED) {
					for (int i = 0; i < 5; i++) {
						if (menuTexts[i].getGlobalBounds().contains(mPos)) {
							if (i == 0) {
								score = 0; level = 1; state = PLAYING;
								struggleTimer.restart(); blockedRowsCount = 0; speedFactor = 1.0f;
								for (int r = 0; r < M; r++)for (int c = 0; c < N; c++)field[r][c] = 0; spawn();
							}
							else if (i == 1) state = HIGHSCORE;
							else if (i == 2) state = HELP;
							else if (i == 3) window.close();
							else if (i == 4 && state == PAUSED) state = PLAYING;
						}
					}
				}
			}

			if (e.type == Event::KeyPressed) {
				if (e.key.code == Keyboard::P && state == PLAYING) state = PAUSED;
				if (e.key.code == Keyboard::Escape) state = MENU;

				if (state == MENU || state == PAUSED) {
					if (e.key.code == Keyboard::Num1) {
						score = 0; level = 1; state = PLAYING;
						struggleTimer.restart(); blockedRowsCount = 0; speedFactor = 1.0f;
						for (int r = 0; r < M; r++)for (int c = 0; c < N; c++)field[r][c] = 0; spawn();
					}
					else if (e.key.code == Keyboard::Num2) state = HIGHSCORE;
					else if (e.key.code == Keyboard::Num3) state = HELP;
					else if (e.key.code == Keyboard::Num4) window.close();
					else if (e.key.code == Keyboard::Num5 && state == PAUSED) state = PLAYING;
				}

				if (state == PLAYING) {
					if (e.key.code == Keyboard::Up) rotate = true;
					if (e.key.code == Keyboard::Left) dx = -1;
					if (e.key.code == Keyboard::Right) dx = 1;
				}
			}
		}

		if (state == PLAYING) {
			timer += time;
			if (Keyboard::isKeyPressed(Keyboard::Down)) delay = 0.05;

			if (struggleTimer.getElapsedTime().asSeconds() >= checkInterval) {
				struggleTimer.restart();
				speedFactor *= 0.9f;
				if (blockedRowsCount < M - 4) {
					for (int j = 0; j < N; j++) field[M - 1 - blockedRowsCount][j] = 8;
					blockedRowsCount++;
				}
			}

			if (score >= 100 && level == 1) level = 2;

			for (int i = 0; i < 4; i++) { b[i] = a[i]; a[i].x += dx; }
			if (!check(a)) for (int i = 0; i < 4; i++) a[i] = b[i];

			if (rotate) {
				Point p = a[1];
				for (int i = 0; i < 4; i++) {
					int x = a[i].y - p.y; int y = a[i].x - p.x;
					a[i].x = p.x - x; a[i].y = p.y + y;
				}
				if (!check(a)) for (int i = 0; i < 4; i++) a[i] = b[i];
			}

			if (timer > (delay * speedFactor)) {
				for (int i = 0; i < 4; i++) { b[i] = a[i]; a[i].y += 1; }
				if (!check(a)) {
					for (int i = 0; i < 4; i++) field[b[i].y][b[i].x] = colorNum;
					spawn();
					if (!check(a)) { state = GAME_OVER; saveScore(score); }
				}
				timer = 0;
			}

			int lines = 0;
			for (int i = M - 1 - blockedRowsCount; i > 0; i--) {
				int cnt = 0;
				for (int j = 0; j < N; j++) if (field[i][j]) cnt++;
				if (cnt == N) {
					lines++;
					for (int k = i; k > 0; k--) for (int j = 0; j < N; j++) field[k][j] = field[k - 1][j];
					i++;
				}
			}
			if (lines == 1) score += 10 * level;
			else if (lines == 2) score += 30 * level;
			else if (lines == 3) score += 60 * level;
			else if (lines >= 4) score += 100 * level;
			// Shadow Logic
			for (int i = 0; i < 4; i++) shadow[i] = a[i];
			while (check(shadow)) { for (int i = 0; i < 4; i++) shadow[i].y++; }
			for (int i = 0; i < 4; i++) shadow[i].y--;

			dx = 0; rotate = 0;
			delay = (level == 1) ? 0.3 : 0.2;
		}

		window.clear();
		window.draw(background);

		if (state == MENU || state == PAUSED) {
			Text title("TETRIS", font, 100);
			title.setPosition(140, 30); title.setFillColor(Color::Yellow);
			title.setOutlineThickness(2); title.setOutlineColor(Color::Red);
			window.draw(title);

			Text head(state == MENU ? "BATTLE MODE" : "PAUSED", font, 40);
			head.setPosition(180, 130); head.setFillColor(Color::Cyan);
			window.draw(head);

			for (int i = 0; i < 5; i++) {
				if (i == 4 && state != PAUSED) continue;
				if (menuTexts[i].getGlobalBounds().contains(mPos)) {
					menuTexts[i].setFillColor(Color::Cyan);
					menuTexts[i].setCharacterSize(48);
					selector.setPosition(menuTexts[i].getPosition().x - 25, menuTexts[i].getPosition().y + 15);
					window.draw(selector);
				}
				else {
					menuTexts[i].setFillColor(Color::White);
					menuTexts[i].setCharacterSize(45);
				}
				window.draw(menuTexts[i]);
			}
		}
		else if (state == HIGHSCORE) {
			Text head("HIGH SCORES", font, 50); head.setPosition(150, 40); head.setFillColor(Color::Yellow); window.draw(head);
			loadScores(globalHighScores);
			string list = "";
			for (int i = 0; i < 10; i++) list += to_string(i + 1) + ". " + to_string(globalHighScores[i]) + "\n";
			Text t(list, font, 30); t.setPosition(180, 110); t.setFillColor(Color::White); window.draw(t);
		}
		else if (state == HELP) {
			Text head("GAME HELP & RULES", font, 45);
			head.setPosition(120, 30); head.setFillColor(Color::Yellow); window.draw(head);
			Text rules("RULES:\n1. Score 100 to reach ADVANCED level.\n2. Every 5 mins, the bottom row FREEZES.\n3. Every 5 mins, game speed increases by 10%.\n4. Complete rows to clear them and gain score.", font, 24);
			rules.setPosition(50, 90); rules.setFillColor(Color::Cyan); window.draw(rules);
			Text keys("CONTROLS:\n- LEFT/RIGHT Arrows: Move Block\n- UP Arrow: Rotate Block\n- DOWN Arrow: Fast Drop\n- P Key: Pause Game\n- 1 to 5 keys: Menu Selection\n- ESC Key: Back to Menu", font, 24);
			keys.setPosition(50, 250); keys.setFillColor(Color::Green); window.draw(keys);
			Text back("Press ESC to return", font, 30);
			back.setPosition(160, 450); back.setFillColor(Color::White); window.draw(back);
		}
		else {
			for (int i = 0; i < M; i++)
				for (int j = 0; j < N; j++) {
					if (field[i][j] == 0) continue;
					s.setTextureRect(IntRect(field[i][j] * 18, 0, 18, 18));
					s.setPosition(j * 18, i * 18); s.move(28, 31); window.draw(s);
				}
			s.setColor(Color(255, 255, 255, 70));
			for (int i = 0; i < 4; i++) {
				s.setTextureRect(IntRect(colorNum * 18, 0, 18, 18));
				s.setPosition(shadow[i].x * 18, shadow[i].y * 18); s.move(28, 31); window.draw(s);
			}
			s.setColor(Color::White);
			for (int i = 0; i < 4; i++) {
				s.setTextureRect(IntRect(colorNum * 18, 0, 18, 18));
				s.setPosition(a[i].x * 18, a[i].y * 18); s.move(28, 31); window.draw(s);
			}

			window.draw(pauseBtn);
			int timeLeft = checkInterval - (int)struggleTimer.getElapsedTime().asSeconds();
			Text t_clock("TIME: " + to_string(timeLeft) + "s", font, 40);
			t_clock.setPosition(405, 60); t_clock.setFillColor(Color::Red); window.draw(t_clock);
			Text t_score("SCORE: " + to_string(score), font, 40);
			t_score.setPosition(410, 100); t_score.setFillColor(Color::Green); window.draw(t_score);
			Text t_level("LEVEL: " + to_string(level), font, 40);
			t_level.setPosition(410, 140); t_level.setFillColor(Color::Yellow); window.draw(t_level);
			string levelName = (level == 1) ? "BEGINNER" : "ADVANCED";
			Text t_lvlName(levelName, font, 40);
			t_lvlName.setPosition(410, 175); t_lvlName.setFillColor(Color::Cyan); window.draw(t_lvlName);

			Text nextLabel("NEXT:", font, 40);
			nextLabel.setPosition(410, 220); nextLabel.setFillColor(Color::Magenta); window.draw(nextLabel);
			for (int i = 0; i < 4; i++) {
				s.setTextureRect(IntRect(nextColor * 18, 0, 18, 18));
				s.setPosition(nextP[i].x * 18 + 410, nextP[i].y * 18 + 270); window.draw(s);
			}
			// Game Over Handling
			window.draw(frame);
			if (state == GAME_OVER) { Text go("GAME OVER!", font, 70); go.setPosition(140, 200); go.setFillColor(Color::Red); window.draw(go); }
		}
		window.display();
	}
	return 0;
}