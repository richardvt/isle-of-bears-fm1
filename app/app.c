/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 HEYBYE GAMING */
/* Isle of Bears for the M-VAVE FM-1: you (the black bears) against the computer (the white), on the FM-1's own screen,
 * keys and speaker. The rules are iob.h (checked against the web version, step by step), the computer iob_ai.h.
 * Part of fm1-base's unity build (device/unity.c, host/plat_host.c), after the platform: gfx.c is in scope.
 *
 * Controls, as Mighty Arcade's: the lowest keys are the pad (F3 left, F#3 or G#3 up, G3 down, A3 right), F5 selects,
 * G5 takes back; the SELECT knob steps through the choices too. EDIT shows them, HOME goes back to the title. */
#include "fastmath.h"
#include "iob.h"
#include "iob_ai.h"
#include "art.h"

#define PAD_L 0u
#define PAD_U 1u
#define PAD_D 2u
#define PAD_U2 3u
#define PAD_R 4u
#define KEY_A 24u                        /* F5: select, play, confirm */
#define KEY_B 26u                        /* G5: take back */
#define PAD_KEYS (1u << PAD_L | 1u << PAD_U | 1u << PAD_D | 1u << PAD_U2 | 1u << PAD_R)

/* the web version's colours */
#define C_NAVY RGB(16, 38, 64)
#define C_ORANGE RGB(255, 90, 0)
#define C_CARD RGB(231, 90, 18)
#define C_CARD_DK RGB(120, 52, 20)
#define C_INK RGB(20, 20, 20)
#define C_PAPER RGB(255, 255, 255)
#define C_SKY RGB(126, 200, 242)
#define C_MUTE RGB(120, 150, 180)

/* the layout: status bar, the computer's hand, the board (four isles of 2 x 2 cells), your hand */
#define BAR_H 19
#define OPP_Y 20
#define OPP_H 16
#define BX 38
#define BY 39
#define CELL 40
#define GAP 4
#define BOARD (4 * CELL + GAP)
#define HAND_Y 206
#define CARD_W 76
#define CARD_H 32
#define BAND 120                         /* the screen is drawn in two bands of 240 x 120 */

static const char *const EFF_NAME[NEFF] = {"STRAIGHT", "DIAGONAL", "LEAP", "SWAP", "PARTY", "PUSH", "DISMISS",
                                           "BANISH", "PLAY TWICE", "DISCARD", "WILD"};
static const char *const EFF_HELP[NEFF] = {
    "MOVE YOUR STACK IN A STRAIGHT LINE",
    "MOVE YOUR STACK DIAGONALLY",
    "MOVE YOUR STACK TO ANY SPACE",
    "SWAP ANY TWO STACKS",
    "GATHER THE NEIGHBOURS ONTO YOUR STACK",
    "PUSH A LINE ONE STEP, YOURS IN FRONT",
    "SPREAD YOUR STACK, BEAR BY BEAR",
    "SEND A WHOLE STACK INTO THE SEA",
    "PLAY TWO CARDS FROM YOUR HAND",
    "MAKE THE COMPUTER DISCARD A CARD",
    "PLAY ANY CARD ON THE TABLE",
};

/* ------------------------------------------------------------------ the record --- */
#define SET_MAGIC 0x494F4231u            /* "IOB1" */
typedef struct {
    uint32_t magic;
    uint16_t wins, losses;
} settings_t;
static settings_t set;

/* ------------------------------------------------------------------ sound --- */
/* a few blips for the game's moments, made in the audio interrupt: one voice, a pitch glide, square or noise */
enum { SFX_TICK, SFX_PICK, SFX_BACK, SFX_PLAY, SFX_MOVE, SFX_SPLASH, SFX_SINK, SFX_NO, SFX_WIN, SFX_LOSE, SFX_THINK };
static volatile uint32_t sfx_req, sfx_seq;
static struct {
    float f0, f1, ph, env, dec, noise;
    uint32_t left, rng;
    const uint8_t *notes;
    uint32_t note_n, note_i, note_len, note_left;
} vx;

static void sfx(uint32_t k)
{
    sfx_req = k;
    sfx_seq++;
}

static void vx_start(uint32_t k)
{
    static const uint8_t WIN[] = {72, 76, 79, 84}, LOSE[] = {67, 63, 60, 55};
    vx.notes = 0;
    vx.noise = 0.0f;
    vx.env = 1.0f;
    switch (k) {
    case SFX_TICK: vx.f0 = vx.f1 = 1800.0f; vx.left = 900; vx.dec = 0.9990f; vx.env = 0.35f; break;
    case SFX_PICK: vx.f0 = 880.0f; vx.f1 = 1320.0f; vx.left = 3500; vx.dec = 0.9995f; break;
    case SFX_BACK: vx.f0 = 660.0f; vx.f1 = 330.0f; vx.left = 4000; vx.dec = 0.9995f; break;
    case SFX_PLAY: vx.f0 = 520.0f; vx.f1 = 1040.0f; vx.left = 6000; vx.dec = 0.9996f; break;
    case SFX_MOVE: vx.f0 = 300.0f; vx.f1 = 500.0f; vx.left = 3000; vx.dec = 0.9993f; break;
    case SFX_SPLASH: vx.noise = 1.0f; vx.f0 = vx.f1 = 0.0f; vx.left = 14000; vx.dec = 0.99975f; break;
    case SFX_SINK: vx.f0 = 400.0f; vx.f1 = 60.0f; vx.left = 30000; vx.dec = 0.99992f; vx.noise = 0.4f; break;
    case SFX_NO: vx.f0 = vx.f1 = 140.0f; vx.left = 4500; vx.dec = 0.9998f; break;
    case SFX_WIN: vx.notes = WIN; vx.note_n = 4; vx.note_len = 6000; vx.left = 24000; vx.dec = 0.99985f; break;
    case SFX_LOSE: vx.notes = LOSE; vx.note_n = 4; vx.note_len = 7000; vx.left = 28000; vx.dec = 0.99985f; break;
    case SFX_THINK: vx.f0 = vx.f1 = 2400.0f; vx.left = 500; vx.dec = 0.998f; vx.env = 0.2f; break;
    }
    if (vx.notes) {
        vx.note_i = 0;
        vx.note_left = 0;
    }
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    static uint32_t seen;
    uint32_t i, seq = sfx_seq;
    float gain = (float)master_q12 / 4096.0f * 2500000.0f;
    if (seq != seen) {
        seen = seq;
        vx_start(sfx_req);
    }
    if (!vx.rng)
        vx.rng = 0x1234567u;
    for (i = 0; i < n; i++) {
        float s = 0.0f;
        if (vx.left) {
            float f;
            if (vx.notes && !vx.note_left) {
                if (vx.note_i < vx.note_n) {
                    vx.f0 = vx.f1 = 440.0f * fm_exp2f((float)(vx.notes[vx.note_i++] - 69) / 12.0f);
                    vx.env = 1.0f;
                }
                vx.note_left = vx.note_len;
            }
            if (vx.note_left)
                vx.note_left--;
            vx.left--;
            f = vx.f1 + (vx.f0 - vx.f1) * vx.env;     /* the glide follows the envelope */
            vx.ph += f / 44100.0f;
            if (vx.ph >= 1.0f)
                vx.ph -= 1.0f;
            vx.rng = vx.rng * 1664525u + 1013904223u;
            s = (vx.ph < 0.5f ? 0.5f : -0.5f) * (1.0f - vx.noise) +
                ((float)(vx.rng >> 9) / 4194304.0f - 1.0f) * vx.noise * 0.6f;
            s *= vx.env * gain;
            vx.env *= vx.dec;
        }
        lr[2 * i] = (int32_t)s;
        lr[2 * i + 1] = (int32_t)s;
    }
}

