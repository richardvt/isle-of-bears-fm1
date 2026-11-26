/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 HEYBYE GAMING */
/* music.h: the game's music, made by the FM-1 as it plays. Included once from app.c.
 *
 * Three original tunes, each four tracks over a loop of bars of sixteen steps: a lead (two-operator FM, a bell), a
 * bass (FM, a pluck), an arpeggio (FM, soft) and drums (kick, snare, hat). The tunes are written as text below and
 * read once at boot into note arrays; the audio interrupt steps through them (mus_render).
 *
 * Melodic tracks: one token a step, space-separated: a note (C4, F#3, A#2 ...), "." to hold, "-" to let go.
 * Drums: one character a step: k kick, s snare, h hat, x kick and hat, . nothing. */

typedef struct {
    const char *name;
    uint16_t bpm;
    uint8_t bars;
    const char *lead, *bass, *arp, *drums;
} song_src_t;

enum { SONG_TITLE, SONG_PLAY, SONG_TENSE, NSONGS, SONG_NONE = 0xFF };

static const song_src_t SONGS[NSONGS] = {
    {"BEAR PARADE", 112, 4,
     /* lead */
     "C5 . E5 . G5 . E5 . A5 . G5 . E5 . C5 . "
     "D5 . F5 . A5 . F5 . G5 . . . - . . . "
     "E5 . G5 . C6 . G5 . A5 . G5 . F5 . D5 . "
     "C5 . . . G4 . . . C5 . . . - . . . ",
     /* bass */
     "C3 . . . C3 . G2 . C3 . . . G2 . . . "
     "F2 . . . F2 . C3 . G2 . . . G2 . D3 . "
     "A2 . . . A2 . E3 . F2 . . . F2 . C3 . "
     "G2 . . . G2 . D3 . C3 . . . - . . . ",
     /* arpeggio */
     "E4 . G4 . C5 . G4 . E4 . G4 . C5 . G4 . "
     "F4 . A4 . C5 . A4 . G4 . B4 . D5 . B4 . "
     "E4 . A4 . C5 . A4 . F4 . A4 . C5 . A4 . "
     "D4 . G4 . B4 . G4 . E4 . G4 . C5 . - . ",
     /* drums */
     "k.h.s.h.k.hks.h."
     "k.h.s.h.k.hks.h."
     "k.h.s.h.k.hks.h."
     "k.h.s.h.kkh.s.ss"},
    {"ICE FLOES", 92, 8,
     "F5 . . . . . C5 . D5 . . . . . . . "
     "A4 . . . C5 . . . F4 . . . - . . . "
     "G5 . . . . . F5 . D5 . . . C5 . . . "
     "D5 . . . . . . . - . . . . . . . "
     "F5 . . . A5 . . . G5 . F5 . D5 . . . "
     "C5 . . . D5 . F5 . . . . . - . . . "
     "G5 . F5 . D5 . C5 . A4 . . . C5 . . . "
     "F4 . . . . . . . - . . . . . . . ",
     "F2 . . . . . . . . . . . C3 . . . "
     "D2 . . . . . . . A2 . . . . . . . "
     "A#2 . . . . . . . F2 . . . . . . . "
     "C3 . . . . . . . G2 . . . C3 . . . "
     "F2 . . . . . . . . . . . C3 . . . "
     "A2 . . . . . . . D2 . . . . . . . "
     "A#2 . . . . . . . C3 . . . . . . . "
     "F2 . . . C3 . . . F2 . . . - . . . ",
     "A3 . C4 . F4 . C4 . A3 . C4 . F4 . C4 . "
     "A3 . D4 . F4 . D4 . A3 . D4 . F4 . D4 . "
     "A#3 . D4 . F4 . D4 . A#3 . D4 . F4 . D4 . "
     "G3 . C4 . E4 . C4 . G3 . C4 . E4 . C4 . "
     "A3 . C4 . F4 . C4 . A3 . C4 . F4 . C4 . "
     "A3 . C4 . E4 . C4 . A3 . D4 . F4 . D4 . "
     "A#3 . D4 . F4 . D4 . G3 . C4 . E4 . C4 . "
     "A3 . C4 . F4 . C4 . A3 . C4 . F4 . - . ",
     "k...h...k.k.h..."
     "k...h...k...h..h"
     "k...h...k.k.h..."
     "k...h...k...h.hh"
     "k...h...k.k.h..."
     "k...h...k...h..h"
     "k...h...k.k.h..."
     "k...h...k...s.s."},
    {"THIN ICE", 128, 4,
     "A4 . C5 . E5 . A4 . D5 . C5 . B4 . G4 . "
     "A4 . C5 . E5 . A5 . G5 . E5 . D5 . . . "
     "F5 . E5 . D5 . C5 . B4 . C5 . D5 . E5 . "
     "E5 . . . G#4 . . . A4 . . . - . . . ",
     "A2 . A2 . A2 . A2 . G2 . G2 . G2 . G2 . "
     "F2 . F2 . F2 . F2 . E2 . E2 . E2 . E2 . "
     "D2 . D2 . D2 . D2 . G2 . G2 . G2 . G2 . "
     "E2 . E2 . E2 . E2 . A2 . A2 . E2 . E2 . ",
     "A3 . E4 . C4 . E4 . G3 . D4 . B3 . D4 . "
     "F3 . C4 . A3 . C4 . E3 . B3 . G#3 . B3 . "
     "D3 . A3 . F3 . A3 . G3 . D4 . B3 . D4 . "
     "E3 . B3 . G#3 . B3 . A3 . E4 . C4 . - . ",
     "k.h.s.hkk.h.s.hh"
     "k.h.s.hkk.h.s.hh"
     "k.h.s.hkk.h.s.hh"
     "k.h.s.hkk.s.ssss"},
};

