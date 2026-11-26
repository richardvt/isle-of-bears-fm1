/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 HEYBYE GAMING */
/* ai_test: the computer against a random player and against itself; no crash, no stall, and it should win. */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../app/iob.h"
#include "../app/iob_ai.h"

static ai_t T;
static int play(uint32_t seed, int ai_mask, uint32_t *maxsteps, double *maxms)
{
    iob_t s;
    uint32_t r = seed * 2654435761u + 1u;
    int turns = 0;
    iob_new(&s, seed, seed & 1);
    while (s.winner == WIN_NONE && turns < 400) {
        if (ai_mask >> s.turn & 1) {
            clock_t c0 = clock();
            int k;
            ai_start(&T, &s, seed + (uint32_t)turns);
            while (!ai_run(&T, 1000, 0))
                ;
            double ms = (double)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC;
            if (ms > *maxms) *maxms = ms;
            if (T.steps > *maxsteps) *maxsteps = T.steps;
            if (!T.len) { printf("seed %u: no move\n", seed); exit(1); }
            for (k = 0; k < T.len; k++) {
                if (!iob_is_legal(&s, T.seq[k])) { printf("seed %u: illegal step\n", seed); exit(1); }
                iob_do(&s, T.seq[k]);
            }
            if (s.nfl) { printf("seed %u: turn left open\n", seed); exit(1); }
        } else {
            uint8_t a[IOB_MAXACT];
            uint32_t n = iob_legal(&s, a);
            if (!n) { printf("seed %u: stuck\n", seed); exit(1); }
            iob_do(&s, a[iob_below(&r, n)]);
        }
        if (!s.nfl) turns++;
    }
    return s.winner;
}
int main(int argc, char **argv)
{
    int games = argc > 1 ? atoi(argv[1]) : 100, g, w[4] = {0}, v[4] = {0};
    uint32_t maxsteps = 0;
    double maxms = 0;
    for (g = 1; g <= games; g++) {
        int x = play((uint32_t)g, 1 << IOB_B, &maxsteps, &maxms);   /* the computer is black */
        w[x == WIN_NONE ? 3 : x]++;
    }
    printf("AI (B) vs random: B %d  W %d  draw %d  unfinished %d\n", w[1], w[0], w[2], w[3]);
    for (g = 1; g <= games / 4; g++) {
        int x = play((uint32_t)g + 5000, 3, &maxsteps, &maxms);
        v[x == WIN_NONE ? 3 : x]++;
    }
    printf("AI vs AI: W %d  B %d  draw %d  unfinished %d\n", v[0], v[1], v[2], v[3]);
    printf("worst turn: %u steps, %.1f ms on this machine; sizeof(ai_t) %zu, sizeof(iob_t) %zu\n", maxsteps, maxms, sizeof(ai_t), sizeof(iob_t));
    return w[1] < games * 8 / 10;
}
