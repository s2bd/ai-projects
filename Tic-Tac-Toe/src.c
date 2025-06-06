// 1) Install dependencies: sudo apt install build-essential libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev libsdl2-gfx-dev
// 2) Compilation: gcc -o game src.c -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_gfx -lSDL2_mixer -lm
// 3) Run: ./game

// src.c

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

/* Window size */
const int WINDOW_WIDTH = 600;
const int WINDOW_HEIGHT = 600;

/* Board cell size */
const int CELL_SIZE = 180;
const int BOARD_PADDING = 30;

/* Colors */
SDL_Color COLOR_BG = {230, 230, 230, 255};
SDL_Color COLOR_GRID = {0, 0, 0, 255};
SDL_Color COLOR_X = {200, 30, 30, 255};
SDL_Color COLOR_O = {30, 30, 200, 255};
SDL_Color COLOR_BUTTON = {50, 50, 50, 255};
SDL_Color COLOR_BUTTON_HOVER = {80, 80, 80, 255};
SDL_Color COLOR_TEXT = {220, 220, 220, 255};

/* Enums for game state */
typedef enum {
    STATE_MENU,
    STATE_GAME,
    STATE_EXPLANATION,
    STATE_EXIT
} AppState;

typedef enum {
    PLAYER_NONE,
    PLAYER_HUMAN,
    PLAYER_AI
} Player;

typedef enum {
    DIFF_EASY,
    DIFF_HARD
} Difficulty;

/* Button structure */
typedef struct {
    SDL_Rect rect;
    const char* text;
    bool hovered;
} Button;

/* Globals */
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TTF_Font* font = NULL;

AppState currentState = STATE_MENU;
Difficulty currentDifficulty = DIFF_EASY;

char board[3][3]; // '_' = empty, 'X' human, 'O' AI
Player currentTurn = PLAYER_HUMAN;

Button buttons[5];
int buttonCount = 0;

/* Forward declarations */
void drawText(const char* text, int x, int y, SDL_Color color);
void drawCenteredText(const char* text, int cx, int cy, SDL_Color color);
void drawBoard();
void drawX(int row, int col);
void drawO(int row, int col);
void resetBoard();
bool isMovesLeft();
int evaluate();
int minimax(int depth, bool isHumanTurn, int alpha, int beta);
void findBestMove(int* bestRow, int* bestCol);
void aiMakeMove();
void easyAIMove();
bool checkWin(Player player);
void handleMenuEvents(SDL_Event* e);
void handleGameEvents(SDL_Event* e);
void handleExplanationEvents(SDL_Event* e);
void renderMenu();
void renderGame();
void renderExplanation();
void initButtonsMenu();
void initButtonsGame();
void initButtonsExplanation();
bool pointInRect(int x, int y, SDL_Rect* r);

/* --- Implementation --- */

