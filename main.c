#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <termios.h>
#include <fcntl.h>

#define WIDTH 25
#define HEIGHT 15
#define GHOST_COUNT 3

typedef struct {
    int x;
    int y;
    int startX;
    int startY;
} Entity;

char map[HEIGHT][WIDTH + 1] = {
    "#########################",
    "#...........#...........#",
    "#.###.#####.#.#####.###.#",
    "#.#.......#...#.......#.#",
    "#.#.###.#.#####.#.###.#.#",
    "#.....#.#...#...#.#.....#",
    "#####.#.### # ###.#.#####",
    "#.........# # #.........#",
    "#.###.###.# # #.###.###.#",
    "#...#.....#   #.....#...#",
    "###.#.###.#####.###.#.###",
    "#.....#...........#.....#",
    "#.#####.###.#.###.#####.#",
    "#...........#...........#",
    "#########################"
};

Entity player = { 1, 1, 1, 1 };

Entity ghosts[GHOST_COUNT] = {
    { 11, 7, 11, 7 },
    { 13, 7, 13, 7 },
    { 12, 9, 12, 9 }
};

/* ── Terminal raw mode ── */
static struct termios orig_termios;

void disableRawMode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int kbhit(void) {
    char c;
    return (int)(read(STDIN_FILENO, &c, 1) == 1 ? (ungetc(c, stdin), 1) : 0);
}

char getch(void) {
    char c = 0;
    read(STDIN_FILENO, &c, 1);
    return c;
}

/* ── Cursor helpers ── */
void gotoxy(int x, int y) {
    printf("\033[%d;%dH", y + 1, x + 1);
    fflush(stdout);
}

void hideCursor(void) {
    printf("\033[?25l");
    fflush(stdout);
}

void showCursor(void) {
    printf("\033[?25h");
    fflush(stdout);
}

/* ── Color helpers (ANSI) ── */
void setColor(int color) {
    switch (color) {
        case 9:  printf("\033[34;1m"); break; /* blue  – wall   */
        case 14: printf("\033[33;1m"); break; /* yellow– dot    */
        case 10: printf("\033[32;1m"); break; /* green – player */
        case 12: printf("\033[31;1m"); break; /* red   – ghost  */
        default: printf("\033[0m");    break; /* reset          */
    }
}

/* ── Game logic (unchanged) ── */
int countPellets(void) {
    int count = 0;
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            if (map[y][x] == '.') count++;
    return count;
}

int isWall(int x, int y) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return 1;
    return map[y][x] == '#';
}

int isGhostPosition(int x, int y) {
    for (int i = 0; i < GHOST_COUNT; i++)
        if (ghosts[i].x == x && ghosts[i].y == y) return 1;
    return 0;
}

int checkCollision(void) {
    for (int i = 0; i < GHOST_COUNT; i++)
        if (player.x == ghosts[i].x && player.y == ghosts[i].y) return 1;
    return 0;
}

void resetPositions(void) {
    player.x = player.startX;
    player.y = player.startY;
    for (int i = 0; i < GHOST_COUNT; i++) {
        ghosts[i].x = ghosts[i].startX;
        ghosts[i].y = ghosts[i].startY;
    }
}

void drawGame(int score, int lives, int pellets) {
    gotoxy(0, 0);
    setColor(7);
    printf("PAC-MAN STYLE GAME\n");
    printf("Score: %d   Lives: %d   Pellets left: %d\n", score, lives, pellets);
    printf("Move: W A S D     Quit: Q\n\n");

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (player.x == x && player.y == y) {
                setColor(10); printf("P");
            } else if (isGhostPosition(x, y)) {
                setColor(12); printf("G");
            } else {
                char tile = map[y][x];
                if      (tile == '#') setColor(9);
                else if (tile == '.') setColor(14);
                else                  setColor(7);
                printf("%c", tile);
            }
        }
        printf("\n");
    }
    setColor(7);
    fflush(stdout);
}

void movePlayer(char input, int *score, int *pellets) {
    int newX = player.x, newY = player.y;
    input = (char)tolower((unsigned char)input);
    if      (input == 'w') newY--;
    else if (input == 's') newY++;
    else if (input == 'a') newX--;
    else if (input == 'd') newX++;

    if (!isWall(newX, newY)) {
        player.x = newX;
        player.y = newY;
        if (map[player.y][player.x] == '.') {
            map[player.y][player.x] = ' ';
            *score += 10;
            (*pellets)--;
        }
    }
}

void moveGhosts(void) {
    int dx[4] = { 0, 0, -1, 1 };
    int dy[4] = { -1, 1,  0, 0 };
    for (int i = 0; i < GHOST_COUNT; i++) {
        for (int t = 0; t < 10; t++) {
            int dir  = rand() % 4;
            int newX = ghosts[i].x + dx[dir];
            int newY = ghosts[i].y + dy[dir];
            if (!isWall(newX, newY)) {
                ghosts[i].x = newX;
                ghosts[i].y = newY;
                break;
            }
        }
    }
}

int main(void) {
    int score = 0, lives = 3, pellets, running = 1;

    srand((unsigned int)time(NULL));
    enableRawMode();
    system("clear");
    hideCursor();

    pellets = countPellets();
    if (map[player.y][player.x] == '.') { map[player.y][player.x] = ' '; pellets--; }

    while (running && lives > 0 && pellets > 0) {
        drawGame(score, lives, pellets);

        char c = 0;
        if (read(STDIN_FILENO, &c, 1) == 1) {
            if (c == 'q' || c == 'Q') { running = 0; break; }
            movePlayer(c, &score, &pellets);
        }

        if (checkCollision()) { lives--; resetPositions(); usleep(700000); continue; }
        moveGhosts();
        if (checkCollision()) { lives--; resetPositions(); usleep(700000); continue; }

        usleep(130000);
    }

    drawGame(score, lives, pellets);
    gotoxy(0, HEIGHT + 5);

    if      (pellets == 0) { setColor(10); printf("YOU WIN! Final Score: %d\n", score); }
    else if (lives   <= 0) { setColor(12); printf("GAME OVER! Final Score: %d\n", score); }
    else                   { setColor(7);  printf("Game ended. Final Score: %d\n", score); }

    setColor(7);
    printf("Press any key to exit...");
    fflush(stdout);
    { char tmp; read(STDIN_FILENO, &tmp, 1); }

    showCursor();
    return 0;
}