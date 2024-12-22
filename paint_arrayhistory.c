/*  
 * コマンドラインペイントプログラム  
 *  
 * 概要:  
 * - キャンバスサイズを指定して起動し、各種コマンドで描画を行うプログラム  
 * - 線の描画、描画の取り消し、履歴の保存などの機能を提供  
 * - 最大5つまでの操作履歴を保持  
 *  
 * 使用方法:   
 * ./paint <width> <height>  
 *  
 * コマンド:  
 * - line x0 y0 x1 y1: 座標(x0,y0)から(x1,y1)まで線を引く  
 * - undo: 直前の描画を取り消す  
 * - save [filename]: 履歴をファイルに保存  
 * - quit: プログラムを終了  
 */  

#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <ctype.h>  
#include <errno.h> // エラー処理用  

/* キャンバスを表現する構造体 */  
typedef struct {  
    int width;      // キャンバスの幅  
    int height;     // キャンバスの高さ  
    char **canvas;  // 描画領域（2次元配列）  
    char pen;       // 描画に使用する文字（デフォルト: '*'）  
} Canvas;  

/* コマンド履歴を管理する構造体 */  
typedef struct {  
    size_t max_history;  // 履歴の最大数（デフォルト: 5）  
    size_t bufsize;      // コマンドの最大長（デフォルト: 1000）  
    size_t hsize;        // 現在の履歴数  
    char **commands;     // コマンド文字列の配列  
} History;  

/* Canvas型に関する関数のプロトタイプ宣言 */  
Canvas *init_canvas(int width, int height, char pen);  
void reset_canvas(Canvas *c);  
void print_canvas(Canvas *c);  
void free_canvas(Canvas *c);  

/* 画面制御用の関数のプロトタイプ宣言 */  
void rewind_screen(unsigned int line);  
void clear_command(void);  
void clear_screen(void);  

/* コマンド実行結果を表す列挙型 */  
typedef enum res{   
    EXIT,       // プログラム終了  
    LINE,       // 線の描画成功  
    UNDO,       // 取り消し成功  
    SAVE,       // 保存成功  
    UNKNOWN,    // 不明なコマンド  
    ERRNONINT,  // 数値以外の入力  
    ERRLACKARGS // 引数不足  
} Result;  

/* 関数のプロトタイプ宣言 */  
char *strresult(Result res);  
int max(const int a, const int b);  
void draw_line(Canvas *c, const int x0, const int y0, const int x1, const int y1);  
Result interpret_command(const char *command, History *his, Canvas *c);  
void save_history(const char *filename, History *his);  

/* メイン関数 */  
int main(int argc, char **argv) {  
    /* 履歴関連の初期設定 */  
    const int max_history = 5;    // 最大履歴数  
    const int bufsize = 1000;     // コマンドの最大長  
    History his = (History){.max_history = max_history, .bufsize = bufsize, .hsize = 0};  
    
    /* 履歴用メモリの確保と初期化 */  
    his.commands = (char**)malloc(his.max_history * sizeof(char*));  
    char* tmp = (char*) malloc(his.max_history * his.bufsize * sizeof(char));  
    for (int i = 0 ; i < his.max_history ; i++)  
        his.commands[i] = tmp + (i * his.bufsize);  
    
    /* コマンドライン引数のチェックと処理 */  
    int width;  
    int height;  
    if (argc != 3){  
        fprintf(stderr,"usage: %s <width> <height>\n",argv[0]);  
        return EXIT_FAILURE;  
    }  
    else {  
        /* 引数を数値に変換 */  
        char *e;  
        long w = strtol(argv[1],&e,10);  
        if (*e != '\0'){  
            fprintf(stderr, "%s: irregular character found %s\n", argv[1],e);  
            return EXIT_FAILURE;  
        }  
        long h = strtol(argv[2],&e,10);  
        if (*e != '\0'){  
            fprintf(stderr, "%s: irregular character found %s\n", argv[2],e);  
            return EXIT_FAILURE;  
        }  
        width = (int)w;  
        height = (int)h;    
    }  
    char pen = '*';  // デフォルトのペン文字  
    
    char buf[his.bufsize];  // コマンド入力用バッファ  

    /* キャンバスの初期化 */  
    Canvas *c = init_canvas(width,height, pen);  

    printf("\n"); // Windows環境用の改行  
    
    /* メインループ */  
    while (his.hsize < his.max_history) {  
        size_t hsize = his.hsize;  
        size_t bufsize = his.bufsize;  
        print_canvas(c);  
        printf("%zu > ", hsize);  
        if(fgets(buf, bufsize, stdin) == NULL) break;  
        
        /* コマンドの解釈と実行 */  
        const Result r = interpret_command(buf, &his,c);  

        if (r == EXIT) break;  

        /* 実行結果の表示と履歴の更新 */  
        clear_command();  
        printf("%s\n",strresult(r));  
        if (r == LINE) {  
            strcpy(his.commands[his.hsize], buf);  
            his.hsize++;        
        }  
        /* 画面の再描画 */  
        rewind_screen(2);  
        clear_command();  
        rewind_screen(height+2);  
    }  
    
    /* 終了処理 */  
    clear_screen();  
    free_canvas(c);  
    
    return 0;  
}  

