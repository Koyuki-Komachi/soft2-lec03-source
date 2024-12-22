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

/*  
 * メイン関数  
 * プログラムの主要な流れを制御し、以下の処理を行う：  
 * 1. 履歴管理用メモリの確保と初期化  
 * 2. コマンドライン引数の解析  
 * 3. キャンバスの初期化  
 * 4. コマンド入力と実行のメインループ  
 * 5. 終了処理とメモリ解放  
 *  
 * 引数:  
 *   argc: コマンドライン引数の数  
 *   argv: コマンドライン引数の配列  
 *     argv[0]: プログラム名  
 *     argv[1]: キャンバスの幅  
 *     argv[2]: キャンバスの高さ  
 *  
 * 戻り値:  
 *   成功時: EXIT_SUCCESS (0)  
 *   失敗時: EXIT_FAILURE (1)  
 */  
int main(int argc, char **argv) {  
    /*  
     * 履歴管理の初期設定  
     * - max_history: 保持する履歴の最大数（5件）  
     * - bufsize: 各コマンドの最大長（1000文字）  
     * - hsize: 現在の履歴数（初期値0）  
     */  
    const int max_history = 5;  
    const int bufsize = 1000;  
    History his = (History){.max_history = max_history, .bufsize = bufsize, .hsize = 0};  
    
    /*  
     * 履歴用メモリの確保  
     * 1. コマンド文字列ポインタの配列を確保（max_history個）  
     * 2. 実際のコマンド文字列用メモリを一括確保  
     * 3. 各ポインタを適切な位置に設定  
     */  
    his.commands = (char**)malloc(his.max_history * sizeof(char*));  
    char* tmp = (char*) malloc(his.max_history * his.bufsize * sizeof(char));  
    for (int i = 0 ; i < his.max_history ; i++)  
        his.commands[i] = tmp + (i * his.bufsize);  
    
/*  
 * コマンドライン引数の処理  
 *   
 * 処理内容：  
 * 1. 引数の数のチェック（プログラム名 + width + height = 3個必要）  
 * 2. 文字列形式の引数を数値に変換  
 * 3. 不正な入力値のチェック  
 *   
 * 引数の要件：  
 * - argv[0]: プログラム名（自動的に設定）  
 * - argv[1]: キャンバスの幅（正の整数）  
 * - argv[2]: キャンバスの高さ（正の整数）  
 *  
 * エラーケース：  
 * 1. 引数の数が3個でない  
 * 2. 数値以外の文字が含まれている  
 * 3. 変換後の値が int の範囲を超える  
 */  
    
    int width;   // キャンバスの幅を格納する変数  
    int height;  // キャンバスの高さを格納する変数  

    /* 引数の数のチェック */  
    if (argc != 3){  
        // 引数が不適切な場合は使用方法を表示  
        // stderr: エラーメッセージ用の出力先  
        // %s: プログラム名（argv[0]）が入る  
        fprintf(stderr,"usage: %s <width> <height>\n",argv[0]);  
        return EXIT_FAILURE;  // エラーを示す終了コード  
    }  
    else {  
        /*  
         * 文字列から数値への変換処理  
         *  
         * strtol関数の使用：  
         * - 第1引数: 変換する文字列（argv[1]またはargv[2]）  
         * - 第2引数: 変換終了位置を示すポインタ（&e）  
         * - 第3引数: 基数（10進数なので10）  
         *  
         * 変換の仕組み：  
         * 1. strtolは文字列を先頭から解析  
         * 2. 数値として解釈できる部分を変換  
         * 3. 変換できなかった位置のポインタをeに格納  
         * 4. 正常な場合、eは文字列終端（'\0'）を指す  
         */  
        char *e;  // 変換終了位置を格納するポインタ  

        /* 幅の変換とチェック */  
        long w = strtol(argv[1],&e,10);  // 文字列から long 型に変換  
        if (*e != '\0'){  // 不正な文字が含まれているかチェック  
            // 変換できない文字が見つかった場合のエラー処理  
            // %s: 入力文字列（argv[1]）  
            // %s: 不正な文字以降の部分（e）  
            fprintf(stderr, "%s: irregular character found %s\n", argv[1],e);  
            return EXIT_FAILURE;  
        }  

        /* 高さの変換とチェック */  
        long h = strtol(argv[2],&e,10);  // 文字列から long 型に変換  
        if (*e != '\0'){  // 不正な文字が含まれているかチェック  
            fprintf(stderr, "%s: irregular character found %s\n", argv[2],e);  
            return EXIT_FAILURE;  
        }  

        /*  
         * long型からint型への変換  
         * - Canvas構造体ではint型を使用するため変換が必要  
         * - 暗黙の型変換による値の切り捨ての可能性あり  
         * （注：より厳密なプログラムでは、値の範囲チェックが必要）  
         */  
        width = (int)w;   // キャンバスの幅を設定  
        height = (int)h;  // キャンバスの高さを設定  
    }

    /*  
     * キャンバスの初期設定  
     * - penに'*'を設定（描画に使用する文字）  
     * - キャンバスを初期化  
     */  
    char pen = '*';  
    char buf[his.bufsize];  // コマンド入力用バッファ  
    Canvas *c = init_canvas(width,height, pen);  

    printf("\n"); // Windows環境で必要な改行  
    
    /*  
     * メインループ  
     * - 履歴数が上限に達するまで続ける  
     * - 各反復で以下の処理を実行：  
     *   1. キャンバスの表示  
     *   2. プロンプト表示と入力受付  
     *   3. コマンドの解釈と実行  
     *   4. 結果の表示と画面の更新  
     */  
    while (his.hsize < his.max_history) {  
        size_t hsize = his.hsize;  
        size_t bufsize = his.bufsize;  

        // キャンバスの表示  
        print_canvas(c);  
        
        // プロンプト表示と入力受付  
        printf("%zu > ", hsize);  
        if(fgets(buf, bufsize, stdin) == NULL) break;  
        
        // コマンドの解釈と実行  
        const Result r = interpret_command(buf, &his,c);  

        // 終了コマンドの場合はループを抜ける  
        if (r == EXIT) break;  

        /*  
         * 実行結果の表示と履歴の更新  
         * - 現在のコマンド行をクリア  
         * - 結果メッセージを表示  
         * - LINE命令の場合は履歴に追加  
         */  
        clear_command();  
        printf("%s\n",strresult(r));  
        if (r == LINE) {  
            strcpy(his.commands[his.hsize], buf);  
            his.hsize++;        
        }  

        /*  
         * 画面の再描画処理  
         * 1. コマンド結果表示部分を巻き戻し（2行）  
         * 2. コマンド行をクリア  
         * 3. キャンバス表示位置まで巻き戻し（height+2行）  
         */  
        rewind_screen(2);  
        clear_command();  
        rewind_screen(height+2);  
    }  
    
    /*  
     * 終了処理  
     * 1. 画面をクリア  
     * 2. キャンバスのメモリを解放  
     *    - canvas配列  
     *    - 構造体自体  
     * 3. プログラム終了  
     */  
    clear_screen();  
    free_canvas(c);  
    
    return 0;  
}  

