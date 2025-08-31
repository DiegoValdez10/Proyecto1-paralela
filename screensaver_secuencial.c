#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

// Constantes del programa
#define CANVAS_WIDTH 80    // Ancho en caracteres
#define CANVAS_HEIGHT 40   // Alto en caracteres
#define MIN_CANVAS_WIDTH 640/8   // Cumple requisito mínimo escalado
#define MIN_CANVAS_HEIGHT 480/12 // Cumple requisito mínimo escalado
#define FPS_TARGET 10  // Reducido de 30 a 10 FPS para movimiento más lento
#define PI 3.14159265359
#define MAX_STARS 1000

// Códigos ANSI para colores y control de terminal
#define CLEAR_SCREEN "\033[2J"
#define CURSOR_HOME "\033[H"
#define HIDE_CURSOR "\033[?25l"
#define SHOW_CURSOR "\033[?25h"

// Colores ANSI
#define COLOR_RED "\033[91m"
#define COLOR_GREEN "\033[92m"
#define COLOR_YELLOW "\033[93m"
#define COLOR_BLUE "\033[94m"
#define COLOR_MAGENTA "\033[95m"
#define COLOR_CYAN "\033[96m"
#define COLOR_WHITE "\033[97m"
#define COLOR_RESET "\033[0m"

// Estructura para representar una estrella
typedef struct {
    float x, y;           // Posición actual (coordenadas float para precisión)
    float vx, vy;         // Velocidad en x e y
    float brightness;     // Brillo de la estrella (0.0 - 1.0)
    float pulse_phase;    // Fase del pulso para animación de brillo
    float pulse_speed;    // Velocidad de pulsación
    float size;           // Tamaño de la estrella
    int color_code;       // Código de color (0-6)
    char symbol;          // Símbolo a mostrar
} Star;

// Variables globales
Star* stars = NULL;
int num_stars = 0;
char canvas[CANVAS_HEIGHT][CANVAS_WIDTH + 1]; // +1 para null terminator
char color_canvas[CANVAS_HEIGHT][CANVAS_WIDTH + 1];

// Array de colores disponibles
const char* colors[] = {
    COLOR_RED, COLOR_GREEN, COLOR_YELLOW, 
    COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN, COLOR_WHITE
};

// Símbolos para diferentes tipos de estrellas
const char star_symbols[] = {'*', '+', 'x', 'o', '#', '@', '.'};

// Función para limpiar el canvas
void clear_canvas() {
    for (int y = 0; y < CANVAS_HEIGHT; y++) {
        for (int x = 0; x < CANVAS_WIDTH; x++) {
            canvas[y][x] = ' ';
            color_canvas[y][x] = 0; // Sin color
        }
        canvas[y][CANVAS_WIDTH] = '\0';
        color_canvas[y][CANVAS_WIDTH] = '\0';
    }
}

// Función para inicializar una estrella con propiedades aleatorias
void init_star(Star* star, int index) {
    // Posición inicial aleatoria
    star->x = (float)(rand() % (CANVAS_WIDTH - 2) + 1);
    star->y = (float)(rand() % (CANVAS_HEIGHT - 2) + 1);
    
    // Velocidad aleatoria con componentes trigonométricos (ultra lenta)
    float angle = (float)(rand() % 360) * PI / 180.0f;
    float speed = (float)(rand() % 1 + 1) / 100000.0f; // 0.00005 - 0.00025 unidades/frame
    star->vx = cos(angle) * speed;
    star->vy = sin(angle) * speed;
    
    // Propiedades de brillo y pulsación (ultra lenta)
    star->brightness = (float)(rand() % 50 + 50) / 100.0f; // 0.5 - 1.0
    star->pulse_phase = (float)(rand() % 360) * PI / 180.0f;
    star->pulse_speed = (float)(rand() % 15 + 5) / 50000.0f; // Pulsación ultra lenta
    
    // Tamaño (afecta intensidad del brillo)
    star->size = (float)(rand() % 3 + 1); // 1-3
    
    // Color aleatorio
    star->color_code = rand() % 7;
    
    // Símbolo aleatorio
    star->symbol = star_symbols[rand() % 7];
}

