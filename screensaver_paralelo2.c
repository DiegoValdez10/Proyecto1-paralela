#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <omp.h>
#include <immintrin.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define MIN_CANVAS_WIDTH 640
#define MIN_CANVAS_HEIGHT 480
#define FPS_TARGET 60
#define PI 3.14159265359
#define MAX_STARS 2000
#define GRID_SIZE 64  // Tamaño de cada celda del grid espacial
#define CACHE_LINE_SIZE 64
#define SIMD_WIDTH 8  

// Variables globales para medición de rendimiento
double frame_time = 0.0;
int current_frame = 0;

// Estructura optimizada
typedef struct {
    // Posiciones alineadas
    float* __restrict__ x;
    float* __restrict__ y;
    float* __restrict__ vx;
    float* __restrict__ vy;
    
    // Propiedades visuales
    float* __restrict__ brightness;
    float* __restrict__ pulse_phase;
    float* __restrict__ pulse_speed;
    float* __restrict__ size;
    
    // Colores
    float* __restrict__ r;
    float* __restrict__ g;
    float* __restrict__ b;
    
    int* __restrict__ star_type;
    float* __restrict__ glow_intensity;
    
    // Grid espacial para optimización de colisiones
    int* __restrict__ grid_cell;
    
    int count;
    int capacity;
} StarSystem;

// Grid espacial para optimizar interacciones
typedef struct {
    int* star_indices;
    int count;
    int capacity;
} GridCell;

typedef struct {
    GridCell* cells;
    int width;
    int height;
    int total_cells;
} SpatialGrid;

StarSystem* star_system = NULL;
SpatialGrid* spatial_grid = NULL;
int window_id;
clock_t last_time;
int frame_count = 0;
float fps = 0.0f;
clock_t fps_timer;
int fps_counter = 0;

// Forward declarations
void destroy_star_system(StarSystem* sys);

void* aligned_malloc(size_t size, size_t alignment) {
    void* ptr = _aligned_malloc(size, alignment);
    
    // Fallback si falla _aligned_malloc
    if (!ptr) {
        ptr = malloc(size + alignment - 1);
        if (ptr) {
            uintptr_t addr = (uintptr_t)ptr;
            uintptr_t aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
            return (void*)aligned_addr;
        }
    }
    
    return ptr;
}

void aligned_free(void* ptr) {
    _aligned_free(ptr);
}

// Inicialización del sistema de estrellas
StarSystem* create_star_system(int count) {
    StarSystem* sys = (StarSystem*)malloc(sizeof(StarSystem));
    if (!sys) return NULL;
    
    sys->count = count;
    sys->capacity = count + (SIMD_WIDTH - (count % SIMD_WIDTH)) % SIMD_WIDTH; // Align 
    size_t aligned_size = sys->capacity * sizeof(float);
    
    sys->x = (float*)aligned_malloc(aligned_size, CACHE_LINE_SIZE);
    sys->y = (float*)aligned_malloc(aligned_size, CACHE_LINE_SIZE);
    sys->vx = (float*)aligned_malloc(aligned_size, CACHE_LINE_SIZE);
    sys->vy = (float*)aligned_malloc(aligned_size, CACHE_LINE_SIZE);
    sys->brightness = (float*)aligned_malloc(aligned_size, CACHE_LINE_SIZE);
    sys->pulse_phase = (float*)aligned_malloc(aligned_size, CACHE_LINE_SIZE);
    sys->pulse_speed = (float*)aligned_malloc(aligned_size, CACHE_LINE_SIZE);
    sys->size = (float*)aligned_malloc(aligned_size, CACHE_LINE_SIZE);
    sys->r = (float*)aligned_malloc(aligned_size, CACHE_LINE_SIZE);
    sys->g = (float*)aligned_malloc(aligned_size, CACHE_LINE_SIZE);
    sys->b = (float*)aligned_malloc(aligned_size, CACHE_LINE_SIZE);
    sys->glow_intensity = (float*)aligned_malloc(aligned_size, CACHE_LINE_SIZE);
    
    sys->star_type = (int*)aligned_malloc(sys->capacity * sizeof(int), CACHE_LINE_SIZE);
    sys->grid_cell = (int*)aligned_malloc(sys->capacity * sizeof(int), CACHE_LINE_SIZE);
    if (!sys->x || !sys->y || !sys->vx || !sys->vy || !sys->brightness || 
        !sys->pulse_phase || !sys->pulse_speed || !sys->size || 
        !sys->r || !sys->g || !sys->b || !sys->glow_intensity || 
        !sys->star_type || !sys->grid_cell) {
        destroy_star_system(sys);
        return NULL;
    }
    
    return sys;
}