/*  
 * キャンバスを初期化する関数  
 *  
 * 機能：  
 * - Canvas構造体のメモリ確保  
 * - 2次元配列として扱える1次元メモリ領域の確保  
 * - キャンバスの初期化（空白文字で埋める）  
 *  
 * 引数：  
 *   width  : キャンバスの幅  
 *   height : キャンバスの高さ  
 *   pen    : 描画に使用する文字  
 *  
 * 戻り値：  
 *   初期化されたCanvas構造体へのポインタ  
 */  
Canvas *init_canvas(int width,int height, char pen) {  
    /* Canvas構造体のメモリを確保 */  
    Canvas *new = (Canvas *)malloc(sizeof(Canvas));  
    new->width = width;   // キャンバスの幅を設定  
    new->height = height; // キャンバスの高さを設定  

    /*   
     * キャンバスの列（縦方向）へのポインタ配列を確保  
     * - width個の char* 型ポインタの配列  
     * - これにより canvas[x] でx列目にアクセス可能  
     */  
    new->canvas = (char **)malloc(width * sizeof(char *));  
    
    /*   
     * キャンバスの実データ領域を一括確保  
     * - width * height 個の文字を格納できるメモリ領域  
     * - 連続したメモリ領域として確保（効率的なメモリ利用）  
     * - すべての要素を空白文字で初期化  
     */  
    char *tmp = (char *)malloc(width*height*sizeof(char));  
    memset(tmp, ' ', width*height*sizeof(char));  

    /*  
     * 各列のポインタを設定  
     * - tmp + (i * height) で、i列目の開始位置を計算  
     * - これにより canvas[x][y] での アクセスが可能に  
     *  
     * メモリレイアウト例（width=3, height=4の場合）：  
     * tmp[] = [列0の4文字][列1の4文字][列2の4文字]  
     * canvas[0] → [列0の4文字]  
     * canvas[1] → [列1の4文字]  
     * canvas[2] → [列2の4文字]  
     */  
    for (int i = 0 ; i < width ; i++) {  
        new->canvas[i] = tmp + i * height;  
    }  
    
    new->pen = pen;  // 描画用文字を設定  

    return new;  // 初期化されたキャンバスを返す  
}  

