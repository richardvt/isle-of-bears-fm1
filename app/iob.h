/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 HEYBYE GAMING */
/* iob.h: Isle of Bears rules engine for the FM-1 (and the host tests). Included once.
 *
 * A line-for-line port of src/game.html's rules layer (卡牌定義, 盤面查詢, 效果流程 FLOWS, 流程控制), so the
 * same game is played on the FM-1 as on the web. tools/fm1_oracle.mjs plays thousands of random games against the
 * web engine (tools/oracle/js-engine.mjs) step by step and compares the whole state and the legal actions.
 *
 * Board: 16 cells, idx = r*4+c; four islands of 2x2 (isle = (r>=2)*2 + (c>=2)).
 * Bears: 0..7 = W1..W8, 8..15 = B1..B8; colour = id >> 3 (0 white, 1 black). A stack lists bottom first.
 * Cards: id = owner*13 + art, art in DECK_LIST order. Actions (one byte):
 *   0..15  a cell          A_CARD+id  a card (to play, or a card target)          A_DIR+k  a push direction */
#pragma once
#include <stdint.h>
#include <string.h>

enum { IOB_W, IOB_B };
enum { WIN_NONE = 0xFF, WIN_DRAW = 2 };
enum { E_STRAIGHT, E_DIAGONAL, E_LEAP, E_SWAP, E_PARTY, E_PUSH, E_DISMISS, E_BANISH, E_PLAYTWICE, E_DISCARD, E_WILD, NEFF };
#define NART 13
#define A_CARD 16u
#define A_DIR (A_CARD + 2u * NART)       /* 42 .. 49 */
#define A_NONE 0xFFu
#define IOB_MAXACT 40u

static const uint8_t ART_EFF[NART] = {E_STRAIGHT, E_STRAIGHT, E_DIAGONAL, E_DIAGONAL, E_LEAP, E_SWAP, E_PARTY,
                                      E_PUSH, E_DISMISS, E_BANISH, E_PLAYTWICE, E_DISCARD, E_WILD};
static const char *const ART_NAME[NART] = {"straight", "straight_1", "diagonal", "diagonal_1", "leap", "swap",
                                           "party", "push", "dismiss", "banish", "playtwice", "discard", "wild"};
static const char *const EFF_KEY[NEFF] = {"straight", "diagonal", "leap", "swap", "party", "push", "dismiss",
                                          "banish", "playtwice", "discard", "wild"};
/* push directions, in PUSH_DIRS order: U D L R UL UR DL DR */
static const int8_t DIR_DR[8] = {-1, 1, 0, 0, -1, -1, 1, 1};
static const int8_t DIR_DC[8] = {0, 0, -1, 1, -1, 1, -1, 1};
static const char *const DIR_KEY[8] = {"U", "D", "L", "R", "UL", "UR", "DL", "DR"};

#define CARD_EFF(id) (ART_EFF[(id) % NART])
#define CARD_OWNER(id) ((id) / NART)
#define BEAR_COLOR(b) ((b) >> 3)

typedef struct {
    uint8_t n;
    uint8_t b[16];
} iob_stack_t;

typedef struct {
    uint8_t deck[NART], hand[NART], disc[NART];
    uint8_t nd, nh, nx;
} iob_player_t;

typedef struct {
    uint8_t card, actor, step;
    int8_t src, last, center, a;
    uint8_t count;                       /* PLAY TWICE: cards started so far */
    uint16_t pending;                    /* PARTY: neighbours still to gather (bits) */
    uint8_t place[16], nplace, head;     /* DISMISS: bears still to place, place[head..nplace) */
    uint8_t used[8], nused;              /* cards this flow started (discarded with the root) */
} iob_flow_t;

typedef struct {
    iob_stack_t sp[16];
    uint8_t alive;                       /* bit k: island k afloat */
    uint8_t turn, winner, skipped;       /* skipped: who lost a turn (no cards), 0xFF none */
    iob_player_t pl[2];
    iob_flow_t fl[4];                    /* the flow stack: fl[k]'s parent is fl[k-1] */
    uint8_t nfl;
    uint32_t rng;
    uint16_t turns;
    uint8_t hold;                        /* the UI's: a finished turn waits for iob_confirm (the web's pendingRoot) */
    uint8_t pend;                        /* a finished turn is waiting */
    iob_flow_t pend_root;
} iob_t;

