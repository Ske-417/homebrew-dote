// meteor.c — Full-screen ANSI "dot" meteor shower (2 spaces = 1 pixel)
// Build: clang meteor.c -o meteor
// Run  : ./meteor   (press 'q' to quit)
// (c) 2025, MIT-like.

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
#include <math.h>

#define VERSION "0.1.0"

typedef struct { unsigned short bg; } Cell;

static struct termios orig_term;
static int term_rows, term_cols;   // characters
static int grid_rows, grid_cols;   // "dot" cells (2 cols = 1)
static int running = 1;

static void get_term_size() {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        term_rows = ws.ws_row > 0 ? ws.ws_row : 24;
        term_cols = ws.ws_col > 0 ? ws.ws_col : 80;
    } else { term_rows = 24; term_cols = 80; }
    grid_rows = term_rows;
    grid_cols = term_cols / 2;
    if (grid_cols < 16) grid_cols = 16;
    if (grid_rows < 16) grid_rows = 16;
}

static void sig_winch(int sig){ (void)sig; get_term_size(); }

static void term_restore(void){
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
    write(STDOUT_FILENO, "\033[?25l\033[2J", 10); // hide cursor & clear
}

// ---------- Frame buffer ----------
static Cell *fb = NULL;
static void fb_alloc(){ fb = (Cell*)realloc(fb, sizeof(Cell)*grid_rows*grid_cols); }
static inline void pset(int y, int x, unsigned short bg){
    if (x<0 || y<0 || x>=grid_cols || y>=grid_rows) return;
    fb[y*grid_cols + x].bg = bg;
}
static void fb_clear(unsigned short bg){
    for (int i=0;i<grid_rows*grid_cols;i++) fb[i].bg = bg;
}
static void render_to_terminal(){
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
            write(STDOUT_FILENO, "  ", 2);
        }
        write(STDOUT_FILENO, "\033[0m\n", 5);
        prev = 0xFFFF;
    }
}

// ---------- Starfield ----------
typedef struct { int x,y; int speed; unsigned short color; } Star;
static Star *stars=NULL; static int star_count=0;

static unsigned short pick_star_color(){
    int p[] = { 236, 237, 238, 244, 250, 251, 252, 189, 195 };
    return p[rand()% (int)(sizeof(p)/sizeof(p[0]))];
}
static void star_spawn(int i){
    stars[i].x = rand()%grid_cols;
    stars[i].y = rand()%grid_rows;
    stars[i].speed = 1 + rand()%3; // slow parallax
    stars[i].color = pick_star_color();
}
static void stars_init(){
    int target = (grid_rows*grid_cols)/70; // density
    if (target < 40) target = 40;
    if (target > 1600) target = 1600;
    star_count = target;
    stars = (Star*)realloc(stars, sizeof(Star)*star_count);
    for (int i=0;i<star_count;i++) star_spawn(i);
}
static void stars_update(){
    for (int i=0;i<star_count;i++){
        // gentle drift to left for-depth feel
        if ((rand()%5)==0) stars[i].x -= stars[i].speed==1?0:1;
        if (stars[i].x<0){ stars[i].x=grid_cols-1; stars[i].y=rand()%grid_rows; }
    }
}
static void stars_draw(){
    for (int i=0;i<star_count;i++) pset(stars[i].y, stars[i].x, stars[i].color);
}

// ---------- Meteors ----------
typedef struct {
    float x, y;     // subcell position
    float vx, vy;   // velocity (cells/frame)
    int len;        // trail length
    unsigned short head, tail1, tail2;
    int alive;
} Meteor;

static Meteor *mets=NULL; static int met_count=0;

static void meteor_spawn(Meteor *m){
    // spawn near top-right, fly to bottom-left (diagonal)
    int side = rand()%3; // 0: top, 1: right, 2: top-right corner
    if (side==0){ m->x = rand()%grid_cols; m->y = -3; }
    else if (side==1){ m->x = grid_cols + 3; m->y = rand()%grid_rows; }
    else { m->x = grid_cols + 2; m->y = -2; }
    float speed = 0.8f + (rand()%120)/100.0f; // 0.8..2.0
    float angle = (35 + rand()%20) * M_PI/180.0f; // 35..55deg
    m->vx = -cosf(angle) * speed;
    m->vy =  sinf(angle) * speed;
    m->len = 4 + rand()%6; // 4..9
    // palette: warm fire
    m->head = 208 + rand()%8; // 208..215
    m->tail1 = 166 + rand()%10; // orange-ish
    m->tail2 = 94 + rand()%20;  // dimmer
    // 1/12 chance: fireball
    if (rand()%12==0){ m->len += 5; m->head=196; m->tail1=202; m->tail2=220; }
    m->alive = 1;
}
static void meteors_init(){
    met_count = (grid_rows*grid_cols)/900 + 10; // scalable count
    if (met_count<12) met_count=12;
    if (met_count>80) met_count=80;
    mets = (Meteor*)realloc(mets, sizeof(Meteor)*met_count);
    for (int i=0;i<met_count;i++) meteor_spawn(&mets[i]);
}
static void meteors_update(){
    for (int i=0;i<met_count;i++){
        if (!mets[i].alive) meteor_spawn(&mets[i]);
        mets[i].x += mets[i].vx;
        mets[i].y += mets[i].vy;
        if (mets[i].x < -10 || mets[i].y > grid_rows+10) meteor_spawn(&mets[i]);
    }
}
static void meteors_draw(){
    for (int i=0;i<met_count;i++){
        Meteor *m=&mets[i];
        // head
        int hx=(int)(m->x), hy=(int)(m->y);
        pset(hy, hx, m->head);
        // trail along reverse direction
        float tx=m->x, ty=m->y;
        for (int k=1;k<=m->len;k++){
            tx -= m->vx*0.9f;
            ty -= m->vy*0.9f;
            int ix=(int)tx, iy=(int)ty;
            unsigned short col = (k< m->len/3)? m->tail1 : m->tail2;
            pset(iy, ix, col);
            // slight thickness for bright trails
            if (k<3){ pset(iy, ix-1, col); pset(iy+1, ix, col); }
        }
    }
}

// ---------- Input ----------
static int key_q_pressed(){
    char ch; ssize_t n = read(STDIN_FILENO, &ch, 1);
    return (n==1 && (ch=='q'||ch=='Q')) ? 1 : 0;
}

int main(int argc, char**argv){
    if (argc>1 && (!strcmp(argv[1],"--version")||!strcmp(argv[1],"-v"))){
        printf("meteor %s\n", VERSION); return 0;
    }

    srand((unsigned)time(NULL));
    get_term_size();
    signal(SIGWINCH, sig_winch);
    term_setup();
    fb_alloc();
    stars_init();
    meteors_init();

    int t=0;
    while (running){
        static int pr=-1, pc=-1;
        if (grid_rows!=pr || grid_cols!=pc){
            fb_alloc(); stars_init(); meteors_init();
            pr=grid_rows; pc=grid_cols;
            write(STDOUT_FILENO, "\033[2J", 4);
        }
        fb_clear(16);          // deep background
        stars_update();
        stars_draw();
        meteors_update();
        meteors_draw();
        render_to_terminal();

        if (key_q_pressed()) break;
        usleep(50000); // ~20 FPS
        t++;
    }
    return 0;
}
