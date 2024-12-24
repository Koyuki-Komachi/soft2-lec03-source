/*  
 * ヘッダーファイルのインクルード  
 */  
#include <stdio.h>   // 標準入出力関数用  
#include <stdlib.h>  // メモリ管理、文字列変換関数用  
#include <string.h>  // 文字列操作関数用  
#include <ctype.h>   // 文字種判定関数用  
#include <errno.h>   // エラー処理用  
#include <math.h>

/*  
 * キャンバスを表現する構造体  
 * - 描画領域の管理に使用  
 */  
typedef struct {  
    int width;      // キャンバスの幅  
    int height;     // キャンバスの高さ  
    char **canvas;  // 実際の描画データを保持する2次元配列  
    char pen;       // 描画に使用する文字  
} Canvas;  

/*  
 * コマンドを表現する構造体（単方向リスト用）  
 * - 以前の配列による実装から線形リストによる実装に変更  
 */  
typedef struct command Command;  
struct command {  
    char *str;       // コマンド文字列  
    size_t bufsize;  // バッファサイズ  
    Command *next;   // 次のコマンドへのポインタ  
};  

/*  
 * 履歴を管理する構造体  
 * - コマンドの線形リストを管理  
 */  
typedef struct {  
    Command *begin;   // リストの先頭要素へのポインタ  
    size_t bufsize;  // コマンドバッファの最大サイズ  
} History;  

/*  
 * キャンバス操作関数のプロトタイプ宣言  
 */  
Canvas *init_canvas(int width, int height, char pen);  // キャンバスの初期化  
void reset_canvas(Canvas *c);                          // キャンバスのリセット  
void print_canvas(Canvas *c);                          // キャンバスの表示  
void free_canvas(Canvas *c);                          // キャンバスのメモリ解放  

/*  
 * 画面制御関数のプロトタイプ宣言  
 */  
void rewind_screen(unsigned int line);  // 指定行数だけカーソルを上に移動  
void clear_command(void);               // コマンド行をクリア  
void clear_screen(void);                // 画面全体をクリア  

/*  
 * コマンド実行結果を表す列挙型  
 * - 以前のバージョンからNOCOMMANDを追加  
 */  
typedef enum res {  
    EXIT,       // 終了コマンド  
    LINE,       // 線描画コマンド  
    RECT,       // 追加：長方形描画
    CIRCLE,     // 追加：円描画
    UNDO,       // 取り消しコマンド  
    SAVE,       // 保存コマンド  
    LOAD,       // 追加：ロードコマンド成功
    CHPEN,      // 追加：ペン文字変更
    UNKNOWN,    // 不明なコマンド
    ERRFILE,    // 追加：ファイルエラー  
    ERRNONINT,  // 整数以外の入力エラー  
    ERRLACKARGS,// 引数不足エラー  
    NOCOMMAND   // 履歴が空のエラー  
} Result;  

/*  
 * その他の関数プロトタイプ宣言  
 */  
char *strresult(Result res);  // 実行結果に対応するメッセージを返す  
int max(const int a, const int b);  // 2つの整数の最大値を返す  
void draw_line(Canvas *c, const int x0, const int y0, const int x1, const int y1);  // 線描画  
Result interpret_command(const char *command, History *his, Canvas *c);  // コマンド解釈  
void save_history(const char *filename, History *his);  // 履歴保存  
Command *push_command(History *his, const char *str);  // コマンドをリストに追加

