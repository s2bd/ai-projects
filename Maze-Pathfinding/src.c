// 1) Install dependencies: sudo apt install build-essential libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev libsdl2-gfx-dev
// 2) Compilation: gcc -o viz src.c -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_gfx -lSDL2_mixer -lm
// 3) Run: ./viz

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#define ROWS 20
#define COLS 20
#define CELL_SIZE 30
#define WIDTH (COLS * CELL_SIZE) + 60
#define HEIGHT (ROWS * CELL_SIZE + 120)  // Extra space for UI and instructions

typedef enum {
    EMPTY, START, END, BARRIER, VISITED, PATH
} CellType;

typedef enum {
    START_MODE, END_MODE, BARRIER_MODE, CONFIRMED_MODE
} InteractionMode;

typedef struct {
    int row, col;
} Point;

typedef struct {
    CellType type;
    SDL_Rect rect;
    int heuristic;  // For displaying heuristic in A* and Greedy
} Cell;

typedef struct {
    SDL_Rect rect;
    char label[32];
    bool selected;
    bool disabled;
} Button;

SDL_Window* window;
SDL_Renderer* renderer;
TTF_Font* font;
Cell grid[ROWS][COLS];
Button buttons[6];  // 5 algorithms + Confirm button
int buttonCount = 6;
const char* algoNames[] = {"A*", "Dijkstra", "BFS", "DFS", "Greedy"};
int selectedAlgo = 0;
char instructionText[100] = "Click on a square to select the starting point.";

Point start = {-1, -1}, end = {-1, -1};
bool running = true, mouseDown = false, drawingBarrier = true;
InteractionMode mode = START_MODE;

void drawCell(Cell* cell) {
    switch (cell->type) {
        case EMPTY: SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); break;
        case START: SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255); break;
        case END: SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255); break;
        case BARRIER: SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); break;
        case VISITED: SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255); break;
        case PATH: SDL_SetRenderDrawColor(renderer, 100, 149, 237, 255); break;
    }
    SDL_RenderFillRect(renderer, &cell->rect);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderDrawRect(renderer, &cell->rect);

    // Display heuristic for A* and Greedy
    if ((strcmp(algoNames[selectedAlgo], "A*") == 0 || strcmp(algoNames[selectedAlgo], "Greedy") == 0) &&
        cell->type == VISITED && cell->heuristic >= 0) {
        char hText[16];
        snprintf(hText, sizeof(hText), "%d", cell->heuristic);
        SDL_Color color = {0, 0, 0};
        drawText(hText, cell->rect.x + 5, cell->rect.y + 5, color);
    }
}

void drawText(const char* text, int x, int y, SDL_Color color) {
    SDL_Surface* surface = TTF_RenderText_Solid(font, text, color);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect dst = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void drawButtons() {
    for (int i = 0; i < buttonCount; i++) {
        Button* btn = &buttons[i];
        if (i < 5 && mode != CONFIRMED_MODE) continue; // Hide algorithm buttons until confirmed
        SDL_SetRenderDrawColor(renderer, btn->disabled ? 128 : (btn->selected ? 0 : 180), 180, 255, 255);
        SDL_RenderFillRect(renderer, &btn->rect);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &btn->rect);
        SDL_Color color = {0, 0, 0};
        drawText(btn->label, btn->rect.x + 10, btn->rect.y + 5, color);
    }
}

void drawGrid() {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            drawCell(&grid[r][c]);
}

void resetGrid() {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            grid[r][c].type = EMPTY;
            grid[r][c].rect = (SDL_Rect){c * CELL_SIZE, r * CELL_SIZE, CELL_SIZE, CELL_SIZE};
            grid[r][c].heuristic = -1;
        }
    start.row = start.col = end.row = end.col = -1;
    mode = START_MODE;
    buttons[5].disabled = true; // Disable Confirm button
    strcpy(instructionText, "Click on a square to select the starting point.");
}

void resetVisited() {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            if (grid[r][c].type == VISITED || grid[r][c].type == PATH) {
                grid[r][c].type = EMPTY;
                grid[r][c].heuristic = -1;
            } else if (grid[r][c].type == START) {
                grid[r][c].type = START;
            } else if (grid[r][c].type == END) {
                grid[r][c].type = END;
            }
        }
}

bool isValid(int r, int c) {
    return r >= 0 && r < ROWS && c >= 0 && c < COLS;
}