/*  
 * キャンバスを初期化する関数  
 * 引数:  
 *   width, height: キャンバスの大きさ  
 *   pen: 描画に使用する文字  
 * 戻り値:  
 *   初期化されたキャンバスへのポインタ  
 */  
Canvas *init_canvas(int width,int height, char pen) {  
    Canvas *new = (Canvas *)malloc(sizeof(Canvas));  
    new->width = width;  
    new->height = height;  
    new->canvas = (char **)malloc(width * sizeof(char *));  
    
    char *tmp = (char *)malloc(width*height*sizeof(char));  
    memset(tmp, ' ', width*height*sizeof(char));  
    for (int i = 0 ; i < width ; i++) {  
        new->canvas[i] = tmp + i * height;  
    }  
    
    new->pen = pen;  
    return new;  
}  

/* キャンバスをクリアする関数 */  
void reset_canvas(Canvas *c) {  
    const int width = c->width;  
    const int height = c->height;  
    memset(c->canvas[0], ' ', width*height*sizeof(char));  
}  

/*   
 * キャンバスを表示する関数  
 * - 上下の枠には'-'を使用  
 * - 左右の枠には'|'を使用  
 */  
void print_canvas(Canvas *c) {  
    const int height = c->height;  
    const int width = c->width;  
    char **canvas = c->canvas;  
    
    /* 上の枠 */  
    printf("+");  
    for (int x = 0 ; x < width ; x++)  
        printf("-");  
    printf("+\n");  
    
    /* キャンバス内容 */  
    for (int y = 0 ; y < height ; y++) {  
        printf("|");  
        for (int x = 0 ; x < width; x++){  
            const char c = canvas[x][y];  
            putchar(c);  
        }  
        printf("|\n");  
    }  
    
    /* 下の枠 */  
    printf("+");  
    for (int x = 0 ; x < width ; x++)  
        printf("-");  
    printf("+\n");  
    fflush(stdout);  
}  

/* キャンバスのメモリを解放する関数 */  
void free_canvas(Canvas *c) {  
    free(c->canvas[0]); // 2次元配列の実体の解放  
    free(c->canvas);    // ポインタ配列の解放  
    free(c);            // 構造体自体の解放  
}  

/* カーソルを指定行数上に移動 */  
void rewind_screen(unsigned int line) {  
    printf("\e[%dA",line);  
}  

/* コマンド行をクリア */  
void clear_command(void) {  
    printf("\e[2K");  
}  

/* 画面全体をクリア */  
void clear_screen(void) {  
    printf("\e[2J");  
}  

/* 2つの整数の大きい方を返す関数 */  
int max(const int a, const int b) {  
    return (a > b) ? a : b;  
}  

/*  
 * 2点間に線を描画する関数  
 * 引数:  
 *   c: キャンバス  
 *   x0, y0: 始点の座標  
 *   x1, y1: 終点の座標  
 */  