#define MUS_MAXSTEPS (8 * 16)
#define NOTE_HOLD 0u
#define NOTE_OFF 255u

typedef struct {
    uint8_t note[3][MUS_MAXSTEPS];       /* lead, bass, arpeggio: a MIDI note, NOTE_HOLD or NOTE_OFF */
    char drum[MUS_MAXSTEPS];
    uint16_t steps;
    uint32_t step_len;                   /* samples a step (a sixteenth) */
} song_t;
static song_t songs[NSONGS];

/* "C#4" -> MIDI 61; 0 if it isn't a note */
static uint8_t note_of(const char *t, const char **end)
{
    static const int8_t SEMI[7] = {9, 11, 0, 2, 4, 5, 7};   /* A B C D E F G */
    int n;
    if (*t < 'A' || *t > 'G')
        return 0;
    n = SEMI[*t++ - 'A'];
    if (*t == '#') {
        n++;
        t++;
    }
    if (*t < '0' || *t > '8')
        return 0;
    n += 12 * (*t++ - '0' + 1);
    *end = t;
    return (uint8_t)n;
}

/* the text into notes; the number of steps read (a track shorter than the song holds its last) */
static uint32_t mus_parse_track(const char *s, uint8_t *out)
{
    uint32_t n = 0;
    while (n < MUS_MAXSTEPS) {
        const char *e;
        uint8_t v;
        while (*s == ' ')
            s++;
        if (!*s)
            break;
        if (*s == '.' || *s == '-') {
            out[n++] = *s++ == '.' ? NOTE_HOLD : NOTE_OFF;
            continue;
        }
        v = note_of(s, &e);
        if (!v)
            break;                       /* (not a note: the track ends here; tests/music_test.c checks none do) */
        out[n++] = v;
        s = e;
    }
    return n;
}

static float SINE[1024];
static uint32_t mus_bad;                 /* tracks whose length isn't the song's (the host test expects none) */

static void mus_init(void)
{
    uint32_t i, k;
    for (i = 0; i < 1024u; i++)
        SINE[i] = fm_sinf((float)i * (6.2831853f / 1024.0f));
    for (k = 0; k < NSONGS; k++) {
        song_t *g = &songs[k];
        const song_src_t *src = &SONGS[k];
        g->steps = (uint16_t)(src->bars * 16u);
        mus_bad += mus_parse_track(src->lead, g->note[0]) != g->steps;
        mus_bad += mus_parse_track(src->bass, g->note[1]) != g->steps;
        mus_bad += mus_parse_track(src->arp, g->note[2]) != g->steps;
        for (i = 0; i < g->steps && src->drums[i]; i++)
            g->drum[i] = src->drums[i];
        mus_bad += i != g->steps || src->drums[i];
        for (; i < g->steps; i++)
            g->drum[i] = '.';
        g->step_len = (uint32_t)(44100.0f * 60.0f / (float)src->bpm / 4.0f);
    }
}

/* ------------------------------------------------------------------ the voices --- */
typedef struct {
    float ph, mph, inc, minc;            /* carrier and modulator phase (0..1) and their steps */
    float env, menv, dec, mdec, rel;     /* the level, the brightness, their decays a sample; rel: let go */
    float index, ratio, amp;
    uint8_t held;
} fmv_t;

static inline float lut(float ph) { return SINE[(uint32_t)(ph * 1024.0f) & 1023u]; }

static void fmv_on(fmv_t *v, uint8_t note)
{
    float f = 440.0f * fm_exp2f((float)((int)note - 69) / 12.0f);
    v->inc = f / 44100.0f;
    v->minc = f * v->ratio / 44100.0f;
    v->env = 1.0f;
    v->menv = 1.0f;
    v->held = 1;
}