void destroy_star_system(StarSystem* sys) {
    if (!sys) return;
    
    aligned_free(sys->x);
    aligned_free(sys->y);
    aligned_free(sys->vx);
    aligned_free(sys->vy);
    aligned_free(sys->brightness);
    aligned_free(sys->pulse_phase);
    aligned_free(sys->pulse_speed);
    aligned_free(sys->size);
    aligned_free(sys->r);
    aligned_free(sys->g);
    aligned_free(sys->b);
    aligned_free(sys->glow_intensity);
    aligned_free(sys->star_type);
    aligned_free(sys->grid_cell);
    
    free(sys);
}

SpatialGrid* create_spatial_grid() {
    SpatialGrid* grid = (SpatialGrid*)malloc(sizeof(SpatialGrid));
    grid->width = (WINDOW_WIDTH + GRID_SIZE - 1) / GRID_SIZE;
    grid->height = (WINDOW_HEIGHT + GRID_SIZE - 1) / GRID_SIZE;
    grid->total_cells = grid->width * grid->height;
    
    grid->cells = (GridCell*)calloc(grid->total_cells, sizeof(GridCell));
    for (int i = 0; i < grid->total_cells; i++) {
        grid->cells[i].capacity = 50; 
        grid->cells[i].star_indices = (int*)malloc(grid->cells[i].capacity * sizeof(int));
        grid->cells[i].count = 0;
    }
    
    return grid;
}

void destroy_spatial_grid(SpatialGrid* grid) {
    if (!grid) return;
    
    for (int i = 0; i < grid->total_cells; i++) {
        free(grid->cells[i].star_indices);
    }
    free(grid->cells);
    free(grid);
}

void generate_star_color(int index) {
    int color_type = rand() % 8;
    switch(color_type) {
        case 0: 
            star_system->r[index] = 0.2f + (rand() % 30) / 100.0f; 
            star_system->g[index] = 0.4f + (rand() % 40) / 100.0f; 
            star_system->b[index] = 0.9f + (rand() % 10) / 100.0f; 
            break;
        case 1: 
            star_system->r[index] = 0.9f + (rand() % 10) / 100.0f; 
            star_system->g[index] = 0.2f + (rand() % 30) / 100.0f; 
            star_system->b[index] = 0.7f + (rand() % 30) / 100.0f; 
            break;
        case 2: 
            star_system->r[index] = 0.9f + (rand() % 10) / 100.0f; 
            star_system->g[index] = 0.8f + (rand() % 20) / 100.0f; 
            star_system->b[index] = 0.1f + (rand() % 20) / 100.0f; 
            break;
        case 3: 
            star_system->r[index] = 0.1f + (rand() % 20) / 100.0f; 
            star_system->g[index] = 0.8f + (rand() % 20) / 100.0f; 
            star_system->b[index] = 0.3f + (rand() % 30) / 100.0f; 
            break;
        case 4: 
            star_system->r[index] = 0.9f + (rand() % 10) / 100.0f; 
            star_system->g[index] = 0.5f + (rand() % 30) / 100.0f; 
            star_system->b[index] = 0.1f + (rand() % 20) / 100.0f; 
            break;
        case 5: 
            star_system->r[index] = 0.7f + (rand() % 30) / 100.0f; 
            star_system->g[index] = 0.2f + (rand() % 20) / 100.0f; 
            star_system->b[index] = 0.9f + (rand() % 10) / 100.0f; 
            break;
        case 6: 
            star_system->r[index] = 0.1f + (rand() % 20) / 100.0f; 
            star_system->g[index] = 0.8f + (rand() % 20) / 100.0f; 
            star_system->b[index] = 0.9f + (rand() % 10) / 100.0f; 
            break;
        default: 
            star_system->r[index] = star_system->g[index] = star_system->b[index] = 0.9f + (rand() % 10) / 100.0f; 
            break;
    }
}

