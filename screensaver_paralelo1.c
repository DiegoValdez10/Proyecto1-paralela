#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <omp.h>  // OpenMP

#define CANVAS_WIDTH 80
#define CANVAS_HEIGHT 40
#define FPS_TARGET 10
#define PI 3.14159265359
#define MAX_STARS 1000

#define CLEAR_SCREEN "\033[2J"
#define CURSOR_HOME "\033[H"
#define HIDE_CURSOR "\033[?25l"
#define SHOW_CURSOR "\033[?25h"

#define COLOR_RED "\033[91m"
#define COLOR_GREEN "\033[92m"
#define COLOR_YELLOW "\033[93m"
#define COLOR_BLUE "\033[94m"
#define COLOR_MAGENTA "\033[95m"
#define COLOR_CYAN "\033[96m"
#define COLOR_WHITE "\033[97m"
#define COLOR_RESET "\033[0m"

typedef struct {
    float x, y;
    float vx, vy;
    float brightness;
    float pulse_phase;
    float pulse_speed;
    float size;
    int color_code;
    char symbol;
} Star;

Star* stars = NULL;
int num_stars = 0;
char canvas[CANVAS_HEIGHT][CANVAS_WIDTH + 1];
char color_canvas[CANVAS_HEIGHT][CANVAS_WIDTH + 1];

const char* colors[] = {
    COLOR_RED, COLOR_GREEN, COLOR_YELLOW,
    COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN, COLOR_WHITE
};

const char star_symbols[] = {'*', '+', 'x', 'o', '#', '@', '.'};

void clear_canvas() {
    #pragma omp parallel for collapse(2)
    for (int y = 0; y < CANVAS_HEIGHT; y++) {
        for (int x = 0; x < CANVAS_WIDTH; x++) {
            canvas[y][x] = ' ';
            color_canvas[y][x] = 0;
        }
        canvas[y][CANVAS_WIDTH] = '\0';
        color_canvas[y][CANVAS_WIDTH] = '\0';
    }
}

void init_star(Star* star, int index) {
    star->x = (float)(rand() % (CANVAS_WIDTH - 2) + 1);
    star->y = (float)(rand() % (CANVAS_HEIGHT - 2) + 1);

    float angle = (float)(rand() % 360) * PI / 180.0f;
    float speed = (float)(rand() % 1 + 1) / 100000.0f;
    star->vx = cos(angle) * speed;
    star->vy = sin(angle) * speed;

    star->brightness = (float)(rand() % 50 + 50) / 100.0f;
    star->pulse_phase = (float)(rand() % 360) * PI / 180.0f;
    star->pulse_speed = (float)(rand() % 15 + 5) / 50000.0f;

    star->size = (float)(rand() % 3 + 1);
    star->color_code = rand() % 7;
    star->symbol = star_symbols[rand() % 7];
}

void apply_physics(Star* star) {
    star->x += star->vx;
    star->y += star->vy;

    if (star->x <= 1 || star->x >= CANVAS_WIDTH - 2) {
        star->vx = -star->vx * 0.95f;
        if (star->x <= 1) star->x = 1;
        if (star->x >= CANVAS_WIDTH - 2) star->x = CANVAS_WIDTH - 2;
    }

    if (star->y <= 1 || star->y >= CANVAS_HEIGHT - 2) {
        star->vy = -star->vy * 0.95f;
        if (star->y <= 1) star->y = 1;
        if (star->y >= CANVAS_HEIGHT - 2) star->y = CANVAS_HEIGHT - 2;
    }

    star->pulse_phase += star->pulse_speed;
    if (star->pulse_phase > 2 * PI) star->pulse_phase -= 2 * PI;

    float center_x = CANVAS_WIDTH / 2.0f;
    float center_y = CANVAS_HEIGHT / 2.0f;
    float dist_x = center_x - star->x;
    float dist_y = center_y - star->y;
    float distance = sqrt(dist_x * dist_x + dist_y * dist_y);

    if (distance > 0) {
        float force = 0.0000001f;
        star->vx += (dist_x / distance) * force;
        star->vy += (dist_y / distance) * force;
    }
}

void draw_star_to_canvas(Star* star) {
    float current_brightness = star->brightness * (0.6f + 0.4f * sin(star->pulse_phase));
    int x = (int)round(star->x);
    int y = (int)round(star->y);

    if (x >= 0 && x < CANVAS_WIDTH && y >= 0 && y < CANVAS_HEIGHT) {
        char symbol;
        if (current_brightness > 0.8f) symbol = '*';
        else if (current_brightness > 0.6f) symbol = '+';
        else if (current_brightness > 0.4f) symbol = '.';
        else symbol = ' ';

        if (symbol != ' ') {
            // Riesgo mínimo de condición de carrera (no crítico visualmente)
            canvas[y][x] = symbol;
            color_canvas[y][x] = star->color_code + 1;
        }
    }
}

void render_canvas() {
    printf(CLEAR_SCREEN CURSOR_HOME);
    for (int y = 0; y < CANVAS_HEIGHT; y++) {
        for (int x = 0; x < CANVAS_WIDTH; x++) {
            int color_code = color_canvas[y][x];
            if (color_code > 0) printf("%s%c", colors[color_code - 1], canvas[y][x]);
            else printf(" ");
        }
        printf(COLOR_RESET "\n");
    }
}

void display_fps_info(double frame_time, int frame_count) {
    static double total_time = 0;
    static int fps_counter = 0;

    total_time += frame_time;
    fps_counter++;

    if (fps_counter >= 30) {
        double avg_fps = fps_counter / total_time;
        printf("FPS: %.1f | Frame time: %.2fms | Estrellas: %d\n",
               avg_fps, (frame_time * 1000), num_stars);
        fps_counter = 0;
        total_time = 0;
    }
}

int validate_input(int argc, char* argv[]) {
    if (argc != 2) return -1;
    int n = atoi(argv[1]);
    if (n <= 0 || n > MAX_STARS) return -1;
    return n;
}

int main(int argc, char* argv[]) {
    num_stars = validate_input(argc, argv);
    if (num_stars == -1) {
        printf("Uso: %s <numero_de_estrellas>\n", argv[0]);
        return 1;
    }

    srand((unsigned int)time(NULL));
    stars = (Star*)malloc(num_stars * sizeof(Star));
    for (int i = 0; i < num_stars; i++) init_star(&stars[i], i);

    printf(HIDE_CURSOR);

    clock_t frame_start, frame_end;
    double frame_time;
    double target_frame_time = 1.0 / FPS_TARGET;
    int frame_count = 0;

    while (1) {
        frame_start = clock();
        clear_canvas();

        // PARALLEL: física + dibujo en paralelo
        #pragma omp parallel for
        for (int i = 0; i < num_stars; i++) {
            apply_physics(&stars[i]);
            draw_star_to_canvas(&stars[i]);
        }

        render_canvas();

        frame_end = clock();
        frame_time = ((double)(frame_end - frame_start)) / CLOCKS_PER_SEC;
        frame_count++;
        display_fps_info(frame_time, frame_count);

        if (frame_time < target_frame_time) {
            double sleep_time = target_frame_time - frame_time;
            usleep((useconds_t)(sleep_time * 1000000));
        } else {
            usleep(50000);
        }
    }

    printf(SHOW_CURSOR COLOR_RESET);
    free(stars);
    return 0;
}
