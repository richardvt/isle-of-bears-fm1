/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 HEYBYE GAMING */
/* iob_ai.h: the computer opponent, after src/game.html's (電腦對手), in slices. Included once, after iob.h.
 *
 * As on the web: every complete turn the computer could play is enumerated (at most 600 per card, 4000 in all), each
 * scored on the board alone (no hand or deck is looked at, so nothing hidden is peeked), and the best 30 searched one
 * ply deeper: every reply of the opponent's (at most 250), the final score its own less the opponent's best.
 *
 * Unlike the web, the search is an explicit stack, so it runs in slices: ai_run(t, budget) does about `budget` steps
 * and returns, and the UI keeps drawing (and the watchdog keeps being fed) while the computer thinks. */
#pragma once

#define AI_SEQ_PER_CARD 600u
#define AI_SEQ_TOTAL 4000u
#define AI_CANDIDATES 30
#define AI_REPLY_CAP 250u
#define AI_MAXD 44                       /* the longest turn: WILD > PLAY TWICE > DISMISS of many, twice */
#define AI_WIN 2000000

typedef struct {
    iob_t st;
    uint8_t acts[IOB_MAXACT];
    uint8_t n, i, a;
} ai_node_t;

typedef struct {
    int32_t score, final;
    uint8_t judged, len;
    uint8_t seq[AI_MAXD];
} ai_cand_t;

enum { AI_IDLE, AI_MINE, AI_REPLIES, AI_DONE };

typedef struct {
    uint8_t phase, me;
    uint32_t rng;
    iob_t root;                          /* the position the computer thinks about */
    /* the enumeration in progress */
    ai_node_t stk[AI_MAXD + 1];
    int depth;
    uint32_t leaves, cap, card_from;
    uint8_t who;                         /* whose turns are enumerated */
    /* phase 1: the best candidates, best first */
    ai_cand_t cand[AI_CANDIDATES];
    int ncand;
    /* phase 2 */
    int ci;
    int32_t best_reply;
    uint32_t steps;                      /* steps done in all (the UI's pacing; the host's report) */
    /* the answer */
    uint8_t seq[AI_MAXD], len;
} ai_t;

/* the board for side me: the web's evalBoard x20 (its halves and its random 0..0.5 made whole) */
static int32_t ai_eval(const iob_t *s, int me)
{
    int you = me ^ 1, i, myTop = 0, youTop = 0, myB = 0, youB = 0, stack = 0;
    if (s->winner == me)
        return AI_WIN;
    if (s->winner == you)
        return -AI_WIN;
    if (s->winner == WIN_DRAW)
        return -AI_WIN / 5;
    for (i = 0; i < 16; i++) {
        const iob_stack_t *k = &s->sp[i];
        uint32_t b;
        int h;
        if (!iob_exists(s, i) || !k->n)
            continue;
        for (b = 0; b < k->n; b++)
            BEAR_COLOR(k->b[b]) == me ? myB++ : youB++;
        h = k->n < 5u ? k->n : 5;
        if (top_color(s, i) == me) {
            myTop++;
            stack += h;
        } else {
            youTop++;
            stack -= h;
        }
    }
    return 20 * (-8 * youTop + 3 * myTop + 2 * (myB - youB)) + 10 * stack;
}

static void ai_push(ai_t *t, const iob_t *s, uint8_t a)
{
    ai_node_t *n = &t->stk[++t->depth];
    if (s != &n->st)
        n->st = *s;
    n->a = a;
    n->n = (uint8_t)iob_legal(&n->st, n->acts);
    n->i = 0;
    iob_shuffle(&t->rng, n->acts, n->n);  /* the caps cut the same way every time otherwise */
}

static void ai_enum_begin(ai_t *t, const iob_t *s, uint32_t cap)
{
    t->depth = -1;
    t->leaves = 0;
    t->card_from = 0;
    t->cap = cap;
    t->who = s->turn;
    ai_push(t, s, A_NONE);
}

static void ai_leaf(ai_t *t, const iob_t *s);

