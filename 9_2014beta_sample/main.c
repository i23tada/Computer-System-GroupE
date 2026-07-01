#include "libs.h" /* 前のmy3664hの内容は，libs/libs.hへ統合した */

volatile int tma_flag  = FALSE;
volatile int sec_flag  = FALSE;
volatile int tmv_flag  = FALSE;
volatile int stop_flag = FALSE;
volatile long sec      = 0;

volatile int tempo_flag=FALSE;
int tempo_compare=0;
int time=0;
int s[5] = {0,0,0,0,0};
char x[4]={'+','-','*','/'};
int i=0,j=0;

#define DEBUG /* デバッグ中は，定義しておく */

unsigned long int seed;

unsigned long int getrand(void) {
  seed = (48271L * seed) & 0x7fffffff;
  return (seed);
}

static unsigned int matrix_led_pattern[8] =
    //{0x007e,0x0011,0x0011,0x0011,0x007e,0x7f00,0x4900,0x4900};
    {0x7e7e, 0x1111, 0x1111, 0x0011, 0x007e, 0x7f00, 0x4900, 0x4900};
/*列0～7のデータ(詳細は，過去のリストを読め)*/

/* int_timera() や int_imterv() の割込ルーチン(ボトムハーフの処理) */

#pragma interrupt /*割込処理ルーチンであることの指定*/
void int_timera(void) {
  volatile static int count = 0;
  BIT_CLR(IRR1, 6); /* 本来のタイマA割り込みフラグクリア */
  tma_flag = TRUE;
  //	EI();         /* 必要に応じて EI()を実行  */
  /*32回呼び出されたら，if文の中が実行されて，sec_flagが有効になる*/
  if (++count >= 32) {
    count    = 0;
    sec_flag = TRUE;
    sec++; /* secは，1秒ごとにインクリメントされる*/
  }

#ifdef DEBUG
  { /*ボトムハーフ処理の余力チェック用コード*/
    volatile long int loop;
    for (loop = 0; loop < 1; loop++);
  }

  if ((IRR1 & 0x40) != 0) { /* 割り込みフラグの状態チェック */
    ENABLE_LED_GREEN();     /* もし割り込みフラグが立ってたら緑色LED点灯 */
  }
#endif
}

#pragma interrupt
void int_timerv(void) { /* 約1[msec] ごとに呼び出されるようにする*/
  static unsigned int column = 0;
  unsigned int i, p;
  /*スピーカ周りの値*/

  // 卒業研究で作成してくれたコードから改変。
  // tempoを変更できるように改造 by T.nitta
  // 下記のtempoやtempo_compareをconstでなく，変数にする。
  // 但し，この計算をボトムハーフで毎回実行すると,処理が重いので
  // トップハーフで計算を行う。
  //        const int tempo=120;
  //        const int tempo_compare = 1000/(tempo * 16 / 60);
  //        const int tempo_compare = 3735 / tempo ;
  static int tempo_count = 0;

  TCSRV &= ~0xe0; /* タイマVの割り込みフラグクリア*/
  tmv_flag = TRUE;
  /*int_timervは，割込禁止のまま,全力動作*/

  /* 1ms ごとにハードリアルタイムで動作させたい処理(matrix_led周り)*/

  column = (column + 1) & 0x0007; /* column=(column++)&0x0007は，NGです…  */
  ENABLE_MATRIX_LATCH();          /*ラッチを有効にし，舞台裏でD-FFをセット */

  /*16bit (1列分) をシリアル転送*/
  for (p = matrix_led_pattern[column], i = 0; i < 16; i++, p <<= 1) {
    if ((p & 0x8000) == 0) {
      DISABLE_MATRIX_SIN();
    } else {
      ENABLE_MATRIX_SIN();
    }
    SET_H_MATRIX_SCLK();
    SET_L_MATRIX_SCLK(); /* posエッジ一つ*/
  }

  ENABLE_MATRIX_BLANK(); /*以下の筒抜けと列変更は表示させない*/
  DISABLE_MATRIX_LATCH();
  ENABLE_MATRIX_LATCH();        /*一瞬筒抜け*/
  SELECT_MATRIX_COLUMN(column); /*j番目の列に変更*/
  DISABLE_MATRIX_BLANK();       /*点灯させる*/


  /*スピーカ関連処理*/
  if (snd_check() == TRUE) {
    if (tempo_compare < ++tempo_count) {
      tempo_count = 0;
      tempo_flag  = TRUE;
    }
  }


#ifdef DEBUG
  { /*ボトムハーフ処理の余力チェック用コード*/
    volatile int loop;
    for (loop = 0; loop < 10; loop++);
    /*とある条件下での挙動(但し，その後にキー入力のコードを追加)*/
    /*loop<461 :10分程度では,破綻を確認できず(但し，検証回数は1回だけ)*/
  }

  if ((TCSRV & 0xe0) != 0) { /* 割り込みフラグの状態チェック */
    ENABLE_LED_RED();        /* もし割り込みフラグが立ってたら赤色LED点灯*/
  }
#endif
}