void init_star(int index) {
    star_system->x[index] = (float)(rand() % WINDOW_WIDTH);
    star_system->y[index] = (float)(rand() % WINDOW_HEIGHT);
    
    float angle = (float)(rand() % 360) * PI / 180.0f;
    float speed = (float)(rand() % 30 + 10) / 1000.0f;
    star_system->vx[index] = cos(angle) * speed;
    star_system->vy[index] = sin(angle) * speed;
    
    star_system->brightness[index] = 0.6f + (float)(rand() % 40) / 100.0f;
    star_system->pulse_phase[index] = (float)(rand() % 360) * PI / 180.0f;
    star_system->pulse_speed[index] = (float)(rand() % 20 + 5) / 10000.0f;
    star_system->size[index] = 2.0f + (float)(rand() % 6);
    star_system->star_type[index] = rand() % 4;
    star_system->glow_intensity[index] = 0.5f + (float)(rand() % 50) / 100.0f;
    
    generate_star_color(index);
}

void update_spatial_grid() {
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < spatial_grid->total_cells; i++) {
        spatial_grid->cells[i].count = 0;
    }
    
    // Asignar estrellas a celdas del grid
    for (int i = 0; i < star_system->count; i++) {
        int grid_x = (int)(star_system->x[i] / GRID_SIZE);
        int grid_y = (int)(star_system->y[i] / GRID_SIZE);

        grid_x = (grid_x < 0) ? 0 : ((grid_x >= spatial_grid->width) ? spatial_grid->width - 1 : grid_x);
        grid_y = (grid_y < 0) ? 0 : ((grid_y >= spatial_grid->height) ? spatial_grid->height - 1 : grid_y);
        
        int cell_index = grid_y * spatial_grid->width + grid_x;
        star_system->grid_cell[i] = cell_index;
        #pragma omp critical
        {
            if (spatial_grid->cells[cell_index].count < spatial_grid->cells[cell_index].capacity) {
                spatial_grid->cells[cell_index].star_indices[spatial_grid->cells[cell_index].count] = i;
                spatial_grid->cells[cell_index].count++;
            }
        }
    }
}

void apply_physics_optimized() {
    const float damping = 0.98f;
    const float force_constant = 0.000005f;
    const float center_x = WINDOW_WIDTH / 2.0f;
    const float center_y = WINDOW_HEIGHT / 2.0f;
    const float two_pi = 2.0f * PI;
    const float window_width_f = (float)WINDOW_WIDTH;
    const float window_height_f = (float)WINDOW_HEIGHT;

    #pragma omp parallel for simd schedule(guided)
    for (int i = 0; i < star_system->count; i++) {
        star_system->x[i] += star_system->vx[i];
        star_system->y[i] += star_system->vy[i];
        float size = star_system->size[i];
        
        if (star_system->x[i] <= size || star_system->x[i] >= window_width_f - size) {
            star_system->vx[i] *= -damping;
            star_system->x[i] = (star_system->x[i] <= size) ? size : window_width_f - size;
        }
        
        if (star_system->y[i] <= size || star_system->y[i] >= window_height_f - size) {
            star_system->vy[i] *= -damping;
            star_system->y[i] = (star_system->y[i] <= size) ? size : window_height_f - size;
        }
        
        star_system->pulse_phase[i] += star_system->pulse_speed[i];
        if (star_system->pulse_phase[i] > two_pi) {
            star_system->pulse_phase[i] -= two_pi;
        }
        
        float dist_x = center_x - star_system->x[i];
        float dist_y = center_y - star_system->y[i];
        float distance = sqrtf(dist_x * dist_x + dist_y * dist_y);
        
        if (distance > 0.0f) {
            float inv_distance = 1.0f / distance;
            star_system->vx[i] += (dist_x * inv_distance) * force_constant;
            star_system->vy[i] += (dist_y * inv_distance) * force_constant;
        }
    }
}

