// dote.c — ANSIカラーのASCIIドット絵がターミナルを走るやつ (sl風)
// Build: cc dote.c -o dote
// Usage: ./dote [-s|-f|-S|-d ms] [-n count] [-y row] [-R]
//
// Options:
//   -s           slow (100ms)
//   -f           fast (40ms)
//   -S           super fast (20ms)
//   -d <ms>      任意フレーム待ち (既定 60ms)
//   -n <count>   走行回数。0 で無限（既定 1）
//   -y <row>     1始まりの描画行（既定：中央）
//   -R           左→右に走る（既定は右→左）
//
// ANSIカラーを1文字ごとに付けるので、横方向のクリッピングでも色崩れしない。
// カーソルは隠す・終了時に戻す。Ctrl-Cでも片付けて終了。

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>

static int term_cols = 80, term_rows = 24;
static int hide_cursor_active = 0;

static void get_term_size(void){
    struct winsize ws;
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)==0 && ws.ws_col>0 && ws.ws_row>0){
        term_cols = ws.ws_col; term_rows = ws.ws_row;
    }else{
        const char *c=getenv("COLUMNS"), *r=getenv("LINES");
        if(c) term_cols = atoi(c); if(r) term_rows = atoi(r);
        if(term_cols<=0) term_cols=80; if(term_rows<=0) term_rows=24;
    }
}
static void ansi_hide_cursor(void){
    if(!hide_cursor_active){ fputs("\x1b[?25l", stdout); fflush(stdout); hide_cursor_active=1; }
}
static void ansi_show_cursor(void){
    if(hide_cursor_active){ fputs("\x1b[?25h\x1b[0m", stdout); fflush(stdout); hide_cursor_active=0; }
}
static void restore_on_exit(void){
    get_term_size();
    fprintf(stdout, "\x1b[%d;1H\x1b[0m\x1b[?25h", term_rows);
    fflush(stdout);
}
static void handle_sigint(int sig){ (void)sig; restore_on_exit(); _exit(0); }
static void msleep(int ms){ if(ms<=0) return; usleep((useconds_t)ms*1000); }

/* ===== ドット絵定義 =====
   - 同寸の 'glyph'（文字）と 'color'（色コード）をフレーム毎に用意
   - color文字の意味: r g y b m c w k .（. は透明＝何もしない）
   - 幅は定数 SPR_W、行は SPR_H
   - 全部ASCIIで幅ズレしにくい記号にしてる
*/
#define SPR_H 8
#define SPR_W 46
static const char *frame_glyph[3][SPR_H]={
/* frame 0 */
{
"        __==__           ___==___            ",
"   ____/|_||_\\__  ____ /_|_____|_\\___       ",
"  / _  \\        \\|__  |  _  _   _   |\\      ",
"=/ |_|  |  ____  |  | | |_|(_)_(_)__|| \\__  ",
"|_  _  _|_|____|_|__|_|___|_____|____|____| ",
"  o  o    o  o    o   o   o  o    o    o    ",
" ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ ",
"        *   *      *   *     *   *          "
},
/* frame 1 */
{
"        __==__           ___==___            ",
"   ____/|_||_\\__  ____ /_|_____|_\\___       ",
"  / _  \\        \\|__  |  _  _   _   |\\      ",
"=/ |_|  |  ____  |  | | |_|(_)_(_)__|| \\__  ",
"|_  _  _|_|____|_|__|_|___|_____|____|____| ",
"    o    o  o    o   o   o  o    o    o     ",
" ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ ",
"      *   *    *    *   *    *              "
},
/* frame 2 */
{
"        __==__           ___==___            ",
"   ____/|_||_\\__  ____ /_|_____|_\\___       ",
"  / _  \\        \\|__  |  _  _   _   |\\      ",
"=/ |_|  |  ____  |  | | |_|(_)_(_)__|| \\__  ",
"|_  _  _|_|____|_|__|_|___|_____|____|____| ",
"  o    o    o    o   o   o  o    o    o     ",
" ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ ",
"        *      *   *    *    *    *         "
}
};

static const char *frame_color[3][SPR_H]={
/* frame 0 colors */
{
"........yyyy.. ........ yyyyyyyy ............",
".. ggggkkkkkkk.. gggg ..kwwwwwwwk.. ....     ",
". .gg.. ...... ..rr.. . rr rr rr .y.         ",
"yy rrrr  . gggg .  . . rrrrccmcmrrrr . gggg  ",
"kk kk kk kkwwww kkww kkwwwwwwwwwwwwww kkkkw ",
"  k  k    k  k    k   k   k  k    k    k    ",
" ccccccccccccccccccccccccccccccccccccccccccc ",
" ....  c   c  ..  c   c  ..  c   c  ....    "
},
/* frame 1 colors */
{
"........yyyy.. ........ yyyyyyyy ............",
".. ggggkkkkkkk.. gggg ..kwwwwwwwk.. ....     ",
". .gg.. ...... ..rr.. . rr rr rr .y.         ",
"yy rrrr  . gggg .  . . rrrrccmcmrrrr . gggg  ",
"kk kk kk kkwwww kkww kkwwwwwwwwwwwwww kkkkw ",
"    k    k  k    k   k   k  k    k    k     ",
" ccccccccccccccccccccccccccccccccccccccccccc ",
"  ..  c   c  .. c  ..  c   c  ..            "
},
/* frame 2 colors */
{
"........yyyy.. ........ yyyyyyyy ............",
".. ggggkkkkkkk.. gggg ..kwwwwwwwk.. ....     ",
". .gg.. ...... ..rr.. . rr rr rr .y.         ",
"yy rrrr  . gggg .  . . rrrrccmcmrrrr . gggg  ",
"kk kk kk kkwwww kkww kkwwwwwwwwwwwwww kkkkw ",
"  k    k    k    k   k   k  k    k    k     ",
" ccccccccccccccccccccccccccccccccccccccccccc ",
" ....  c  ..  c   c  ..  c   c  ..  c       "
}
};

