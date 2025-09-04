#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

// Constantes del programa
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define MIN_CANVAS_WIDTH 640
#define MIN_CANVAS_HEIGHT 480
#define FPS_TARGET 60
#define PI 3.14159265359
#define MAX_STARS 2000

// Estructura para representar una estrella
typedef struct {
    float x, y;           // Posición actual
    float vx, vy;         // Velocidad en x e y
    float brightness;     // Brillo de la estrella (0.0 - 1.0)
    float pulse_phase;    // Fase del pulso para animación de brillo
    float pulse_speed;    // Velocidad de pulsación
    float size;           // Tamaño de la estrella
    float r, g, b;        // Colores RGB (0.0 - 1.0)
    int star_type;        // Tipo de estrella (afecta forma de renderizado)
    float glow_intensity; // Intensidad del efecto de brillo
} Star;

// Variables globales
Star* stars = NULL;
int num_stars = 0;
int window_id;
clock_t last_time;
int frame_count = 0;
float fps = 0.0f;

// Variables para control de FPS
clock_t fps_timer;
int fps_counter = 0;

// Función para generar color pseudoaleatorio brillante y saturado
void generate_star_color(Star* star) {
    int color_type = rand() % 8;
    
    switch(color_type) {
        case 0: // Azul eléctrico
            star->r = 0.2f + (rand() % 30) / 100.0f;
            star->g = 0.4f + (rand() % 40) / 100.0f;
            star->b = 0.9f + (rand() % 10) / 100.0f;
            break;
        case 1: // Rosa brillante/Magenta
            star->r = 0.9f + (rand() % 10) / 100.0f;
            star->g = 0.2f + (rand() % 30) / 100.0f;
            star->b = 0.7f + (rand() % 30) / 100.0f;
            break;
        case 2: // Amarillo/Dorado
            star->r = 0.9f + (rand() % 10) / 100.0f;
            star->g = 0.8f + (rand() % 20) / 100.0f;
            star->b = 0.1f + (rand() % 20) / 100.0f;
            break;
        case 3: // Verde esmeralda
            star->r = 0.1f + (rand() % 20) / 100.0f;
            star->g = 0.8f + (rand() % 20) / 100.0f;
            star->b = 0.3f + (rand() % 30) / 100.0f;
            break;
        case 4: // Naranja/Rojo fuego
            star->r = 0.9f + (rand() % 10) / 100.0f;
            star->g = 0.5f + (rand() % 30) / 100.0f;
            star->b = 0.1f + (rand() % 20) / 100.0f;
            break;
        case 5: // Púrpura místico
            star->r = 0.7f + (rand() % 30) / 100.0f;
            star->g = 0.2f + (rand() % 20) / 100.0f;
            star->b = 0.9f + (rand() % 10) / 100.0f;
            break;
        case 6: // Cian
            star->r = 0.1f + (rand() % 20) / 100.0f;
            star->g = 0.8f + (rand() % 20) / 100.0f;
            star->b = 0.9f + (rand() % 10) / 100.0f;
            break;
        default: // Blanco brillante
            star->r = 0.9f + (rand() % 10) / 100.0f;
            star->g = 0.9f + (rand() % 10) / 100.0f;
            star->b = 0.9f + (rand() % 10) / 100.0f;
            break;
    }
}

// Función para inicializar una estrella
void init_star(Star* star, int index) {
    // Posición inicial aleatoria
    star->x = (float)(rand() % WINDOW_WIDTH);
    star->y = (float)(rand() % WINDOW_HEIGHT);
    
    // Velocidad aleatoria con componentes trigonométricos (movimiento lento)
    float angle = (float)(rand() % 360) * PI / 180.0f;
    float speed = (float)(rand() % 30 + 10) / 1000.0f; // 0.01 - 0.04 px/frame
    star->vx = cos(angle) * speed;
    star->vy = sin(angle) * speed;
    
    // Propiedades de brillo y pulsación
    star->brightness = 0.6f + (float)(rand() % 40) / 100.0f; // 0.6 - 1.0
    star->pulse_phase = (float)(rand() % 360) * PI / 180.0f;
    star->pulse_speed = (float)(rand() % 20 + 5) / 10000.0f; // Pulsación lenta
    
    // Tamaño variable
    star->size = 2.0f + (float)(rand() % 6); // 2-7 pixels de radio
    
    // Tipo de estrella
    star->star_type = rand() % 4;
    
    // Intensidad del brillo
    star->glow_intensity = 0.5f + (float)(rand() % 50) / 100.0f;
    
    // Color pseudoaleatorio
    generate_star_color(star);
}