void apply_star_interactions() {
    const float interaction_radius = 50.0f;
    const float interaction_strength = 0.000001f;
    
    #pragma omp parallel for schedule(dynamic) collapse(2)
    for (int gy = 0; gy < spatial_grid->height; gy++) {
        for (int gx = 0; gx < spatial_grid->width; gx++) {
            int cell_index = gy * spatial_grid->width + gx;
            GridCell* current_cell = &spatial_grid->cells[cell_index];
            for (int i = 0; i < current_cell->count; i++) {
                int star_a = current_cell->star_indices[i];
                
                for (int j = i + 1; j < current_cell->count; j++) {
                    int star_b = current_cell->star_indices[j];
                    
                    float dx = star_system->x[star_a] - star_system->x[star_b];
                    float dy = star_system->y[star_a] - star_system->y[star_b];
                    float distance_sq = dx * dx + dy * dy;
                    
                    if (distance_sq < interaction_radius * interaction_radius && distance_sq > 0.1f) {
                        float distance = sqrtf(distance_sq);
                        float inv_distance = 1.0f / distance;
                        float force = interaction_strength * inv_distance;
                        
                        // Aplicar fuerzas mutuamente
                        star_system->vx[star_a] += dx * force;
                        star_system->vy[star_a] += dy * force;
                        star_system->vx[star_b] -= dx * force;
                        star_system->vy[star_b] -= dy * force;
                    }
                }
                
                // Interacciones con celdas vecinas (solo derecha y abajo para evitar duplicados)
                for (int nx = 0; nx <= 1; nx++) {
                    for (int ny = 0; ny <= 1; ny++) {
                        if (nx == 0 && ny == 0) continue; // Misma celda ya procesada
                        
                        int neighbor_gx = gx + nx;
                        int neighbor_gy = gy + ny;
                        
                        if (neighbor_gx < spatial_grid->width && neighbor_gy < spatial_grid->height) {
                            int neighbor_index = neighbor_gy * spatial_grid->width + neighbor_gx;
                            GridCell* neighbor_cell = &spatial_grid->cells[neighbor_index];
                            
                            for (int k = 0; k < neighbor_cell->count; k++) {
                                int star_b = neighbor_cell->star_indices[k];
                                
                                float dx = star_system->x[star_a] - star_system->x[star_b];
                                float dy = star_system->y[star_a] - star_system->y[star_b];
                                float distance_sq = dx * dx + dy * dy;
                                
                                if (distance_sq < interaction_radius * interaction_radius && distance_sq > 0.1f) {
                                    float distance = sqrtf(distance_sq);
                                    float inv_distance = 1.0f / distance;
                                    float force = interaction_strength * inv_distance;
                                    
                                    star_system->vx[star_a] += dx * force;
                                    star_system->vy[star_a] += dy * force;
                                    star_system->vx[star_b] -= dx * force;
                                    star_system->vy[star_b] -= dy * force;
                                }
                            }
                        }
                    }
                }
            }
        }
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

void render_star(int index) {
    float current_brightness = star_system->brightness[index] * 
                              (0.7f + 0.3f * sinf(star_system->pulse_phase[index]));
    float r = star_system->r[index] * current_brightness;
    float g = star_system->g[index] * current_brightness;
    float b = star_system->b[index] * current_brightness;
    float x = star_system->x[index];
    float y = star_system->y[index];
    float size = star_system->size[index];
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    
    switch(star_system->star_type[index]) {
        case 0:
            draw_star_glow(x, y, size * 3, r, g, b, 0.1f * star_system->glow_intensity[index]);
            draw_star_glow(x, y, size * 2, r, g, b, 0.2f * star_system->glow_intensity[index]);
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
            draw_star_glow(x, y, size * 4, r, g, b, 0.15f * star_system->glow_intensity[index]);
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
            draw_star_glow(x, y, size * 5, r, g, b, 0.08f * star_system->glow_intensity[index]);
            draw_star_glow(x, y, size * 3, r, g, b, 0.15f * star_system->glow_intensity[index]);
            draw_star_glow(x, y, size * 1.5f, r, g, b, 0.3f * star_system->glow_intensity[index]);
            glColor3f(r, g, b);
            glBegin(GL_LINES);
            for (int i = 0; i < 8; i++) {
                float angle = (PI * 2 * i) / 8;
                float ray_length = size * (1.2f + 0.3f * sinf(star_system->pulse_phase[index] + i));
                glVertex2f(x, y);
                glVertex2f(x + cosf(angle) * ray_length, y + sinf(angle) * ray_length);
            }
            glEnd();
            glPointSize(5.0f);
            glBegin(GL_POINTS);
            glColor3f(1.0f, 1.0f, 1.0f);
            glVertex2f(x, y);
            glEnd();
            break;
            
        case 3:
            float pulse_factor = 1.0f + 0.5f * sinf(star_system->pulse_phase[index] * 2);
            draw_star_glow(x, y, size * 6 * pulse_factor, r, g, b, 0.05f * star_system->glow_intensity[index]);
            draw_star_glow(x, y, size * 3 * pulse_factor, r, g, b, 0.1f * star_system->glow_intensity[index]);
            glColor3f(r, g, b);
            glBegin(GL_LINE_LOOP);
            for (int i = 0; i < 10; i++) {
                float angle = (PI * 2 * i) / 10;
                float radius = (i % 2 == 0) ? size : size * 0.5f;
                radius *= pulse_factor;
                glVertex2f(x + cosf(angle) * radius, y + sinf(angle) * radius);
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
        snprintf(title, sizeof(title), "Screensaver Optimizado - Estrellas: %d | FPS: %.1f | Threads: %d", 
                star_system->count, fps, omp_get_max_threads());
        glutSetWindowTitle(title);
        
        printf("FPS: %.1f | Threads activos: %d\n", fps, omp_get_max_threads());
        if (fps < 30) printf("ADVERTENCIA: FPS por debajo de 30!\n");
    }
}

void display() {
    glClearColor(0.02f, 0.01f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    double start_time = omp_get_wtime();
    update_spatial_grid();
    
    apply_physics_optimized();
    
    apply_star_interactions();
    
    frame_time = omp_get_wtime() - start_time;
    for (int i = 0; i < star_system->count; i++) {
        render_star(i);
    }
    
    current_frame++;
    
    // Mostrar estadísticas cada 200 frames
    if (current_frame % 200 == 0) {
        printf("\n=== ESTADÍSTICAS DE RENDIMIENTO ===\n");
        printf("Frame %d: %.6f segundos de cálculo\n", current_frame, frame_time);
        printf("Estrellas: %d | Threads activos: %d\n", star_system->count, omp_get_max_threads());
        printf("Tiempo promedio por estrella: %.8f segundos\n", frame_time / star_system->count);
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
            destroy_star_system(star_system);
            destroy_spatial_grid(spatial_grid);
            glutDestroyWindow(window_id);
            exit(0);
            break;
            
        case '+':
            if (star_system->count < MAX_STARS - 50) {
                int old_count = star_system->count;
                star_system->count += 50;

                if (star_system->count > star_system->capacity) {
                    StarSystem* new_system = create_star_system(star_system->count);
                    memcpy(new_system->x, star_system->x, old_count * sizeof(float));
                    memcpy(new_system->y, star_system->y, old_count * sizeof(float));
                    memcpy(new_system->vx, star_system->vx, old_count * sizeof(float));
                    memcpy(new_system->vy, star_system->vy, old_count * sizeof(float));
                    memcpy(new_system->brightness, star_system->brightness, old_count * sizeof(float));
                    memcpy(new_system->pulse_phase, star_system->pulse_phase, old_count * sizeof(float));
                    memcpy(new_system->pulse_speed, star_system->pulse_speed, old_count * sizeof(float));
                    memcpy(new_system->size, star_system->size, old_count * sizeof(float));
                    memcpy(new_system->r, star_system->r, old_count * sizeof(float));
                    memcpy(new_system->g, star_system->g, old_count * sizeof(float));
                    memcpy(new_system->b, star_system->b, old_count * sizeof(float));
                    memcpy(new_system->glow_intensity, star_system->glow_intensity, old_count * sizeof(float));
                    memcpy(new_system->star_type, star_system->star_type, old_count * sizeof(int));
                    
                    destroy_star_system(star_system);
                    star_system = new_system;
                }
                #pragma omp parallel for schedule(static)
                for (int i = old_count; i < star_system->count; i++) {
                    init_star(i);
                }
            }
            break;
            
        case '-':
            if (star_system->count > 50) {
                star_system->count -= 50;
            }
            break;
            
        case 't': case 'T':
            // Toggle número de threads
            {
                int current_threads = omp_get_max_threads();
                int new_threads = (current_threads == 1) ? omp_get_num_procs() : 1;
                omp_set_num_threads(new_threads);
                printf("Threads cambiados a: %d\n", new_threads);
            }
            break;
            
        case 'b': case 'B':
            printf("\n=== OPTIMIZACIONES IMPLEMENTADAS ===\n");
            printf("Memory alignment (%d bytes) para cache efficiency\n", CACHE_LINE_SIZE);
            printf("Grid espacial (%dx%d) para optimizar interacciones O(N²)→O(N)\n", 
                   GRID_SIZE, GRID_SIZE);
            printf("Threads disponibles: %d\n", omp_get_max_threads());
            printf("Tiempo actual por frame: %.6f segundos\n", frame_time);
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
        printf("Controles:\n");
        printf("  ESC/Q: Salir\n");
        printf("  +: Agregar 50 estrellas\n");
        printf("  -: Quitar 50 estrellas\n");
        printf("  T: Toggle número de threads\n");
        printf("  B: Mostrar optimizaciones implementadas\n");
        return -1;
    }
    int n = atoi(argv[1]);
    if (n <= 0 || n > MAX_STARS) {
        printf("Error: Número de estrellas debe estar entre 1 y %d\n", MAX_STARS);
        return -1;
    }
    return n;
}

int main(int argc, char* argv[]) {
    int num_stars = validate_input(argc, argv);
    if (num_stars == -1) return 1;
    omp_set_dynamic(0);  
    omp_set_nested(1);   
    
    printf("Inicializando screensaver optimizado con %d estrellas...\n", num_stars);
    printf("Threads disponibles: %d\n", omp_get_max_threads());
    printf("Presiona 'B' para ver optimizaciones implementadas\n");
    
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_ALPHA);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutInitWindowPosition(100, 100);
    
    char title[256];
    snprintf(title, sizeof(title), "Screensaver Optimizado - Estrellas: %d | Threads: %d", 
             num_stars, omp_get_max_threads());
    window_id = glutCreateWindow(title);
    
    srand((unsigned int)time(NULL));
    star_system = create_star_system(num_stars);
    if (!star_system) {
        printf("Error: No se pudo allocar memoria para el sistema de estrellas\n");
        return 1;
    }
    
    // Crear grid espacial
    spatial_grid = create_spatial_grid();
    if (!spatial_grid) {
        printf("Error: No se pudo crear grid espacial\n");
        destroy_star_system(star_system);
        return 1;
    }
    
    printf("Inicializando %d estrellas en paralelo...\n", num_stars);
    double start_time = omp_get_wtime();
    
    #pragma omp parallel for schedule(static) num_threads(omp_get_max_threads())
    for (int i = 0; i < num_stars; i++) {
        init_star(i);
    }
    
    double init_time = omp_get_wtime() - start_time;
    printf("Inicialización completada en %.4f segundos\n", init_time);
    
    init_opengl();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(0, timer, 0);
    fps_timer = clock();
    
    glutMainLoop();

    destroy_star_system(star_system);
    destroy_spatial_grid(spatial_grid);
    return 0;
}