int main(int argc, char **argv) {  
    /*  
     * バッファサイズの設定  
     * - コマンド入力用の固定サイズバッファ  
     */  
    const int bufsize = 1000;  
    
    /*  
     * 履歴構造体の初期化  
     * - beginをNULLに設定（空のリスト）  
     * - bufsizeをコマンドバッファサイズに設定  
     */  
    History his = (History){ .begin = NULL, .bufsize = bufsize};  
    
    /*  
     * コマンドライン引数の処理  
     * - width（幅）とheight（高さ）を取得  
     */  
    int width;  
    int height;  
    if (argc != 3){  
        // 引数の数が不正な場合のエラー処理  
        fprintf(stderr,"usage: %s <width> <height>\n",argv[0]);  
        return EXIT_FAILURE;  
    } else {  
        /*  
         * 文字列から数値への変換処理  
         * - strtol: 文字列を長整数に変換  
         * - エラーチェック付きの変換  
         */  
        char *e;  
        // 幅の処理  
        long w = strtol(argv[1], &e, 10);  
        if (*e != '\0'){  // 不正な文字が含まれている場合  
            fprintf(stderr, "%s: irregular character found %s\n", argv[1], e);  
            return EXIT_FAILURE;  
        }  
        // 高さの処理  
        long h = strtol(argv[2], &e, 10);  
        if (*e != '\0'){  // 不正な文字が含まれている場合  
            fprintf(stderr, "%s: irregular character found %s\n", argv[2], e);  
            return EXIT_FAILURE;  
        }  
        // long型からint型への変換  
        width = (int)w;  
        height = (int)h;  
    }  

    /*  
     * ペン文字の設定とバッファの準備  
     */  
    char pen = '*';  // 描画に使用する文字  
    char buf[bufsize];  // コマンド入力用バッファ  

    /*  
     * キャンバスの初期化  
     */  
    Canvas *c = init_canvas(width, height, pen);  
    
    printf("\n");  // Windows環境用の改行  

    // 初期ペン設定を履歴に追加
    sprintf(buf, "chpen %c\n", pen);
    if (push_command(&his, buf) == NULL){
        fprintf(stderr, "error: cannot save initial pen command.\n");
        free_canvas(c);
        return EXIT_FAILURE;
    }

    /*  
     * メインループ  
     * - ユーザーからのコマンド入力を処理  
     */  
    while(1){  
        /*  
         * キャンバスの表示とプロンプト出力  
         */  
        print_canvas(c);  
        printf("* > ");  

        /*  
         * コマンド入力の受付  
         * - fgets: 標準入力から1行読み込み  
         * - NULL: EOF（Ctrl+D）で終了  
         */  
        if(fgets(buf, bufsize, stdin) == NULL) break;  
        
        /*  
         * コマンドの解釈と実行  
         */  
        const Result r = interpret_command(buf, &his, c);  

        /*  
         * 終了コマンドの処理  
         */  
        if (r == EXIT) break;  

        /*  
         * コマンド実行結果の表示  
         */  
        clear_command();  // 現在のコマンド行をクリア  
        printf("%s\n", strresult(r));  // 結果メッセージの表示  

        /*  
         * 線描画コマンド、長方形描画コマンド、円描画コマンドの場合、履歴に追加  
         */  
        if (r == LINE || r == RECT || r == CIRCLE || r == CHPEN) {  
            push_command(&his, buf);  
        }  
        
        /*  
         * 画面の再描画処理  
         */  
        rewind_screen(2);           // コマンド結果表示部分の巻き戻し  
        clear_command();            // コマンド自体をクリア  
        rewind_screen(height + 2);  // キャンバス表示位置まで巻き戻し  
    }  
    
    /*  
     * 終了処理  
     * - 画面クリア  
     * - キャンバスのメモリ解放  
     */  
    clear_screen();  
    free_canvas(c);  
    
    return 0;  
}

/*  
 * キャンバスの初期化関数  
 * 引数：  
 * - width: キャンバスの幅  
 * - height: キャンバスの高さ  
 * - pen: 描画に使用する文字  
 */  
Canvas *init_canvas(int width, int height, char pen)  
{  
    /*  
     * Canvas構造体のメモリ確保  
     * mallocを使用して動的にメモリを確保  
     */  
    Canvas *new = (Canvas *)malloc(sizeof(Canvas));  
    new->width = width;  
    new->height = height;  
    
    /*  
     * キャンバスの2次元配列を効率的に作成  
     * 1. まず、ポインタの配列を作成（width個）  
     */  
    new->canvas = (char **)malloc(width * sizeof(char *));  
    
    /*  
     * 2. 実際のデータ領域を一括で確保  
     * - 2次元配列を1次元的に確保することで、メモリの効率化  
     * - メモリの断片化を防ぐ  
     */  
    char *tmp = (char *)malloc(width * height * sizeof(char));  
    
    /*  
     * 3. 確保した領域を空白文字で初期化  
     */  
    memset(tmp, ' ', width * height * sizeof(char));  
    
    /*  
     * 4. 各行のポインタを設定  
     * - 1次元配列を2次元的にアクセスできるように設定  
     */  
    for (int i = 0; i < width; i++){  
        new->canvas[i] = tmp + i * height;  
    }  
    
    /*  
     * 5. ペン文字の設定  
     */  
    new->pen = pen;  
    
    return new;  
}  

