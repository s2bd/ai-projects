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
#define WIDTH (COLS * CELL_SIZE)
#define HEIGHT (ROWS * CELL_SIZE + 100)  // Extra space for UI

typedef enum {
    EMPTY, START, END, BARRIER, VISITED, PATH
} CellType;

typedef struct {
    int row, col;
} Point;

typedef struct {
    CellType type;
    SDL_Rect rect;
} Cell;

typedef struct {
    SDL_Rect rect;
    char label[32];
    bool selected;
} Button;

SDL_Window* window;
SDL_Renderer* renderer;
TTF_Font* font;
Cell grid[ROWS][COLS];
Button buttons[5];
int buttonCount = 5;
const char* algoNames[] = {"A*", "Dijkstra", "BFS", "DFS", "Greedy"};
int selectedAlgo = 0;

Point start = {-1, -1}, end = {-1, -1};
bool running = true, confirmed = false, mouseDown = false;

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
        SDL_SetRenderDrawColor(renderer, btn->selected ? 0 : 180, 180, 255, 255);
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
            grid[r][c].rect = (SDL_Rect){ c * CELL_SIZE, r * CELL_SIZE, CELL_SIZE, CELL_SIZE };
        }
    start.row = start.col = end.row = end.col = -1;
    confirmed = false;
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
        SDL_Delay(30);
        SDL_RenderClear(renderer);
        drawGrid();
        drawButtons();
        SDL_RenderPresent(renderer);
    }
}

void runSelectedAlgorithm() {
    bool visited[ROWS][COLS] = { false };
    Point parent[ROWS][COLS];
    int cost[ROWS][COLS];
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            cost[r][c] = INT_MAX;

    cost[start.row][start.col] = 0;

    Point queue[ROWS * COLS];
    int qSize = 0;
    queue[qSize++] = start;

    int directions[4][2] = {{0,1},{1,0},{0,-1},{-1,0}};

    while (qSize > 0) {
        Point current;
        if (strcmp(algoNames[selectedAlgo], "DFS") == 0) {
            current = queue[--qSize];
        } else {
            int bestIdx = 0;
            for (int i = 1; i < qSize; i++) {
                Point p = queue[i];
                int f1 = cost[p.row][p.col] + heuristic(p, end);
                Point q = queue[bestIdx];
                int f2 = cost[q.row][q.col] + heuristic(q, end);
                if (strcmp(algoNames[selectedAlgo], "A*") == 0 || strcmp(algoNames[selectedAlgo], "Dijkstra") == 0)
                    if (f1 < f2) bestIdx = i;
                if (strcmp(algoNames[selectedAlgo], "Greedy") == 0)
                    if (heuristic(p, end) < heuristic(q, end)) bestIdx = i;
            }
            current = queue[bestIdx];
            queue[bestIdx] = queue[--qSize];
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

            if (strcmp(algoNames[selectedAlgo], "Dijkstra") == 0 && newCost < cost[nr][nc]) update = true;
            if (strcmp(algoNames[selectedAlgo], "A*") == 0 && newCost + heuristic((Point){nr, nc}, end) < cost[nr][nc]) update = true;
            if (strcmp(algoNames[selectedAlgo], "Greedy") == 0) update = !visited[nr][nc];
            if (strcmp(algoNames[selectedAlgo], "BFS") == 0) update = !visited[nr][nc];
            if (strcmp(algoNames[selectedAlgo], "DFS") == 0) update = !visited[nr][nc];

            if (update) {
                parent[nr][nc] = current;
                cost[nr][nc] = newCost;
                queue[qSize++] = (Point){nr, nc};
                visited[nr][nc] = true;
                if (grid[nr][nc].type != END)
                    grid[nr][nc].type = VISITED;
            }
        }

        SDL_Delay(10);
        SDL_RenderClear(renderer);
        drawGrid();
        drawButtons();
        SDL_RenderPresent(renderer);
    }
}

void handleClick(int x, int y) {
    if (y >= ROWS * CELL_SIZE) {
        for (int i = 0; i < buttonCount; i++) {
            if (x >= buttons[i].rect.x && x <= buttons[i].rect.x + buttons[i].rect.w &&
                y >= buttons[i].rect.y && y <= buttons[i].rect.y + buttons[i].rect.h) {
                for (int j = 0; j < buttonCount; j++) buttons[j].selected = false;
                buttons[i].selected = true;
                selectedAlgo = i;
                if (start.row != -1 && end.row != -1)
                    runSelectedAlgorithm();
                return;
            }
        }
    }

    int r = y / CELL_SIZE;
    int c = x / CELL_SIZE;
    if (!isValid(r, c)) return;

    if (start.row == -1) {
        start = (Point){r, c};
        grid[r][c].type = START;
    } else if (end.row == -1 && (r != start.row || c != start.col)) {
        end = (Point){r, c};
        grid[r][c].type = END;
    } else {
        if ((r != start.row || c != start.col) && (r != end.row || c != end.col))
            grid[r][c].type = (grid[r][c].type == BARRIER) ? EMPTY : BARRIER;
    }
}

void setupButtons() {
    for (int i = 0; i < buttonCount; i++) {
        buttons[i].rect = (SDL_Rect){10 + i * 110, ROWS * CELL_SIZE + 20, 100, 40};
        strcpy(buttons[i].label, algoNames[i]);
        buttons[i].selected = (i == 0);
    }
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
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_r) resetGrid();
        }

        if (mouseDown) {
            int x, y;
            SDL_GetMouseState(&x, &y);
            handleClick(x, y);
        }

        SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
        SDL_RenderClear(renderer);
        drawGrid();
        drawButtons();
        SDL_RenderPresent(renderer);
    }

    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}