/*タイマ関係の値設定*/
void timer_init(void) {
  /*タイマAの設定(1/32[sec]ごとのタイマ割り込みを生成) */
  TMA = 0x9f; /* タイマA TCAとプリスケーラWのリセット(2011/12/15追加) */
  TMA = 0x9b; /* タイマA 1/32[sec]毎の割込の設定                      */
  /* 0x9b => 1001 1011 の内訳 (解説は，過去のリストを見よ)            */

  IENR1 |= 0x40; /* タイマAの割り込み生成を有効にする*/

  /*タイマVの設定(マニュアルを見て,以下の3行の右辺を書き換えよ)*/
  TCORA = 124;  /*割り込み間隔が約1msになるようにする(125分周)*/
  TCRV0 = 0x4b; /*コンペアマッチAで割り込み。その際にカウンタクリア*/
  TCRV1 = 0x01; /*クロックは，内部のΦの128分周(16MHz/128)を用いる*/

  /*timervも必ず0からスタートさせる */
  TCNTV = 0x00;     /* タイマVの内部カウンタを0から開始*/
  BIT_CLR(IRR1, 6); /* タイマAの割り込みフラグクリア   */
  TCSRV &= ~0xe0;   /* タイマVの割り込みフラグクリア   */

  /* タイマWの設定(スピーカ用)*/
  TCRW = 0xbc;
}

/* main() などの割り込みルーチン以外の処理は，トップハーフの処理*/

/* User Interface のステートマシン (ここを作り込む)*/

/* 適切なモード名を入れることが望ましいが，MODE_0～とする。 */
/* 美しい書き方をするならば，適切なテーブルを持つ必要がある */
enum MENU_MODE {
  MODE_OUT_OF_MIN = -1,
  MODE_0,
  MODE_1,
  MODE_2,
  MODE_3,
  MODE_4,
  MODE_5,
  MODE_6,
  MODE_7,
  MODE_8,
  MODE_OUT_OF_MAX
};

// うぅ。下記のKとkの見分け(大文字小文字の見分け)が付かずに，
// １時間半の痛恨のロス(2011/12/19 00:37 by T.NITTA)
enum SW_CODE {
  KEY_NONE    = 0,                  // キー入力無し
  KEY_SHORT_U = (1 << 4),           // 上短押し
  KEY_SHORT_D = (1 << 3),           // 下短押し
  KEY_SHORT_L = (1 << 2),           // 左短押し
  KEY_SHORT_R = (1 << 1),           // 右短押し
  KEY_SHORT_C = (1 << 0),           // 中央短押し
  KEY_LONG_U  = (0x80 | (1 << 4)),  // 上長押し
  KEY_LONG_D  = (0x80 | (1 << 3)),  // 下長押し
  KEY_LONG_L  = (0x80 | (1 << 2)),  // 左長押し
  KEY_LONG_R  = (0x80 | (1 << 1)),  // 右長押し
  KEY_LONG_C  = (0x80 | (1 << 0))   // 中央長押し
};

typedef struct _UI_DATA {
  int mode;
  int prev_mode;
  unsigned char sw;
} UI_DATA;

