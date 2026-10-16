//=========================================================================
// 五目並べ（連珠）共通ライブラリ：勝敗判定・禁じ手判定
//
// 禁じ手は連珠の公式ルール（日本連珠社／RIF）の定義に従って判定する．
//   五　　：同色の石がちょうど5つ連続した形
//           （check に CHOLEN が含まれるとき，黒の6つ以上の連続は五ではない）
//   長連　：6つ以上の連続
//   四　　：あと1手で五になる形
//   達四　：五になる点が2つある四（・●●●●・）
//   三　　：あと1手で達四になる形．
//           ただし，達四を作るその1手が禁じ手になる場合は三とみなさない（偽の三）
//   四四　：1手で四を2つ以上作る手．1本のライン上に2つの四ができる形
//           （●・●●●・● や ●●・●●・●● の中央）も含む
//   三三　：1手で三を2つ以上作る手（四三三，四四三などを含む）
// 黒が五を作った場合は，同時に禁じ手の形ができていても黒の勝ちとする．
// 黒石が置かれるたびに，その石が作った三・四（三連・飛び三・四連・飛び四）を表示する．
// 白に禁じ手はなく，白は五以上（長連を含む）で勝ち．
// 有効にする禁じ手は gomoku.c の大域変数 check で切り替える．
//
// 禁じ手の判定は「直前に置かれた石」についてのみ行う．
// gameEndProcess の引数 c33Board は直前の盤面の記録に使う
// （gomoku.c の gameInit で 0 クリアされるので，対局開始時は空盤面となる）．
// c44Board は引数の互換性のために残しているが使用しない．
//=========================================================================

#include "gomoku.h"

#include <stdio.h>

int gameEndProcess(int board[][BOARD_SIZE], int c33Board[][BOARD_SIZE],
                   int c44Board[][BOARD_SIZE]);
int lenCheck(int board[][BOARD_SIZE], int x, int y);
int isSan(int board[][BOARD_SIZE], int x, int y);
int isShi(int board[][BOARD_SIZE], int x, int y);
int isRoku(int board[][BOARD_SIZE], int x, int y);
int isForbidden(int board[][BOARD_SIZE], int x, int y);

// 方向 0:[─] 1:[│] 2:[＼] 3:[／]
static const int DX[4] = {1, 0, 1, 1};
static const int DY[4] = {0, 1, 1, -1};

static int judgeBlack(int board[][BOARD_SIZE], int x, int y);

//-------------------------------------------------
// (x,y) の状態．盤外なら -1
//-------------------------------------------------
static int cellAt(int board[][BOARD_SIZE], int x, int y) {
    return checkOutPos(x, y) ? board[y][x] : -1;
}

//-------------------------------------------------
// (x,y) の石を含む，方向 d に連続する同色の石の範囲を
// (x,y) からのオフセット [*lo, *hi] で返す
//-------------------------------------------------
static void runRange(int board[][BOARD_SIZE], int x, int y, int d, int *lo,
                     int *hi) {
    int color = board[y][x];
    int k;
    for (k = 1; cellAt(board, x - k * DX[d], y - k * DY[d]) == color; k++) {
        // 同じ色の石が続く間 k を進めるだけ（ループ本体の処理はない）
    }
    *lo = 1 - k;
    for (k = 1; cellAt(board, x + k * DX[d], y + k * DY[d]) == color; k++) {
        // 同じ色の石が続く間 k を進めるだけ（ループ本体の処理はない）
    }
    *hi = k - 1;
}

static int runLength(int board[][BOARD_SIZE], int x, int y, int d) {
    int lo, hi;
    runRange(board, x, y, d, &lo, &hi);
    return hi - lo + 1;
}

//-------------------------------------------------
// 連続数 len が color にとって「五（勝ち）」かどうか
//-------------------------------------------------
static int isFiveLength(int color, int len) {
    if (color == STONE_BLACK && (check & CHOLEN)) {
        return len == 5; // 黒は長連が禁じ手のとき，ちょうど5つだけが五
    }
    return len >= 5;
}

//-------------------------------------------------
// (x,y) の石が五を作っているか
//-------------------------------------------------
static int makesFive(int board[][BOARD_SIZE], int x, int y) {
    int d;
    for (d = 0; d < 4; d++) {
        if (isFiveLength(board[y][x], runLength(board, x, y, d))) {
            return 1;
        }
    }
    return 0;
}