/*  
 * キャンバスのリセット関数  
 * - 描画内容をすべて消去  
 */  
void reset_canvas(Canvas *c)  
{  
    const int width = c->width;  
    const int height = c->height;  
    
    /*  
     * キャンバス全体を空白文字で上書き  
     * - canvas[0]は実データの先頭アドレス  
     */  
    memset(c->canvas[0], ' ', width * height * sizeof(char));  
}  

/*  
 * キャンバスの表示関数  
 * - 枠線付きでキャンバスを表示  
 */  
void print_canvas(Canvas *c)  
{  
    const int height = c->height;  
    const int width = c->width;  
    char **canvas = c->canvas;  
    
    /*  
     * 上部の枠線を描画  
     * 例：width=3の場合  
     * +---+  
     */  
    printf("+");  
    for (int x = 0; x < width; x++)  
        printf("-");  
    printf("+\n");  
    
    /*  
     * キャンバスの内容を1行ずつ表示  
     * - 左右に'|'を付けて表示  
     * 例：  
     * |   |  
     * | * |  
     * |   |  
     */  
    for (int y = 0; y < height; y++) {  
        printf("|");  
        for (int x = 0; x < width; x++){  
            const char c = canvas[x][y];  
            putchar(c);  
        }  
        printf("|\n");  
    }  
    
    /*  
     * 下部の枠線を描画  
     */  
    printf("+");  
    for (int x = 0; x < width; x++)  
        printf("-");  
    printf("+\n");  
    
    /*  
     * バッファの内容を確実に出力  
     */  
    fflush(stdout);  
}  

/*  
 * キャンバスのメモリ解放関数  
 * - メモリリークを防ぐため、確保したメモリを適切に解放  
 */  
void free_canvas(Canvas *c)  
{  
    /*  
     * メモリ解放の順序が重要  
     * 1. まず実データ領域を解放（canvas[0]が先頭アドレス）  
     * 2. 次にポインタ配列を解放  
     * 3. 最後にCanvas構造体自体を解放  
     */  
    free(c->canvas[0]);  // 実データの解放  
    free(c->canvas);     // ポインタ配列の解放  
    free(c);             // 構造体の解放  
}

void rewind_screen(unsigned int line)
{
    printf("\e[%dA",line);
}

void clear_command(void)
{
    printf("\e[2K");
}

void clear_screen(void)
{
    printf( "\e[2J");
}


int max(const int a, const int b)
{
    return (a > b) ? a : b;
}
void draw_line(Canvas *c, const int x0, const int y0, const int x1, const int y1)
{
    const int width = c->width;
    const int height = c->height;
    char pen = c->pen;
    
    const int n = max(abs(x1 - x0), abs(y1 - y0));
    if ( (x0 >= 0) && (x0 < width) && (y0 >= 0) && (y0 < height))
	c->canvas[x0][y0] = pen;
    for (int i = 1; i <= n; i++) {
	const int x = x0 + i * (x1 - x0) / n;
	const int y = y0 + i * (y1 - y0) / n;
	if ( (x >= 0) && (x< width) && (y >= 0) && (y < height))
	    c->canvas[x][y] = pen;
    }
}

void draw_rect(Canvas *c, const int x0, const int y0, const int width, const int height){
    if (width <= 0 || height <= 0) return;

    int x1 = x0 + width - 1;
    int y1 = y0 + height - 1;

    draw_line(c, x0, y0, x1, y0);
    draw_line(c, x0, y1, x1, y1);
    draw_line(c, x0, y0, x0, y1);
    draw_line(c, x1, y0, x1, y1);
}

void draw_circle(Canvas *c, const int x0, const int y0, const int r){
    if (r <= 0) return;

    for (int deg = 0; deg < 360; deg++){
        double rad = deg * M_PI / 180.0;
        int x = x0 + (int)(r * cos(rad));
        int y = y0 + (int)(r * sin(rad));

        if (x >= 0 && x < c->width && y >= 0 && y < c->height){
            c->canvas[x][y] = c->pen;
        }
    }
}