/* ------------------------------------------------------------------ random (tools/oracle/rng.mjs) --- */
static uint32_t iob_rand(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}
/* floor(next() * n), exactly as the JS does it in doubles. Out of line: on the FM-1's CPU a 64-bit multiply inlined
 * among 16-bit loads can leave a register a bit off (felucca-bsp docs/CPU.md) */
static __attribute__((noinline)) uint32_t iob_below(uint32_t *s, uint32_t n) { return (uint32_t)(((uint64_t)iob_rand(s) * n) >> 32); }
static void iob_shuffle(uint32_t *s, uint8_t *a, uint32_t n)
{
    uint32_t i;
    for (i = n; i-- > 1u;) {
        uint32_t j = iob_below(s, i + 1u);
        uint8_t t = a[i];
        a[i] = a[j];
        a[j] = t;
    }
}

/* ------------------------------------------------------------------ board queries --- */
#define ISLE_OF(i) ((((i) >> 2) >= 2 ? 2 : 0) + (((i) & 3) >= 2 ? 1 : 0))
static int in_b(int r, int c) { return r >= 0 && r < 4 && c >= 0 && c < 4; }
static int iob_exists(const iob_t *s, int i) { return (s->alive >> ISLE_OF(i)) & 1u; }
static int top_color(const iob_t *s, int i)
{
    const iob_stack_t *k = &s->sp[i];
    return k->n ? BEAR_COLOR(k->b[k->n - 1u]) : -1;
}
static int occupied_at(const iob_t *s, int i) { return iob_exists(s, i) && s->sp[i].n; }
static uint16_t occupied(const iob_t *s)
{
    uint16_t m = 0;
    int i;
    for (i = 0; i < 16; i++)
        if (occupied_at(s, i))
            m |= (uint16_t)(1u << i);
    return m;
}
static uint16_t controlled_by(const iob_t *s, int p)
{
    uint16_t m = 0;
    int i;
    for (i = 0; i < 16; i++)
        if (occupied_at(s, i) && top_color(s, i) == p)
            m |= (uint16_t)(1u << i);
    return m;
}
static uint16_t line_dests(const iob_t *s, int i, int diag)
{
    static const int8_t DR[2][4] = {{-1, 1, 0, 0}, {-1, -1, 1, 1}}, DC[2][4] = {{0, 0, -1, 1}, {-1, 1, -1, 1}};
    int r = i >> 2, c = i & 3, d, k;
    uint16_t m = 0;
    for (d = 0; d < 4; d++)
        for (k = 1; k < 4; k++) {
            int nr = r + DR[diag][d] * k, nc = c + DC[diag][d] * k;
            if (!in_b(nr, nc))
                break;
            if (iob_exists(s, nr * 4 + nc))  /* over a sunk gap */
                m |= (uint16_t)(1u << (nr * 4 + nc));
        }
    return m;
}
static uint16_t leap_dests(const iob_t *s, int i)
{
    uint16_t m = 0;
    int j;
    for (j = 0; j < 16; j++)
        if (j != i && iob_exists(s, j))
            m |= (uint16_t)(1u << j);
    return m;
}
static uint16_t move_dests(const iob_t *s, int eff, int i)
{
    return eff == E_LEAP ? leap_dests(s, i) : line_dests(s, i, eff == E_DIAGONAL);
}
static uint16_t orth_neighbors(const iob_t *s, int i)
{
    int r = i >> 2, c = i & 3, d;
    uint16_t m = 0;
    for (d = 0; d < 4; d++) {
        int nr = r + DIR_DR[d], nc = c + DIR_DC[d];
        if (in_b(nr, nc) && iob_exists(s, nr * 4 + nc))
            m |= (uint16_t)(1u << (nr * 4 + nc));
    }
    return m;
}
static uint16_t all_neighbors(const iob_t *s, int i)
{
    int r = i >> 2, c = i & 3, d;
    uint16_t m = 0;
    for (d = 0; d < 8; d++) {
        int nr = r + DIR_DR[d], nc = c + DIR_DC[d];
        if (in_b(nr, nc) && iob_exists(s, nr * 4 + nc))
            m |= (uint16_t)(1u << (nr * 4 + nc));
    }
    return m;
}
static uint8_t push_dirs(const iob_t *s, int i)
{
    int r = i >> 2, c = i & 3, d;
    uint8_t m = 0;
    for (d = 0; d < 8; d++) {
        int nr = r + DIR_DR[d], nc = c + DIR_DC[d];
        if (in_b(nr, nc) && iob_exists(s, nr * 4 + nc))
            m |= (uint8_t)(1u << d);
    }
    return m;
}
static void move_stack(iob_t *s, int from, int to)
{
    iob_stack_t *f = &s->sp[from], *t = &s->sp[to];
    uint32_t k;
    for (k = 0; k < f->n && t->n < 16u; k++)
        t->b[t->n++] = f->b[k];
    f->n = 0;
}