void app_panic(void) { vx.left = 0; }

/* ------------------------------------------------------------------ the game --- */
enum { SC_TITLE, SC_PLAY, SC_OVER };
enum { AI_OFF, AI_THINK, AI_PLAY };
static iob_t G;                          /* the game */
static iob_t undo_g;                     /* before you played this turn's card: G5 takes it back */
static ai_t AI;

static struct {
    uint8_t scene, human, help, redraw;
    uint8_t cur_cell;                    /* the board's cursor */
    uint8_t cur_card;                    /* the cursor among the cards that may be picked (an index into choices) */
    uint8_t cur_dir;                     /* PUSH: the direction picked (0..7) */
    /* the computer */
    uint8_t ai;                          /* AI_* */
    uint32_t ai_t0, ai_next;
    uint8_t ai_k, ai_card;               /* the step being shown; the card it played (0xFF none) */
    uint16_t ai_marks;                   /* the cells its last turn touched: framed until you play */
    uint32_t seed;
    /* a message over the status bar for a moment (what just happened) */
    char msg[48];
    uint32_t msg_until;
    uint16_t msg_col;
    /* input */
    uint32_t keys, btn;
    uint32_t pad_t, pad_rep;             /* the pad held: repeat */
    uint32_t frame;
} ui;

static void say_status(void);

static void msg_set(const char *s, uint16_t col, uint32_t ms)
{
    uint32_t i;
    for (i = 0; i < sizeof ui.msg - 1u && s[i]; i++)
        ui.msg[i] = s[i];
    ui.msg[i] = 0;
    ui.msg_col = col;
    ui.msg_until = plat_ms() + ms;
    ui.redraw = 1;
}

static void text_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}
static void num_cat(char *d, uint32_t v)
{
    char t[12];
    int n = 0;
    do
        t[n++] = (char)('0' + v % 10u);
    while ((v /= 10u) && n < 11);
    while (*d)
        d++;
    while (n)
        *d++ = t[--n];
    *d = 0;
}

/* the bears still in play: on the board, and those a DISMISS holds while it places them */
static uint32_t bears_on_board(const iob_t *s)
{
    uint32_t i, n = 0;
    for (i = 0; i < 16; i++)
        n += s->sp[i].n;
    for (i = 0; i < s->nfl; i++)
        if (CARD_EFF(s->fl[i].card) == E_DISMISS && s->fl[i].step)
            n += (uint32_t)(s->fl[i].nplace - s->fl[i].head);
    return n;
}

/* what an action did that the board alone doesn't say: bears into the sea, an isle sunk, a turn lost */
static void tell_aftermath(const iob_t *before, const iob_t *after)
{
    uint32_t nb = bears_on_board(before), na = bears_on_board(after), lost = nb > na ? nb - na : 0u;
    char t[48] = "";
    if (before->alive != after->alive) {
        sfx(SFX_SINK);
        msg_set("AN ISLE SANK INTO THE SEA", C_SKY, 1800);
    } else if (lost) {
        sfx(SFX_SPLASH);
        num_cat(t, lost);
        text_cat(t, lost == 1u ? " BEAR FELL INTO THE SEA" : " BEARS FELL INTO THE SEA");
        msg_set(t, C_SKY, 1800);
    }
    if (after->skipped != 0xFF && before->skipped != after->skipped)
        msg_set(after->skipped == ui.human ? "NO CARDS: YOUR TURN IS SKIPPED" : "NO CARDS: THE COMPUTER SKIPS",
                C_PAPER, 2200);
}

static void game_over_check(void)
{
    if (G.winner == WIN_NONE || ui.scene == SC_OVER)
        return;
    ui.scene = SC_OVER;
    ui.ai = AI_OFF;
    if (G.winner == ui.human) {
        set.wins++;
        sfx(SFX_WIN);
    } else if (G.winner != WIN_DRAW) {
        set.losses++;
        sfx(SFX_LOSE);
    }
    plat_store_save(OBJ_SETTINGS, &set, sizeof set);
    ui.redraw = 1;
    say_status();
}

static void ai_begin(void)
{
    ui.ai = AI_THINK;
    ui.ai_t0 = plat_ms();
    ui.ai_card = 0xFF;
    G.hold = 0;
    ai_start(&AI, &G, ui.seed++ * 2654435761u + plat_ms());
    ui.redraw = 1;
}