// Función para aplicar física de rebote en los bordes
void apply_physics(Star* star) {
    // Actualizar posición basada en velocidad
    star->x += star->vx;
    star->y += star->vy;
    
    // Rebote en bordes con pérdida de energía (física realista)
    if (star->x <= 1 || star->x >= CANVAS_WIDTH - 2) {
        star->vx = -star->vx * 0.95f; // Pérdida de energía del 5%
        // Corregir posición para evitar que se salga
        if (star->x <= 1) star->x = 1;
        if (star->x >= CANVAS_WIDTH - 2) star->x = CANVAS_WIDTH - 2;
    }
    
    if (star->y <= 1 || star->y >= CANVAS_HEIGHT - 2) {
        star->vy = -star->vy * 0.95f; // Pérdida de energía del 5%
        // Corregir posición para evitar que se salga
        if (star->y <= 1) star->y = 1;
        if (star->y >= CANVAS_HEIGHT - 2) star->y = CANVAS_HEIGHT - 2;
    }
    
    // Actualizar fase de pulsación usando trigonometría
    star->pulse_phase += star->pulse_speed;
    if (star->pulse_phase > 2 * PI) {
        star->pulse_phase -= 2 * PI;
    }
    
    // Añadir pequeña aceleración gravitacional hacia el centro (efecto atractivo muy sutil)
    float center_x = CANVAS_WIDTH / 2.0f;
    float center_y = CANVAS_HEIGHT / 2.0f;
    float dist_x = center_x - star->x;
    float dist_y = center_y - star->y;
    float distance = sqrt(dist_x * dist_x + dist_y * dist_y);
    
    // Aplicar fuerza muy pequeña hacia el centro
    if (distance > 0) {
        float force = 0.0000001f; // Fuerza casi imperceptible
        star->vx += (dist_x / distance) * force;
        star->vy += (dist_y / distance) * force;
    }
}

// Función para dibujar una estrella en el canvas
void draw_star_to_canvas(Star* star) {
    // Calcular brillo actual usando función seno para pulsación suave
    float current_brightness = star->brightness * (0.6f + 0.4f * sin(star->pulse_phase));
    
    int x = (int)round(star->x);
    int y = (int)round(star->y);
    
    // Verificar límites del canvas
    if (x >= 0 && x < CANVAS_WIDTH && y >= 0 && y < CANVAS_HEIGHT) {
        // Determinar símbolo basado en brillo
        char symbol;
        if (current_brightness > 0.8f) {
            symbol = '*'; // Muy brillante
        } else if (current_brightness > 0.6f) {
            symbol = '+'; // Brillante
        } else if (current_brightness > 0.4f) {
            symbol = '.'; // Medio
        } else {
            symbol = ' '; // Muy tenue, casi invisible
        }
        
        // Solo dibujar si hay brillo suficiente
        if (symbol != ' ') {
            canvas[y][x] = symbol;
            color_canvas[y][x] = star->color_code + 1; // +1 porque 0 es sin color
        }
    }
}

// Función para renderizar el canvas completo en terminal
void render_canvas() {
    // Limpiar pantalla y volver cursor al inicio
    printf(CLEAR_SCREEN CURSOR_HOME);
    
    // Renderizar cada línea del canvas con colores
    for (int y = 0; y < CANVAS_HEIGHT; y++) {
        for (int x = 0; x < CANVAS_WIDTH; x++) {
            int color_code = color_canvas[y][x];
            
            if (color_code > 0) {
                printf("%s%c", colors[color_code - 1], canvas[y][x]);
            } else {
                printf(" ");
            }
        }
        printf(COLOR_RESET "\n"); // Reset color al final de línea
    }
}

// Función para calcular y mostrar FPS
void display_fps_info(double frame_time, int frame_count) {
    static double total_time = 0;
    static int fps_counter = 0;
    
    total_time += frame_time;
    fps_counter++;
    
    // Mostrar FPS cada 30 frames
    if (fps_counter >= 30) {
        double avg_fps = fps_counter / total_time;
        printf("FPS: %.1f | Frame time: %.2fms | Estrellas: %d\n", 
               avg_fps, (frame_time * 1000), num_stars);
        
        if (avg_fps < 30) {
            printf("⚠️  ADVERTENCIA: FPS por debajo de 30!\n");
        }
        
        fps_counter = 0;
        total_time = 0;
    }
}