void drawText(const char* text, int x, int y, SDL_Color color) {
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) {
        printf("TTF Render error: %s\n", TTF_GetError());
        return;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect dst = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void drawCenteredText(const char* text, int cx, int cy, SDL_Color color) {
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect dst = {cx - surface->w/2, cy - surface->h/2, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void drawBoard() {
    // Draw grid lines
    SDL_SetRenderDrawColor(renderer, COLOR_GRID.r, COLOR_GRID.g, COLOR_GRID.b, 255);
    int startX = BOARD_PADDING;
    int startY = BOARD_PADDING;
    int endX = startX + CELL_SIZE * 3;
    int endY = startY + CELL_SIZE * 3;

    // Vertical lines
    for (int i = 1; i <= 2; i++) {
        int x = startX + i * CELL_SIZE;
        SDL_RenderDrawLine(renderer, x, startY, x, endY);
    }
    // Horizontal lines
    for (int i = 1; i <= 2; i++) {
        int y = startY + i * CELL_SIZE;
        SDL_RenderDrawLine(renderer, startX, y, endX, y);
    }

    // Draw X and O
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            if (board[r][c] == 'X') drawX(r, c);
            else if (board[r][c] == 'O') drawO(r, c);
        }
    }
}

void drawX(int row, int col) {
    int startX = BOARD_PADDING + col * CELL_SIZE + 20;
    int startY = BOARD_PADDING + row * CELL_SIZE + 20;
    int endX = BOARD_PADDING + (col + 1) * CELL_SIZE - 20;
    int endY = BOARD_PADDING + (row + 1) * CELL_SIZE - 20;

    SDL_SetRenderDrawColor(renderer, COLOR_X.r, COLOR_X.g, COLOR_X.b, 255);
    SDL_RenderDrawLine(renderer, startX, startY, endX, endY);
    SDL_RenderDrawLine(renderer, startX, endY, endX, startY);
}

void drawO(int row, int col) {
    int cx = BOARD_PADDING + col * CELL_SIZE + CELL_SIZE / 2;
    int cy = BOARD_PADDING + row * CELL_SIZE + CELL_SIZE / 2;
    int radius = CELL_SIZE/2 - 20;

    SDL_SetRenderDrawColor(renderer, COLOR_O.r, COLOR_O.g, COLOR_O.b, 255);
    // Draw circle using midpoint circle algorithm approximation with 36 lines
    const int points = 36;
    for (int i = 0; i < points; i++) {
        double angle1 = 2.0 * M_PI * i / points;
        double angle2 = 2.0 * M_PI * (i + 1) / points;
        int x1 = cx + (int)(radius * cos(angle1));
        int y1 = cy + (int)(radius * sin(angle1));
        int x2 = cx + (int)(radius * cos(angle2));
        int y2 = cy + (int)(radius * sin(angle2));
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
    }
}

/* Initialize the board with empty cells */
void resetBoard() {
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            board[i][j] = '_';
    currentTurn = PLAYER_HUMAN;
}

/* Check if any moves left */
bool isMovesLeft() {
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            if (board[i][j] == '_')
                return true;
    return false;
}

/* Evaluate board for win */
int evaluate() {
    for (int r = 0; r < 3; r++) {
        if (board[r][0] == board[r][1] && board[r][1] == board[r][2]) {
            if (board[r][0] == 'X') return 10;
            else if (board[r][0] == 'O') return -10;
        }
    }
    for (int c = 0; c < 3; c++) {
        if (board[0][c] == board[1][c] && board[1][c] == board[2][c]) {
            if (board[0][c] == 'X') return 10;
            else if (board[0][c] == 'O') return -10;
        }
    }
    if (board[0][0] == board[1][1] && board[1][1] == board[2][2]) {
        if (board[0][0] == 'X') return 10;
        else if (board[0][0] == 'O') return -10;
    }
    if (board[0][2] == board[1][1] && board[1][1] == board[2][0]) {
        if (board[0][2] == 'X') return 10;
        else if (board[0][2] == 'O') return -10;
    }
    return 0;
}

/* Minimax AI with alpha-beta pruning */
int minimax(int depth, bool isHumanTurn, int alpha, int beta) {
    int score = evaluate();

    if (score == 10) return score - depth;   // Human wins
    if (score == -10) return score + depth;  // AI wins
    if (!isMovesLeft()) return 0;             // Draw

    if (isHumanTurn) {
        int best = -1000;
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                if (board[i][j] == '_') {
                    board[i][j] = 'X';
                    int val = minimax(depth+1, false, alpha, beta);
                    board[i][j] = '_';
                    if (val > best) best = val;
                    if (best > alpha) alpha = best;
                    if (beta <= alpha) break;
                }
            }
        }
        return best;
    } else {
        int best = 1000;
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                if (board[i][j] == '_') {
                    board[i][j] = 'O';
                    int val = minimax(depth+1, true, alpha, beta);
                    board[i][j] = '_';
                    if (val < best) best = val;
                    if (best < beta) beta = best;
                    if (beta <= alpha) break;
                }
            }
        }
        return best;
    }
}

/* Find best move for AI */
void findBestMove(int* bestRow, int* bestCol) {
    int bestVal = 1000;
    *bestRow = -1;
    *bestCol = -1;

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (board[i][j] == '_') {
                board[i][j] = 'O';
                int moveVal = minimax(0, true, -1000, 1000);
                board[i][j] = '_';
                if (moveVal < bestVal) {
                    bestVal = moveVal;
                    *bestRow = i;
                    *bestCol = j;
                }
            }
        }
    }
}

/* AI move for hard mode */
void aiMakeMove() {
    int r, c;
    findBestMove(&r, &c);
    if (r != -1 && c != -1) {
        board[r][c] = 'O';
        currentTurn = PLAYER_HUMAN;
    }
}

/* AI move for easy mode (random) */
void easyAIMove() {
    int emptyCells[9][2];
    int count = 0;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            if (board[i][j] == '_') {
                emptyCells[count][0] = i;
                emptyCells[count][1] = j;
                count++;
            }
    if (count == 0) return;
    int choice = rand() % count;
    board[emptyCells[choice][0]][emptyCells[choice][1]] = 'O';
    currentTurn = PLAYER_HUMAN;
}

/* Check if player wins */
bool checkWin(Player player) {
    char c = (player == PLAYER_HUMAN) ? 'X' : 'O';

    for (int i = 0; i < 3; i++) {
        if (board[i][0] == c && board[i][1] == c && board[i][2] == c) return true;
        if (board[0][i] == c && board[1][i] == c && board[2][i] == c) return true;
    }
    if (board[0][0] == c && board[1][1] == c && board[2][2] == c) return true;
    if (board[0][2] == c && board[1][1] == c && board[2][0] == c) return true;
    return false;
}