static inline float fmv_run(fmv_t *v)
{
    float m, s;
    if (v->env < 0.0005f)
        return 0.0f;
    v->mph += v->minc;
    if (v->mph >= 1.0f)
        v->mph -= 1.0f;
    v->ph += v->inc;
    if (v->ph >= 1.0f)
        v->ph -= 1.0f;
    m = lut(v->mph) * v->index * v->menv;
    s = lut(v->ph + m + 4.0f);           /* (+4: the lookup wraps, the index never reaches 4) */
    v->env *= v->held ? v->dec : v->rel;
    v->menv *= v->mdec;
    return s * v->env * v->amp;
}

static struct {
    fmv_t ch[3];                         /* lead, bass, arpeggio */
    float kph, kenv, kamp, sph, samp, hamp, hprev;
    uint32_t noise;
    uint8_t song;                        /* playing; SONG_NONE silent */
    uint16_t step;
    uint32_t left;                       /* samples to the next step */
} mus;

static volatile uint8_t mus_want = SONG_NONE;   /* the UI's choice: the interrupt follows it */
static volatile float mus_gain = 0.5f;          /* the music's level (the UI's setting), 0 silent */

static void mus_voices(void)
{
    static const float RATIO[3] = {3.5f, 1.0f, 2.0f}, INDEX[3] = {1.6f, 1.4f, 0.7f};
    static const float DEC[3] = {0.99990f, 0.99985f, 0.99975f}, MDEC[3] = {0.9993f, 0.9990f, 0.9995f};
    static const float AMP[3] = {0.34f, 0.40f, 0.16f};
    uint32_t c;
    for (c = 0; c < 3u; c++) {
        mus.ch[c].ratio = RATIO[c];
        mus.ch[c].index = INDEX[c];
        mus.ch[c].dec = DEC[c];
        mus.ch[c].mdec = MDEC[c];
        mus.ch[c].rel = 0.9985f;
        mus.ch[c].amp = AMP[c];
    }
}

static void mus_step(void)
{
    const song_t *g = &songs[mus.song];
    uint32_t c, s = mus.step;
    for (c = 0; c < 3u; c++) {
        uint8_t n = g->note[c][s];
        if (n == NOTE_OFF)
            mus.ch[c].held = 0;
        else if (n != NOTE_HOLD)
            fmv_on(&mus.ch[c], n);
    }
    switch (g->drum[s]) {
    case 'x': mus.hamp = 0.25f; /* fall through */
    case 'k': mus.kenv = 1.0f; mus.kamp = 1.0f; mus.kph = 0.0f; break;
    case 's': mus.samp = 0.5f; break;
    case 'h': mus.hamp = 0.25f; break;
    }
    mus.step = (uint16_t)((s + 1u) % g->steps);
    mus.left = g->step_len;
}

/* the music for n frames, mono, added to out[] (scaled later with MASTER) */
static void mus_render(float *out, uint32_t n)
{
    uint32_t i;
    float g = mus_gain;
    if (mus.song != mus_want) {          /* another song: from its start, the voices let go */
        mus.song = mus_want;
        mus.step = 0;
        mus.left = 0;
        for (i = 0; i < 3u; i++)
            mus.ch[i].held = 0;
        if (mus.ch[0].amp == 0.0f)
            mus_voices();
    }
    if (mus.song == SONG_NONE || g <= 0.0f) {
        for (i = 0; i < 3u; i++)         /* (silent: let the voices die away, so a song starts clean) */
            mus.ch[i].env = 0.0f;
        return;
    }
    for (i = 0; i < n; i++) {
        float s = 0.0f, nz;
        if (!mus.left)
            mus_step();
        mus.left--;
        s += fmv_run(&mus.ch[0]) + fmv_run(&mus.ch[1]) + fmv_run(&mus.ch[2]);
        mus.noise = mus.noise * 1664525u + 1013904223u;
        nz = (float)(mus.noise >> 9) / 4194304.0f - 1.0f;
        if (mus.kamp > 0.001f) {         /* kick: a sine falling from 160 Hz to 45 */
            mus.kph += (45.0f + 115.0f * mus.kenv) / 44100.0f;
            if (mus.kph >= 1.0f)
                mus.kph -= 1.0f;
            s += lut(mus.kph) * mus.kamp * 0.55f;
            mus.kenv *= 0.9988f;
            mus.kamp *= 0.99945f;
        }
        if (mus.samp > 0.001f) {         /* snare: noise and a little tone */
            mus.sph += 190.0f / 44100.0f;
            if (mus.sph >= 1.0f)
                mus.sph -= 1.0f;
            s += (nz * 0.8f + lut(mus.sph) * 0.3f) * mus.samp * 0.5f;
            mus.samp *= 0.9986f;
        }
        if (mus.hamp > 0.001f) {         /* hat: the noise's edges only */
            s += (nz - mus.hprev) * mus.hamp * 0.25f;
            mus.hamp *= 0.9955f;
        }
        mus.hprev = nz;
        out[i] += s * g;
    }
}
