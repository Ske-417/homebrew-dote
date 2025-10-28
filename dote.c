// spaceship.c — Full-screen ANSI "dot" animation (2 spaces = 1 pixel)
// Build: clang spaceship.c -o spaceship
// Run  : ./spaceship
// Quit : press 'q'
// (c) 2025, MIT-like. No warranties. Have fun, そーす。

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>

#define VERSION "0.1.0"

typedef struct { unsigned short bg; } Cell;

static struct termios orig_term;
static int term_rows, term_cols;        // characters
static int grid_rows, grid_cols;        // "dot" cells (2 cols = 1 cell)
static int running = 1;

static void get_term_size() {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        term_rows = ws.ws_row > 0 ? ws.ws_row : 24;
        term_cols = ws.ws_col > 0 ? ws.ws_col : 80;
    } else { term_rows = 24; term_cols = 80; }
    grid_rows = term_rows;
    grid_cols = term_cols / 2; // 2 spaces = 1 pixel
    if (grid_cols < 16) grid_cols = 16;
    if (grid_rows < 16) grid_rows = 16;
}

static void sig_winch(int sig){ (void)sig; get_term_size(); }

static void term_restore(void){
    // show cursor, reset colors
    write(STDOUT_FILENO, "\033[0m\033[?25h\033[H", 15);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term);
}

static void term_setup(void){
    tcgetattr(STDIN_FILENO, &orig_term);
    struct termios raw = orig_term;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0; raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    atexit(term_restore);
    // hide cursor & clear screen
    write(STDOUT_FILENO, "\033[?25l\033[2J", 10);
}

// ---------- Starfield ----------
typedef struct {
    int x, y;      // in grid cells
    int speed;     // 1..3
    unsigned short color; // 8-bit 256 color index
} Star;

static Star *stars = NULL;
static int star_count = 0;

static unsigned short rand_star_color(){
    // dim whites/cyans
    int palette[] = { 250, 251, 252, 189, 195, 153, 111, 147 };
    return palette[rand() % (int)(sizeof(palette)/sizeof(palette[0]))];
}

static void spawn_star(int i){
    stars[i].x = rand() % grid_cols;
    stars[i].y = rand() % grid_rows;
    stars[i].speed = 1 + rand() % 3;
    stars[i].color = rand_star_color();
}

static void init_stars(){
    // density: about one star per ~60 cells
    int target = (grid_rows * grid_cols) / 60;
    if (target < 40) target = 40;
    if (target > 1500) target = 1500;
    star_count = target;
    stars = (Star*)realloc(stars, sizeof(Star) * star_count);
    for (int i=0; i<star_count; ++i) spawn_star(i);
}

static void update_stars(){
    for (int i=0; i<star_count; ++i){
        stars[i].x -= stars[i].speed;
        if (stars[i].x < 0){
            stars[i].x = grid_cols - 1;
            stars[i].y = rand() % grid_rows;
            stars[i].speed = 1 + rand() % 3;
            stars[i].color = rand_star_color();
        }
    }
}

// ---------- Ship sprite (16x16), 0=transparent, others=256-color idx ----------
static const unsigned short SHIP_W = 16, SHIP_H = 16;
// Colors (256-color indices)
enum {
    C_TRAN = 0,
    C_HULL = 245,
    C_HULL_D = 240,
    C_GLOW1 = 33,   // window
    C_GLOW2 = 81,   // window alt
    C_NOSE = 250,
    C_STRIPE = 210,
    C_OUTLINE = 238,
    C_ENGINE = 244,
    C_THR1 = 208,   // thruster flame colors (toggle)
    C_THR2 = 203,
    C_THR3 = 214
};

// A simple chunky spaceship (16x16)
static const unsigned short ship_base[16][16] = {
 //  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
 { 0, 0, 0, 0, 0, C_OUTLINE, C_OUTLINE, C_NOSE, C_NOSE, C_OUTLINE, C_OUTLINE, 0, 0, 0, 0, 0 },
 { 0, 0, 0, 0, C_OUTLINE, C_HULL, C_NOSE, C_NOSE, C_NOSE, C_NOSE, C_HULL, C_OUTLINE, 0, 0, 0, 0 },
 { 0, 0, 0, C_OUTLINE, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_OUTLINE, 0, 0, 0 },
 { 0, 0, C_OUTLINE, C_HULL_D, C_HULL, C_HULL, C_HULL, C_GLOW1, C_GLOW1, C_HULL, C_HULL, C_HULL, C_HULL_D, C_OUTLINE, 0, 0 },
 { 0, C_OUTLINE, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_GLOW2, C_GLOW2, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_OUTLINE, 0 },
 { C_OUTLINE, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_OUTLINE },
 { C_OUTLINE, C_HULL, C_HULL, C_HULL, C_HULL, C_STRIPE, C_STRIPE, C_HULL, C_HULL, C_STRIPE, C_STRIPE, C_HULL, C_HULL, C_HULL, C_HULL, C_OUTLINE },
 { C_OUTLINE, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_OUTLINE },
 { C_OUTLINE, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_ENGINE, C_ENGINE, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_OUTLINE },
 { C_OUTLINE, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_ENGINE, C_ENGINE, C_ENGINE, C_ENGINE, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_OUTLINE },
 { C_OUTLINE, C_HULL, C_HULL, C_HULL, C_HULL, C_ENGINE, C_ENGINE, C_ENGINE, C_ENGINE, C_ENGINE, C_ENGINE, C_HULL, C_HULL, C_HULL, C_HULL, C_OUTLINE },
 { C_OUTLINE, C_HULL, C_HULL, C_HULL, C_ENGINE, C_ENGINE, C_ENGINE, C_ENGINE, C_ENGINE, C_ENGINE, C_ENGINE, C_ENGINE, C_HULL, C_HULL, C_HULL, C_OUTLINE },
 { 0, C_OUTLINE, C_HULL, C_HULL, C_HULL, C_ENGINE, C_ENGINE, C_ENGINE, C_ENGINE, C_ENGINE, C_ENGINE, C_HULL, C_HULL, C_HULL, C_OUTLINE, 0 },
 { 0, 0, C_OUTLINE, C_HULL_D, C_HULL, C_HULL, C_ENGINE, C_ENGINE, C_ENGINE, C_ENGINE, C_HULL, C_HULL, C_HULL_D, C_OUTLINE, 0, 0 },
 { 0, 0, 0, C_OUTLINE, C_OUTLINE, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_HULL, C_OUTLINE, C_OUTLINE, 0, 0, 0 },
 { 0, 0, 0, 0, 0, C_OUTLINE, C_OUTLINE, 0, 0, C_OUTLINE, C_OUTLINE, 0, 0, 0, 0, 0 },
};