/* Button utility */
bool pointInRect(int x, int y, SDL_Rect* r) {
    return (x >= r->x && x <= r->x + r->w && y >= r->y && y <= r->y + r->h);
}

/* Initialize buttons for main menu */
void initButtonsMenu() {
    buttonCount = 3;
    buttons[0].rect = (SDL_Rect){WINDOW_WIDTH/2 - 100, 200, 200, 60};
    buttons[0].text = "Start Easy";
    buttons[0].hovered = false;

    buttons[1].rect = (SDL_Rect){WINDOW_WIDTH/2 - 100, 280, 200, 60};
    buttons[1].text = "Start Hard";
    buttons[1].hovered = false;

    buttons[2].rect = (SDL_Rect){WINDOW_WIDTH/2 - 100, 360, 200, 60};
    buttons[2].text = "How AI Works";
    buttons[2].hovered = false;
}

/* Initialize buttons for game screen */
void initButtonsGame() {
    buttonCount = 1;
    buttons[0].rect = (SDL_Rect){WINDOW_WIDTH - 150, 20, 130, 50};
    buttons[0].text = "Back to Menu";
    buttons[0].hovered = false;
}

/* Initialize buttons for explanation screen */
void initButtonsExplanation() {
    buttonCount = 1;
    buttons[0].rect = (SDL_Rect){WINDOW_WIDTH - 150, 20, 130, 50};
    buttons[0].text = "Back to Menu";
    buttons[0].hovered = false;
}

/* Handle main menu events */
void handleMenuEvents(SDL_Event* e) {
    if (e->type == SDL_QUIT) currentState = STATE_EXIT;
    else if (e->type == SDL_MOUSEMOTION) {
        int mx = e->motion.x, my = e->motion.y;
        for (int i = 0; i < buttonCount; i++) {
            buttons[i].hovered = pointInRect(mx, my, &buttons[i].rect);
        }
    } else if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        int mx = e->button.x, my = e->button.y;
        for (int i = 0; i < buttonCount; i++) {
            if (pointInRect(mx, my, &buttons[i].rect)) {
                if (i == 0) {
                    // Start Easy
                    currentDifficulty = DIFF_EASY;
                    resetBoard();
                    currentState = STATE_GAME;
                    initButtonsGame();
                } else if (i == 1) {
                    // Start Hard
                    currentDifficulty = DIFF_HARD;
                    resetBoard();
                    currentState = STATE_GAME;
                    initButtonsGame();
                } else if (i == 2) {
                    // Explanation screen
                    currentState = STATE_EXPLANATION;
                    initButtonsExplanation();
                }
            }
        }
    }
}

/* Handle game events */
void handleGameEvents(SDL_Event* e) {
    if (e->type == SDL_QUIT) currentState = STATE_EXIT;
    else if (e->type == SDL_MOUSEMOTION) {
        int mx = e->motion.x, my = e->motion.y;
        for (int i = 0; i < buttonCount; i++) {
            buttons[i].hovered = pointInRect(mx, my, &buttons[i].rect);
        }
    } else if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        int mx = e->button.x, my = e->button.y;

        // Check buttons
        for (int i = 0; i < buttonCount; i++) {
            if (pointInRect(mx, my, &buttons[i].rect)) {
                // Back to menu
                currentState = STATE_MENU;
                initButtonsMenu();
                return;
            }
        }

        // If human turn, try placing move on board
        if (currentTurn == PLAYER_HUMAN) {
            int c = (mx - BOARD_PADDING) / CELL_SIZE;
            int r = (my - BOARD_PADDING) / CELL_SIZE;
            if (r >= 0 && r < 3 && c >= 0 && c < 3) {
                if (board[r][c] == '_') {
                    board[r][c] = 'X';
                    currentTurn = PLAYER_AI;
                }
            }
        }
    }
}

/* Handle explanation events */
void handleExplanationEvents(SDL_Event* e) {
    if (e->type == SDL_QUIT) currentState = STATE_EXIT;
    else if (e->type == SDL_MOUSEMOTION) {
        int mx = e->motion.x, my = e->motion.y;
        for (int i = 0; i < buttonCount; i++) {
            buttons[i].hovered = pointInRect(mx, my, &buttons[i].rect);
        }
    } else if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        int mx = e->button.x, my = e->button.y;
        for (int i = 0; i < buttonCount; i++) {
            if (pointInRect(mx, my, &buttons[i].rect)) {
                currentState = STATE_MENU;
                initButtonsMenu();
            }
        }
    }
}

