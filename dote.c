// dote.c — 物語つきドット絵(2スペース=1ドット)列車アニメ
// Build: cc dote.c -o dote
// Run:   ./dote [-s|-f|-S|-d ms] [-n count] [-y row]
//
// カラー記号: K,R,G,Y,B,M,C,W,.（.は透明）
// 描画は ANSI 背景色 + "  "（半角スペース2個）
// シーン: 夜明けの駅 → トンネル → 橋と川（到着＆花火）
//
// そーす向けポイント
// - 背景と列車を「合成」してからまとめ描画。背景も2フレームでチラつき演出。
// - ドット絵を書き換えたい場合は、下の BG_* と TRAIN_* を同寸で編集。

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>

#define CLAMP(a,l,h) ((a)<(l)?(l):((a)>(h)?(h):(a)))

/* ===== 端末ユーティリティ ===== */
static int term_cols=80, term_rows=24, cursor_hidden=0;
static void get_term_size(void){
    struct winsize ws;
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ,&ws)==0 && ws.ws_col>0 && ws.ws_row>0){
        term_cols=ws.ws_col; term_rows=ws.ws_row;
    }else{
        const char *c=getenv("COLUMNS"), *r=getenv("LINES");
        if(c) term_cols=atoi(c); if(r) term_rows=atoi(r);
        if(term_cols<=0) term_cols=80; if(term_rows<=0) term_rows=24;
    }
}
static void hide_cursor(void){ if(!cursor_hidden){ fputs("\x1b[?25l", stdout); fflush(stdout); cursor_hidden=1; } }
static void show_cursor(void){ if(cursor_hidden){ fputs("\x1b[?25h\x1b[0m", stdout); fflush(stdout); cursor_hidden=0; } }
static void at_exit_restore(void){ get_term_size(); fprintf(stdout,"\x1b[%d;1H\x1b[0m\x1b[?25h", term_rows); fflush(stdout); }
static void on_sig(int s){ (void)s; at_exit_restore(); _exit(0); }
static void msleep(int ms){ if(ms>0) usleep((useconds_t)ms*1000); }
static const char* bg_for(char c){
    switch(c){
        case 'K': return "\x1b[100m"; case 'R': return "\x1b[101m";
        case 'G': return "\x1b[102m"; case 'Y': return "\x1b[103m";
        case 'B': return "\x1b[104m"; case 'M': return "\x1b[105m";
        case 'C': return "\x1b[106m"; case 'W': return "\x1b[107m";
        default:  return "\x1b[0m";
    }
}

/* ===== 固定キャンバス（H×Wドット、2Wカラム使用） ===== */
#define H 16
#define W 36

/* === 列車（3フレーム）: 幅16×高さ6 === */
#define TW 16
#define TH 6
static const char *TRAIN[3][TH]={
/* f0 */{
"....KKKKKKKKKK..",
"..KKYYYYYYYYKK..",
"..KYYYYYYYYYKK..",
"..KYYRRGGRR YK..",
"..KYYYYYYYYYKK..",
"..KKKKKK..KKKK..",
},
/* f1 */{
"....KKKKKKKKKK..",
"..KKYYYYYYYYKK..",
"..KYYYYYYYYYKK..",
"..KYYRRGGRR YK..",
"..KYYYYYYYYYKK..",
"..KK..KKKK..KK..",
},
/* f2 */{
"....KKKKKKKKKK..",
"..KKYYYYYYYYKK..",
"..KYYYYYYYYYKK..",
"..KYYRRGGRR YK..",
"..KYYYYYYYYYKK..",
"..KKK....KKKKK..",
}
};

/* === 煙（3パターン）: 幅8×高さ4（列車の上に載せる） === */
#define SW 8
#define SH 4
static const char *SMOKE[3][SH]={
{
".C......",
"..C.....",
"...C....",
"....C...",
},
{
"..C.....",
"...C....",
"....C...",
".....C..",
},
{
"...C....",
"....C...",
".....C..",
"......C.",
}
};

/* === 背景 ===
 * すべて H×W。各シーン2フレーム用意し、水面・光・花火のチラつきに使用。
 */

/* シーン1: 夜明けの駅（空のグラデ＋プラットフォーム） */
static const char *BG_STATION[2][H]={
/* v0 */{
"CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC", // 空（夜明け）
"CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC",
"CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC",
"YYCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCYY",
"YYCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCYY",
"YYCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCYY",
"YYCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCYY",
"GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG", // 線路の草
"GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK", // プラットフォーム
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
"KKKKWWWWWWKKKKKKKKKKKKKKKKKKKKKKKKK", // 駅名板（白）
"KKKKWWWWWWKKKKKKKKKKKKKKKKKKKKKKKKK",
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
},
/* v1 (薄明るく) */{
"WCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCW",
"CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC",
"CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC",
"YYCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCYY",
"YYCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCYY",
"YYCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCYY",
"YYCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCYY",
"GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
"GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
"KKKKWWWWWWKKKKKKKKKKKKKKKKKKKKKKKKK",
"KKKKWWWWWWKKKKKKKKKKKKKKKKKKKKKKKKK",
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
}
};