// ---------- Frame buffer ----------
static Cell *fb = NULL;

static void fb_alloc(){
    fb = (Cell*)realloc(fb, sizeof(Cell)*grid_rows*grid_cols);
}

static inline void pset(int y, int x, unsigned short bg){
    if (x<0 || y<0 || x>=grid_cols || y>=grid_rows) return;
    fb[y*grid_cols + x].bg = bg;
}

static void fb_clear(unsigned short bg){
    for (int i=0;i<grid_rows*grid_cols;i++) fb[i].bg = bg;
}

static void draw_stars(){
    for (int i=0;i<star_count;i++){
        pset(stars[i].y, stars[i].x, stars[i].color);
    }
}

static void draw_ship(int cy, int cx, int thr_phase){
    // cy, cx = top-left anchor in grid cells
    // thruster flicker colors
    unsigned short thr_colors[3] = { C_THR1, C_THR3, C_THR2 };
    unsigned short thr = thr_colors[thr_phase % 3];

    for (int r=0; r<SHIP_H; ++r){
        for (int c=0; c<SHIP_W; ++c){
            unsigned short col = ship_base[r][c];
            if (col==C_TRAN) continue;
            // replace engine core rows behind with thruster flames (left side)
            // Add flame plume to the left of engine block
            int gy = cy + r;
            int gx = cx + c;
            pset(gy, gx, col);
            // flame to the left of engine blocks in lower mid rows
            if (r>=8 && r<=12 && c>=5 && c<=9 && (rand()%3==0)){
                pset(gy, gx-1, thr);
                if (rand()%2) pset(gy, gx-2, thr);
            }
        }
    }
}

static void render_to_terminal(){
    // Move cursor home
    write(STDOUT_FILENO, "\033[H", 3);
    unsigned short prev = 0xFFFF;
    char buf[64];
    for (int y=0; y<grid_rows; ++y){
        for (int x=0; x<grid_cols; ++x){
            unsigned short c = fb[y*grid_cols + x].bg;
            if (c != prev){
                int n = snprintf(buf, sizeof(buf), "\033[48;5;%dm", c);
                write(STDOUT_FILENO, buf, n);
                prev = c;
            }
            // two spaces per cell
            write(STDOUT_FILENO, "  ", 2);
        }
        // reset bg and newline
        write(STDOUT_FILENO, "\033[0m\n", 5);
        prev = 0xFFFF;
    }
}

static int key_pressed_q(){
    char ch;
    ssize_t n = read(STDIN_FILENO, &ch, 1);
    if (n == 1){
        return (ch=='q' || ch=='Q');
    }
    return 0;
}

int main(int argc, char**argv){
    if (argc>1 && strcmp(argv[1], "--version")==0){
        printf("spaceship %s\n", VERSION);
        return 0;
    }

    srand((unsigned)time(NULL));
    get_term_size();
    signal(SIGWINCH, sig_winch);
    term_setup();
    fb_alloc();
    init_stars();

    int cx = grid_cols/2 - SHIP_W/2;
    int cy = grid_rows/2 - SHIP_H/2;

    // slight bobbing
    int t = 0;
    while (running){
        // Resize-aware
        static int prev_rows=-1, prev_cols=-1;
        if (grid_rows != prev_rows || grid_cols != prev_cols){
            fb_alloc();
            init_stars();
            prev_rows = grid_rows;
            prev_cols = grid_cols;
            cx = grid_cols/2 - SHIP_W/2;
            cy = grid_rows/2 - SHIP_H/2;
            // clear screen when resized
            write(STDOUT_FILENO, "\033[2J", 4);
        }

        fb_clear(16); // background: dark gray (16)
        update_stars();
        draw_stars();

        // bobbing vertically & slight horizontal drift
        int bob = (int)(1.5 * sin(t/6.0));
        int drift = (int)(1.0 * sin(t/23.0));
        int thr_phase = (t/2) % 3;
        draw_ship(cy + bob, cx + drift, thr_phase);

        render_to_terminal();

        if (key_pressed_q()) break;
        usleep(50000); // ~20 FPS
        t++;
    }
    return 0;
}
