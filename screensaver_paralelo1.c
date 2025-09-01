#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <omp.h> // <-- OpenMP

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define MIN_CANVAS_WIDTH 640
#define MIN_CANVAS_HEIGHT 480
#define FPS_TARGET 60
#define PI 3.14159265359
#define MAX_STARS 2000

typedef struct {
    float x, y;
    float vx, vy;
    float brightness;
    float pulse_phase;
    float pulse_speed;
    float size;
    float r, g, b;
    int star_type;
    float glow_intensity;
} Star;

Star* stars = NULL;
int num_stars = 0;
int window_id;
clock_t last_time;
int frame_count = 0;
float fps = 0.0f;
clock_t fps_timer;
int fps_counter = 0;

void generate_star_color(Star* star) {
    int color_type = rand() % 8;
    switch(color_type) {
        case 0: star->r = 0.2f + (rand() % 30) / 100.0f; star->g = 0.4f + (rand() % 40) / 100.0f; star->b = 0.9f + (rand() % 10) / 100.0f; break;
        case 1: star->r = 0.9f + (rand() % 10) / 100.0f; star->g = 0.2f + (rand() % 30) / 100.0f; star->b = 0.7f + (rand() % 30) / 100.0f; break;
        case 2: star->r = 0.9f + (rand() % 10) / 100.0f; star->g = 0.8f + (rand() % 20) / 100.0f; star->b = 0.1f + (rand() % 20) / 100.0f; break;
        case 3: star->r = 0.1f + (rand() % 20) / 100.0f; star->g = 0.8f + (rand() % 20) / 100.0f; star->b = 0.3f + (rand() % 30) / 100.0f; break;
        case 4: star->r = 0.9f + (rand() % 10) / 100.0f; star->g = 0.5f + (rand() % 30) / 100.0f; star->b = 0.1f + (rand() % 20) / 100.0f; break;
        case 5: star->r = 0.7f + (rand() % 30) / 100.0f; star->g = 0.2f + (rand() % 20) / 100.0f; star->b = 0.9f + (rand() % 10) / 100.0f; break;
        case 6: star->r = 0.1f + (rand() % 20) / 100.0f; star->g = 0.8f + (rand() % 20) / 100.0f; star->b = 0.9f + (rand() % 10) / 100.0f; break;
        default: star->r = star->g = star->b = 0.9f + (rand() % 10) / 100.0f; break;
    }
}

void init_star(Star* star, int index) {
    star->x = (float)(rand() % WINDOW_WIDTH);
    star->y = (float)(rand() % WINDOW_HEIGHT);
    float angle = (float)(rand() % 360) * PI / 180.0f;
    float speed = (float)(rand() % 30 + 10) / 1000.0f;
    star->vx = cos(angle) * speed;
    star->vy = sin(angle) * speed;
    star->brightness = 0.6f + (float)(rand() % 40) / 100.0f;
    star->pulse_phase = (float)(rand() % 360) * PI / 180.0f;
    star->pulse_speed = (float)(rand() % 20 + 5) / 10000.0f;
    star->size = 2.0f + (float)(rand() % 6);
    star->star_type = rand() % 4;
    star->glow_intensity = 0.5f + (float)(rand() % 50) / 100.0f;
    generate_star_color(star);
}

void apply_physics(Star* star) {
    star->x += star->vx;
    star->y += star->vy;
    if (star->x <= star->size || star->x >= WINDOW_WIDTH - star->size) {
        star->vx = -star->vx * 0.98f;
        if (star->x <= star->size) star->x = star->size;
        if (star->x >= WINDOW_WIDTH - star->size) star->x = WINDOW_WIDTH - star->size;
    }
    if (star->y <= star->size || star->y >= WINDOW_HEIGHT - star->size) {
        star->vy = -star->vy * 0.98f;
        if (star->y <= star->size) star->y = star->size;
        if (star->y >= WINDOW_HEIGHT - star->size) star->y = WINDOW_HEIGHT - star->size;
    }
    star->pulse_phase += star->pulse_speed;
    if (star->pulse_phase > 2 * PI) star->pulse_phase -= 2 * PI;
    float center_x = WINDOW_WIDTH / 2.0f;
    float center_y = WINDOW_HEIGHT / 2.0f;
    float dist_x = center_x - star->x;
    float dist_y = center_y - star->y;
    float distance = sqrt(dist_x * dist_x + dist_y * dist_y);
    if (distance > 0) {
        float force = 0.000005f;
        star->vx += (dist_x / distance) * force;
        star->vy += (dist_y / distance) * force;
    }
}

void draw_star_glow(float x, float y, float size, float r, float g, float b, float alpha) {
    int segments = 16;
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(r, g, b, alpha);
    glVertex2f(x, y);
    for (int i = 0; i <= segments; i++) {
        float angle = 2.0f * PI * i / segments;
        float px = x + cos(angle) * size;
        float py = y + sin(angle) * size;
        glColor4f(r, g, b, 0.0f);
        glVertex2f(px, py);
    }
    glEnd();
}