extern void do_mode0(UI_DATA* ui_data);
extern void do_mode1(UI_DATA* ui_data);
extern void do_mode2(UI_DATA* ui_data);
extern void do_mode3(UI_DATA* ui_data);
extern void do_mode4(UI_DATA* ui_data);
extern void do_mode5(UI_DATA* ui_data);
extern void do_mode6(UI_DATA* ui_data);
extern void do_mode7(UI_DATA* ui_data);
extern void do_mode8(UI_DATA* ui_data);

UI_DATA* ui(char sw) { /* ミーリ型？ムーア型？どっちで実装？良く考えて */
  static UI_DATA ui_data = {
      MODE_0,
      MODE_0,
  };
  int prev_mode;

  ui_data.sw = (sw & 0x9f); /*念のために，b6,b5を0にしておく*/
  prev_mode  = ui_data.mode;

  switch (ui_data.mode) {
    case MODE_0:
      do_mode0(&ui_data);
      break;
    case MODE_1:
      do_mode1(&ui_data);
      break;
    case MODE_2:
      do_mode2(&ui_data);
      break;
    case MODE_3:
      do_mode3(&ui_data);
      break;
    case MODE_4:
      do_mode4(&ui_data);
      break;
    case MODE_5:
      do_mode5(&ui_data);
      break;
    case MODE_6:
      do_mode6(&ui_data);
      break;
    case MODE_7:
      do_mode7(&ui_data);
      break;
    case MODE_8:
      do_mode8(&ui_data);
      break;
    default:
      break;
  }

  ui_data.prev_mode = prev_mode;

  return &ui_data;
}

void do_mode0(UI_DATA* ud) {
  static int matrix_scroll  = FALSE;
  static int next_mode_data = MODE_0;
  int prev_next_mode_data;
  static char str[2];

  /*モード0で必ず実行するコードを記述*/
  prev_next_mode_data = next_mode_data;

  if (ud->prev_mode != ud->mode) { /* 他のモードからモード0に遷移した時に実行 */
    /*必要なら，何らかのモードの初期化処理*/
    lcd_putstr(0, 0, "MODE0->MODE0"); /*モード0の初期表示*/
    next_mode_data      = MODE_0;
    prev_next_mode_data = MODE_0;
    matrix_scroll       = FALSE;
  }

  switch (ud->sw) { /*モード内でのキー入力別操作*/

    case KEY_SHORT_U: /* 上短押し */
      if (next_mode_data < (MODE_OUT_OF_MAX - 1)) {
        next_mode_data++;
      }
      break;

    case KEY_SHORT_D: /* 下短押し */
      if (next_mode_data > (MODE_OUT_OF_MIN + 1)) {
        next_mode_data--;
      }
      break;

    case KEY_SHORT_L:  /* 左短押し */
      FLIP_LED_RED();  // 赤色LEDのフリップを実行
      break;

    case KEY_SHORT_R: /* 右短押し */
      FLIP_LED_GREEN();
      break;

    case KEY_LONG_L: /* 左長押し */
      FLIP_LED_BLUE();
      break;

    case KEY_LONG_R:                 /* 右長押し */
      if (matrix_scroll == FALSE) {  // マトリクスLEDのスクロールフラグのフリップ
        matrix_scroll = TRUE;
      } else {
        matrix_scroll = FALSE;
      }

      break;

    case KEY_LONG_C:             /* 中央キーの長押し */
      ud->mode = next_mode_data; /*次は，モード変更*/
      break;

    default: /*上記以外*/
      break;
  }

  /* モードの終了時に処理するコード */
  if ((prev_next_mode_data != next_mode_data) || sec_flag == TRUE) {
    /* 次の希望するモードの値が変わった時の処理 */
    str[0] = '0' + next_mode_data;
    str[1] = '\0';
    lcd_putstr(11, 0, str);
  }

  if (sec_flag == TRUE) { /* 1秒ごとの処理*/
    lcd_clear();
    lcd_putdec(0, 1, 5, sec); /* LCDの下の行(1行目)に，経過秒数を表示 */
    lcd_putstr(0, 0, "MODE0->MODE");
    lcd_putstr(11, 0, str);

    /*コメント：ここでは，LCDの再描画処理を1秒ごとに行っている。        */
    /*これは，万が一，予期せぬノイズで，LCDの表示に誤動作が発生しても， */
    /*1秒後には，回復させるという効果を期待している。ハードの世界では， */
    /*いくら工夫しても，防ぎようが無いノイズがあったりするのです…。    */
    /*不具合の発生確率は，「コストをある程度かければ」下げることが可能。*/

    /*ついでに，matrix_ledのスクロールも行ってみる*/
    if (matrix_scroll != FALSE) {
      /*FALSEでチェックしているのは，0でのチェックの方が一般に高速だから*/
      /*なお，下記の関数は，単なるデバッグアウトなので，実行をし続けたら*/
      /*フォントテーブルを抜け出してしまい，表示が変になります。*/
      matrix_font_debug_out_sample(matrix_led_pattern);
    }

    sec_flag = FALSE;
  }
}