//-------------------------------------------------
// 方向 d 上で，石を1つ足すと (x,y) の石を含む五ができる空点を探す．
// 見つかった空点の (x,y) からのオフセットを昇順で pts に入れ，個数を返す
//-------------------------------------------------
static int fivePointsInDir(int board[][BOARD_SIZE], int x, int y, int d,
                           int pts[]) {
    int color = board[y][x];
    int n = 0;
    int k, lo, hi;
    for (k = -4; k <= 4; k++) {
        int qx = x + k * DX[d];
        int qy = y + k * DY[d];
        if (k == 0 || cellAt(board, qx, qy) != STONE_SPACE) {
            continue;
        }
        board[qy][qx] = color; // 仮に置く
        runRange(board, x, y, d, &lo, &hi);
        if (lo <= k && k <= hi && isFiveLength(color, hi - lo + 1)) {
            pts[n++] = k;
        }
        board[qy][qx] = STONE_SPACE; // 元に戻す
    }
    return n;
}

//-------------------------------------------------
// 方向 d 上で (x,y) の石を含む四の数
//   五になる点が2つあり，間隔が5なら同じ4石による達四なので1つと数える．
//   それ以外（●・●●●・● など）は別々の四なので点の数だけ数える
//-------------------------------------------------
static int foursInDir(int board[][BOARD_SIZE], int x, int y, int d) {
    int pts[8];
    int n = fivePointsInDir(board, x, y, d, pts);
    if (n == 2 && pts[1] - pts[0] == 5) {
        return 1;
    }
    return n;
}

//-------------------------------------------------
// 方向 d 上で (x,y) の石が達四の一部になっているか
//-------------------------------------------------
static int isStraightFourInDir(int board[][BOARD_SIZE], int x, int y, int d) {
    int pts[8];
    int n = fivePointsInDir(board, x, y, d, pts);
    return n == 2 && pts[1] - pts[0] == 5;
}

//-------------------------------------------------
// 方向 d 上で (x,y) の石が三になっているか
//   空点に1つ置くと (x,y) を含む達四ができ，かつその手が禁じ手でないなら三．
//   同じ方向にすでに四があれば，その方向は三とは数えない
//-------------------------------------------------
static int threeInDir(int board[][BOARD_SIZE], int x, int y, int d) {
    int color = board[y][x];
    int k, lo, hi, ok;
    if (foursInDir(board, x, y, d) > 0) {
        return 0;
    }
    for (k = -4; k <= 4; k++) {
        int qx = x + k * DX[d];
        int qy = y + k * DY[d];
        if (k == 0 || cellAt(board, qx, qy) != STONE_SPACE) {
            continue;
        }
        board[qy][qx] = color; // 仮に置く
        ok = 0;
        if (isStraightFourInDir(board, x, y, d)) {
            runRange(board, x, y, d, &lo, &hi);
            if (lo <= k && k <= hi) {
                // 達四を作る手そのものが禁じ手なら偽の三（黒のみ）
                ok = (color != STONE_BLACK) ||
                     (judgeBlack(board, qx, qy) == NOKINJITE);
            }
        }
        board[qy][qx] = STONE_SPACE; // 元に戻す
        if (ok) {
            return 1;
        }
    }
    return 0;
}

//-------------------------------------------------
// (x,y) に置かれた黒石が禁じ手かどうか（石は置いた状態で呼ぶ）
//   戻り値 NOKINJITE / CHOLEN / SHISHI / SANSAN
//   check で無効になっている禁じ手は判定しない
//-------------------------------------------------
static int judgeBlack(int board[][BOARD_SIZE], int x, int y) {
    int d, n;

    if (makesFive(board, x, y)) {
        return NOKINJITE; // 五ができれば禁じ手より優先
    }

    if (check & CHOLEN) {
        for (d = 0; d < 4; d++) {
            if (runLength(board, x, y, d) >= 6) {
                return CHOLEN;
            }
        }
    }
    if (check & SHISHI) {
        for (d = 0, n = 0; d < 4; d++) {
            n += foursInDir(board, x, y, d);
        }
        if (n >= 2) {
            return SHISHI;
        }
    }
    if (check & SANSAN) {
        for (d = 0, n = 0; d < 4 && n < 2; d++) {
            n += threeInDir(board, x, y, d);
        }
        if (n >= 2) {
            return SANSAN;
        }
    }
    return NOKINJITE;
}