static void turn_begins(void)
{
    game_over_check();
    if (ui.scene != SC_PLAY)
        return;
    if (G.turn != ui.human)
        ai_begin();
    else {
        G.hold = 1;
        ui.cur_card = 0;
        say_status();
    }
    ui.redraw = 1;
}

static void new_game(void)
{
    ui.seed = ui.seed * 1103515245u + plat_ms() + 12345u;
    ui.human = IOB_B;                    /* as the web: you are the black bears and you start */
    iob_new(&G, ui.seed | 1u, ui.human);
    ui.scene = SC_PLAY;
    ui.ai = AI_OFF;
    ui.ai_marks = 0;
    ui.ai_card = 0xFF;
    ui.cur_cell = 5;
    ui.msg_until = 0;
    turn_begins();
}

/* ------------------------------------------------------------------ choices --- */
/* what you may pick now (iob_legal), split by kind */
static uint8_t acts[IOB_MAXACT], nacts;
static uint8_t choice_cards[IOB_MAXACT], ncards; /* card actions, in screen order: the computer's hand, then yours */
static uint16_t choice_cells;
static uint8_t choice_dirs;

static int human_turn(void) { return ui.scene == SC_PLAY && ui.ai == AI_OFF && G.turn == ui.human; }

static void choices(void)
{
    uint32_t k, p, h;
    nacts = human_turn() ? (uint8_t)iob_legal(&G, acts) : 0;
    ncards = 0;
    choice_cells = 0;
    choice_dirs = 0;
    for (k = 0; k < nacts; k++) {
        if (acts[k] < 16u)
            choice_cells |= (uint16_t)(1u << acts[k]);
        else if (acts[k] >= A_DIR)
            choice_dirs |= (uint8_t)(1u << (acts[k] - A_DIR));
    }
    for (p = 0; p < 2; p++) {            /* screen order: the computer's row, then yours */
        const iob_player_t *P = &G.pl[p == 0 ? ui.human ^ 1u : ui.human];
        for (h = 0; h < P->nh; h++)
            for (k = 0; k < nacts; k++)
                if (acts[k] == A_CARD + P->hand[h])
                    choice_cards[ncards++] = acts[k];
    }
    if (ui.cur_card >= ncards)
        ui.cur_card = ncards ? (uint8_t)(ncards - 1u) : 0;
    if (choice_cells && !(choice_cells >> ui.cur_cell & 1u)) {   /* the cursor onto the nearest choice */
        int best = -1, bd = 99, i;
        for (i = 0; i < 16; i++)
            if (choice_cells >> i & 1u) {
                int dr = (i >> 2) - (ui.cur_cell >> 2), dc = (i & 3) - (ui.cur_cell & 3);
                int d = (dr < 0 ? -dr : dr) + (dc < 0 ? -dc : dc);
                if (d < bd) {
                    bd = d;
                    best = i;
                }
            }
        ui.cur_cell = (uint8_t)best;
    }
    if (choice_dirs && !(choice_dirs >> ui.cur_dir & 1u)) {
        uint32_t d;
        for (d = 0; d < 8u && !(choice_dirs >> d & 1u); d++)
            ;
        ui.cur_dir = (uint8_t)d;
    }
}

/* the prompt for where you are, and the card under the cursor */
static void prompt(char *t)
{
    const iob_flow_t *f = G.nfl ? &G.fl[G.nfl - 1] : 0;
    t[0] = 0;
    if (ui.help == 2) {
        text_cat(t, "THE RULEBOOK: SCAN THE CODE WITH YOUR PHONE");
        return;
    }
    if (ui.help == 1) {
        text_cat(t, "HOW TO PLAY");
        return;
    }
    if (ui.scene == SC_TITLE) {
        text_cat(t, "F5 START   EDIT HELP");
        return;
    }
    if (ui.scene == SC_OVER) {
        text_cat(t, "F5 PLAY AGAIN   HOME TITLE");
        return;
    }
    if (ui.ai == AI_THINK) {
        static const char *const DOTS[4] = {"", ".", "..", "..."};
        text_cat(t, "THE COMPUTER IS THINKING");
        text_cat(t, DOTS[(plat_ms() / 300u) & 3u]);
        return;
    }
    if (ui.ai == AI_PLAY) {
        text_cat(t, "COMPUTER: ");
        text_cat(t, ui.ai_card != 0xFF ? EFF_NAME[CARD_EFF(ui.ai_card)] : "...");
        return;
    }
    if (G.pend) {
        text_cat(t, "F5 CONFIRM   G5 TAKE BACK");
        return;
    }
    if (!f) {                            /* a card to play: what it does */
        if (ncards) {
            uint8_t c = (uint8_t)(choice_cards[ui.cur_card] - A_CARD);
            text_cat(t, EFF_HELP[CARD_EFF(c)]);
        }
        return;
    }
    switch (CARD_EFF(f->card)) {
    case E_STRAIGHT:
    case E_DIAGONAL:
    case E_LEAP:
        text_cat(t, f->step ? "WHERE TO?" : "PICK A STACK TOPPED BY YOUR BEAR");
        break;
    case E_SWAP: text_cat(t, f->step ? "PICK THE SECOND STACK" : "PICK THE FIRST STACK"); break;
    case E_PARTY:
        if (f->step) {
            text_cat(t, "PICK THE NEXT TO JOIN (FIRST GOES LOWEST)");
        } else
            text_cat(t, "PICK THE CENTRE: YOUR BEAR ON TOP");
        break;
    case E_PUSH: text_cat(t, f->step ? "PICK A DIRECTION, F5 TO PUSH" : "PICK YOUR BEAR TO PUSH WITH"); break;
    case E_DISMISS:
        if (f->step) {
            text_cat(t, "PLACE THE ");
            text_cat(t, BEAR_COLOR(f->place[f->head]) ? "BLACK" : "WHITE");
            text_cat(t, " BEAR (");
            num_cat(t, (uint32_t)(f->nplace - f->head));
            text_cat(t, " LEFT)");
        } else
            text_cat(t, "PICK YOUR STACK TO DISMISS");
        break;
    case E_BANISH: text_cat(t, "PICK A STACK TO BANISH"); break;
    case E_PLAYTWICE:
        text_cat(t, "PLAY TWICE: CARD ");
        num_cat(t, f->count + 1u);
        text_cat(t, " OF 2");
        break;
    case E_DISCARD: text_cat(t, "PICK A COMPUTER CARD TO DISCARD"); break;
    case E_WILD: text_cat(t, "PICK ANY CARD ON THE TABLE TO PLAY"); break;
    }
}