void do_mode1(UI_DATA* ud) {
  static int tempo = 120;

  if (ud->prev_mode != ud->mode) { /* 他のモード遷移した時に実行 */
    /*必要なら，何らかのモードの初期化処理*/
    lcd_clear();
    lcd_putstr(0, 0, "MODE1");     /*モード1の初期表示*/
    lcd_putstr(0, 1, "TEMPO=120"); /*モード1の初期表示*/
    tempo         = 120;
    tempo_compare = 3735 / tempo; /* 1000/(tempo * 16 / 60) を展開 */
  }

  /*モード1は，真中ボタンが押されたら，MODE0に戻るだけの単純な処理*/
  /*それに，beta2のバージョンでは，音楽再生機能の起動部分を追加*/
  /*但し，main関数内で，キーのデバッグ表示を行っている*/
  switch (ud->sw) {      /*モード内でのキー入力別操作*/
    case KEY_LONG_C:     /* 中央キーの長押し */
      ud->mode = MODE_0; /* 次は，モード0に戻る */
      break;

    case KEY_SHORT_L: /* 左のキーが押されたら,演奏開始*/
      snd_play("CDEFEDC EFG^A_GFE C C C C !C!C!D!D!E!E!F!FEDC");
      break;

    case KEY_SHORT_R: /* 右のキーが押されたら,演奏終了*/
      snd_stop();
      break;

    case KEY_SHORT_U: /* テンポUP */
      tempo += 10;
      if (tempo >= 240) tempo = 240;
      tempo_compare = 3735 / tempo;
      lcd_putdec(6, 1, 3, tempo);
      break;

    case KEY_SHORT_D: /* テンポDOWN */
      tempo -= 10;
      if (tempo <= 60) tempo = 60;
      tempo_compare = 3735 / tempo;
      lcd_putdec(6, 1, 3, tempo);
      break;
  }
}


/*時計表示のアルゴリズムの一部*/
void show_sec(void) {
  char data[6];
  int h, s;
  long sec_hold = sec; /* 値を生成している最中に，secが変わると嫌なので，   */
                       /* ここで，secの値を捕まえる。secの値は，ボトムハーフ*/
                       /* で変化させているので，運が悪いと処理中に変化する。*/

  s = sec_hold % 60;
  h = (sec_hold / 60); /* ここで，hの値の健全性は，検証していないからね。*/
                       /* ヒントは，「secは，int型」*/

  data[0] = '0' + h / 10;
  data[1] = '0' + h % 10;
  data[2] = ':';
  data[3] = '0' + s / 10;
  data[4] = '0' + s % 10;
  data[5] = '\0';

  lcd_putstr(16 - 5, 1, data);
}

void do_mode2(UI_DATA* ud) {
  if (ud->prev_mode != ud->mode || sec_flag == TRUE) {
    /* 他のモード遷移した時に実行 もしくは，1秒ごとに表示*/
    /*必要なら，何らかのモードの初期化処理*/
    lcd_clear();                          // 0123456789ABCDEF
    lcd_putstr(0, 0, "MODE2:secｦ ﾋｮｳｼﾞ"); /*モード2の初期表示*/
    show_sec();
    sec_flag = FALSE;
  }

  /*モード2は，真中ボタンが押されたら，MODE0に戻る*/
  switch (ud->sw) {      /*モード内でのキー入力別操作*/
    case KEY_LONG_C:     /* 中央キーの長押し */
      ud->mode = MODE_0; /* 次は，モード0に戻る */
      break;
  }
}