/* シーン2: トンネル（暗転＋坑口） */
static const char *BG_TUNNEL[2][H]={
/* v0 */{
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
"KKKKKKKCCCCCCCCCCCCCCCCCCCCCKKKKKKK",
"KKKKKKCCCCCCCCCCCCCCCCCCCCCCCKKKKKK",
"KKKKKCCCCCCCCCCCCCCCCCCCCCCCCCKKKKK",
"KKKKCCCCCCCCCCCCCCCCCCCCCCCCCCCKKKK",
"KKKKCCCCCCCCCCCCCCCCCCCCCCCCCCCKKKK",
"KKKKKCCCCCCCCCCCCCCCCCCCCCCCCCKKKKK",
"KKKKKKCCCCCCCCCCCCCCCCCCCCCCCKKKKKK",
"KKKKKKKCCCCCCCCCCCCCCCCCCCCCKKKKKKK",
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
},
/* v1 (わずかにヘッドライトが反射) */{
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
"KKKKKKKCYCCCCCCCCCCCCCCCCCCCYKKKKKK",
"KKKKKKCCCCYCCCCCCCCCCCCCCCYCCKKKKK",
"KKKKKCCCCCCYCCCCCCCCCCCCYCCCCKKKKK",
"KKKKCCCCCCCCCYCCCCCCCCYCCCCCCCKKKK",
"KKKKCCCCCCCCCCYCCCCCCYCCCCCCCCKKKK",
"KKKKKCCCCCCCCCCYCCCCYCCCCCCCCKKKKK",
"KKKKKKCCCCCCCCCCYCCYCCCCCCCCC KKKKK",
"KKKKKKKCCCCCCCCCCYYCCCCCCCCCKKKKKK",
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
}
};

/* シーン3: 橋と川（川のきらめき、最後に花火） */
static const char *BG_BRIDGE[2][H]={
/* v0 */{
"CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC",
"CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC",
"CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC",
"CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC",
"....................................",
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK", // 橋
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
"....................................",
"BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB", // 川
"BCBCBCBCBCBCBCBCBCBCBCBCBCBCBCBCBCB",
"BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB",
"BCBCBCBCBCBCBCBCBCBCBCBCBCBCBCBCBCB",
"....................................",
".M......Y.....C....M.....Y.....C....", // 花火ベース
"....................................",
"....................................",
},
/* v1 (水面と花火の点滅) */{
"CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC",
"CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC",
"CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC",
"CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC",
"....................................",
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
"KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
"....................................",
"BCBCBCBCBCBCBCBCBCBCBCBCBCBCBCBCBCB",
"BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB",
"BCBCBCBCBCBCBCBCBCBCBCBCBCBCBCBCBCB",
"BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB",
"....................................",
".Y..M......C....Y.....C....M........",
"....................................",
"....................................",
}
};

/* ===== 合成バッファ ===== */
static char CANVAS[H][W+1]; // 1行終端 '\0'

/* 背景コピー v:0/1 */
static void copy_bg(const char *bg[H], int v){
    (void)v;
    for(int r=0;r<H;r++){
        memcpy(CANVAS[r], bg[r], W);
        CANVAS[r][W]='\0';
    }
}

/* 列車＋煙をオーバーレイ（x,y はキャンバス内左上） */
static void blit_sprite(int x, int y, int f){
    /* 煙（列車の上に少し前方へ） */
    int sf = f%3;
    for(int r=0;r<SH;r++){
        for(int c=0;c<SW;c++){
            char px = SMOKE[sf][r][c];
            if(px=='.') continue;
            int rr = y + r - 3;   // 列車より上
            int cc = x + c + 4;   // 少し前へ
            if(rr>=0 && rr<H && cc>=0 && cc<W) CANVAS[rr][cc] = px;
        }
    }
    /* 列車本体 */
    for(int r=0;r<TH;r++){
        for(int c=0;c<TW;c++){
            char px = TRAIN[sf][r][c];
            if(px=='.') continue;
            int rr = y + r;
            int cc = x + c;
            if(rr>=0 && rr<H && cc>=0 && cc<W) CANVAS[rr][cc] = px;
        }
    }
}