int heuristic(Point a, Point b) {
    return abs(a.row - b.row) + abs(a.col - b.col);
}

void visualizePath(Point parent[ROWS][COLS], Point current) {
    while (!(current.row == start.row && current.col == start.col)) {
        current = parent[current.row][current.col];
        if (!(current.row == start.row && current.col == start.col))
            grid[current.row][current.col].type = PATH;
        else
            grid[current.row][current.col].type = START;
        SDL_Delay(20);
        SDL_RenderClear(renderer);
        drawGrid();
        drawButtons();
        drawText(instructionText, 10, ROWS * CELL_SIZE + 80, (SDL_Color){0, 0, 255});
        SDL_RenderPresent(renderer);
    }
}

void runSelectedAlgorithm() {
    resetVisited();
    bool visited[ROWS][COLS] = {false};
    Point parent[ROWS][COLS];
    int cost[ROWS][COLS];
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            cost[r][c] = INT_MAX;

    cost[start.row][start.col] = 0;
    visited[start.row][start.col] = true;

    typedef struct {
        int priority; // Cost or heuristic value
        Point point;
    } QueueItem;

    QueueItem queue[ROWS * COLS];
    int qSize = 0;

    // Initialize queue based on algorithm
    if (strcmp(algoNames[selectedAlgo], "Dijkstra") == 0 || strcmp(algoNames[selectedAlgo], "A*") == 0 ||
        strcmp(algoNames[selectedAlgo], "Greedy") == 0) {
        queue[qSize++] = (QueueItem){0, start};
    } else {
        queue[qSize++] = (QueueItem){0, start};
    }

    int directions[4][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};

    while (qSize > 0) {
        Point current;
        if (strcmp(algoNames[selectedAlgo], "DFS") == 0) {
            current = queue[--qSize].point;
        } else {
            // Sort queue by priority (ascending)
            for (int i = 1; i < qSize; i++) {
                for (int j = i; j > 0 && queue[j].priority < queue[j - 1].priority; j--) {
                    QueueItem temp = queue[j];
                    queue[j] = queue[j - 1];
                    queue[j - 1] = temp;
                }
            }
            current = queue[0].point;
            queue[0] = queue[--qSize];
        }

        if (!(current.row == start.row && current.col == start.col)) {
            grid[current.row][current.col].type = VISITED;
            if (strcmp(algoNames[selectedAlgo], "A*") == 0 || strcmp(algoNames[selectedAlgo], "Greedy") == 0)
                grid[current.row][current.col].heuristic = heuristic(current, end);
        }

        if (current.row == end.row && current.col == end.col) {
            visualizePath(parent, current);
            return;
        }

        for (int d = 0; d < 4; d++) {
            int nr = current.row + directions[d][0];
            int nc = current.col + directions[d][1];
            if (!isValid(nr, nc) || grid[nr][nc].type == BARRIER) continue;

            int newCost = cost[current.row][current.col] + 1;
            bool update = false;

            if (strcmp(algoNames[selectedAlgo], "Dijkstra") == 0 && newCost < cost[nr][nc]) {
                cost[nr][nc] = newCost;
                queue[qSize++] = (QueueItem){newCost, (Point){nr, nc}};
                update = true;
            } else if (strcmp(algoNames[selectedAlgo], "A*") == 0 && newCost + heuristic((Point){nr, nc}, end) < cost[nr][nc]) {
                cost[nr][nc] = newCost + heuristic((Point){nr, nc}, end);
                queue[qSize++] = (QueueItem){newCost + heuristic((Point){nr, nc}, end), (Point){nr, nc}};
                update = true;
            } else if (strcmp(algoNames[selectedAlgo], "Greedy") == 0 && heuristic((Point){nr, nc}, end) < cost[nr][nc]) {
                cost[nr][nc] = heuristic((Point){nr, nc}, end);
                queue[qSize++] = (QueueItem){heuristic((Point){nr, nc}, end), (Point){nr, nc}};
                update = true;
            } else if (!visited[nr][nc] && (strcmp(algoNames[selectedAlgo], "BFS") == 0 || strcmp(algoNames[selectedAlgo], "DFS") == 0)) {
                queue[qSize++] = (QueueItem){0, (Point){nr, nc}};
                update = true;
            }

            if (update) {
    parent[nr][nc] = current;
    visited[nr][nc] = true;
    if (nr != end.row || nc != end.col) { // Only mark as VISITED if not the end cell
        grid[nr][nc].type = VISITED;
        if (strcmp(algoNames[selectedAlgo], "A*") == 0 || strcmp(algoNames[selectedAlgo], "Greedy") == 0)
            grid[nr][nc].heuristic = heuristic((Point){nr, nc}, end);
    }
}
        }

        SDL_Delay(10);
        SDL_RenderClear(renderer);
        drawGrid();
        drawButtons();
        drawText(instructionText, 10, ROWS * CELL_SIZE + 80, (SDL_Color){0, 0, 255});
        SDL_RenderPresent(renderer);
    }
}