void show_tokei(void) {
  char data[9];
  int h,m,s;
  long sec_hold=sec; /* 値を生成している最中に，secが変わると嫌なので，   */
                    /* ここで，secの値を捕まえる。secの値は，ボトムハーフ*/
                    /* で変化させているので，運が悪いと処理中に変化する。*/

  s=sec_hold % 60;
  m=((sec_hold / 60)%60); /* ここで，hの値の健全性は，検証していないからね。*/
                     /* ヒントは，「secは，int型」*/
  h=((sec_hold / 3600)%24);

  data[0]='0'+h/10;
  data[1]='0'+h%10;
  data[2]=':';
  data[3]='0'+m/10;
  data[4]='0'+m%10;
  data[5]=':';
  data[6]='0'+s/10;
  data[7]='0'+s%10;
  data[8]='\0';

  lcd_putstr(16-8,1,data);
  if(stop_flag==TRUE) {
    lcd_putstr(0,1,"ｾｯﾃｲ");
    if(time==0) lcd_putstr(5,1,"h");
    if(time==1) lcd_putstr(5,1,"m");
    if(time==2) lcd_putstr(5,1,"s");
    }
  }

void do_mode3(UI_DATA* ud) {
  if (ud->prev_mode != ud->mode || sec_flag == TRUE) {
    lcd_clear();
    lcd_putstr(0, 0, "MODE3:24ｼﾞｶﾝｲﾄｹｲ");
    show_tokei();
    sec_flag=FALSE;
  }

  if(stop_flag == TRUE) {
    switch(ud->sw){
    case KEY_LONG_C:
      ud->mode=MODE_0;
      break;
    case KEY_SHORT_U:
      if(time==0)sec+=3600;
      if(time==1)sec+=60;
      if(time==2)sec++;
      break;
    case KEY_SHORT_D:
      if(time==0)sec-=3600;
      if(time==1)sec-=60;
      if(time==2)sec--;
      break;
    case KEY_SHORT_L:
      if(time>0)time--;
      break;
    case KEY_SHORT_R:
      if(time<2)time++;
      break;
    case KEY_SHORT_C:
      stop_flag=FALSE;
    }
  }
  
  if(stop_flag==FALSE){
    switch(ud->sw){  /*モード内でのキー入力別操作*/
    case KEY_LONG_C:  /* 中央キーの長押し */
      ud->mode=MODE_0; /* 次は，モード0に戻る */
      break;
    case KEY_SHORT_C:
      stop_flag=TRUE;
      break;
    }
  }

  if(ud->sw) show_tokei();
}

// ゲームの横と縦の高さ設定
#define GAME_W 8
#define GAME_H 8

// LEDを全て消す
static void matrix_clear_all(void) {
  int x;
  for (x = 0; x < 8; x++) {
    matrix_led_pattern[x] = 0x0000;
  }
}

// 任意の(x,y)のLEDを点灯
static void matrix_set_dot(int x, int y) {
  // 変な値を弾く処理
  if (x < 0 || x >= GAME_W || y < 0 || y >= GAME_H) return;
  // matrix配列のx列目とのor
  // x列目のデータのうち、y行目に対応するbitだけ1にする
  // yが0だと赤8個、緑8個のLEDの列の一番奥に1が送られるから、8×8LEDのx列目の一番下の部分になる
  matrix_led_pattern[x] |= (0x8000 >> y);
}