//-------------------------------------------------
// 方向 d 上で，オフセット [lo, hi] の範囲（skip を除く）にある石のうち，
// 盤面の上（同じ行なら左）にある石の座標を返す．形の表示位置に使う
//-------------------------------------------------
static void firstStone(int x, int y, int d, int lo, int hi, int skip, int *sx,
                       int *sy) {
    int k, first = 1;
    for (k = lo; k <= hi; k++) {
        int cx = x + k * DX[d];
        int cy = y + k * DY[d];
        if (k == skip) {
            continue;
        }
        if (first || cy < *sy || (cy == *sy && cx < *sx)) {
            *sx = cx;
            *sy = cy;
            first = 0;
        }
    }
}

//-------------------------------------------------
// (x,y) に置かれた石が作った三・四を表示する
//   四連：4つ連続した四      飛び四：間に1つ空きのある四
//   三連：3つ連続した三      飛び三：間に1つ空きのある三
//   座標は形の端の石（上側，同じ行なら左側）
//   三は check に SANSAN，四は check に SHISHI が含まれるときに表示する
//-------------------------------------------------
static void printShapes(int board[][BOARD_SIZE], int x, int y) {
    int color = board[y][x];
    int d, i, k, lo, hi, sx = x, sy = y;
    int pts[8], n;

    for (d = 0; d < 4; d++) {
        n = fivePointsInDir(board, x, y, d, pts);
        if (n > 0) {
            if (!(check & SHISHI)) {
                continue;
            }
            if (n == 2 && pts[1] - pts[0] == 5) {
                // 達四：・●●●●・
                runRange(board, x, y, d, &lo, &hi);
                firstStone(x, y, d, lo, hi, 99, &sx, &sy);
                printf("四連(%d %d)\n", sx, sy);
                continue;
            }
            for (i = 0; i < n; i++) {
                k = pts[i];
                // 五になる点に仮に置く
                board[y + k * DY[d]][x + k * DX[d]] = color;
                runRange(board, x, y, d, &lo, &hi);
                board[y + k * DY[d]][x + k * DX[d]] = STONE_SPACE;
                // 長連が勝ちになる設定で6連以上になる場合は，
                // (x,y) と k を含む5マスに絞る
                if (lo < k - 4) {
                    lo = k - 4;
                }
                if (hi > k + 4) {
                    hi = k + 4;
                }
                if (lo < -4) {
                    lo = -4;
                }
                if (hi > 4) {
                    hi = 4;
                }
                firstStone(x, y, d, lo, hi, k, &sx, &sy);
                printf("%s(%d %d)\n", (k == lo || k == hi) ? "四連" : "飛び四",
                       sx, sy);
            }
            continue;
        }

        if (!(check & SANSAN)) {
            continue;
        }
        // 三：達四を作れる点（禁じ手でないもの）を探す．端に置いて達四になるなら三連
        {
            int found = 0, renzoku = 0;
            int flo = 0, fhi = 0, fk = 0;
            for (k = -4; k <= 4 && !renzoku; k++) {
                int qx = x + k * DX[d];
                int qy = y + k * DY[d];
                int ok = 0;
                if (k == 0 || cellAt(board, qx, qy) != STONE_SPACE) {
                    continue;
                }
                board[qy][qx] = color;
                if (isStraightFourInDir(board, x, y, d)) {
                    runRange(board, x, y, d, &lo, &hi);
                    ok = lo <= k && k <= hi &&
                         ((color != STONE_BLACK) ||
                          (judgeBlack(board, qx, qy) == NOKINJITE));
                }
                board[qy][qx] = STONE_SPACE;
                if (ok) {
                    found = 1;
                    flo = lo;
                    fhi = hi;
                    fk = k;
                    renzoku = (k == lo || k == hi);
                }
            }
            if (found) {
                firstStone(x, y, d, flo, fhi, fk, &sx, &sy);
                printf("%s(%d %d)\n", renzoku ? "三連" : "飛び三", sx, sy);
            }
        }
    }
}

//=========================================================================
// 以下は外部から呼べる関数
//=========================================================================