/* Render main menu */
void renderMenu() {
    SDL_SetRenderDrawColor(renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(renderer);

    drawCenteredText("Tic-Tac-Toe", WINDOW_WIDTH / 2, 100, COLOR_TEXT);

    for (int i = 0; i < buttonCount; i++) {
        SDL_Color col = buttons[i].hovered ? COLOR_BUTTON_HOVER : COLOR_BUTTON;
        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 255);
        SDL_RenderFillRect(renderer, &buttons[i].rect);
        drawCenteredText(buttons[i].text,
                         buttons[i].rect.x + buttons[i].rect.w / 2,
                         buttons[i].rect.y + buttons[i].rect.h / 2, COLOR_TEXT);
    }
}

/* Render game */
void renderGame() {
    SDL_SetRenderDrawColor(renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(renderer);

    drawBoard();

    // Status text
    if (checkWin(PLAYER_HUMAN))
        drawCenteredText("You Win!", WINDOW_WIDTH / 2, WINDOW_HEIGHT - 40, COLOR_X);
    else if (checkWin(PLAYER_AI))
        drawCenteredText("AI Wins!", WINDOW_WIDTH / 2, WINDOW_HEIGHT - 40, COLOR_O);
    else if (!isMovesLeft())
        drawCenteredText("Draw!", WINDOW_WIDTH / 2, WINDOW_HEIGHT - 40, COLOR_TEXT);
    else if (currentTurn == PLAYER_HUMAN)
        drawCenteredText("Your turn", WINDOW_WIDTH / 2, WINDOW_HEIGHT - 40, COLOR_TEXT);
    else
        drawCenteredText("AI thinking...", WINDOW_WIDTH / 2, WINDOW_HEIGHT - 40, COLOR_TEXT);

    // Buttons
    for (int i = 0; i < buttonCount; i++) {
        SDL_Color col = buttons[i].hovered ? COLOR_BUTTON_HOVER : COLOR_BUTTON;
        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 255);
        SDL_RenderFillRect(renderer, &buttons[i].rect);
        drawCenteredText(buttons[i].text,
                         buttons[i].rect.x + buttons[i].rect.w / 2,
                         buttons[i].rect.y + buttons[i].rect.h / 2, COLOR_TEXT);
    }
}

/* Render explanation screen */
void renderExplanation() {
    SDL_SetRenderDrawColor(renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(renderer);

    const char* explanation[] = {
        "AI Explanation:",
        "",
        "Easy mode picks random moves.",
        "",
        "Hard mode uses Minimax with alpha-beta pruning:",
        "- Minimax tries to maximize AI chances to win",
        "- Alpha-beta pruning cuts unnecessary branches",
        "- This leads to optimal play",
        "",
        "In Tic-Tac-Toe, optimal play leads to",
        "a draw or win depending on opponent moves.",
        "",
        "Click 'Back to Menu' to return."
    };

    int y = 50;
    for (int i = 0; i < sizeof(explanation) / sizeof(explanation[0]); i++) {
        drawText(explanation[i], 40, y, COLOR_TEXT);
        y += 28;
    }

    for (int i = 0; i < buttonCount; i++) {
        SDL_Color col = buttons[i].hovered ? COLOR_BUTTON_HOVER : COLOR_BUTTON;
        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 255);
        SDL_RenderFillRect(renderer, &buttons[i].rect);
        drawCenteredText(buttons[i].text,
                         buttons[i].rect.x + buttons[i].rect.w / 2,
                         buttons[i].rect.y + buttons[i].rect.h / 2, COLOR_TEXT);
    }
}

/* Main */
int main(int argc, char* argv[]) {
    srand(time(NULL));
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() != 0) {
        printf("TTF_Init Error: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    window = SDL_CreateWindow("Tic-Tac-Toe SDL2",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              WINDOW_WIDTH, WINDOW_HEIGHT,
                              SDL_WINDOW_SHOWN);
    if (!window) {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        SDL_DestroyWindow(window);
        printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    font = TTF_OpenFont("Assets/font.ttf", 24);
    if (!font) {
        printf("Failed to load font.ttf\n");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    initButtonsMenu();
    resetBoard();

    SDL_Event e;
    bool quit = false;

    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (currentState == STATE_MENU) handleMenuEvents(&e);
            else if (currentState == STATE_GAME) handleGameEvents(&e);
            else if (currentState == STATE_EXPLANATION) handleExplanationEvents(&e);
        }

        if (currentState == STATE_EXIT) {
            quit = true;
            continue;
        }

        // AI move for hard mode if AI turn
        if (currentState == STATE_GAME && currentTurn == PLAYER_AI) {
            if (checkWin(PLAYER_HUMAN) || checkWin(PLAYER_AI) || !isMovesLeft()) {
                // Game finished, do nothing
            } else {
                if (currentDifficulty == DIFF_HARD) {
                    aiMakeMove();
                } else {
                    easyAIMove();
                }
            }
        }

        // Render screen
        if (currentState == STATE_MENU) renderMenu();
        else if (currentState == STATE_GAME) renderGame();
        else if (currentState == STATE_EXPLANATION) renderExplanation();

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