// LEDキャッチゲーム
void do_mode4(UI_DATA* ud) {
  static int player_x;
  static int fall_x;
  static int fall_y;
  static int score;
  static int miss;
  static int tick;
  static int start_sec;
  static int game_over;
  static int restart;

  // 別のモードからこのモードに来たときの処理、もしくはリスタート時
  if (ud->prev_mode != ud->mode || restart) {
    restart = FALSE;
    lcd_clear();

    // プレイヤーの位置
    player_x = 3;
    // 落ちてくるやつのx座標
    fall_x = 0;
    // 落ちてくるやつのy座標
    fall_y = 0;
    // スコア
    score = 0;
    // ミスした回数
    miss = 0;
    // 何秒ごとにLEDを落とすかに関係する変数
    tick = 0;
    // 始まったsec
    start_sec = sec;
    // ゲーム終了かどうか
    game_over = FALSE;

    // LEDをクリアする
    matrix_clear_all();

    // 初期表示
    lcd_putstr(0, 0, "CATCH GAME");
    lcd_putstr(0, 1, "SCORE=000 M=0");
  }

  switch (ud->sw) {
    // 左に動く
    case KEY_SHORT_L:
      player_x--;
      if (player_x < 0) player_x = 0;
      break;

    // 右に動く
    case KEY_SHORT_R:
      player_x++;
      if (player_x >= GAME_W) player_x = GAME_W - 1;
      break;

    // ゲームオーバーから復活
    case KEY_SHORT_U:
      restart = TRUE;
      break;

    // モード0に戻る
    case KEY_LONG_C:
      matrix_clear_all();
      ud->mode = MODE_0;
      break;

    default:
      break;
  }

  // 30秒経ったかどうか
  if ((sec - start_sec) >= 30) {
    game_over = TRUE;
  }

  if (game_over == FALSE) {
    /* do_mode4は約1/32秒ごとに呼ばれるので、数回に1回だけ落とす */
    // 5回に一回
    // ここの回数を変えると落ちる速度を変えられる
    tick++;
    if (tick >= 5) {
      tick = 0;
      // 5回に1回yを足して上に動かす
      fall_y++;

      // 一番上まで来たらそのときのプレイヤーと位置が一緒かチェック
      if (fall_y >= GAME_H) {
        if (fall_x == player_x) {
          score++;
        } else {
          miss++;
        }

        fall_y = 0;

        /* 簡易ランダムっぽく横位置を変える */
        // if文の中に入れることで、上に来るまではxは固定
        fall_x = (fall_x + 3) & 0x07;
      }
    }
  }

  matrix_clear_all();

  if (game_over) {
    matrix_clear_all();

    lcd_clear();
    lcd_putstr(0, 0, "FINISH");
    lcd_putstr(0, 1, "S=");
    lcd_putdec(2, 1, 3, score);
    lcd_putstr(6, 1, "M=");
    lcd_putdec(8, 1, 2, miss);
    return;
  }

  /* 落ちてくるLED */
  // 毎回呼ばれるたびにセットし直す
  matrix_set_dot(fall_x, fall_y);

  /* 下のキャッチャー */
  // 一番上に固定
  matrix_set_dot(player_x, GAME_H - 1);

  lcd_putstr(0, 0, "CATCH GAME");
  // 残り時間
  lcd_putstr(11, 0, "T=");
  lcd_putdec(13, 0, 2, 30 - (sec - start_sec));
  // スコア
  lcd_putstr(0, 1, "SCORE=");
  lcd_putdec(6, 1, 3, score);
  // ミス回数
  lcd_putstr(10, 1, "M=");
  lcd_putdec(12, 1, 2, miss);
}

void do_mode5(UI_DATA* ud) {}