/* ------------------------------------------------------------------ cards --- */
static int hand_find(const iob_player_t *p, uint8_t id)
{
    int k;
    for (k = 0; k < p->nh; k++)
        if (p->hand[k] == id)
            return k;
    return -1;
}
static void hand_remove(iob_player_t *p, int k)
{
    for (; k + 1 < p->nh; k++)
        p->hand[k] = p->hand[k + 1];
    p->nh--;
}
static void refill_to(iob_t *s, int color, int n)
{
    iob_player_t *p = &s->pl[color];
    while (p->nh < n) {
        if (!p->nd) {
            if (!p->nx)
                break;
            memcpy(p->deck, p->disc, p->nx);  /* deck = shuffle(discard) */
            p->nd = p->nx;
            p->nx = 0;
            iob_shuffle(&s->rng, p->deck, p->nd);
        }
        p->hand[p->nh++] = p->deck[--p->nd];
    }
}

static void iob_new(iob_t *s, uint32_t seed, int first)
{
    uint8_t bears[16];
    int i, p;
    memset(s, 0, sizeof *s);
    s->rng = seed ? seed : 0x9e3779b9u;
    for (i = 0; i < 16; i++)             /* W1..W8, B1..B8 */
        bears[i] = (uint8_t)i;
    iob_shuffle(&s->rng, bears, 16);
    for (i = 0; i < 16; i++) {
        s->sp[i].n = 1;
        s->sp[i].b[0] = bears[i];
    }
    for (p = 0; p < 2; p++) {
        iob_player_t *P = &s->pl[p];
        for (i = 0; i < NART; i++)
            P->deck[i] = (uint8_t)(p * NART + i);
        P->nd = NART;
        iob_shuffle(&s->rng, P->deck, NART);
        for (i = 0; i < 3; i++)
            P->hand[P->nh++] = P->deck[--P->nd];
    }
    s->alive = 15;
    s->turn = (uint8_t)first;
    s->winner = WIN_NONE;
    s->skipped = 0xFF;
}

/* ------------------------------------------------------------------ flows --- */
static int can_start(const iob_t *s, uint8_t card, int actor);