/* ------------------------------------------------------------------ drawing --- */
static int32_t by;                       /* the band's top row on the screen */

static void rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c) { cv_rect(x, y - by, w, h, c); }
static void frame(int32_t x, int32_t y, int32_t w, int32_t h, int32_t t, uint16_t c)
{
    rect(x, y, w, t, c);
    rect(x, y + h - t, w, t, c);
    rect(x, y, t, h, c);
    rect(x + w - t, y, t, h, c);
}
static int32_t text(int32_t x, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    if (y + f->h <= by || y >= by + BAND)
        return x + text_w(f, s);
    return cv_text(x, y - by, f, s, c);
}
static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    text(cx - text_w(f, s) / 2, y, f, s, c);
}

static void sprite(int32_t x, int32_t y, const spr_t *s)
{
    int32_t r, c, y0 = y - by;
    for (r = 0; r < s->h; r++) {
        int32_t yy = y0 + r;
        const uint16_t *src;
        uint16_t *dst;
        if (yy < 0 || yy >= (int32_t)cv_h)
            continue;
        src = s->px + (uint32_t)r * s->w;
        dst = cv_px + (uint32_t)yy * cv_w;
        for (c = 0; c < s->w; c++) {
            int32_t xx = x + c;
            if (src[c] != SPR_KEY && xx >= 0 && xx < (int32_t)cv_w)
                dst[xx] = src[c];
        }
    }
}

static int32_t cell_x(int i) { return BX + (i & 3) * CELL + ((i & 3) >= 2 ? GAP : 0); }
static int32_t cell_y(int i) { return BY + (i >> 2) * CELL + ((i >> 2) >= 2 ? GAP : 0); }

static void draw_stack(int i)
{
    const iob_stack_t *k = &G.sp[i];
    int32_t x = cell_x(i), y = cell_y(i), vis, j;
    if (!k->n)
        return;
    vis = k->n < 4u ? k->n : 4;
    for (j = 0; j < vis; j++) {          /* the top few, each a step higher */
        const spr_t *s = BEARS[k->b[k->n - (uint32_t)vis + (uint32_t)j]];
        sprite(x + (CELL - s->w) / 2, y + CELL - 3 - s->h - (vis - 1 - j) * 3, s);
    }
    if (k->n > 1u) {                     /* how many */
        char t[4] = "";
        num_cat(t, k->n);
        rect(x + CELL - 13, y + 1, 12, 12, BEAR_COLOR(k->b[k->n - 1u]) ? C_INK : C_PAPER);
        text_c(x + CELL - 7, y, &FONT_XS, t, BEAR_COLOR(k->b[k->n - 1u]) ? C_PAPER : C_INK);
    }
}

static void draw_sunk(int isle)
{
    int32_t x = BX + (isle & 1) * (2 * CELL + GAP), y = BY + (isle >> 1) * (2 * CELL + GAP), r, c;
    for (r = 0; r < 4; r++)              /* a few waves where it was */
        for (c = 0; c < 3; c++) {
            int32_t wx = x + 10 + c * 22 + (r & 1) * 10, wy = y + 14 + r * 18;
            rect(wx, wy, 4, 1, C_SKY);
            rect(wx + 4, wy - 1, 4, 1, C_SKY);
            rect(wx + 8, wy, 4, 1, C_SKY);
        }
}

static const int8_t DIR_ORDER[8] = {0, 5, 3, 7, 1, 6, 2, 4};   /* U UR R DR D DL L UL: clockwise */

static void draw_arrows(int src)
{
    int32_t cx = cell_x(src) + CELL / 2, cy = cell_y(src) + CELL / 2, d;
    for (d = 0; d < 8; d++) {
        int32_t ax, ay, sel;
        if (!(choice_dirs >> d & 1u))
            continue;
        ax = cx + DIR_DC[d] * 30;
        ay = cy + DIR_DR[d] * 30;
        sel = d == ui.cur_dir;
        rect(ax - 7, ay - 7, 14, 14, sel ? C_ORANGE : C_NAVY);
        frame(ax - 7, ay - 7, 14, 14, 1, C_PAPER);
        rect(ax - 1 + DIR_DC[d] * 3, ay - 1 + DIR_DR[d] * 3, 3, 3, C_PAPER);   /* a head... */
        rect(ax - 1, ay - 1, 3, 3, C_PAPER);                                  /* ...and its shaft */
        rect(ax - 1 - DIR_DC[d] * 3, ay - 1 - DIR_DR[d] * 3, 3, 3, C_PAPER);
        if (sel)
            frame(ax - 9, ay - 9, 18, 18, 2, C_PAPER);
    }
}

/* the cell a flow has picked already (the stack moving, the centre, the first of a swap), or -1 */
static int flow_anchor(const iob_flow_t *f)
{
    if (!f || !f->step)
        return -1;
    switch (CARD_EFF(f->card)) {
    case E_STRAIGHT: case E_DIAGONAL: case E_LEAP: case E_PUSH: return f->src;
    case E_SWAP: return f->a;
    case E_PARTY: return f->center;
    case E_DISMISS: return f->last;
    default: return -1;
    }
}