// Función para validar argumentos de entrada
int validate_input(int argc, char* argv[]) {
    if (argc != 2) {
        printf("═══════════════════════════════════════════════════════════\n");
        printf("           SCREENSAVER - ESTRELLAS BRILLANTES\n");
        printf("═══════════════════════════════════════════════════════════\n");
        printf("Uso: %s <numero_de_estrellas>\n", argv[0]);
        printf("Ejemplo: %s 100\n", argv[0]);
        printf("\nRango recomendado: 10-500 estrellas\n");
        printf("Controles: ESC para salir\n");
        printf("═══════════════════════════════════════════════════════════\n");
        return -1;
    }
    
    int n = atoi(argv[1]);
    
    if (n <= 0) {
        printf("❌ Error: El número de estrellas debe ser un entero positivo.\n");
        return -1;
    }
    
    if (n > MAX_STARS) {
        printf("❌ Error: Máximo permitido: %d estrellas.\n", MAX_STARS);
        return -1;
    }
    
    if (n > 500) {
        printf("⚠️  Advertencia: %d estrellas pueden reducir el rendimiento.\n", n);
        printf("¿Continuar? (y/n): ");
        char response;
        scanf(" %c", &response);
        if (response != 'y' && response != 'Y') {
            return -1;
        }
    }
    
    return n;
}

// Función principal del programa
int main(int argc, char* argv[]) {
    // Validación de argumentos con programación defensiva
    num_stars = validate_input(argc, argv);
    if (num_stars == -1) {
        return 1;
    }
    
    printf("\n🌟 Iniciando screensaver con %d estrellas...\n", num_stars);
    
    // Inicializar generador de números aleatorios
    srand((unsigned int)time(NULL));
    
    // Asignar memoria para las estrellas
    stars = (Star*)malloc(num_stars * sizeof(Star));
    if (!stars) {
        printf("❌ Error: No se pudo asignar memoria para %d estrellas.\n", num_stars);
        return 1;
    }
    
    // Inicializar todas las estrellas
    printf("⚙️  Inicializando estrellas...\n");
    for (int i = 0; i < num_stars; i++) {
        init_star(&stars[i], i);
    }
    
    // Ocultar cursor para mejor experiencia visual
    printf(HIDE_CURSOR);
    
    printf("✅ Screensaver iniciado!\n");
    printf("📺 Resolución: %dx%d caracteres\n", CANVAS_WIDTH, CANVAS_HEIGHT);
    printf("🎯 FPS objetivo: %d\n", FPS_TARGET);
    printf("⌨️  Presiona Ctrl+C para salir...\n\n");
    
    sleep(2); // Pausa para leer la información
    
    // Variables para control de FPS
    clock_t frame_start, frame_end;
    double frame_time;
    double target_frame_time = 1.0 / FPS_TARGET;
    int frame_count = 0;
    
    // Bucle principal del programa (versión secuencial)
    while (1) {
        frame_start = clock();
        
        // Limpiar canvas
        clear_canvas();
        
        // Actualizar y dibujar cada estrella (SECUENCIAL - ideal para paralelizar)
        for (int i = 0; i < num_stars; i++) {
            // Aplicar física y movimiento a la estrella i
            apply_physics(&stars[i]);
            
            // Dibujar la estrella i en el canvas
            draw_star_to_canvas(&stars[i]);
        }
        
        // Renderizar canvas completo en terminal
        render_canvas();
        
        // Calcular tiempo de frame y FPS
        frame_end = clock();
        frame_time = ((double)(frame_end - frame_start)) / CLOCKS_PER_SEC;
        frame_count++;
        
        // Mostrar información de rendimiento
        display_fps_info(frame_time, frame_count);
        
        // Control de FPS - pausas más largas para movimiento ultra lento
        if (frame_time < target_frame_time) {
            double sleep_time = target_frame_time - frame_time;
            usleep((useconds_t)(sleep_time * 1000000)); // usleep usa microsegundos
        } else {
            // Pausa mínima más larga para garantizar movimiento lento
            usleep(50000); // 50ms de pausa mínima
        }
    }
    
    // Esta parte nunca se ejecuta normalmente (Ctrl+C termina el programa)
    // pero es buena práctica incluir limpieza
    printf(SHOW_CURSOR COLOR_RESET);
    if (stars) {
        free(stars);
    }
    
    return 0;
}