void save_history(const char *filename, History *his)
{
    const char *default_history_file = "history.txt";
    if (filename == NULL)
	filename = default_history_file;
    
    FILE *fp;
    if ((fp = fopen(filename, "w")) == NULL) {
	fprintf(stderr, "error: cannot open %s.\n", filename);
	return;
    }
    // [*] 線形リスト版
    for (Command *p = his->begin ; p != NULL ; p = p->next){
	fprintf(fp, "%s", p->str);
    }
    
    fclose(fp);
}

Result load_history(const char *filename, History *his, Canvas *c){
    // ファイル名の指定がない場合は、history.txtに保存する
    const char *default_history_file = "history.txt";
    if (filename == NULL){
        filename = default_history_file;
    }

    FILE *fp;
    if ((fp = fopen(filename, "r")) == NULL){
        fprintf(stderr, "error: cannot open %s.\n", filename);
        return ERRFILE;
    }

    reset_canvas(c);

    // 履歴ファイルの内容を読み込む
    char buf[his->bufsize];
    while (fgets(buf, his->bufsize, fp) != NULL){
        size_t len = strlen(buf);

        // 履歴ファイルの中のコマンドが長すぎる場合のエラー
        if (len >= his->bufsize){
            fprintf(stderr, "error: command too long.\n");
            fclose(fp);
            return ERRFILE;
        }

        // コマンドをtmpにコピーし、strtokにより、コマンド名だけをcmdに入れる
        char tmp[his->bufsize];
        strcpy(tmp, buf);
        char *cmd = strtok(tmp, " ");

        if (cmd != NULL){
            if (strcmp(cmd, "line") == 0 || 
                strcmp(cmd, "rect") == 0 ||
                strcmp(cmd, "circle") == 0 ||
                strcmp(cmd, "chpen") == 0){

                    char *new_str = (char*)malloc(his->bufsize);

                    if (new_str == NULL){
                        fprintf(stderr, "error: memory allocation failed.\n");
                        fclose(fp);
                        return ERRFILE;
                    }

                    strcpy(new_str, buf);

                    Result r = interpret_command(new_str, his, c);
                    if (r == ERRNONINT || r == ERRLACKARGS){
                        free(new_str);
                        fclose(fp);
                        return r;
                    }

                    Command *cmd = (Command*)malloc(sizeof(Command));
                    if (cmd == NULL){
                        free(new_str);
                        fclose(fp);
                        fprintf(stderr, "error: memory allocation failed.\n");
                        return ERRFILE;
                    }

                    // コマンドをCommand構造体の形にする
                    *cmd = (Command){
                        .str = new_str,
                        .bufsize = his->bufsize,
                        .next = NULL
                    };

                    // コマンドを線形リストの最後尾に追加
                    Command *p = his->begin;
                    if (p == NULL){
                        his->begin = cmd;
                    } else {
                        while (p->next != NULL){
                            p = p->next;
                        }
                        p->next = cmd;
                    }
                }
        }
    }
    fclose(fp);
    return LOAD;
}