static void draw_board(void)
{
    int i;
    const iob_flow_t *f = G.nfl ? &G.fl[G.nfl - 1] : 0;
    rect(BX - 3, BY - 3, BOARD + 6, BOARD + 6, ART_SEA);
    for (i = 0; i < 4; i++) {
        if (G.alive >> i & 1u)
            sprite(BX + (i & 1) * (2 * CELL + GAP), BY + (i >> 1) * (2 * CELL + GAP), ISLES[i]);
        else
            draw_sunk(i);
    }
    for (i = 0; i < 16; i++) {           /* what's picked already, the computer's last move, the choices */
        int32_t x = cell_x(i), y = cell_y(i);
        if (flow_anchor(f) == i)
            rect(x + 1, y + 1, CELL - 2, CELL - 2, C_SKY);
        if (ui.ai_marks >> i & 1u)
            frame(x + 1, y + 1, CELL - 2, CELL - 2, 2, C_ORANGE);
        if (choice_cells >> i & 1u)
            frame(x + 3, y + 3, CELL - 6, CELL - 6, 1, C_PAPER);
    }
    for (i = 0; i < 16; i++)             /* the stacks, top rows first: the lower rows' bears stand in front */
        draw_stack(i);
    if (human_turn() && choice_cells)
        frame(cell_x(ui.cur_cell), cell_y(ui.cur_cell), CELL, CELL, 3, C_ORANGE);
    if (human_turn() && choice_dirs && f)
        draw_arrows(f->src);
}

/* the card's face as at the table: its colour (orange: yours, sky blue: the computer's), its wavy edges, its icon, and
 * its name. A card that can't be picked now is shaded */
static const char *const CARD_SHORT[NEFF] = {"STRAIGHT", "DIAGONAL", "LEAP", "SWAP", "PARTY", "PUSH", "DISMISS",
                                             "BANISH", "TWICE", "DISCARD", "WILD"};
static void shade(int32_t x, int32_t y, int32_t w, int32_t h)
{
    int32_t i, j;
    for (j = 0; j < h; j++)
        for (i = (j & 1); i < w; i += 2)
            rect(x + i, y + j, 1, 1, C_NAVY);
}
static void draw_card(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t c, int pickable, int cur, int small)
{
    int eff = CARD_EFF(c), o = CARD_OWNER(c);
    const spr_t *ic = (small ? CARD_S : CARD_L)[o][c % NART];
    uint16_t ink = o ? C_PAPER : C_NAVY;
    int32_t iw = small ? 18 : 32, nx;
    rect(x, y, w, h, o ? ART_CARD_1 : ART_CARD_0);
    if (!small) {
        sprite(x, y, CARD_EDGE[o][0]);
        sprite(x, y + h - 5, CARD_EDGE[o][1]);
    }
    sprite(x + 2 + (iw - ic->w) / 2, y + (h - ic->h) / 2, ic);
    nx = x + iw + (w - iw) / 2;
    text_c(nx, y + (small ? 2 : (h - 12) / 2), &FONT_T, CARD_SHORT[eff], ink);
    if (human_turn() && ncards && !choice_cells && !choice_dirs && !pickable && (G.nfl || o == ui.human))   /* picking: the others */
        shade(x, y, w, h);
    if (pickable && !cur)
        frame(x, y, w, h, 1, C_PAPER);
    if (cur)
        frame(x - 1, y - 1, w + 2, h + 2, 2, C_ORANGE);
}

static int card_choice(uint8_t c, int *cur)
{
    uint32_t k;
    for (k = 0; k < ncards; k++)
        if (choice_cards[k] == A_CARD + c) {
            *cur = k == ui.cur_card && !choice_cells && !choice_dirs;
            return 1;
        }
    *cur = 0;
    return 0;
}

static void draw_hands(void)
{
    uint32_t k;
    const iob_player_t *me = &G.pl[ui.human], *op = &G.pl[ui.human ^ 1u];
    char t[16];
    for (k = 0; k < op->nh; k++) {       /* the computer's hand: face up, as at the table */
        int cur, pick = card_choice(op->hand[k], &cur);
        int32_t x = 3 + (int32_t)k * 79;
        draw_card(x, OPP_Y, CARD_W, OPP_H, op->hand[k], pick, cur, 1);
        if (ui.ai == AI_PLAY && ui.ai_card == op->hand[k])
            frame(x - 1, OPP_Y - 1, CARD_W + 2, OPP_H + 2, 2, C_SKY);
    }
    for (k = 0; k < me->nh; k++) {
        int cur, pick = card_choice(me->hand[k], &cur);
        int32_t x = 3 + (int32_t)k * 79, lift = cur ? 3 : 0;
        draw_card(x, HAND_Y - lift, CARD_W, CARD_H, me->hand[k], pick, cur, 0);
    }
    /* the sides: decks and discards */
    t[0] = 0;
    text_c(BX / 2, BY + 2, &FONT_T, "CPU", C_MUTE);
    num_cat(t, op->nd);
    text_c(BX / 2, BY + 14, &FONT_XS, t, C_PAPER);
    text_c(BX / 2, BY + 29, &FONT_T, "DECK", C_MUTE);
    t[0] = 0;
    text_c(BX / 2, HAND_Y - 46, &FONT_T, "YOU", C_MUTE);
    num_cat(t, me->nd);
    text_c(BX / 2, HAND_Y - 34, &FONT_XS, t, C_PAPER);
    text_c(BX / 2, HAND_Y - 19, &FONT_T, "DECK", C_MUTE);
    {                                    /* the record, on the right */
        int32_t rx = BX + BOARD + (240 - BX - BOARD) / 2;
        t[0] = 0;
        num_cat(t, set.wins);
        text_c(rx, BY + 2, &FONT_T, "WON", C_MUTE);
        text_c(rx, BY + 14, &FONT_XS, t, C_PAPER);
        t[0] = 0;
        num_cat(t, set.losses);
        text_c(rx, BY + 34, &FONT_T, "LOST", C_MUTE);
        text_c(rx, BY + 46, &FONT_XS, t, C_PAPER);
        if (ui.scene == SC_PLAY) {       /* whose turn: a bear */
            const spr_t *s = BEARS[G.turn == IOB_B ? 9 : 1];
            text_c(rx, HAND_Y - 46, &FONT_T, G.turn == ui.human ? "YOU" : "CPU", C_ORANGE);
            sprite(rx - s->w / 2, HAND_Y - 33, s);
        }
    }
}