void handleClick(int x, int y) {
    if (mode == CONFIRMED_MODE && y >= ROWS * CELL_SIZE) {
        for (int i = 0; i < buttonCount - 1; i++) { // Algorithm buttons
            if (x >= buttons[i].rect.x && x <= buttons[i].rect.x + buttons[i].rect.w &&
                y >= buttons[i].rect.y && y <= buttons[i].rect.y + buttons[i].rect.h) {
                for (int j = 0; j < buttonCount - 1; j++) buttons[j].selected = false;
                buttons[i].selected = true;
                selectedAlgo = i;
                runSelectedAlgorithm();
                return;
            }
        }
    }

    if (y >= buttons[5].rect.y && y <= buttons[5].rect.y + buttons[5].rect.h &&
        x >= buttons[5].rect.x && x <= buttons[5].rect.x + buttons[5].rect.w && !buttons[5].disabled) {
        mode = CONFIRMED_MODE;
        buttons[5].disabled = true;
        strcpy(instructionText, "Select an algorithm and click to visualize.");
        return;
    }

    if (mode == CONFIRMED_MODE) return;

    int r = y / CELL_SIZE;
    int c = x / CELL_SIZE;
    if (!isValid(r, c)) return;

    if (mode == START_MODE) {
        if (start.row != -1) grid[start.row][start.col].type = EMPTY;
        start = (Point){r, c};
        grid[r][c].type = START;
        strcpy(instructionText, "Click on a square to select the ending point.");
        mode = END_MODE;
    } else if (mode == END_MODE) {
        if (end.row != -1) grid[end.row][end.col].type = EMPTY;
        if (r == start.row && c == start.col) return;
        end = (Point){r, c};
        grid[r][c].type = END;
        strcpy(instructionText, "Click to add/remove barriers. Then click Confirm.");
        mode = BARRIER_MODE;
        buttons[5].disabled = false;
   } else if (mode == BARRIER_MODE) {
    if (r == start.row && c == start.col) return;
    if (r == end.row && c == end.col) return;
    grid[r][c].type = (grid[r][c].type == BARRIER) ? EMPTY : BARRIER;
}
}

void setupButtons() {
    for (int i = 0; i < buttonCount - 1; i++) {
        buttons[i].rect = (SDL_Rect){10 + i * 110, ROWS * CELL_SIZE + 20, 100, 40};
        strcpy(buttons[i].label, algoNames[i]);
        buttons[i].selected = (i == 0);
        buttons[i].disabled = false;
    }
    // Confirm button
    buttons[5].rect = (SDL_Rect){10 + 5 * 110, ROWS * CELL_SIZE + 20, 100, 40};
    strcpy(buttons[5].label, "Confirm");
    buttons[5].selected = false;
    buttons[5].disabled = true;
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    font = TTF_OpenFont("font.ttf", 20);
    if (!font) {
        printf("Error loading font.ttf: %s\n", TTF_GetError());
        return 1;
    }

    window = SDL_CreateWindow("AI Pathfinding Visualizer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, 0);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    resetGrid();
    setupButtons();

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                mouseDown = true;
                handleClick(e.button.x, e.button.y);
            }
            if (e.type == SDL_MOUSEBUTTONUP) mouseDown = false;
            if (e.type == SDL_MOUSEMOTION && mouseDown && mode == BARRIER_MODE) {
                handleClick(e.motion.x, e.motion.y);
            }
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_r) {
                resetGrid();
                setupButtons();
            }
        }

        SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
        SDL_RenderClear(renderer);
        drawGrid();
        drawButtons();
        drawText(instructionText, 10, ROWS * CELL_SIZE + 80, (SDL_Color){0, 0, 255});
        SDL_RenderPresent(renderer);
    }

    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