Result interpret_command(const char *command, History *his, Canvas *c)
{
    char buf[his->bufsize];
    strcpy(buf, command);
    buf[strlen(buf) - 1] = 0; // remove the newline character at the end
    
    const char *s = strtok(buf, " ");
    if (s == NULL){ // 改行だけ入力された場合
	return UNKNOWN;
    }

    // chpenコマンドを認識して、文字種を変える
    if (strcmp(s, "chpen") == 0){
        char *pen = strtok(NULL, " ");

        if (pen == NULL){
            return ERRLACKARGS;
        }

        // 特殊文字のチェック
        if (pen[0] == ' ' || pen[0] =='\t' || pen[0] == '\n' || pen[0] == '\0'){
            return ERRLACKARGS;
        }

        // 文字列長とヌル文字の位置を確認
        if (pen[1] != '\0'){
            return ERRLACKARGS;
        }

        // 追加の引数がないかチェック
        if (strtok(NULL, " ") != NULL){
            return UNKNOWN;
        }

        c->pen = pen[0];
        return CHPEN;
    }

    // loadコマンドを認識して、load_historyを実行する
    if (strcmp(s, "load") == 0){
        const char *filename = strtok(NULL, " ");
        
        if (strtok(NULL, " ") != NULL){
            return UNKNOWN;
        }
        return load_history(filename, his, c);
    }

    // rectコマンドを認識して、draw_rectを実行する
    if (strcmp(s, "rect") == 0){
        int p[4] = {0};
        char *b[4];

        for (int i = 0; i < 4; ++i){
            b[i] = strtok(NULL, " ");
            if (b[i] == NULL){
                return ERRLACKARGS;
            }
        }
        for (int i = 0; i < 4; ++i){
            char *e;
            long v = strtol(b[i], &e, 10);
            if (*e != '\0'){
                return ERRNONINT;
            }
            p[i] = (int)v;
        }

        draw_rect(c, p[0], p[1], p[2], p[3]);
        return RECT;
    }

    // circleコマンドを認識して、draw_circleを実行する
    if (strcmp(s, "circle") == 0){
        int p[3] = {0};
        char *b[3];

        for (int i = 0; i < 3; ++i){
            b[i] = strtok(NULL, " ");
            if (b[i] == NULL){
                return ERRLACKARGS;
            }
        }

        for (int i = 0; i < 3; ++i){
            char *e;
            long v = strtol(b[i], &e, 10);
            if (*e != '\0'){
                return ERRNONINT;
            }
            p[i] = (int)v;
        }

        draw_circle(c, p[0], p[1], p[2]);
        return CIRCLE;
    }

    // lineコマンドを認識して、draw_lineを実行する
    if (strcmp(s, "line") == 0) {
	int p[4] = {0}; // p[0]: x0, p[1]: y0, p[2]: x1, p[3]: x1 
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
    
    // saveコマンドを認識して、save_historyを実行する
    if (strcmp(s, "save") == 0) {
	s = strtok(NULL, " ");
	save_history(s, his);
	return SAVE;
    }
    
    // undoコマンドを認識して、これを実行する
    if (strcmp(s, "undo") == 0) {
	reset_canvas(c);
	//[*] 線形リストの先頭からスキャンして逐次実行
	// pop_back のスキャン中にinterpret_command を絡めた感じ
	Command *p = his->begin;
	if (p == NULL){
	    return NOCOMMAND;
	}
	else{
	    Command *q = NULL; // 新たな終端を決める時に使う
	    while (p->next != NULL){ // 終端でないコマンドは実行して良い
		interpret_command(p->str, his, c);
		q = p;
		p = p->next;
	    }
	    // 1つしかないコマンドのundoではリストの先頭を変更する
	    if (q == NULL) {
		his->begin = NULL;
	    }
	    else{
		q->next = NULL;
	    }
	    free(p->str);
	    free(p);	
	    return UNDO;
	}  
    }
    
    if (strcmp(s, "quit") == 0) {
	return EXIT;
    }
    return UNKNOWN;
}


// [*] 線形リストの末尾にpush する
Command *push_command(History *his, const char *str){
    Command *c = (Command*)malloc(sizeof(Command));
    char *s = (char*)malloc(his->bufsize);
    strcpy(s, str);
    
    *c = (Command){ .str = s, .bufsize = his->bufsize, .next = NULL};
    
    Command *p = his->begin;
    
    if ( p == NULL) {
	his->begin = c;
    }
    else{
	while (p->next != NULL){
	    p = p->next;
	}
	p->next = c;
    }
    return c;
}

char *strresult(Result res){
    switch(res) {
    case EXIT:
	break;
    case SAVE:
	return "history saved";
    case LOAD:
    return "loaded history file";
    case LINE:
	return "1 line drawn";
    case RECT:
    return "1 rectangle drawn";
    case CIRCLE:
    return "1 circle drawn";
    case CHPEN:
    return "pen changed";
    case UNDO:
	return "undo!";
    case UNKNOWN:
	return "error: unknown command";
    case ERRNONINT:
	return "Non-int value is included";
    case ERRLACKARGS:
	return "Too few arguments";
    case ERRFILE:
    return "file not open or memory not allocated";
    case NOCOMMAND:
	return "No command in history";
    }
    return NULL;
}