/*  
 * キャンバスをクリアする関数  
 *  
 * 機能：キャンバス全体を空白文字で埋めて初期化する  
 *  
 * 引数：  
 *   Canvas *c - 初期化するキャンバスへのポインタ  
 *  
 * 動作の詳細：  
 * 1. キャンバスの2次元配列は、実際には1次元のメモリブロックとして確保されている  
 * 2. c->canvas[0]は、その1次元メモリブロックの先頭を指している  
 * 3. memsetで一括して空白文字でクリアする  
 */  
void reset_canvas(Canvas *c) {  
    /* キャンバスのサイズを局所変数として保存 */  
    const int width = c->width;   // キャンバスの幅  
    const int height = c->height; // キャンバスの高さ  

    /*  
     * memsetによるキャンバスのクリア  
     *   
     * 引数1: c->canvas[0] - クリアを開始するメモリアドレス  
     *        canvas[0]は2次元配列の先頭要素（1次元メモリブロックの開始位置）  
     *  
     * 引数2: ' ' (スペース) - 埋める値  
     *        キャンバスを空白文字でクリア  
     *  
     * 引数3: width * height * sizeof(char) - クリアするバイト数  
     *        - width * height: キャンバス全体の要素数  
     *        - sizeof(char): 各要素のサイズ（通常は1バイト）  
     *        これにより、キャンバス全体が一度にクリアされる  
     *  
     * 注意：この方法が効率的な理由  
     * - 2次元配列を行や列ごとにループする必要がない  
     * - メモリを連続的にアクセスするため高速  
     * - 1回のmemset呼び出しで完了  
     */  
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

/*   
 * キャンバスのメモリを解放する関数  
 *   
 * 機能：  
 * Canvas構造体で確保された全てのメモリを適切な順序で解放する  
 *  
 * 引数：  
 * Canvas *c ... 解放するキャンバスへのポインタ  
 *  
 * メモリ解放の順序が重要：  
 * 1. まず2次元配列の実体（文字データ領域）  
 * 2. 次にポインタの配列  
 * 3. 最後に構造体自体  
 * この順序を守らないとメモリリークやダングリングポインタの原因となる  
 */  
void free_canvas(Canvas *c) {  
    /*  
     * 2次元配列の実体の解放  
     * - init_canvasでは、width*height分の文字データを1度に確保  
     * - その領域の先頭アドレスはcanvas[0]に格納されている  
     * - この解放により、全文字データ領域が一度に解放される  
     */  
    free(c->canvas[0]);  

    /*  
     * ポインタ配列の解放  
     * - canvas配列はwidth個のポインタを持つ配列  
     * - 各ポインタは上で解放した文字データ領域の各行の先頭を指していた  
     * - この解放により、ポインタの配列領域が解放される  
     */  
    free(c->canvas);    

    /*  
     * Canvas構造体自体の解放  
     * - 最後に構造体のメモリ領域を解放  
     * - この時点で、構造体が保持していた全てのメモリが解放済みであることが重要  
     */  
    free(c);  
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

/*  
 * 2つの整数の大きい方を返す関数  
 *  
 * 目的：  
 * - 2つの整数値を比較し、より大きい方の値を返す  
 * - draw_line関数で、2点間の描画に必要な点の数を決定するために使用  
 *  
 * 引数：  
 * - const int a: 比較する1つ目の整数  
 * - const int b: 比較する2つ目の整数  
 * - const修飾子により、関数内で引数の値は変更不可  
 *  
 * 実装の詳細：  
 * - 三項演算子 (a > b) ? a : b を使用  
 * - (a > b): 条件部。aがbより大きいかを評価  
 * - ? a : b  
 *   - 条件が真の場合: aを返す  
 *   - 条件が偽の場合: bを返す  
 *  
 * 戻り値：  
 * - aとbのうち、大きい方の整数値  
 * - aとbが等しい場合は、どちらの値も同じなのでaを返す  
 *  
 * 使用例：  
 * max(5, 3) -> 5  
 * max(-1, 2) -> 2  
 * max(4, 4) -> 4  
 */  
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
 *  
 * 引数:  
 *   filename: 保存先のファイル名  
 *     - NULLが渡された場合はデフォルトのファイル名を使用  
 *     - 文字列が渡された場合はその名前でファイルを作成  
 *   his: 履歴データを含む構造体へのポインタ  
 *     - commands: コマンド文字列の配列  
 *     - hsize: 保存されているコマンドの数  
 */  
void save_history(const char *filename, History *his) {  
    /* デフォルトのファイル名を設定 */  
    const char *default_history_file = "history.txt";  
    if (filename == NULL)  
        filename = default_history_file;  // filenameがNULLの場合、デフォルト名を使用  
    
    /* ファイルを書き込みモード('w')で開く */  
    FILE *fp;  
    if ((fp = fopen(filename, "w")) == NULL) {  
        // ファイルオープンに失敗した場合（権限エラーやディスク容量不足など）  
        // エラーメッセージを標準エラー出力に表示  
        fprintf(stderr, "error: cannot open %s.\n", filename);  
        return;  // エラーが発生したら関数を終了  
    }  
    
    /* 履歴データをファイルに書き込む */  
    for (int i = 0; i < his->hsize; i++) {  
        // his->commands[i]には改行文字が含まれているため  
        // 追加の改行は不要  
        fprintf(fp, "%s", his->commands[i]);  
    }  
    
    /* ファイルを閉じる */  
    fclose(fp);  // 正常にクローズし、バッファをフラッシュ  
}  

/*  
 * コマンドを解釈して実行する関数  
 *   
 * 役割：  
 * 1. ユーザー入力を解析してコマンドを特定  
 * 2. コマンドに応じた処理を実行  
 * 3. 実行結果をResult型で返す  
 *  
 * 引数:  
 *   command: ユーザーが入力したコマンド文字列  
 *   his: コマンド履歴を管理する構造体へのポインタ  
 *   c: 描画用キャンバスへのポインタ  
 *  
 * 戻り値: Result型（実行結果を示す列挙型）  
 */  
Result interpret_command(const char *command, History *his, Canvas *c) {  
    /*   
     * コマンド文字列の前処理  
     * 1. バッファにコピー（元の文字列は変更しない）  
     * 2. 末尾の改行文字を削除  
     *  
     * 例：入力が "line 1 2 3 4\n" の場合  
     * → buf = "line 1 2 3 4" （改行削除後）  
     */  
    char buf[his->bufsize];  
    strcpy(buf, command);  
    buf[strlen(buf) - 1] = 0; // 末尾の改行を削除  

    /*  
     * コマンド名の抽出（strtokの初回呼び出し）  
     * - 第1引数：分割する文字列（buf）  
     * - 第2引数：区切り文字（スペース）  
     * - 戻り値：最初のトークン（コマンド名）へのポインタ  
     *  
     * 例："line 1 2 3 4"の場合：  
     * 1. スペースでトークン分割  
     * 2. 最初のトークン"line"へのポインタを返す  
     */  
    const char *s = strtok(buf, " ");  
    if (s == NULL) {  
        return UNKNOWN;  
    }  

    /*   
     * lineコマンドの処理  
     * 書式: line x0 y0 x1 y1  
     * 例: "line 1 2 3 4"  
     */  
    if (strcmp(s, "line") == 0) {  
        int p[4] = {0}; // 座標値を格納する配列  
        char *b[4];     // 引数文字列へのポインタ配列  

        /*   
         * 4つの引数を取得（strtokの後続呼び出し）  
         * - 第1引数：NULL（内部状態から継続）  
         * - 第2引数：区切り文字（スペース）  
         *  
         * 例："line 1 2 3 4"の分割過程：  
         * b[0] = "1"  
         * b[1] = "2"  
         * b[2] = "3"  
         * b[3] = "4"  
         */  
        for (int i = 0 ; i < 4; i++){  
            b[i] = strtok(NULL, " ");  
            if (b[i] == NULL){  
                return ERRLACKARGS;  
            }  
        }  

        /*   
         * 各引数を整数値に変換（strtol関数の使用）  
         *  
         * strtolの動作：  
         * 1. 文字列から数値への変換  
         * 2. エラーチェック  
         * 3. 結果の保存  
         *  
         * パラメータの説明：  
         * - 第1引数：変換する文字列（例："123"）  
         * - 第2引数：変換終了位置を示すポインタのアドレス  
         * - 第3引数：基数（10 = 10進数）  
         *  
         * 戻り値：  
         * - 成功：変換された数値（long型）  
         * - 失敗：0（エラーはerrno経由で通知）  
         *  
         * エンドポインタ（e）の使用：  
         * - 変換後、eは最初の無効文字を指す  
         * - 完全な変換の場合、e は '\0' を指す  
         * - 不完全な変換の場合、e は無効文字を指す  
         *  
         * 例：  
         * 1. 正常な場合："123" → v=123, e='\0'  
         * 2. 異常な場合："123abc" → v=123, e='a'  
         */  
        for (int i = 0 ; i < 4 ; i++){  
            char *e;  // 変換終了位置を格納するポインタ  
            long v = strtol(b[i], &e, 10);  
            
            /*  
             * 変換結果の検証  
             * *e != '\0' の場合：  
             * - 数値以外の文字が含まれている  
             * - または文字列全体が変換されていない  
             */  
            if (*e != '\0'){  
                return ERRNONINT;  
            }  
            p[i] = (int)v;  // long型からint型への変換  
        }  
        
        /* 線を描画して結果を返す */  
        draw_line(c,p[0],p[1],p[2],p[3]);  
        return LINE;  
    }  
    
    /*  
     * saveコマンドの処理  
     * 書式: save [filename]  
     * - strtokで次のトークン（ファイル名）を取得  
     */  
    if (strcmp(s, "save") == 0) {  
        s = strtok(NULL, " ");  
        save_history(s, his);  
        return SAVE;  
    }  
    
    /*  
     * undoコマンドの処理  
     * 目的：直前の描画操作を取り消す  
     *  
     * 処理の流れ：  
     * 1. キャンバスを初期状態に戻す  
     * 2. 最後のコマンドを除く全履歴を再実行  
     * 3. 履歴サイズを1減らす  
     */  
    if (strcmp(s, "undo") == 0) {  
        /*  
         * キャンバスのリセット  
         * - 全ピクセルを空白文字で上書き  
         * - キャンバスの構造体自体は維持  
         * - 枠線は影響を受けない（print_canvas時に再描画）  
         */  
        reset_canvas(c);  

        /*   
         * 履歴からの再構築処理  
         * 条件：履歴が存在する場合のみ実行（his->hsize > 0）  
         *  
         * 例：履歴が3つある状態でundoを実行する場合  
         * 初期状態：  
         * his->commands[0] = "line 0 0 1 1"    （1番目のコマンド）  
         * his->commands[1] = "line 1 1 2 2"    （2番目のコマンド）  
         * his->commands[2] = "line 2 2 3 3"    （3番目のコマンド、これを取り消す）  
         * his->hsize = 3  
         */  
        if (his->hsize != 0){  
            /*  
             * コマンドの再実行ループ  
             * - his->hsize - 1 まで実行（最後のコマンドを除く）  
             * - 各コマンドを元の順序で解釈・実行  
             *  
             * 処理例：  
             * 1回目: his->commands[0]を実行 → "line 0 0 1 1"  
             * 2回目: his->commands[1]を実行 → "line 1 1 2 2"  
             * （his->commands[2]は実行しない）  
             *  
             * 注意点：  
             * - interpret_commandの再帰的な呼び出し  
             * - 再実行時はコマンドが履歴に追加されない  
             * - エラーが発生した場合も考慮が必要  
             */  
            for (int i = 0; i < his->hsize - 1; i++) {  
                interpret_command(his->commands[i], his, c);  
            }  

            /*  
             * 履歴サイズの更新  
             * - サイズを1減らすことで最後のコマンドを「削除」  
             * - 実際のメモリは解放せず、次の操作で上書き可能  
             *  
             * 例：  
             * 更新前: his->hsize = 3  
             * 更新後: his->hsize = 2  
             * （his->commands[2]のデータは残っているが、  
             *   his->hsizeが2になることで実質的に無視される）  
             */  
            his->hsize--;  
        }  

        /*  
         * 処理結果を返す  
         * - UNDO: 操作が成功したことを示す  
         * - メイン処理でこの値に応じたメッセージが表示される  
         */  
        return UNDO;  
    }  

    /*  
     * quitコマンドの処理  
     * - プログラムの終了を指示  
     * - 引数は不要  
     */  
    if (strcmp(s, "quit") == 0) {  
        return EXIT;  
    }  
    
    /*   
     * 未知のコマンドの場合  
     * - 定義されていないコマンドが入力された  
     * - 不正なフォーマットのコマンドが入力された  
     */  
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