/* ANSI色コード（明るめ） */
static inline const char* ansi_for(char c){
    switch(c){
        case 'k': return "\x1b[90m";  // bright black (gray)
        case 'r': return "\x1b[91m";
        case 'g': return "\x1b[92m";
        case 'y': return "\x1b[93m";
        case 'b': return "\x1b[94m";
        case 'm': return "\x1b[95m";
        case 'c': return "\x1b[96m";
        case 'w': return "\x1b[97m";
        default:  return "\x1b[0m";
    }
}

/* (row,col) 1始まり。colは画面外でもOK。1文字単位で可視範囲だけ描く */
static void draw_sprite_frame(int base_row, int base_col, int f){
    if(f<0) f=0; if(f>2) f=2;
    for(int r=0;r<SPR_H;r++){
        int screen_r = base_row + r;
        if(screen_r<1 || screen_r>term_rows) continue;

        const char *g = frame_glyph[f][r];
        const char *c = frame_color[f][r];
        int prev_col = -9999;
        char last_color = 0;

        for(int j=0;j<SPR_W;j++){
            int screen_c = base_col + j;
            if(g[j]=='\0') break;

            if(c[j]=='.' || g[j]==' '){
                continue; // 透明
            }
            if(screen_c<1 || screen_c>term_cols){
                continue; // 画面外
            }

            // カーソル移動（同じ行で離れた位置に飛ぶだけ）
            if(prev_col != screen_c){
                fprintf(stdout, "\x1b[%d;%dH", screen_r, screen_c);
                prev_col = screen_c;
            }
            // 色が変わるときだけ切り替え
            if(c[j]!=last_color){
                fputs(ansi_for(c[j]), stdout);
                last_color = c[j];
            }
            fputc(g[j], stdout);
        }
        // 行末でリセット（色残り防止）
        fputs("\x1b[0m", stdout);
    }
}

/* 前フレーム領域の簡易消去（背景に空白描き） */
static void clear_sprite_area(int base_row, int base_col){
    for(int r=0;r<SPR_H;r++){
        int screen_r = base_row + r;
        if(screen_r<1 || screen_r>term_rows) continue;

        int start_c = base_col;
        int end_c   = base_col + SPR_W - 1;
        if(end_c < 1 || start_c > term_cols) continue;
        if(start_c < 1) start_c = 1;
        if(end_c > term_cols) end_c = term_cols;

        int width = end_c - start_c + 1;
        if(width <= 0) continue;
        fprintf(stdout, "\x1b[%d;%dH\x1b[0m", screen_r, start_c);
        for(int i=0;i<width;i++) fputc(' ', stdout);
    }
}

int main(int argc, char **argv){
    int delay_ms = 60;
    int passes = 1;      // 0 = infinite
    int row = -1;        // auto center
    int reverse = 0;

    for(int i=1;i<argc;i++){
        if(!strcmp(argv[i], "-s")) delay_ms = 100;
        else if(!strcmp(argv[i], "-f")) delay_ms = 40;
        else if(!strcmp(argv[i], "-S")) delay_ms = 20;
        else if(!strcmp(argv[i], "-R")) reverse  = 1;
        else if(!strcmp(argv[i], "-d") && i+1<argc){
            delay_ms = atoi(argv[++i]); if(delay_ms<0) delay_ms=0;
        }else if(!strcmp(argv[i], "-n") && i+1<argc){
            passes = atoi(argv[++i]); if(passes<0) passes=0;
        }else if(!strcmp(argv[i], "-y") && i+1<argc){
            row = atoi(argv[++i]);
        }else if(!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")){
            fprintf(stdout,
                "Usage: %s [-s|-f|-S|-d ms] [-n count] [-y row] [-R]\n"
                "  -s / -f / -S : speed presets\n"
                "  -d <ms>      : custom frame delay (default 60)\n"
                "  -n <count>   : number of passes; 0=infinite (default 1)\n"
                "  -y <row>     : 1-based row to draw (default centered)\n"
                "  -R           : run left-to-right\n", argv[0]);
            return 0;
        }
    }

    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    atexit(restore_on_exit);

    get_term_size();
    if(row <= 0 || row > term_rows - SPR_H + 1) row = (term_rows - SPR_H)/2 + 1;

    ansi_hide_cursor();

    int run_count = 0;
    int done = 0;
    int frame = 0;

    while(!done){
        get_term_size(); // 端末サイズが変わっても追従

        int start_col = reverse ? (1 - SPR_W) : term_cols;
        int end_col   = reverse ? term_cols : (1 - SPR_W);
        int step      = reverse ? +1 : -1;

        int prev_col = start_col;

        for(int col = start_col; reverse ? (col <= end_col) : (col >= end_col); col += step){
            clear_sprite_area(row, prev_col);
            draw_sprite_frame(row, col, frame);
            fflush(stdout);
            msleep(delay_ms);
            frame = (frame + 1) % 3;
            prev_col = col;
        }
        clear_sprite_area(row, prev_col);
        fflush(stdout);

        run_count++;
        if(passes > 0 && run_count >= passes) done = 1;
    }

    return 0;
}