void render_star(Star* star) {
    float current_brightness = star->brightness * (0.7f + 0.3f * sin(star->pulse_phase));
    float r = star->r * current_brightness;
    float g = star->g * current_brightness;
    float b = star->b * current_brightness;
    float x = star->x;
    float y = star->y;
    float size = star->size;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    switch(star->star_type) {
        case 0:
            draw_star_glow(x, y, size * 3, r, g, b, 0.1f * star->glow_intensity);
            draw_star_glow(x, y, size * 2, r, g, b, 0.2f * star->glow_intensity);
            glColor3f(r, g, b);
            glBegin(GL_LINES);
            glVertex2f(x - size, y); glVertex2f(x + size, y);
            glVertex2f(x, y - size); glVertex2f(x, y + size);
            glEnd();
            glPointSize(3.0f);
            glBegin(GL_POINTS);
            glColor3f(r * 1.2f, g * 1.2f, b * 1.2f);
            glVertex2f(x, y);
            glEnd();
            break;
        case 1:
            draw_star_glow(x, y, size * 4, r, g, b, 0.15f * star->glow_intensity);
            glColor3f(r, g, b);
            glBegin(GL_LINES);
            glVertex2f(x - size, y); glVertex2f(x + size, y);
            glVertex2f(x, y - size); glVertex2f(x, y + size);
            float diag = size * 0.7f;
            glVertex2f(x - diag, y - diag); glVertex2f(x + diag, y + diag);
            glVertex2f(x - diag, y + diag); glVertex2f(x + diag, y - diag);
            glEnd();
            glPointSize(4.0f);
            glBegin(GL_POINTS);
            glColor3f(1.0f, 1.0f, 1.0f);
            glVertex2f(x, y);
            glEnd();
            break;
        case 2:
            draw_star_glow(x, y, size * 5, r, g, b, 0.08f * star->glow_intensity);
            draw_star_glow(x, y, size * 3, r, g, b, 0.15f * star->glow_intensity);
            draw_star_glow(x, y, size * 1.5f, r, g, b, 0.3f * star->glow_intensity);
            glColor3f(r, g, b);
            glBegin(GL_LINES);
            for (int i = 0; i < 8; i++) {
                float angle = (PI * 2 * i) / 8;
                float ray_length = size * (1.2f + 0.3f * sin(star->pulse_phase + i));
                glVertex2f(x, y);
                glVertex2f(x + cos(angle) * ray_length, y + sin(angle) * ray_length);
            }
            glEnd();
            glPointSize(5.0f);
            glBegin(GL_POINTS);
            glColor3f(1.0f, 1.0f, 1.0f);
            glVertex2f(x, y);
            glEnd();
            break;
        case 3:
            float pulse_factor = 1.0f + 0.5f * sin(star->pulse_phase * 2);
            draw_star_glow(x, y, size * 6 * pulse_factor, r, g, b, 0.05f * star->glow_intensity);
            draw_star_glow(x, y, size * 3 * pulse_factor, r, g, b, 0.1f * star->glow_intensity);
            glColor3f(r, g, b);
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < 10; i++) {
                float angle = (PI * 2 * i) / 10;
                float radius = (i % 2 == 0) ? size : size * 0.5f;
                radius *= pulse_factor;
                glVertex2f(x + cos(angle) * radius, y + sin(angle) * radius);
            }
            glEnd();
            break;
    }
    glDisable(GL_BLEND);
}

void display_fps() {
    fps_counter++;
    clock_t current_time = clock();
    if (current_time - fps_timer >= CLOCKS_PER_SEC) {
        fps = (float)fps_counter / ((current_time - fps_timer) / (float)CLOCKS_PER_SEC);
        fps_counter = 0;
        fps_timer = current_time;
        char title[256];
        snprintf(title, sizeof(title), "Screensaver OpenGL - Estrellas: %d | FPS: %.1f", num_stars, fps);
        glutSetWindowTitle(title);
        printf("FPS: %.1f\n", fps);
        if (fps < 30) printf("⚠️  ADVERTENCIA: FPS por debajo de 30!\n");
    }
}

void display() {
    glClearColor(0.02f, 0.01f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Primero física en paralelo
    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < num_stars; i++) {
        apply_physics(&stars[i]);
    }

    // Luego renderizado secuencial
    for (int i = 0; i < num_stars; i++) {
        render_star(&stars[i]);
    }

    display_fps();
    glutSwapBuffers();
}

void reshape(int width, int height) {
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, width, 0, height);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void keyboard(unsigned char key, int x, int y) {
    switch(key) {
        case 27: case 'q': case 'Q':
            if (stars) free(stars);
            glutDestroyWindow(window_id);
            exit(0);
        case '+':
            if (num_stars < MAX_STARS - 50) {
                stars = realloc(stars, (num_stars + 50) * sizeof(Star));
                #pragma omp parallel for
                for (int i = num_stars; i < num_stars + 50; i++)
                    init_star(&stars[i], i);
                num_stars += 50;
            }
            break;
        case '-':
            if (num_stars > 50) {
                num_stars -= 50;
                stars = realloc(stars, num_stars * sizeof(Star));
            }
            break;
    }
}

void timer(int value) {
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

void init_opengl() {
    glEnable(GL_POINT_SMOOTH);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.0f);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

int validate_input(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Uso: %s <numero_de_estrellas>\n", argv[0]);
        return -1;
    }
    int n = atoi(argv[1]);
    if (n <= 0 || n > MAX_STARS) return -1;
    return n;
}

int main(int argc, char* argv[]) {
    num_stars = validate_input(argc, argv);
    if (num_stars == -1) return 1;
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_ALPHA);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutInitWindowPosition(100, 100);
    char title[256];
    snprintf(title, sizeof(title), "Screensaver OpenGL - Estrellas: %d", num_stars);
    window_id = glutCreateWindow(title);
    srand((unsigned int)time(NULL));
    stars = (Star*)malloc(num_stars * sizeof(Star));
    if (!stars) return 1;

    // Inicialización de estrellas en paralelo
    #pragma omp parallel for
    for (int i = 0; i < num_stars; i++) init_star(&stars[i], i);

    init_opengl();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(0, timer, 0);
    fps_timer = clock();
    glutMainLoop();

    if (stars) free(stars);
    return 0;
}