//-------------------------------------------------
// 空点 (x,y) に黒を置いたら禁じ手になるか（盤面は変更しない）
//   戻り値 NOKINJITE(0) / CHOLEN / SHISHI / SANSAN
//   AI から候補手のチェックに使える．
//   使う場合は extern int isForbidden(int board[][BOARD_SIZE], int x, int y);
//   を宣言すること
//-------------------------------------------------
int isForbidden(int board[][BOARD_SIZE], int x, int y) {
    int r;
    if (!checkOutPos(x, y) || board[y][x] != STONE_SPACE) {
        return NOKINJITE;
    }
    board[y][x] = STONE_BLACK;
    r = judgeBlack(board, x, y);
    board[y][x] = STONE_SPACE;
    return r;
}

//-------------------------------------------------
// ゲーム終了判定処理
//   直前に置かれた石について，五（勝ち）と禁じ手を判定する
//   戻り値 勝負がついたとき 1，それ以外 0
//-------------------------------------------------
int gameEndProcess(int board[][BOARD_SIZE], int c33Board[][BOARD_SIZE],
                   int c44Board[][BOARD_SIZE]) {
    int (*prev)[BOARD_SIZE] = c33Board; // 直前の盤面
    int x, y, kind;
    int result = 0;
    int n_empty = 0;
    (void)c44Board;

    for (y = 0; y < BOARD_SIZE; y++) {
        for (x = 0; x < BOARD_SIZE; x++) {
            if (board[y][x] == STONE_SPACE) {
                n_empty++;
                continue;
            }
            if (result || board[y][x] == prev[y][x]) {
                continue;
            }

            // 新しく置かれた石
            if (makesFive(board, x, y)) {
                printf("%sの勝ちです。\n",
                       (board[y][x] == STONE_BLACK) ? "●" : "○");
                result = 1;
                continue;
            }
            if (board[y][x] == STONE_BLACK) {
                printShapes(board, x, y); // 置いた石が作った三・四を表示
            }
            if (board[y][x] == STONE_BLACK &&
                (kind = judgeBlack(board, x, y)) != NOKINJITE) {
                switch (kind) {
                case CHOLEN:
                    printf("長連の禁じ手です。%sの勝ちです\n", "○");
                    break;
                case SHISHI:
                    printf("四四の禁じ手です。%sの勝ちです\n", "○");
                    break;
                case SANSAN:
                    printf("三三の禁じ手です。%sの勝ちです\n", "○");
                    break;
                }
                result = 1;
            }
        }
    }

    // 直前の盤面を更新
    for (y = 0; y < BOARD_SIZE; y++) {
        for (x = 0; x < BOARD_SIZE; x++) {
            prev[y][x] = board[y][x];
        }
    }

    if (!result && n_empty == 0) {
        printf("引き分けです。\n");
        result = 1;
    }
    return result;
}

//-------------------------------------------------
// 5連確認
//   (x,y) の石を含む五ができていれば 1
//   （check に CHOLEN が含まれるとき，黒の長連は五ではないので 0）
//-------------------------------------------------
int lenCheck(int board[][BOARD_SIZE], int x, int y) {
    if (!checkOutPos(x, y) || board[y][x] == STONE_SPACE) {
        return 0;
    }
    return makesFive(board, x, y);
}

//-------------------------------------------------
// 三の確認
//   (x,y) の石を含む三の数（三になっている方向の数）を返す．三がなければ 0
//-------------------------------------------------
int isSan(int board[][BOARD_SIZE], int x, int y) {
    int d, n = 0;
    if (!checkOutPos(x, y) || board[y][x] == STONE_SPACE) {
        return 0;
    }
    for (d = 0; d < 4; d++) {
        n += threeInDir(board, x, y, d);
    }
    return n;
}

//-------------------------------------------------
// 四の確認
//   (x,y) の石を含む四の数を返す（1ライン上に2つある場合は2と数える）．
//   四がなければ 0
//-------------------------------------------------
int isShi(int board[][BOARD_SIZE], int x, int y) {
    int d, n = 0;
    if (!checkOutPos(x, y) || board[y][x] == STONE_SPACE) {
        return 0;
    }
    for (d = 0; d < 4; d++) {
        n += foursInDir(board, x, y, d);
    }
    return n;
}

//-------------------------------------------------
// 6連（長連）の確認
//   (x,y) の石を含む6つ以上の連続があれば 1
//-------------------------------------------------
int isRoku(int board[][BOARD_SIZE], int x, int y) {
    int d;
    if (!checkOutPos(x, y) || board[y][x] == STONE_SPACE) {
        return 0;
    }
    for (d = 0; d < 4; d++) {
        if (runLength(board, x, y, d) >= 6) {
            return 1;
        }
    }
    return 0;
}