/* the actions open to flow f, into out (if given); returns how many */
static uint32_t flow_targets(const iob_t *s, const iob_flow_t *f, uint8_t *out)
{
    uint32_t n = 0;
    uint16_t m = 0;
    int i, eff = CARD_EFF(f->card), p;
#define PUT(a) do { if (out) out[n] = (uint8_t)(a); n++; } while (0)
    switch (eff) {
    case E_STRAIGHT:
    case E_DIAGONAL:
    case E_LEAP:
        if (f->step == 0) {
            uint16_t c = controlled_by(s, f->actor);
            for (i = 0; i < 16; i++)
                if ((c >> i & 1u) && move_dests(s, eff, i))
                    m |= (uint16_t)(1u << i);
        } else
            m = move_dests(s, eff, f->src);
        break;
    case E_SWAP:
        m = occupied(s);
        if (f->step)
            m &= (uint16_t)~(1u << f->a);
        break;
    case E_PARTY:
        if (f->step == 0) {
            uint16_t c = controlled_by(s, f->actor), o = occupied(s);
            for (i = 0; i < 16; i++)
                if ((c >> i & 1u) && (orth_neighbors(s, i) & o))
                    m |= (uint16_t)(1u << i);
        } else
            m = f->pending;
        break;
    case E_PUSH:
        if (f->step == 0) {
            uint16_t c = controlled_by(s, f->actor);
            for (i = 0; i < 16; i++)
                if ((c >> i & 1u) && push_dirs(s, i))
                    m |= (uint16_t)(1u << i);
        } else {
            uint8_t d = push_dirs(s, f->src);
            for (i = 0; i < 8; i++)
                if (d >> i & 1u)
                    PUT(A_DIR + (uint32_t)i);
        }
        break;
    case E_DISMISS:
        if (f->step == 0) {
            uint16_t c = controlled_by(s, f->actor);
            for (i = 0; i < 16; i++)
                if ((c >> i & 1u) && all_neighbors(s, i))
                    m |= (uint16_t)(1u << i);
        } else
            m = all_neighbors(s, f->last);
        break;
    case E_BANISH:
        m = occupied(s);
        break;
    case E_PLAYTWICE: {
        const iob_player_t *P = &s->pl[f->actor];
        for (i = 0; i < P->nh; i++) {
            uint8_t c = P->hand[i];
            if (c != f->card && CARD_EFF(c) != E_PLAYTWICE && CARD_EFF(c) != E_WILD && can_start(s, c, f->actor))
                PUT(A_CARD + c);
        }
        break;
    }
    case E_DISCARD: {
        const iob_player_t *P = &s->pl[f->actor ^ 1];
        for (i = 0; i < P->nh; i++)
            PUT(A_CARD + P->hand[i]);
        break;
    }
    case E_WILD:
        for (p = 0; p < 2; p++)
            for (i = 0; i < s->pl[p].nh; i++) {
                uint8_t c = s->pl[p].hand[i];
                if (c != f->card && CARD_EFF(c) != E_WILD && can_start(s, c, f->actor))
                    PUT(A_CARD + c);
            }
        break;
    }
    for (i = 0; i < 16; i++)
        if (m >> i & 1u)
            PUT(i);
#undef PUT
    return n;
}

static int can_start(const iob_t *s, uint8_t card, int actor)
{
    iob_flow_t probe;
    memset(&probe, 0, sizeof probe);
    probe.card = card;
    probe.actor = (uint8_t)actor;
    return flow_targets(s, &probe, 0) != 0;
}

static void used_add(iob_flow_t *f, uint8_t c)
{
    if (f->nused < sizeof f->used)
        f->used[f->nused++] = c;
}
static void used_merge(iob_flow_t *to, iob_flow_t *from)
{
    uint32_t k;
    for (k = 0; k < from->nused; k++)
        used_add(to, from->used[k]);
}

static void resolve_aftermath(iob_t *s)
{
    int i, k;
    uint16_t occ;
    for (i = 0; i < 16; i++) {           /* 1. crowding: six or more, the other colour falls into the sea */
        iob_stack_t *t = &s->sp[i];
        if (iob_exists(s, i) && t->n >= 6u) {
            uint32_t a, n = 0;
            for (a = 0; a < t->n; a++)
                if (BEAR_COLOR(t->b[a]) == s->turn)
                    t->b[n++] = t->b[a];
            t->n = (uint8_t)n;
        }
    }
    for (k = 0; k < 4; k++) {            /* 2. an empty island sinks */
        int empty = 1;
        if (!(s->alive >> k & 1u))
            continue;
        for (i = 0; i < 16; i++)
            if (ISLE_OF(i) == k && s->sp[i].n)
                empty = 0;
        if (empty)
            s->alive &= (uint8_t)~(1u << k);
    }
    occ = occupied(s);                   /* 3. every top one colour wins */
    if (!occ) {
        s->winner = WIN_DRAW;
        return;
    }
    {
        int seen[2] = {0, 0};
        for (i = 0; i < 16; i++)
            if (occ >> i & 1u)
                seen[top_color(s, i)] = 1;
        if (seen[0] != seen[1])
            s->winner = (uint8_t)(seen[0] ? IOB_W : IOB_B);
    }
}