// Here is my work space!
void do_mode6(UI_DATA* ud) {
  static char stage[2][16] = {{0}};
  static int player_y      = 0;
  static int timing        = 0;
  static int material_flag = 0;
  int temp                 = 3;  // 速度3
  int i, j;
  static unsigned int shuffle_counter = 0;

  // 初めてこのモードに入った時だけ初期化する
  if (ud->prev_mode != ud->mode) {
    lcd_clear();

    // 配列全体を半角スペースで埋める
    for (i = 0; i < 2; i++) {
      for (j = 0; j < 16; j++) {
        stage[i][j] = ' ';
      }
    }
    timing        = 0;
    material_flag = 0;

    // グローバル変数のseedを「奇数」で1回だけ初期化する
    seed = 12345;
  }

  // 1. キー入力の更新
  switch (ud->sw) {
    case KEY_SHORT_U:
      player_y = 0;
      break;
    case KEY_SHORT_D:
      player_y = 1;
      break;
    case KEY_LONG_C:
      ud->mode = MODE_0;
      return;
  }

  // 2. 時間のカウント（元の形に戻す・フラグは折らない）
  if (tmv_flag == TRUE) {
    timing++;
  }

  shuffle_counter++;

  // 3. 一定時間（temp）ごとに障害物を動かす処理
  // timingがtemp以上になったら実行する
  if (timing >= temp) {
    timing = 0;

    // stage配列の文字を左にずらす処理
    for (i = 0; i <= 1; i++) {
      for (j = 0; j < 15; j++) {
        stage[i][j] = stage[i][j + 1];
      }
      stage[i][15] = ' ';
    }

    // 障害物の生成
    if (material_flag == 0) {
      int y        = shuffle_counter & 1;
      stage[y][15] = '*';

      // 出現間隔決定
      material_flag = (shuffle_counter % 3) + 1;
    } else {
      material_flag--;
    }
  }

  if (stage[player_y][0] == '*') {
    // 1. 液晶をクリアしてゲームオーバー画面を出す
    lcd_clear();
    lcd_putstr(4, 0, "GAME OVER");  // 中央寄りに表示

    volatile long int delay;
    for (delay = 0; delay < 500000; delay++);

    // 2. モードをタイトル画面（MODE_0）に戻す
    ud->mode = MODE_0;
    return;  // 関数を抜ける
  }

  // 4. 画面の再描画（毎回実行）
  for (i = 0; i <= 1; i++) {
    for (j = 0; j < 16; j++) {
      if (j == 0 && i == player_y) {
        lcd_putstr(0, i, ">");
      } else {
        // 自機がいないマスは、配列の中身（'*' か ' '）をそのまま描画する
        if (stage[i][j] == '*') {
          lcd_putstr(j, i, "*");
        } else {
          lcd_putstr(j, i, " ");
        }
      }
    }
  }
}


void do_mode7(UI_DATA* ud) {}

void show_dentaku() {
  int s1             = (unsigned)(s[0] * 10 + s[1]);
  int s2             = (unsigned)(s[3] * 10 + s[4]);
  unsigned int kekka = 0;
  char data[14];
  data[0] = '0' + s[0];
  data[1] = '0' + s[1];
  data[2] = x[j];
  data[3] = '0' + s[3];
  data[4] = '0' + s[4];
  data[5] = '=';

  if (s2 == 0 && j == 3) {
    data[6]  = '!';
    data[7]  = '!';
    data[8]  = '!';
    data[9]  = '!';
    data[10] = '!';
    data[11] = '!';
    data[12] = '!';
  } else {
    if (j == 0) kekka = s1 + s2;
    if (j == 1) kekka = abs(s1 - s2);
    if (j == 2) kekka = s1 * s2;
    if (j == 3) kekka = s1 / s2;

    data[6] = '0' + ((kekka / 1000) % 10);
    if (s1 - s2 < 0 && j == 1) data[6] = '-';
    data[7] = '0' + ((kekka / 100) % 10);
    data[8] = '0' + ((kekka / 10) % 10);
    data[9] = '0' + (kekka % 10);

    data[10] = '.';
    data[11] = '0';
    data[12] = '0';
    if (j == 3) {
      data[11] += ((s1 * 10 / s2) % 10);
      data[12] += ((s1 * 100 / s2) % 10);
    }
  }

  data[13] = '\0';

  lcd_putstr(0, 1, data);
}

void do_mode8(UI_DATA* ud) {
  if (ud->prev_mode != ud->mode || sec_flag == TRUE) {
    lcd_clear();
    lcd_putstr(0, 0, "MODE8:ﾃﾞﾝﾀｸ");
    show_dentaku();
  }

  switch (ud->sw) {
    case KEY_LONG_C:
      ud->mode = MODE_0;
      break;
    case KEY_SHORT_R:
      if (i < 4) i++;
      break;
    case KEY_SHORT_L:
      if (i > 0) i--;
      break;
    case KEY_SHORT_U:
      if (i == 2) {
        if (j < 3)
          j++;
        else
          j = 0;
      } else {
        if (s[i] < 9)
          s[i]++;
        else
          s[i] = 0;
      }
      break;
    case KEY_SHORT_D:
      if (i == 2) {
        if (j > 0)
          j--;
        else
          j = 3;
      } else {
        if (s[i] > 0)
          s[i]--;
        else
          s[i] = 9;
      }
      break;
  }
}