static void draw_bar(void)
{
    char t[64];
    uint32_t now = plat_ms();
    uint16_t bg = human_turn() ? C_ORANGE : C_NAVY;
    rect(0, 0, 240, BAR_H, bg);
    if (now < ui.msg_until) {
        rect(0, 0, 240, BAR_H, C_INK);
        text_c(120, 2, text_w(&FONT_XS, ui.msg) > 232 ? &FONT_T : &FONT_XS, ui.msg, ui.msg_col);
        return;
    }
    prompt(t);
    text_c(120, 2, text_w(&FONT_XS, t) > 232 ? &FONT_T : &FONT_XS, t, C_PAPER);
}

static void draw_box(const char *a, const char *b, const char *c)
{
    rect(24, 84, 192, 72, C_INK);
    frame(24, 84, 192, 72, 2, C_ORANGE);
    text_c(120, 90, &FONT_M, a, C_PAPER);
    if (b)
        text_c(120, 118, &FONT_XS, b, C_SKY);
    if (c)
        text_c(120, 135, &FONT_XS, c, C_ORANGE);
}

static const char *const HELP[][2] = {
    {"F3 / A3", "LEFT / RIGHT"}, {"F#3 / G3", "UP / DOWN"}, {"SELECT KNOB", "NEXT / PREVIOUS"},
    {"F5", "PICK, PLAY, CONFIRM"}, {"G5", "TAKE BACK THIS TURN"}, {"HOME", "TITLE"}, {"EDIT", "HELP, THEN THE RULEBOOK"},
};

static void draw_help(void)
{
    uint32_t k;
    rect(10, 22, 220, 196, C_INK);
    frame(10, 22, 220, 196, 2, C_ORANGE);
    text_c(120, 28, &FONT_B, "HOW TO PLAY", C_PAPER);
    for (k = 0; k < sizeof HELP / sizeof HELP[0]; k++) {
        text(20, 50 + (int32_t)k * 17, &FONT_XS, HELP[k][0], C_ORANGE);
        text(104, 50 + (int32_t)k * 17, &FONT_XS, HELP[k][1], C_PAPER);
    }
    text_c(120, 172, &FONT_T, "WIN: EVERY STACK TOPPED BY YOUR BEARS", C_SKY);
    text_c(120, 186, &FONT_T, "SIX ON ONE SPACE: THE OTHERS FALL IN", C_SKY);
    text_c(120, 200, &FONT_T, "AN EMPTY ISLE SINKS  -  EDIT: THE RULEBOOK", C_SKY);
}

/* the rulebook is on the web: its QR code (tools/gen_art.py), for a phone */
static void draw_rules(void)
{
    int32_t r, c, m = 4, q = (QR_N + 4) * m, x0 = (240 - q) / 2, y0 = 34;
    rect(6, 4, 228, 232, C_INK);
    frame(6, 4, 228, 232, 2, C_ORANGE);
    text_c(120, 10, &FONT_B, "THE RULEBOOK", C_PAPER);
    rect(x0, y0, q, q, C_PAPER);         /* the quiet zone, two modules all round */
    for (r = 0; r < QR_N; r++)
        for (c = 0; c < QR_N; c++)
            if (QR_ROWS[r] >> (QR_N - 1 - c) & 1u)
                rect(x0 + (2 + c) * m, y0 + (2 + r) * m, m, m, C_INK);
    text_c(120, y0 + q + 6, &FONT_XS, "SCAN WITH YOUR PHONE", C_ORANGE);
    text_c(120, y0 + q + 22, &FONT_T, "ISLE-OF-BEARS.HEYBYEDESIGN.COM", C_SKY);
}

static void draw_title(void)
{
    int i;
    rect(0, 0, 240, 240, C_ORANGE);
    for (i = 0; i < 4; i++)
        sprite(14 + i * 54, 118 - (i & 1) * 6, BEARS[i * 4 + (i & 1)]);
    rect(0, 156, 240, 84, ART_SEA);
    text_c(120, 28, &FONT_L, "ISLE OF", C_PAPER);
    text_c(120, 60, &FONT_L, "BEARS", C_PAPER);
    text_c(120, 98, &FONT_XS, "STACK YOUR BEARS. RULE THE ISLES.", C_INK);
    text_c(120, 168, &FONT_B, "F5  START", C_PAPER);
    text_c(120, 190, &FONT_XS, "YOU: BLACK BEARS  -  CPU: WHITE", C_SKY);
    text_c(120, 214, &FONT_T, "A GAME BY HEYBYE GAMING  -  EDIT: HELP, RULES", C_SKY);
}

static void draw(void)
{
    choices();
    for (by = 0; by < 240; by += BAND) {
        cv_begin(240, BAND, C_NAVY);
        if (ui.scene == SC_TITLE) {
            draw_title();
        } else {
            draw_bar();
            draw_board();
            draw_hands();
            if (ui.scene == SC_OVER)
                draw_box(G.winner == ui.human ? "YOU WIN!" : G.winner == WIN_DRAW ? "A DRAW" : "THE CPU WINS",
                         G.winner == WIN_DRAW ? "NO BEARS LEFT AT SEA" : "EVERY STACK IS ONE COLOUR",
                         "F5  PLAY AGAIN");
        }
        if (ui.help == 1)
            draw_help();
        else if (ui.help == 2)
            draw_rules();
        cv_blit(0, (uint32_t)by);
    }
}

/* ------------------------------------------------------------------ playing --- */
static void act(uint8_t a)
{
    iob_t before = G;
    if (!G.nfl && !G.pend) {             /* a card played: the turn can be taken back to here */
        undo_g = G;
        ui.ai_marks = 0;
        sfx(SFX_PLAY);
    } else
        sfx(a < 16u ? SFX_MOVE : SFX_PICK);
    iob_do(&G, a);
    tell_aftermath(&before, &G);
    ui.cur_card = 0;
    if (!G.nfl && !G.pend)               /* no target, or won: settled at once */
        turn_begins();
    else if (G.winner != WIN_NONE)
        game_over_check();
    ui.redraw = 1;
    say_status();
}