static void finish_turn(iob_t *s, const iob_flow_t *root)
{
    int p, k;
    s->turns++;
    for (p = 0; p < 2; p++) {            /* the root card and the cards it started go to their owners' discards */
        iob_player_t *P = &s->pl[p];
        uint8_t keep[NART];
        uint32_t nk = 0;
        for (k = 0; k < P->nh; k++) {
            uint8_t c = P->hand[k];
            uint32_t u, used = c == root->card;
            for (u = 0; u < root->nused; u++)
                used |= root->used[u] == c;
            if (used)
                P->disc[P->nx++] = c;
            else
                keep[nk++] = c;
        }
        memcpy(P->hand, keep, nk);
        P->nh = (uint8_t)nk;
    }
    if (s->winner != WIN_NONE)
        return;
    s->skipped = 0xFF;
    refill_to(s, s->turn, 3);
    s->turn ^= 1u;
    if (!s->pl[s->turn].nh) {            /* R-1: no cards, the turn is skipped and the hand filled */
        refill_to(s, s->turn, 3);
        s->skipped = s->turn;
        s->turn ^= 1u;
    }
}

static void flow_done(iob_t *s)
{
    int k = s->nfl - 1;
    iob_flow_t f = s->fl[k];
    s->nfl--;
    if (k > 0 && f.nused)
        used_merge(&s->fl[k - 1], &f);
    resolve_aftermath(s);
    if (k > 0 && s->winner == WIN_NONE) {
        iob_flow_t *par = &s->fl[k - 1];
        int more = CARD_EFF(par->card) == E_PLAYTWICE && par->count < 2u;
        if (more && flow_targets(s, par, 0))
            return;                      /* PLAY TWICE: the second card */
        flow_done(s);
        return;
    }
    if (k > 0) {                         /* won midway: every used card up the chain to the root */
        int x;
        iob_flow_t root;
        for (x = k - 1; x >= 1; x--) {
            used_merge(&s->fl[x - 1], &s->fl[x]);
            s->fl[x].nused = 0;
        }
        root = s->fl[0];
        s->nfl = 0;
        finish_turn(s, &root);
        return;
    }
    if (s->hold && s->winner == WIN_NONE) {   /* the board as it will be: confirmed (or taken back) first */
        s->pend_root = f;
        s->pend = 1;
        return;
    }
    finish_turn(s, &f);
}

/* the waiting turn ends: cards discarded and drawn, the other side to play */
static void iob_confirm(iob_t *s)
{
    if (s->pend) {
        s->pend = 0;
        finish_turn(s, &s->pend_root);
    }
}

static void start_flow(iob_t *s, uint8_t card)
{
    iob_flow_t *f = &s->fl[s->nfl++];
    memset(f, 0, sizeof *f);
    f->card = card;
    f->actor = s->turn;
    if (!flow_targets(s, f, 0))
        flow_done(s);                    /* no target: the card is spent all the same */
}

/* the actions open now: the cards in hand (no flow), or the flow's targets */
static uint32_t iob_legal(const iob_t *s, uint8_t *out)
{
    uint32_t n = 0, k;
    if (s->winner != WIN_NONE || s->pend)
        return 0;
    if (!s->nfl) {
        const iob_player_t *P = &s->pl[s->turn];
        for (k = 0; k < P->nh; k++)
            out[n++] = (uint8_t)(A_CARD + P->hand[k]);
        return n;
    }
    return flow_targets(s, &s->fl[s->nfl - 1], out);
}