// Función para aplicar física de movimiento y rebote
void apply_physics(Star* star) {
    // Actualizar posición
    star->x += star->vx;
    star->y += star->vy;
    
    // Rebote en bordes con pérdida de energía
    if (star->x <= star->size || star->x >= WINDOW_WIDTH - star->size) {
        star->vx = -star->vx * 0.98f; // Pérdida mínima de energía
        if (star->x <= star->size) star->x = star->size;
        if (star->x >= WINDOW_WIDTH - star->size) star->x = WINDOW_WIDTH - star->size;
    }
    
    if (star->y <= star->size || star->y >= WINDOW_HEIGHT - star->size) {
        star->vy = -star->vy * 0.98f;
        if (star->y <= star->size) star->y = star->size;
        if (star->y >= WINDOW_HEIGHT - star->size) star->y = WINDOW_HEIGHT - star->size;
    }
    
    // Actualizar pulsación con trigonometría
    star->pulse_phase += star->pulse_speed;
    if (star->pulse_phase > 2 * PI) {
        star->pulse_phase -= 2 * PI;
    }
    
    // Atracción gravitacional sutil hacia el centro
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

// Función para dibujar estrella con efecto de brillo
void draw_star_glow(float x, float y, float size, float r, float g, float b, float alpha) {
    int segments = 16;
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(r, g, b, alpha);
    glVertex2f(x, y); // Centro
    
    for (int i = 0; i <= segments; i++) {
        float angle = 2.0f * PI * i / segments;
        float px = x + cos(angle) * size;
        float py = y + sin(angle) * size;
        glColor4f(r, g, b, 0.0f); // Transparente en los bordes
        glVertex2f(px, py);
    }
    glEnd();
}

// Función para renderizar diferentes tipos de estrellas
void render_star(Star* star) {
    float current_brightness = star->brightness * (0.7f + 0.3f * sin(star->pulse_phase));
    float r = star->r * current_brightness;
    float g = star->g * current_brightness;
    float b = star->b * current_brightness;
    
    float x = star->x;
    float y = star->y;
    float size = star->size;
    
    // Habilitar blending para efectos de brillo
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    
    switch(star->star_type) {
        case 0: // Estrella cruz simple con brillo
            // Efecto de brillo externo
            draw_star_glow(x, y, size * 3, r, g, b, 0.1f * star->glow_intensity);
            draw_star_glow(x, y, size * 2, r, g, b, 0.2f * star->glow_intensity);
            
            // Cruz principal
            glColor3f(r, g, b);
            glBegin(GL_LINES);
            glVertex2f(x - size, y);
            glVertex2f(x + size, y);
            glVertex2f(x, y - size);
            glVertex2f(x, y + size);
            glEnd();
            
            // Centro brillante
            glPointSize(3.0f);
            glBegin(GL_POINTS);
            glColor3f(r * 1.2f, g * 1.2f, b * 1.2f);
            glVertex2f(x, y);
            glEnd();
            break;
            
        case 1: // Estrella de 6 puntas
            // Efecto de brillo
            draw_star_glow(x, y, size * 4, r, g, b, 0.15f * star->glow_intensity);
            
            glColor3f(r, g, b);
            glBegin(GL_LINES);
            // Cruz principal
            glVertex2f(x - size, y);
            glVertex2f(x + size, y);
            glVertex2f(x, y - size);
            glVertex2f(x, y + size);
            // Diagonales
            float diag = size * 0.7f;
            glVertex2f(x - diag, y - diag);
            glVertex2f(x + diag, y + diag);
            glVertex2f(x - diag, y + diag);
            glVertex2f(x + diag, y - diag);
            glEnd();
            
            // Centro
            glPointSize(4.0f);
            glBegin(GL_POINTS);
            glColor3f(1.0f, 1.0f, 1.0f);
            glVertex2f(x, y);
            glEnd();
            break;
            
        case 2: // Círculo brillante con rayos
            // Múltiples capas de brillo
            draw_star_glow(x, y, size * 5, r, g, b, 0.08f * star->glow_intensity);
            draw_star_glow(x, y, size * 3, r, g, b, 0.15f * star->glow_intensity);
            draw_star_glow(x, y, size * 1.5f, r, g, b, 0.3f * star->glow_intensity);
            
            // Rayos
            glColor3f(r, g, b);
            glBegin(GL_LINES);
            for (int i = 0; i < 8; i++) {
                float angle = (PI * 2 * i) / 8;
                float ray_length = size * (1.2f + 0.3f * sin(star->pulse_phase + i));
                glVertex2f(x, y);
                glVertex2f(x + cos(angle) * ray_length, y + sin(angle) * ray_length);
            }
            glEnd();
            
            // Centro súper brillante
            glPointSize(5.0f);
            glBegin(GL_POINTS);
            glColor3f(1.0f, 1.0f, 1.0f);
            glVertex2f(x, y);
            glEnd();
            break;
            
        case 3: // Estrella pulsante compleja
            // Brillo variable con múltiples capas
            float pulse_factor = 1.0f + 0.5f * sin(star->pulse_phase * 2);
            draw_star_glow(x, y, size * 6 * pulse_factor, r, g, b, 0.05f * star->glow_intensity);
            draw_star_glow(x, y, size * 3 * pulse_factor, r, g, b, 0.1f * star->glow_intensity);
            
            // Forma de estrella más compleja
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

// Función para mostrar FPS en pantalla
void display_fps() {
    fps_counter++;
    clock_t current_time = clock();
    
    if (current_time - fps_timer >= CLOCKS_PER_SEC) {
        fps = (float)fps_counter / ((current_time - fps_timer) / (float)CLOCKS_PER_SEC);
        fps_counter = 0;
        fps_timer = current_time;
        
        // Mostrar FPS en título de ventana
        char title[256];
        snprintf(title, sizeof(title), "Screensaver OpenGL - Estrellas: %d | FPS: %.1f", num_stars, fps);
        glutSetWindowTitle(title);
        
        // También en consola para debugging
        printf("FPS: %.1f\n", fps);
        if (fps < 30) {
            printf("  ADVERTENCIA: FPS por debajo de 30!\n");
        }
    }
}

// Función de renderizado principal de OpenGL
void display() {
    // Limpiar buffer con fondo negro espacial
    glClearColor(0.02f, 0.01f, 0.05f, 1.0f); // Azul muy oscuro
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Actualizar y renderizar cada estrella (SECUENCIAL - perfecto para OpenMP)
    for (int i = 0; i < num_stars; i++) {
        apply_physics(&stars[i]);
        render_star(&stars[i]);
    }
    
    display_fps();
    
    glutSwapBuffers();
}

// Función de reshape para mantener aspecto
void reshape(int width, int height) {
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, width, 0, height);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

// Función para manejar input del teclado
void keyboard(unsigned char key, int x, int y) {
    switch(key) {
        case 27: // ESC
        case 'q':
        case 'Q':
            printf("\nCerrando screensaver...\n");
            if (stars) free(stars);
            glutDestroyWindow(window_id);
            exit(0);
            break;
        case '+':
            // Añadir más estrellas dinámicamente
            if (num_stars < MAX_STARS - 50) {
                stars = realloc(stars, (num_stars + 50) * sizeof(Star));
                for (int i = num_stars; i < num_stars + 50; i++) {
                    init_star(&stars[i], i);
                }
                num_stars += 50;
                printf("Estrellas aumentadas a: %d\n", num_stars);
            }
            break;
        case '-':
            // Reducir estrellas
            if (num_stars > 50) {
                num_stars -= 50;
                stars = realloc(stars, num_stars * sizeof(Star));
                printf("Estrellas reducidas a: %d\n", num_stars);
            }
            break;
    }
}

// Timer para animación constante
void timer(int value) {
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0); // ~60 FPS
}

// Función de inicialización de OpenGL
void init_opengl() {
    glEnable(GL_POINT_SMOOTH);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.0f);
    
    // Configurar blending para efectos de brillo
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// Validación de argumentos con programación defensiva
int validate_input(int argc, char* argv[]) {
    if (argc != 2) {
        printf("═══════════════════════════════════════════════════════════\n");
        printf("       SCREENSAVER OPENGL - ESTRELLAS BRILLANTES\n");
        printf("═══════════════════════════════════════════════════════════\n");
        printf("Uso: %s <numero_de_estrellas>\n", argv[0]);
        printf("Ejemplo: %s 200\n", argv[0]);
        printf("\nRango recomendado: 50-1000 estrellas\n");
        printf("Controles:\n");
        printf("  ESC/Q - Salir\n");
        printf("  +     - Añadir 50 estrellas\n");
        printf("  -     - Quitar 50 estrellas\n");
        printf("═══════════════════════════════════════════════════════════\n");
        return -1;
    }
    
    int n = atoi(argv[1]);
    
    if (n <= 0) {
        printf("Error: El número de estrellas debe ser un entero positivo.\n");
        return -1;
    }
    
    if (n > MAX_STARS) {
        printf("Error: Máximo permitido: %d estrellas.\n", MAX_STARS);
        return -1;
    }
    
    if (n > 1000) {
        printf("Advertencia: %d estrellas pueden reducir el rendimiento.\n", n);
    }
    
    return n;
}

// Función principal
int main(int argc, char* argv[]) {
    // Marcar tiempo de inicio de inicialización
    clock_t start_init_time = clock();
    
    // Validar argumentos
    num_stars = validate_input(argc, argv);
    if (num_stars == -1) {
        return 1;
    }
    
    printf("🌟 Iniciando screensaver OpenGL con %d estrellas...\n", num_stars);
    
    // Inicializar GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_ALPHA);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutInitWindowPosition(100, 100);
    
    char title[256];
    snprintf(title, sizeof(title), "Screensaver OpenGL - Estrellas: %d", num_stars);
    window_id = glutCreateWindow(title);
    
    // Inicializar generador aleatorio
    srand((unsigned int)time(NULL));
    
    // Asignar memoria para estrellas
    stars = (Star*)malloc(num_stars * sizeof(Star));
    if (!stars) {
        printf("Error: No se pudo asignar memoria para %d estrellas.\n", num_stars);
        return 1;
    }
    
    // Marcar tiempo de inicio de inicialización de estrellas
    clock_t start_stars_init = clock();
    
    // Inicializar estrellas
    printf("Inicializando %d estrellas...\n", num_stars);
    for (int i = 0; i < num_stars; i++) {
        init_star(&stars[i], i);
    }
    
    // Calcular tiempo de inicialización de estrellas
    clock_t end_stars_init = clock();
    
    // Configurar OpenGL
    init_opengl();
    
    // Configurar callbacks
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(0, timer, 0);
    
    fps_timer = clock();
    
    // Calcular tiempo total de inicialización
    clock_t end_init_time = clock();
    double total_init_time = ((double)(end_init_time - start_init_time)) / CLOCKS_PER_SEC;
    double stars_init_time = ((double)(end_stars_init - start_stars_init)) / CLOCKS_PER_SEC;
    double time_per_star = (stars_init_time * 1000000.0) / num_stars; // En microsegundos
    
    printf("════════════════════════════════════════════════════════════\n");
    printf("Inicialización completada en %.3f segundos\n", total_init_time);
    printf("Tiempo promedio por estrella: %.2f microsegundos\n", time_per_star);
    printf("════════════════════════════════════════════════════════════\n");
    ;
    printf("Resolución: %dx%d pixels\n", WINDOW_WIDTH, WINDOW_HEIGHT);
    printf("FPS objetivo: %d\n", FPS_TARGET);
    printf("Controles: ESC para salir, +/- para ajustar estrellas\n\n");
    
    // Iniciar bucle principal
    glutMainLoop();
    if (stars) free(stars);
    return 0;
}