static void take_back(void)
{
    if (!human_turn() || (!G.nfl && !G.pend))
        return;
    G = undo_g;
    G.hold = 1;
    sfx(SFX_BACK);
    msg_set("TAKEN BACK", C_PAPER, 900);
    say_status();
}

static void confirm(void)
{
    iob_t before = G;
    iob_confirm(&G);
    tell_aftermath(&before, &G);
    sfx(SFX_PICK);
    turn_begins();
}

static void select_pressed(void)
{
    if (!human_turn())
        return;
    if (G.pend) {
        confirm();
        return;
    }
    choices();
    if (choice_cells) {
        if (choice_cells >> ui.cur_cell & 1u)
            act(ui.cur_cell);
        else
            sfx(SFX_NO);
    } else if (choice_dirs)
        act((uint8_t)(A_DIR + ui.cur_dir));
    else if (ncards)
        act(choice_cards[ui.cur_card]);
}

/* the cursor moved by the pad: to the nearest choice that way */
static void pad_move(int dr, int dc)
{
    choices();
    if (choice_cells) {
        int best = -1, bscore = 1 << 20, i;
        int r0 = ui.cur_cell >> 2, c0 = ui.cur_cell & 3;
        for (i = 0; i < 16; i++) {
            int r = (i >> 2) - r0, c = (i & 3) - c0, along = r * dr + c * dc, across = r * dc + c * dr;
            if (!(choice_cells >> i & 1u) || along <= 0)
                continue;
            across = across < 0 ? -across : across;
            if (across > along + 1)
                continue;                /* (too far to the side to be "that way") */
            if (along * 4 + across * 3 < bscore) {
                bscore = along * 4 + across * 3;
                best = i;
            }
        }
        if (best >= 0) {
            ui.cur_cell = (uint8_t)best;
            sfx(SFX_TICK);
        }
    } else if (choice_dirs) {
        uint32_t held = ui.keys, d;      /* the pad's keys held together: a diagonal */
        int vr = ((held >> PAD_D) & 1) - (int)(((held >> PAD_U) | (held >> PAD_U2)) & 1);
        int vc = ((held >> PAD_R) & 1) - ((held >> PAD_L) & 1);
        if (!vr && !vc) {
            vr = dr;
            vc = dc;
        }
        for (d = 0; d < 8u; d++)
            if (DIR_DR[d] == vr && DIR_DC[d] == vc && (choice_dirs >> d & 1u)) {
                ui.cur_dir = (uint8_t)d;
                sfx(SFX_TICK);
            }
    } else if (ncards) {
        int n = ncards, c = ui.cur_card, mine = 0, k;
        const iob_player_t *op = &G.pl[ui.human ^ 1u];
        for (k = 0; k < n; k++)          /* the first of yours, in screen order */
            if (hand_find(op, (uint8_t)(choice_cards[k] - A_CARD)) < 0)
                break;
        mine = k;
        if (dc)
            c += dc;
        else if (dr > 0 && c < mine)
            c = mine < n ? mine : c;     /* down to your row */
        else if (dr < 0 && c >= mine && mine > 0)
            c = 0;                       /* up to the computer's */
        if (c >= 0 && c < n && c != ui.cur_card) {
            ui.cur_card = (uint8_t)c;
            sfx(SFX_TICK);
        }
    }
    ui.redraw = 1;
    say_status();
}

/* the SELECT knob: the next or previous choice, in reading order */
static void knob_step(int d)
{
    choices();
    if (choice_cells) {
        int i = ui.cur_cell, k;
        for (k = 0; k < 16; k++) {
            i = (i + d + 16) & 15;
            if (choice_cells >> i & 1u)
                break;
        }
        ui.cur_cell = (uint8_t)i;
    } else if (choice_dirs) {
        int k, p = 0, j;
        for (k = 0; k < 8; k++)
            if (DIR_ORDER[k] == ui.cur_dir)
                p = k;
        for (j = 0; j < 8; j++) {
            p = (p + d + 8) & 7;
            if (choice_dirs >> DIR_ORDER[p] & 1u)
                break;
        }
        ui.cur_dir = (uint8_t)DIR_ORDER[p];
    } else if (ncards)
        ui.cur_card = (uint8_t)((ui.cur_card + ncards + d) % ncards);
    sfx(SFX_TICK);
    ui.redraw = 1;
    say_status();
}

/* the computer: think in slices of the frame, then play its turn step by step, slow enough to follow */
static void ai_frame(void)
{
    uint32_t now = plat_ms();
    if (ui.ai == AI_THINK) {
        uint32_t t0 = now;
        int done = 0, hurry = now - ui.ai_t0 > 2500u;
        while (!done && plat_ms() - t0 < 9u)
            done = ai_run(&AI, 48, hurry);
        if (ui.frame % 18u == 0u)
            ui.redraw = 1;               /* the dots */
        if (done && plat_ms() - ui.ai_t0 >= 500u) {
            ui.ai = AI_PLAY;
            ui.ai_k = 0;
            ui.ai_marks = 0;
            ui.ai_next = plat_ms();
        }
        return;
    }
    if (ui.ai == AI_PLAY && now >= ui.ai_next) {
        iob_t before = G;
        uint32_t step;
        uint8_t a;
        if (ui.ai_k >= AI.len || G.winner != WIN_NONE || !iob_is_legal(&G, AI.seq[ui.ai_k])) {
            ui.ai = AI_OFF;              /* its turn is over (or, should a step not fit, given up: never seen) */
            if (G.nfl) {                 /* (never leave a flow open) */
                G = AI.root;
                G.hold = 0;
            }
            turn_begins();
            return;
        }
        a = AI.seq[ui.ai_k++];
        if (G.nfl && flow_anchor(&G.fl[G.nfl - 1]) >= 0)
            ui.ai_marks |= (uint16_t)(1u << flow_anchor(&G.fl[G.nfl - 1]));
        if (a < 16u)
            ui.ai_marks |= (uint16_t)(1u << a);
        if (!G.nfl)
            ui.ai_card = (uint8_t)(a - A_CARD);
        iob_do(&G, a);
        tell_aftermath(&before, &G);
        if (G.alive == before.alive && bears_on_board(&G) == bears_on_board(&before))
            sfx(ui.ai_k == 1 ? SFX_PLAY : SFX_MOVE);
        step = AI.len > 1 ? 2500u / (AI.len - 1u) : 700u;
        step = step < 340u ? 340u : step > 700u ? 700u : step;
        ui.ai_next = plat_ms() + (ui.ai_k == 1 ? 1100u : step);
        if (ui.ai_k >= AI.len)
            ui.ai_next = plat_ms() + 500u;
        ui.redraw = 1;
    }
}