static void flow_space(iob_t *s, iob_flow_t *f, int i)
{
    int eff = CARD_EFF(f->card);
    switch (eff) {
    case E_STRAIGHT:
    case E_DIAGONAL:
    case E_LEAP:
        if (f->step == 0) {
            f->src = (int8_t)i;
            f->step = 1;
            return;
        }
        move_stack(s, f->src, i);
        break;
    case E_SWAP:
        if (f->step == 0) {
            f->a = (int8_t)i;
            f->step = 1;
            return;
        } else {
            iob_stack_t t = s->sp[f->a];
            s->sp[f->a] = s->sp[i];
            s->sp[i] = t;
        }
        break;
    case E_PARTY:
        if (f->step == 0) {
            f->center = (int8_t)i;
            f->pending = (uint16_t)(orth_neighbors(s, i) & occupied(s));
            f->step = 1;
            return;
        }
        move_stack(s, i, f->center);
        f->pending &= (uint16_t)~(1u << i);
        if (f->pending)
            return;
        break;
    case E_PUSH:
        f->src = (int8_t)i;
        f->step = 1;
        return;
    case E_DISMISS:
        if (f->step == 0) {
            f->src = f->last = (int8_t)i;
            memcpy(f->place, s->sp[i].b, s->sp[i].n);
            f->nplace = s->sp[i].n;
            f->head = 0;
            s->sp[i].n = 0;
            f->step = 1;
            return;
        }
        {
            iob_stack_t *t = &s->sp[i];
            if (t->n < 16u)
                t->b[t->n++] = f->place[f->head];
            f->head++;
        }
        f->last = (int8_t)i;
        if (f->head < f->nplace && all_neighbors(s, f->last))
            return;
        if (f->head < f->nplace) {       /* nowhere left: the rest stay on the first cell */
            iob_stack_t *t = &s->sp[f->src];
            t->n = (uint8_t)(f->nplace - f->head);
            memcpy(t->b, f->place + f->head, t->n);
            f->head = f->nplace;
        }
        break;
    case E_BANISH:
        s->sp[i].n = 0;
        break;
    default:
        return;
    }
    flow_done(s);
}

static void flow_edge(iob_t *s, iob_flow_t *f, int d)
{
    int8_t chain[5];
    int n = 0, r = f->src >> 2, c = f->src & 3, j;
    chain[n++] = f->src;                 /* the pusher moves too */
    r += DIR_DR[d];
    c += DIR_DC[d];
    while (in_b(r, c) && iob_exists(s, r * 4 + c) && s->sp[r * 4 + c].n) {
        chain[n++] = (int8_t)(r * 4 + c);
        r += DIR_DR[d];
        c += DIR_DC[d];
    }
    for (j = n - 1; j >= 0; j--) {       /* from the front back */
        int tr = (chain[j] >> 2) + DIR_DR[d], tc = (chain[j] & 3) + DIR_DC[d];
        if (!in_b(tr, tc) || !iob_exists(s, tr * 4 + tc))
            s->sp[chain[j]].n = 0;       /* into the sea */
        else
            move_stack(s, chain[j], tr * 4 + tc);
    }
    flow_done(s);
}

static void flow_card(iob_t *s, iob_flow_t *f, uint8_t c)
{
    int eff = CARD_EFF(f->card), k;
    if (eff == E_PLAYTWICE) {
        f->count++;
        for (k = 0; k < f->nused && f->used[k] != c; k++)
            ;
        if (k == f->nused)
            used_add(f, c);
        start_flow(s, c);
    } else if (eff == E_DISCARD) {
        iob_player_t *P = &s->pl[f->actor ^ 1];
        k = hand_find(P, c);
        if (k >= 0) {
            hand_remove(P, k);
            P->disc[P->nx++] = c;
        }
        flow_done(s);
    } else if (eff == E_WILD) {
        f->nused = 0;
        used_add(f, c);
        start_flow(s, c);
    }
}

/* one action: a card played (no flow open) or a step of the open flow. The caller gives a legal one. */
static void iob_do(iob_t *s, uint8_t a)
{
    if (s->winner != WIN_NONE || s->pend)
        return;
    if (!s->nfl) {
        if (a >= A_CARD && a < A_DIR && hand_find(&s->pl[s->turn], (uint8_t)(a - A_CARD)) >= 0)
            start_flow(s, (uint8_t)(a - A_CARD));
        return;
    }
    {
        iob_flow_t *f = &s->fl[s->nfl - 1];
        if (a < 16u)
            flow_space(s, f, a);
        else if (a < A_DIR)
            flow_card(s, f, (uint8_t)(a - A_CARD));
        else
            flow_edge(s, f, a - A_DIR);
    }
}

static int iob_is_legal(const iob_t *s, uint8_t a)
{
    uint8_t acts[IOB_MAXACT];
    uint32_t n = iob_legal(s, acts), k;
    for (k = 0; k < n; k++)
        if (acts[k] == a)
            return 1;
    return 0;
}