int main(void) {
  UI_DATA* ui_data = NULL;
  unsigned char sw = KEY_NONE;

  //	char test[]="hoge";

  DI();       /* 念のために速やかにDI() */
  io_init();  /* I/Oポートの初期化ルーチン(matrix,lcd周りも含む) */
  lcd_init(); /* LCDの初期化ルーチンを実行 */

  /*LCDの初期状態は，カーソルOFFで,ブリンクもOFFと仮にしておく*/
  lcd_cursor(OFF);
  lcd_blink(OFF);

  /*スピーカ用のテンポ計算(とりあえずの初期値を設定)*/
  {
    int tempo     = 120;
    tempo_compare = 3735 / tempo; /* 1000/(tempo * 16 / 60) を展開 */
  }

  timer_init(); /* タイマの初期設定を実行(EIの直前に実行) */
  EI();         /* 割り込みを許可  */

  // 半角カタカナ一覧:LCDのカナ文字の記述は、半角カタカナで行う。
  // 入力できないなら，ここからコピペすると良い。
  // ｱｲｳｴｵ ｶｷｸｹｺ ｻｼｽｾｿ ﾀﾁﾂﾃﾄ ﾅﾆﾇﾈﾉ ﾊﾋﾌﾍﾎ ﾏﾐﾑﾒﾓ ﾔﾕﾖ ﾗﾘﾙﾚﾛ  ﾜｦﾝ ﾞﾟ ｯ ｧｨｩｪｫ ｢｣

  // 最初は，モード0から実行を想定。
  lcd_putstr(0, 0, "MODE0->MODE0");

  for (;;) { /* 組み込みシステムは，基本的には無限ループで実行 */


    /* 以下のif文の中は，多分，1/32秒ごとに処理を行う*/
    if (tma_flag == TRUE) {
      sw      = sw_mng(); /* スイッチの入力チェック libs/key.c */
      ui_data = ui(sw);   /* ユーザインタフェースの実行 */

      if (ui_data->mode == MODE_1) {
        /* MODE1だったら，キー入力のデバッグアウトのデバッグ出力 */
        key_debug_out_sample();
      }

      tma_flag = FALSE;
    }

    /* 以下の処理は，1msごとに行う */
    if (tmv_flag == TRUE) {
#if 0
	    /*スピーカのダイレクトコントロールは，beta2から*/
	    /*できないようになりました。*/
	    /*スピーカ用のポートは，タイマWというタイマから*/
	    /*タイマ出力を直接出力することになります*/
	    /*スピーカ用の関数 snd_mng(),snd_play(),snd_stop()などを*/
	    /*活用すること*/

	    /*とりあえず，モード1の時に，上のキーが短押しされたら，   */
	    /*(正確には，短押しの離された時に)，500Hzの音を短く出す   */
	    /*ようなスピーカのサンプルコード。                        */
	    /*スマートにスピーカを鳴らすのだったら，どこかで， フラグ */
	    /*が立ったら， FLIP_SPEAKER_DIRECT_CONTROL();を実行する   */
	    /*ようにするのだろうなぁ。きっと。多分。                  */
	    if(ui_data!=NULL && ui_data->mode==MODE_1 && sw==KEY_SHORT_U){
		FLIP_SPEAKER_DIRECT_CONTROL();
	    }
#endif
      tmv_flag = FALSE;
    }

    if (tempo_flag) {
      snd_mng();
      tempo_flag = FALSE;
    }


    SLEEP(); /*割込が入るまで，スリープさせる(消費電力低減!)*/
  }
  return 0; /*この行は実行されないが、警告を出さないおまじない*/
}

/*************************************************************
 (1)  同様の指示で省略
 (2)  ~nitta/2021exp4/9_2014beta_sample/ の
     (以下も同様で省略)
 (3) 同様の指示で省略
 (4)～(9) 初回と同様。
 ************************************************************/
