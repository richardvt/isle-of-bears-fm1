/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 HEYBYE GAMING */
/* iob_cli: the C rules engine behind a line protocol, for tools/fm1_oracle.mjs.
 *   new SEED FIRST(W|B)   legal   do ACTION   digest   quit
 * Actions are spelled as the oracle's: space:N  card:UID  push:DIR */
#include <stdio.h>
#include <stdlib.h>
#include "../app/iob.h"

static iob_t S;
static const char *uid(uint8_t c, char *b) { sprintf(b, "%c-%s", CARD_OWNER(c) ? 'B' : 'W', ART_NAME[c % NART]); return b; }
static void act_text(uint8_t a, char *b)
{
    char u[32];
    if (a < 16) sprintf(b, "space:%u", a);
    else if (a < A_DIR) sprintf(b, "card:%s", uid((uint8_t)(a - A_CARD), u));
    else sprintf(b, "push:%s", DIR_KEY[a - A_DIR]);
}
static void pile(const uint8_t *a, int n)
{
    char u[32];
    int k;
    putchar('[');
    for (k = 0; k < n; k++) printf("%s\"%s\"", k ? "," : "", uid(a[k], u));
    putchar(']');
}
int main(void)
{
    char line[256];
    setvbuf(stdout, 0, _IOLBF, 0);
    while (fgets(line, sizeof line, stdin)) {
        char cmd[32], arg[64];
        unsigned seed;
        int n = sscanf(line, "%31s %63s", cmd, arg);
        if (n < 1) continue;
        if (!strcmp(cmd, "new") && sscanf(line, "new %u %63s", &seed, arg) == 2) {
            iob_new(&S, seed, arg[0] == 'B');
            puts("ok");
        } else if (!strcmp(cmd, "legal")) {
            uint8_t acts[IOB_MAXACT];
            uint32_t k, m = iob_legal(&S, acts);
            char b[48];
            for (k = 0; k < m; k++) { act_text(acts[k], b); printf("%s%s", k ? " " : "", b); }
            putchar('\n');
        } else if (!strcmp(cmd, "do") && n == 2) {
            uint8_t acts[IOB_MAXACT];
            uint32_t k, m = iob_legal(&S, acts);
            char b[48];
            for (k = 0; k < m; k++) { act_text(acts[k], b); if (!strcmp(b, arg)) break; }
            if (k == m) { printf("illegal %s\n", arg); continue; }
            iob_do(&S, acts[k]);
            puts("ok");
        } else if (!strcmp(cmd, "digest")) {
            int i, p;
            printf("{\"b\":[");
            for (i = 0; i < 16; i++) {
                printf("%s[", i ? "," : "");
                for (p = 0; p < S.sp[i].n; p++) printf("%s\"%c%d\"", p ? "," : "", S.sp[i].b[p] >> 3 ? 'B' : 'W', (S.sp[i].b[p] & 7) + 1);
                putchar(']');
            }
            printf("],\"i\":[%d,%d,%d,%d],\"t\":\"%c\",\"w\":", S.alive & 1, S.alive >> 1 & 1, S.alive >> 2 & 1, S.alive >> 3 & 1, S.turn ? 'B' : 'W');
            if (S.winner == WIN_NONE) printf("null");
            else if (S.winner == WIN_DRAW) printf("\"draw\"");
            else printf("\"%c\"", S.winner ? 'B' : 'W');
            printf(",\"h\":{\"W\":"); pile(S.pl[0].hand, S.pl[0].nh); printf(",\"B\":"); pile(S.pl[1].hand, S.pl[1].nh);
            printf("},\"d\":{\"W\":"); pile(S.pl[0].deck, S.pl[0].nd); printf(",\"B\":"); pile(S.pl[1].deck, S.pl[1].nd);
            printf("},\"x\":{\"W\":"); pile(S.pl[0].disc, S.pl[0].nx); printf(",\"B\":"); pile(S.pl[1].disc, S.pl[1].nx);
            printf("},\"s\":[");
            for (i = 0; i < S.nfl; i++) printf("%s\"%s\"", i ? "," : "", EFF_KEY[CARD_EFF(S.fl[i].card)]);
            printf("]}\n");
        } else if (!strcmp(cmd, "quit")) break;
        else puts("?");
    }
    return 0;
}