void draw_line(Canvas *c, const int x0, const int y0, const int x1, const int y1) {  
    const int width = c->width;  
    const int height = c->height;  
    char pen = c->pen;  
    
    const int n = max(abs(x1 - x0), abs(y1 - y0));  // 描画点の数  
    if ( (x0 >= 0) && (x0 < width) && (y0 >= 0) && (y0 < height))  
        c->canvas[x0][y0] = pen;  
    for (int i = 1; i <= n; i++) {  
        const int x = x0 + i * (x1 - x0) / n;  // x座標を線形補間  
        const int y = y0 + i * (y1 - y0) / n;  // y座標を線形補間  
        if ( (x >= 0) && (x< width) && (y >= 0) && (y < height))  
            c->canvas[x][y] = pen;  
    }  
}  

/*  
 * 履歴をファイルに保存する関数  
 * 引数:  
 *   filename: 保存先のファイル名（NULLの場合はデフォルト名を使用）  
 *   his: 履歴構造体  
 */  
void save_history(const char *filename, History *his) {  
    const char *default_history_file = "history.txt";  
    if (filename == NULL)  
        filename = default_history_file;  
    
    FILE *fp;  
    if ((fp = fopen(filename, "w")) == NULL) {  
        fprintf(stderr, "error: cannot open %s.\n", filename);  
        return;  
    }  
    
    for (int i = 0; i < his->hsize; i++) {  
        fprintf(fp, "%s", his->commands[i]);  
    }  
    
    fclose(fp);  
}  

/*  
 * コマンドを解釈して実行する関数  
 * 引数:  
 *   command: 入力されたコマンド  
 *   his: 履歴構造体  
 *   c: キャンバス  
 * 戻り値:  
 *   コマンドの実行結果  
 */  
Result interpret_command(const char *command, History *his, Canvas *c) {  
    char buf[his->bufsize];  
    strcpy(buf, command);  
    buf[strlen(buf) - 1] = 0; // 末尾の改行を削除  
    
    const char *s = strtok(buf, " ");  
    if (s == NULL) {  
        return UNKNOWN;  
    }  

    /* lineコマンドの処理 */  
    if (strcmp(s, "line") == 0) {  
        int p[4] = {0}; // p[0]: x0, p[1]: y0, p[2]: x1, p[3]: y1   
        char *b[4];  
        for (int i = 0 ; i < 4; i++){  
            b[i] = strtok(NULL, " ");  
            if (b[i] == NULL){  
                return ERRLACKARGS;  
            }  
        }  
        for (int i = 0 ; i < 4 ; i++){  
            char *e;  
            long v = strtol(b[i],&e, 10);  
            if (*e != '\0'){  
                return ERRNONINT;  
            }  
            p[i] = (int)v;  
        }  
        
        draw_line(c,p[0],p[1],p[2],p[3]);  
        return LINE;  
    }  
    
    /* saveコマンドの処理 */  
    if (strcmp(s, "save") == 0) {  
        s = strtok(NULL, " ");  
        save_history(s, his);  
        return SAVE;  
    }  
    
    /* undoコマンドの処理 */  
    if (strcmp(s, "undo") == 0) {  
        reset_canvas(c);  
        if (his->hsize != 0){  
            for (int i = 0; i < his->hsize - 1; i++) {  
                interpret_command(his->commands[i], his, c);  
            }  
            his->hsize--;  
        }  
        return UNDO;  
    }  
    
    /* quitコマンドの処理 */  
    if (strcmp(s, "quit") == 0) {  
        return EXIT;  
    }  
    
    return UNKNOWN;  
}  

/*  
 * コマンド実行結果に応じたメッセージを返す関数  
 * 引数:  
 *   res: 実行結果  
 * 戻り値:  
 *   対応するメッセージ文字列  
 */  
char *strresult(Result res) {  
    switch(res) {  
    case EXIT:  
        break;  
    case SAVE:  
        return "history saved";  
    case LINE:  
        return "1 line drawn";  
    case UNDO:  
        return "undo!";  
    case UNKNOWN:  
        return "error: unknown command";  
    case ERRNONINT:  
        return "Non-int value is included";  
    case ERRLACKARGS:  
        return "Too few arguments";  
    }  
    return NULL;  
}