/* キャンバスを2スペースドットで描く（top,left は端末上の座標・1始まり） */
static void draw_canvas_at(int top, int left){
    for(int r=0;r<H;r++){
        int row = top + r;
        if(row<1 || row>term_rows) continue;
        int col = left;
        if(col<1) col=1;
        fprintf(stdout, "\x1b[%d;%dH", row, col);
        /* 左から右へ、色塗り替えを最小化 */
        char last='?';
        for(int c=0;c<W;c++){
            char px = CANVAS[r][c];
            if(px=='.'){ fputs("\x1b[0m  ", stdout); last='.'; continue; }
            if(px!=last){ fputs(bg_for(px), stdout); last=px; }
            fputs("  ", stdout);
        }
        fputs("\x1b[0m", stdout);
    }
}

/* ===== アニメーション制御 ===== */

typedef struct {
    const char *(*bg)[H];   // [2][H]
    int start_x, end_x, step; // 列車x（キャンバス内）
    int train_y;              // 列車y
    int frames;               // 通過に要する描画フレーム数(=移動回数)
} Scene;

/* 背景配列を取り出すヘルパ */
static void get_bg_ptrs(const char *dst[H], int scene_variant, int variant){
    (void)scene_variant; (void)variant; (void)dst;
}

/* 実際のシーン定義 */
static const char *(*BG1)[H] = BG_STATION;
static const char *(*BG2)[H] = BG_TUNNEL;
static const char *(*BG3)[H] = BG_BRIDGE;

static Scene SCENES[] = {
    // シーン1: 駅（左から発車→右へ）
    { BG_STATION,  -TW, W-4, +1, H-8, (W-4) - (-TW) + 1 },
    // シーン2: トンネル（右→左へ戻る: 進行感の演出）
    { BG_TUNNEL,   W-2, -TW,  -1, H-8, (W-2) - (-TW) + 1 },
    // シーン3: 橋と川（左→右、最後は花火）
    { BG_BRIDGE,   -TW, W-6, +1, H-8, (W-6) - (-TW) + 1 },
};
static const int NUM_SCENES = sizeof(SCENES)/sizeof(SCENES[0]);

int main(int argc, char **argv){
    int delay_ms=70, passes=1, row=-1;
    for(int i=1;i<argc;i++){
        if(!strcmp(argv[i],"-s")) delay_ms=110;
        else if(!strcmp(argv[i],"-f")) delay_ms=45;
        else if(!strcmp(argv[i],"-S")) delay_ms=25;
        else if(!strcmp(argv[i],"-d") && i+1<argc){ delay_ms=atoi(argv[++i]); if(delay_ms<0) delay_ms=0; }
        else if(!strcmp(argv[i],"-n") && i+1<argc){ passes=atoi(argv[++i]); if(passes<0) passes=0; }
        else if(!strcmp(argv[i],"-y") && i+1<argc){ row=atoi(argv[++i]); }
        else if(!strcmp(argv[i],"-h")||!strcmp(argv[i],"--help")){
            fprintf(stdout,
            "Usage: %s [-s|-f|-S|-d ms] [-n count] [-y row]\n"
            "  2スペース=1ドットのカラー背景で物語つき列車が走る。\n", argv[0]);
            return 0;
        }
    }

    signal(SIGINT,on_sig); signal(SIGTERM,on_sig); atexit(at_exit_restore);
    get_term_size(); if(row<=0 || row>term_rows-H+1) row=(term_rows-H)/2+1;
    int left = (term_cols - (W*2))/2 + 1; left = CLAMP(left,1,term_cols- (W*2) + 1);

    hide_cursor();

    int loop=0;
    while(passes==0 || loop<passes){
        for(int s=0;s<NUM_SCENES;s++){
            Scene sc = SCENES[s];
            int x = sc.start_x;
            int frames = sc.frames;
            for(int i=0;i<frames;i++, x+=sc.step){
                get_term_size(); // サイズ変化に追随
                left = (term_cols - (W*2))/2 + 1; left = CLAMP(left,1,term_cols- (W*2) + 1);
                int f = i % 3;        // 列車・煙フレーム
                int bv = (i/2) % 2;   // 背景点滅（ゆっくり）

                // 背景コピー（シーンごとの2バージョン）
                if(sc.bg == BG_STATION)      copy_bg(BG1[bv], bv);
                else if(sc.bg == BG_TUNNEL)  copy_bg(BG2[bv], bv);
                else                          copy_bg(BG3[bv], bv);

                // 列車を合成
                blit_sprite(x, sc.train_y, f);

                // 花火をちょっと増やす（終盤だけ）
                if(sc.bg == BG_BRIDGE && i > frames*3/4){
                    int t = i % 6;
                    int rr = 13 + (t%2);
                    int cc = 3 + (t*5 % (W-6));
                    if(rr>=0&&rr<H&&cc>=0&&cc<W) CANVAS[rr][cc] = (t%3==0)?'M':(t%3==1)?'Y':'C';
                }

                draw_canvas_at(row, left);
                fflush(stdout);
                msleep(delay_ms);
            }
        }
        loop++;
    }
    return 0;
}