/* ------------------------------------------------------------------ input --- */
static void input(void)
{
    uint32_t keys = plat_keys(), btn = plat_buttons(), down = keys & ~ui.keys, bdown = btn & ~ui.btn, now = plat_ms();
    int32_t e = plat_enc(EN_SELECT);
    ui.keys = keys;
    ui.btn = btn;
    if (bdown >> B_EDIT & 1u) {          /* EDIT: the controls, then the rulebook's QR code, then back */
        ui.help = (uint8_t)((ui.help + 1u) % 3u);
        ui.redraw = 1;
        sfx(SFX_TICK);
        say_status();
        return;
    }
    if (ui.help) {                       /* left and right turn the page, any other key closes it */
        if (down & (1u << PAD_L | 1u << PAD_R))
            ui.help = ui.help == 1 ? 2 : 1;
        else if (down || bdown)
            ui.help = 0;
        if (down || bdown) {
            ui.redraw = 1;
            sfx(SFX_TICK);
        }
        return;
    }
    if (bdown >> B_HOME & 1u) {
        ui.scene = SC_TITLE;
        ui.ai = AI_OFF;
        ui.redraw = 1;
        sfx(SFX_BACK);
        say_status();
        return;
    }
    if (ui.scene != SC_PLAY) {
        if ((down >> KEY_A & 1u) || (bdown >> B_PLAY & 1u)) {
            sfx(SFX_PLAY);
            new_game();
        }
        return;
    }
    if (!human_turn())
        return;
    if ((down >> KEY_B & 1u))
        take_back();
    if ((down >> KEY_A & 1u) || (bdown >> B_PLAY & 1u))
        select_pressed();
    if (e)
        knob_step(e > 0 ? 1 : -1);
    {                                    /* the pad: a press, then repeats while held */
        uint32_t pad = keys & PAD_KEYS, fresh = down & PAD_KEYS;
        if (fresh) {
            ui.pad_t = now;
            ui.pad_rep = now + 320u;
        } else if (pad && now >= ui.pad_rep) {
            fresh = pad;
            ui.pad_rep = now + 120u;
        }
        if (fresh) {
            if (fresh >> PAD_L & 1u)
                pad_move(0, -1);
            else if (fresh >> PAD_R & 1u)
                pad_move(0, 1);
            else if ((fresh >> PAD_U & 1u) || (fresh >> PAD_U2 & 1u))
                pad_move(-1, 0);
            else if (fresh >> PAD_D & 1u)
                pad_move(1, 0);
        }
    }
}

void app_input_only(void) { input(); }

/* ------------------------------------------------------------------ the screen reader --- */
void app_say(const char *a, const char *b)
{
    char t[96] = "";
    if (!ann_on())
        return;
    text_cat(t, a ? a : "");
    if (b && b[0]) {
        text_cat(t, ". ");
        text_cat(t, b);
    }
    ann_say(t, 0, ANN_NOW);
}

static void say_status(void)
{
    char t[64];
    if (!ann_on())
        return;
    choices();
    prompt(t);
    if (human_turn() && !G.nfl && !G.pend && ncards)
        app_say(EFF_NAME[CARD_EFF(choice_cards[ui.cur_card] - A_CARD)], t);
    else
        app_say(t, 0);
}

/* ------------------------------------------------------------------ the contract --- */
void app_init(void)
{
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC) {
        set.magic = SET_MAGIC;
        set.wins = set.losses = 0;
    }
}

void app_ui_init(void)
{
    ui.scene = SC_TITLE;
    ui.redraw = 1;
}

void app_frame(void)
{
    static uint32_t last_msg, last_draw;
    uint32_t now = plat_ms();
    ui.frame++;
    input();
    ai_frame();
    ann_tick(now);
    if (ui.msg_until && now >= ui.msg_until && last_msg != ui.msg_until) {
        last_msg = ui.msg_until;
        ui.redraw = 1;
    }
    plat_glow(1);                        /* the controls in play lit: the pad, F5, G5, EDIT and HOME */
    plat_leds(1u << B_EDIT | 1u << B_HOME | (ui.scene != SC_PLAY ? 1u << B_PLAY : 0u),
              human_turn() || ui.scene != SC_PLAY
                  ? PAD_KEYS | 1u << KEY_A | (human_turn() && (G.nfl || G.pend) ? 1u << KEY_B : 0u)
                  : 0u);
    if (ui.redraw && now - last_draw >= 30u) {
        ui.redraw = 0;
        last_draw = now;
        draw();
    }
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "scene"))
        *got = ui.scene;
    else if (!strcmp(what, "turn"))
        *got = G.turn;
    else if (!strcmp(what, "winner"))
        *got = G.winner == WIN_NONE ? -1 : G.winner;
    else if (!strcmp(what, "flows"))
        *got = G.nfl;
    else if (!strcmp(what, "pend"))
        *got = G.pend;
    else if (!strcmp(what, "ai"))
        *got = ui.ai;
    else if (!strcmp(what, "turns"))
        *got = G.turns;
    else if (!strcmp(what, "cursor"))
        *got = ui.cur_cell;
    else if (!strcmp(what, "finished"))  /* a game played to the end since boot (or before: the record) */
        *got = set.wins + set.losses > 0;
    else if (!strcmp(what, "help"))
        *got = ui.help;
    else
        return 0;
    return 1;
}
#endif