/* one step of the enumeration: 0 when it is over */
static int ai_enum_step(ai_t *t)
{
    ai_node_t *top, *ch;
    uint8_t a;
    if (t->depth < 0 || t->leaves >= t->cap)
        return 0;
    if (t->depth > 0 && t->leaves - t->card_from >= AI_SEQ_PER_CARD)
        t->depth = 0;                    /* this card has had its share */
    top = &t->stk[t->depth];
    if (top->i >= top->n) {
        t->depth--;
        return t->depth >= 0;
    }
    a = top->acts[top->i++];
    if (t->depth == 0)
        t->card_from = t->leaves;
    if (t->depth >= AI_MAXD)
        return 1;                        /* (deeper than any turn should go: skipped) */
    ch = &t->stk[t->depth + 1];
    ch->st = top->st;
    iob_do(&ch->st, a);
    t->steps++;
    if (!ch->st.nfl) {                   /* the turn is complete */
        ch->a = a;
        t->depth++;
        t->leaves++;
        ai_leaf(t, &ch->st);
        t->depth--;
        return 1;
    }
    ai_push(t, &ch->st, a);
    if (!t->stk[t->depth].n)
        t->depth--;                      /* (a flow with nothing to do: cannot happen) */
    return 1;
}

static void ai_leaf(ai_t *t, const iob_t *s)
{
    if (t->phase == AI_MINE) {
        int32_t sc = ai_eval(s, t->me) + (int32_t)iob_below(&t->rng, 10);
        int k, j;
        ai_cand_t *c;
        if (t->ncand == AI_CANDIDATES && sc <= t->cand[AI_CANDIDATES - 1].score)
            return;
        for (k = t->ncand < AI_CANDIDATES ? t->ncand : AI_CANDIDATES - 1; k > 0 && t->cand[k - 1].score < sc; k--)
            ;
        j = t->ncand < AI_CANDIDATES ? t->ncand++ : AI_CANDIDATES - 1;
        for (; j > k; j--)
            t->cand[j] = t->cand[j - 1];
        c = &t->cand[k];
        c->score = sc;
        c->judged = 0;
        c->len = (uint8_t)t->depth;
        for (j = 1; j <= t->depth; j++)
            c->seq[j - 1] = t->stk[j].a;
    } else {
        int32_t v = ai_eval(s, t->who);
        if (v > t->best_reply)
            t->best_reply = v;
        if (v >= AI_WIN)
            t->cap = 0;                  /* the opponent has a win: no need to look further */
    }
}

static void ai_start(ai_t *t, const iob_t *s, uint32_t seed)
{
    t->phase = AI_MINE;
    t->me = s->turn;
    t->rng = seed | 1u;
    t->root = *s;
    t->root.hold = 0;                    /* (searching: every turn ends at once) */
    t->ncand = 0;
    t->ci = 0;
    t->len = 0;
    t->steps = 0;
    ai_enum_begin(t, &t->root, AI_SEQ_TOTAL);
}

/* the next candidate's replies; 0 when none is left */
static int ai_next_candidate(ai_t *t)
{
    while (t->ci < t->ncand) {
        ai_cand_t *c = &t->cand[t->ci];
        iob_t s = t->root;
        int k;
        for (k = 0; k < c->len; k++)
            iob_do(&s, c->seq[k]);
        if (s.winner != WIN_NONE) {      /* decided: no reply */
            c->final = c->score - (s.winner == t->me ? -AI_WIN : AI_WIN);
            c->judged = 1;
            t->ci++;
            continue;
        }
        t->best_reply = INT32_MIN;
        ai_enum_begin(t, &s, AI_REPLY_CAP);
        return 1;
    }
    return 0;
}

static void ai_finish(ai_t *t)
{
    int k, best = -1;
    for (k = 0; k < t->ncand; k++)
        if (t->cand[k].judged && (best < 0 || t->cand[k].final > t->cand[best].final))
            best = k;
    if (best < 0 && t->ncand)
        best = 0;
    t->len = 0;
    if (best >= 0) {
        t->len = t->cand[best].len;
        memcpy(t->seq, t->cand[best].seq, t->len);
    }
    t->phase = AI_DONE;
}

/* about `budget` steps of thinking; `hurry`: stop judging candidates (at least one judged) and answer.
 * Returns 1 when the answer is in t->seq[0 .. len). */
static int ai_run(ai_t *t, uint32_t budget, int hurry)
{
    while (budget--) {
        if (t->phase == AI_MINE) {
            if (ai_enum_step(t))
                continue;
            t->phase = AI_REPLIES;
            if (!ai_next_candidate(t)) {
                ai_finish(t);
                return 1;
            }
        } else if (t->phase == AI_REPLIES) {
            if (ai_enum_step(t))
                continue;
            {
                ai_cand_t *c = &t->cand[t->ci++];
                c->final = c->score - (t->best_reply == INT32_MIN ? 0 : t->best_reply);
                c->judged = 1;
            }
            if ((hurry && t->ci > 0) || !ai_next_candidate(t)) {
                ai_finish(t);
                return 1;
            }
        } else
            return t->phase == AI_DONE;
    }
    return 0;
}
