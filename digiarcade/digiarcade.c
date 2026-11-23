/* SPDX-License-Identifier: GPL-2.0-or-later */
/* DIGIARCADE for the Digitakt mk1 (core 4.4 + seq-events, OS 1.54): a game pack on one master page (FUNC + LFO,
 * then LEFT / RIGHT to ARC). UP / DOWN, any knob or a trig key (1-16: the first 16 games) picks a game, YES
 * opens it, NO goes back to the list. While a game is open, the 16 trig keys are the game's (they don't edit
 * the pattern); off the page they're yours again.
 *
 * The 21 games: RHYTHM GAME, SIMON, BREAKOUT, SNAKE, PONG (here); TETRIS, FLAPPY, SPACE INVADERS,
 * WHACK-A-MOLE, LIGHTS OUT (games2.h); EAR TRAINER, BEAT MATCH, LUNAR LANDER, TRON, MINESWEEPER (games3.h);
 * HELLTAKT (games4.h); DRUM HERO 2P, PATTERN COPY, PINBALL, SEQUENCER PUZZLE, DUNGEON (games5.h). Each
 * game's keys are described where it's written, and in the README.
 *
 *   RHY  a rhythm game on your own pattern: the trigs of tracks 1-8 fall down 8 lanes in time, and you hit
 *        them on the trig keys (1-8, or 9-16 below them) as they reach the line. PERFECT within 40 ms,
 *        GOOD within 100 ms. It learns the pattern as it plays: a trig counts once it has been heard a
 *        loop. YES starts while the pattern plays, NO (or STOP) ends it. Knob A: how far ahead you see.
 *        DRUM HERO 2P is the same game for two: player 1 on keys 1-8, player 2 on 9-16.
 *   SMN  Simon: the pads play a growing sequence, you play it back. Knob A: TRK (8 pads, the sounds of
 *        tracks 1-8: keys 1-8 or 9-16) or NOT (16 pads, notes of a pentatonic scale on the active track).
 *        YES starts.
 *   BRK  Breakout: knob A (or LEFT / RIGHT) moves the paddle, YES launches, NO pauses. Knob D picks a track
 *        (OFF, 1-8) whose sound plays on every brick, higher rows higher notes.
 *
 * With LED COLORS 1.0 or later in the build, the trig keys light up for every game (leds.h).
 * The best scores and the settings are saved. Time is kept in audio blocks (1500 a second), counted in
 * ev_render_master; the pattern's steps come from ev_gen_trig and its hits from ev_fire_vel (both at
 * interrupt level: stores only). */
#include <stdint.h>
#include "../common/heapbuf.h"
#include "digitakt-mk1/core3.h"
#include "digitakt-mk1/seq.h"
#include "../common/mpui.h"

#define K_YES   12
#define K_NO    13
#define K_UP    14
#define K_DOWN  15
#define K_LEFT  16
#define K_RIGHT 17
#define K_TRIG1 24
#define F_DOWN    0x01
#define F_REPEAT  0x08
#define F_RELEASE 0x10
#define PANEL 0x40
#define SEC 1500                                /* audio blocks a second */
#define NOTE  60

enum { G_IDLE, G_RUN, G_PAUSE, G_OVER };

static volatile uint32_t blks;
static uint32_t rnd_s = 0x2468aceu;
static uint32_t rnd(uint32_t n) { rnd_s ^= rnd_s << 13; rnd_s ^= rnd_s >> 17; rnd_s ^= rnd_s << 5; return n ? (rnd_s >> 9) % n : 0; }   /* the better upper bits */
static char *put(char *p, const char *s) { while (*s) *p++ = *s++; *p = 0; return p; }
static int32_t active(void) { int32_t t = (int32_t)fw_active_track; return t < 0 ? 0 : t > 15 ? 15 : t; }
static int32_t iabs(int32_t v) { return v < 0 ? -v : v; }

typedef int32_t (*emmc_fn)(int32_t block, int32_t bytes, void *buf);   /* 0 on success */
#define EMMC_READ  ((emmc_fn)EMMC_READ_AT)
#define EMMC_WRITE ((emmc_fn)EMMC_WRITE_AT)

/* ---- settings and best scores ------------------------------------------------------------------------ */
static int32_t rhy_hi, smn_hi, brk_hi, rhy_look = 1, smn_mode, brk_snd;
static int32_t sn_hi, png_snd, p_two;          /* Snake's best, the sound Snake and Pong play, Pong 1P/2P */
static int32_t tet_hi, flp_hi, inv_hi, wak_hi, lts_hi, tet_sync, flp_bpm, wak_rate = 1;   /* the second batch's */
static int32_t ear_hi, bpm_hi, lnd_hi, trn_hi, min_hi, ear_mode;                          /* the third's */
static int32_t hel_hi, dh_hi, pc_hi, pb_hi, sq_hi, dg_hi;   /* Drum Hero, Pattern Copy, Pinball, Sequencer Puzzle, Dungeon */
static int32_t loaded, save_wait;
HEAP_BUF(block, 512)                            /* borrowed from the heap while loading or saving */
#define MAGIC 0x47414d32u                       /* "GAM2" */
static uint32_t sum_of(const uint8_t *b) { uint32_t s = 0x55; for (int32_t i = 4; i < 124; i++) s = s * 31u + b[i]; return s; }
static void load(void)
{
    if (!block_ready()) return;
    if (EMMC_READ(SAVE_BLOCK, 512, block) != 0) return;
    uint32_t *w = (uint32_t *)block;
    if (w[0] != MAGIC || w[31] != sum_of(block)) return;
    rhy_hi = (int32_t)w[1]; smn_hi = (int32_t)w[2]; brk_hi = (int32_t)w[3];
    rhy_look = ui_clamp(block[16], 0, 3); smn_mode = block[17] & 1; brk_snd = ui_clamp(block[18], 0, 8);
    sn_hi = (int32_t)w[5]; png_snd = ui_clamp(block[19], 0, 8); p_two = block[20] & 1;
    tet_hi = (int32_t)w[6]; flp_hi = (int32_t)w[7]; inv_hi = (int32_t)w[8]; wak_hi = (int32_t)w[9]; lts_hi = (int32_t)w[10];
    tet_sync = block[21] & 1; flp_bpm = block[22] & 1; wak_rate = ui_clamp(block[23], 0, 2);
    ear_hi = (int32_t)w[11]; bpm_hi = (int32_t)w[12]; lnd_hi = (int32_t)w[13]; trn_hi = (int32_t)w[14]; min_hi = (int32_t)w[15]; ear_mode = block[24] & 1;
    hel_hi = (int32_t)w[16];
    dh_hi = (int32_t)w[17]; pc_hi = (int32_t)w[18]; pb_hi = (int32_t)w[19]; sq_hi = (int32_t)w[20]; dg_hi = (int32_t)w[21];
}
static void save(void)
{
    if (!block_ready()) return;
    for (int32_t i = 0; i < 512; i++) block[i] = 0;
    uint32_t *w = (uint32_t *)block;
    w[0] = MAGIC; w[1] = (uint32_t)rhy_hi; w[2] = (uint32_t)smn_hi; w[3] = (uint32_t)brk_hi;
    block[16] = (uint8_t)rhy_look; block[17] = (uint8_t)smn_mode; block[18] = (uint8_t)brk_snd;
    w[5] = (uint32_t)sn_hi; block[19] = (uint8_t)png_snd; block[20] = (uint8_t)p_two;
    w[6] = (uint32_t)tet_hi; w[7] = (uint32_t)flp_hi; w[8] = (uint32_t)inv_hi; w[9] = (uint32_t)wak_hi; w[10] = (uint32_t)lts_hi;
    block[21] = (uint8_t)tet_sync; block[22] = (uint8_t)flp_bpm; block[23] = (uint8_t)wak_rate;
    w[11] = (uint32_t)ear_hi; w[12] = (uint32_t)bpm_hi; w[13] = (uint32_t)lnd_hi; w[14] = (uint32_t)trn_hi; w[15] = (uint32_t)min_hi; block[24] = (uint8_t)ear_mode;
    w[16] = (uint32_t)hel_hi;
    w[17] = (uint32_t)dh_hi; w[18] = (uint32_t)pc_hi; w[19] = (uint32_t)pb_hi; w[20] = (uint32_t)sq_hi; w[21] = (uint32_t)dg_hi;
    w[31] = sum_of(block);
    EMMC_WRITE(SAVE_BLOCK, 512, block);
}

/* ---- playing a pad: a note on, then its release a moment later ---------------------------------------- */
static int32_t snd_trk = -1, snd_note;
static uint32_t snd_off_at;
static void pad_sound(int32_t track, int32_t note, int32_t vel, int32_t len)
{
    if (snd_trk >= 0) fw_note_off(snd_trk, snd_note, PANEL);
    fw_note_on(track, note, vel, PANEL, 0, -1, -1);
    snd_trk = track; snd_note = note; snd_off_at = blks + (uint32_t)len;
}
static void sound_tick(void)
{
    if (snd_trk >= 0 && (int32_t)(blks - snd_off_at) >= 0) { fw_note_off(snd_trk, snd_note, PANEL); snd_trk = -1; }
}

/* ==== RHY: the rhythm game ================================================================================ */
/* what the sequencer does (interrupt level) */
static volatile uint32_t r_mask[8][2];          /* the steps that fired, the last time each was reached */
static volatile uint32_t r_known[8][2];         /* the step just begun: did it fire last loop? */
static volatile int32_t r_cur[8];               /* the step each track is on */
static volatile uint32_t r_at[8];               /* when it began (blocks) */
static volatile uint32_t r_len_q8[8] = { 48000, 48000, 48000, 48000, 48000, 48000, 48000, 48000 };
#define RF 6                                    /* the hits remembered per lane */
struct rfire { uint32_t at; int32_t step; uint8_t live, judged; };
static volatile struct rfire r_fire[8][RF];
static volatile uint32_t r_fire_n[8];
static volatile uint32_t r_last_step;           /* blocks: the last step of an audio track (is it playing?) */
static volatile uint32_t r_last_any;            /* the last step of any track, MIDI too */
static volatile int32_t snk_step_req;            /* the active track took a step: Snake moves */
static uint32_t g_ticks;

void gm_master(int32_t *pair) { (void)pair; blks++; }

void gm_step(int32_t track, int32_t step, int32_t first, uint32_t *trig)
{
    (void)first; (void)trig;
    if (track < 0 || track > 15 || step < 0) return;
    r_last_any = blks;
    if (track == active()) snk_step_req = 1;
    if (track > 7 || step > 63) return;
    uint32_t now = blks, d = now - r_at[track];
    if (step == r_cur[track] && d < 8) return;
    if (d >= 8 && d < (r_len_q8[track] >> 6))   /* a step's length, smoothed (not across a stop) */
        r_len_q8[track] += ((int32_t)(d << 8) - (int32_t)r_len_q8[track]) >> 2;
    r_cur[track] = step; r_at[track] = now; r_last_step = now;
    uint32_t bit = 1u << (step & 31);
    if (r_mask[track][step >> 5] & bit) r_known[track][step >> 5] |= bit; else r_known[track][step >> 5] &= ~bit;
    r_mask[track][step >> 5] &= ~bit;            /* until it fires again */
}

/* a hit a lane expected (it fired here last loop too): one to judge */
static volatile int32_t r_pre_step[2][8] = { { -1, -1, -1, -1, -1, -1, -1, -1 }, { -1, -1, -1, -1, -1, -1, -1, -1 } };   /* hit early */
static int32_t r_two;                            /* DRUM HERO: two players, on trigs 1-8 and 9-16 */
void gm_fire(int32_t track, int32_t step, int32_t *vel)
{
    if (track < 0 || track > 7 || step < 0 || step > 63 || (vel && *vel <= 0)) return;
    uint32_t bit = 1u << (step & 31);
    int32_t known = (r_known[track][step >> 5] & bit) != 0;
    r_mask[track][step >> 5] |= bit;
    if (!known) return;                          /* not seen before: no score either way */
    volatile struct rfire *f = &r_fire[track][r_fire_n[track]++ % RF];
    f->at = blks; f->step = step; f->live = 1; f->judged = r_two ? 0 : 2;    /* a bit a player; 1P: player 2's done */
    for (int32_t pl = 0; pl < 2; pl++) if (r_pre_step[pl][track] == step) { f->judged |= (uint8_t)(1u << pl); r_pre_step[pl][track] = -1; }
}

/* the UI side: one or two players */
#define PERFECT 60                               /* blocks: 40 ms */
#define GOOD   150                               /* 100 ms */
static int32_t r_state = G_IDLE, r_score[2], r_combo[2], r_best_combo[2], r_perf[2], r_good[2], r_miss[2];
static const char *r_judge[2];
static uint32_t r_judge_at[2], r_flash[2][8], r_pre_at[2][8];
static int32_t r_len(int32_t t) { struct sq_track i; return sq_track_info(t, &i, sizeof i) == 0 && i.len > 0 ? i.len : 16; }
static int32_t r_has(int32_t t, int32_t s) { return (r_mask[t][s >> 5] >> (s & 31)) & 1; }
static void r_say(int32_t pl, const char *s) { r_judge[pl] = s; r_judge_at[pl] = blks; }
static void r_award(int32_t pl, int32_t delta)
{
    delta = iabs(delta);
    r_combo[pl]++;
    if (r_combo[pl] > r_best_combo[pl]) r_best_combo[pl] = r_combo[pl];
    int32_t mult = 1 + r_combo[pl] / 10;
    if (delta <= PERFECT) { r_perf[pl]++; r_score[pl] += 100 * mult; r_say(pl, "PERFECT"); }
    else { r_good[pl]++; r_score[pl] += 50 * mult; r_say(pl, "GOOD"); }
}
static void r_start(void)
{
    for (int32_t pl = 0; pl < 2; pl++) {
        r_score[pl] = r_combo[pl] = r_best_combo[pl] = r_perf[pl] = r_good[pl] = r_miss[pl] = 0; r_judge[pl] = 0;
        for (int32_t t = 0; t < 8; t++) r_pre_step[pl][t] = -1;
    }
    for (int32_t t = 0; t < 8; t++) for (int32_t i = 0; i < RF; i++) r_fire[t][i].judged = 3;
    r_state = G_RUN;
}
static void r_end(void)
{
    r_state = G_OVER;
    if (r_two) { int32_t w = r_score[0] > r_score[1] ? r_score[0] : r_score[1]; if (w > dh_hi) { dh_hi = w; save_wait = 60; } }
    else if (r_score[0] > rhy_hi) { rhy_hi = r_score[0]; save_wait = 60; }
}
static void r_press(int32_t lane, int32_t pl)
{
    uint32_t now = blks, bit = 1u << pl;
    r_flash[pl][lane] = now + 120;
    if (r_state != G_RUN) return;
    /* late: a hit that already fired, not yet judged for this player */
    int32_t best = -1, bd = GOOD + 1;
    for (int32_t i = 0; i < RF; i++) {
        volatile struct rfire *f = &r_fire[lane][i];
        int32_t d = (int32_t)(now - f->at);
        if (f->live && !(f->judged & bit) && d <= GOOD && d < bd) { bd = d; best = i; }
    }
    if (best >= 0) { r_fire[lane][best].judged |= (uint8_t)bit; r_award(pl, bd); return; }
    /* early: the next step it fired on last loop */
    int32_t len = r_len(lane), steplen = (int32_t)(r_len_q8[lane] >> 8);
    for (int32_t k = 1; k <= 2; k++) {
        int32_t s = (r_cur[lane] + k) % len;
        if (!r_has(lane, s)) continue;
        int32_t due = (int32_t)(r_at[lane] - now) + steplen * k;
        if (due >= 0 && due <= GOOD && r_pre_step[pl][lane] != s) { r_pre_step[pl][lane] = s; r_pre_at[pl][lane] = now; r_award(pl, due); return; }
        break;
    }
    r_combo[pl] = 0;                             /* nothing there */
    r_say(pl, "X");
}
static void r_tick(void)
{
    uint32_t now = blks;
    for (int32_t t = 0; t < 8; t++) {
        for (int32_t pl = 0; pl < 2; pl++) if (r_pre_step[pl][t] >= 0 && now - r_pre_at[pl][t] > 2 * GOOD) r_pre_step[pl][t] = -1;   /* never fired */
        if (r_state != G_RUN) continue;
        for (int32_t i = 0; i < RF; i++) {
            volatile struct rfire *f = &r_fire[t][i];
            if (!f->live || f->judged == 3 || now - f->at <= GOOD) continue;
            for (int32_t pl = 0; pl < 2; pl++)
                if (!(f->judged & (1u << pl))) { f->judged |= (uint8_t)(1u << pl); r_miss[pl]++; r_combo[pl] = 0; r_say(pl, "MISS"); }
        }
    }
    if (r_state == G_RUN && now - r_last_step > SEC) r_end();      /* the pattern stopped */
}
static void r_judge_box(void *bmp, int32_t pl, int32_t cx)
{
    const char *j = r_judge[pl];
    if (!j || blks - r_judge_at[pl] >= 500) return;
    int32_t w = 4 * ui_len(j) + 3, x = cx - w / 2;
    fw_fillrect(bmp, x, 24, x + w, 32, 0); fw_framerect(bmp, x, 24, x + w, 32, 1); ui_text(bmp, x + 2, 26, j);
}
static void rhy_draw(void *view, void *bmp)
{
    char s[40], *p;
    (void)view;
    uint32_t now = blks;
    fw_fillrect(bmp, 0, 0, 127, 52, 0);
    if (r_state == G_IDLE || r_state == G_OVER) {
        if (r_state == G_OVER && r_two) {
            ui_centred(bmp, 0, 128, 44, r_score[0] == r_score[1] ? "A DRAW" : r_score[0] > r_score[1] ? "PLAYER 1 WINS" : "PLAYER 2 WINS");
            for (int32_t pl = 0; pl < 2; pl++) {
                p = put(s, pl ? "P2 " : "P1 "); p = ui_num(p, r_score[pl]); p = put(p, "  P"); p = ui_num(p, r_perf[pl]);
                p = put(p, " G"); p = ui_num(p, r_good[pl]); p = put(p, " M"); ui_num(p, r_miss[pl]);
                ui_centred(bmp, 0, 128, 32 - pl * 8, s);
            }
        } else if (r_state == G_OVER) {
            ui_centred(bmp, 0, 128, 44, "SONG OVER");
            p = put(s, "SCORE "); ui_num(p, r_score[0]); ui_centred(bmp, 0, 128, 34, s);
            p = put(s, "PERFECT "); p = ui_num(p, r_perf[0]); p = put(p, "  GOOD "); p = ui_num(p, r_good[0]);
            p = put(p, "  MISS "); ui_num(p, r_miss[0]); ui_centred(bmp, 0, 128, 26, s);
            p = put(s, "BEST COMBO "); ui_num(p, r_best_combo[0]); ui_centred(bmp, 0, 128, 18, s);
        } else {
            ui_centred(bmp, 0, 128, 44, "PLAY YOUR PATTERN THEN YES");
            ui_centred(bmp, 0, 128, 34, r_two ? "P1: TRIGS 1-8   P2: TRIGS 9-16" : "HIT THE TRIG KEYS AS THE");
            ui_centred(bmp, 0, 128, 27, r_two ? "SAME NOTES - BEST SCORE WINS" : "NOTES REACH THE LINE");
        }
        p = put(s, "HI "); ui_num(p, r_two ? dh_hi : rhy_hi); ui_text(bmp, 2, 2, s);
        static const char *const look[4] = { "SEE 4", "SEE 6", "SEE 8", "SEE 12" };
        ui_text(bmp, 100, 2, look[rhy_look]);
        return;
    }
    static const uint8_t looks[4] = { 4, 6, 8, 12 };
    int32_t L = looks[rhy_look];
    fw_fillrect(bmp, 0, 7, 127, 7, 1);                            /* the line */
    for (int32_t t = 0; t < 8; t++) {
        int32_t x0 = t * 16, len = r_len(t), steplen = (int32_t)(r_len_q8[t] >> 8);
        char d[2] = { (char)('1' + t), 0 };
        ui_text(bmp, x0 + 6, 0, d);
        if (t) for (int32_t y = 9; y < 45; y += 4) fw_fillrect(bmp, x0, y, x0, y, 1);   /* the lanes */
        if ((int32_t)(r_flash[0][t] - now) > 0) fw_fillrect(bmp, x0 + 1, 5, r_two ? x0 + 7 : x0 + 15, 9, -1);
        if (r_two && (int32_t)(r_flash[1][t] - now) > 0) fw_fillrect(bmp, x0 + 8, 5, x0 + 15, 9, -1);
        if (steplen < 8) continue;
        int32_t since = (int32_t)(now - r_at[t]);
        for (int32_t k = 1; k <= L && k < 64; k++) {
            int32_t st = (r_cur[t] + k) % len;
            if (!r_has(t, st) || (!r_two && st == r_pre_step[0][t])) continue;
            int32_t until = steplen * k - since;
            if (until < 0) continue;
            int32_t y = 8 + until * 34 / (L * steplen);
            if (y > 42) break;
            fw_fillrect(bmp, x0 + 3, y, x0 + 13, y + 2, 1);
        }
    }
    if (r_two) {
        p = put(s, "P1 "); ui_num(p, r_score[0]); ui_text(bmp, 2, 47, s);
        p = put(s, "P2 "); ui_num(p, r_score[1]); ui_text(bmp, 126 - 4 * ui_len(s), 47, s);
        r_judge_box(bmp, 0, 32); r_judge_box(bmp, 1, 96);
    } else {
        p = ui_num(s, r_score[0]); ui_text(bmp, 2, 47, s);
        if (r_combo[0] >= 2) { p = put(s, "COMBO "); ui_num(p, r_combo[0]); ui_text(bmp, 80, 47, s); }
        r_judge_box(bmp, 0, 64);
    }
}
static int32_t enc_acc;
static int32_t detents(int32_t turn) { enc_acc += turn; int32_t d = 0; while (enc_acc >= 16) { enc_acc -= 16; d++; } while (enc_acc <= -16) { enc_acc += 16; d--; } return d; }
static int32_t rhy_enc(void *view, void *event)
{
    int32_t knob = *(int32_t *)((char *)event + 12), turn = *(int32_t *)((char *)event + 16);
    if (knob != 1 || r_state == G_RUN) return 0;
    rhy_look = ui_clamp(rhy_look + detents(turn), 0, 3);
    save_wait = 60;
    fw_invalidate(view);
    return 1;
}
static int32_t rhy_key(void *view, void *event)
{
    int32_t key = *(int32_t *)((char *)event + 12), fl = *(int32_t *)((char *)event + 16);
    int32_t press = (fl & F_DOWN) && !(fl & (F_RELEASE | F_REPEAT));
    if (key == K_YES) { if (press && r_state != G_RUN) r_start(); fw_invalidate(view); return 1; }
    if (key == K_NO && r_state == G_RUN) { if (press) r_end(); return 1; }
    return mp_key(view, event);
}

/* ==== SMN: Simon ============================================================================================ */
enum { S_IDLE, S_SHOW, S_WAIT, S_INPUT, S_OVER };
#define SLEN 64
static uint8_t s_seq[SLEN];
static int32_t s_state = S_IDLE, s_n, s_pos, s_lit = -1, s_wrong = -1;
static uint32_t s_next, s_lit_until;
static int32_t s_pads(void) { return smn_mode ? 16 : 8; }
static void s_play(int32_t pad, int32_t len)
{
    static const uint8_t pent[5] = { 0, 2, 4, 7, 9 };
    if (smn_mode) pad_sound(active(), 48 + 12 * (pad / 5) + pent[pad % 5], 110, len);
    else pad_sound(pad, 60, 110, len);
    s_lit = pad; s_lit_until = blks + (uint32_t)len;
}
static int32_t s_on(void) { int32_t on = 600 - s_n * 20; return on < 225 ? 225 : on; }   /* faster each round */
static void s_add(void) { if (s_n < SLEN) s_seq[s_n++] = (uint8_t)rnd((uint32_t)s_pads()); }
static void s_start(void)
{
    s_n = 0; s_add(); s_pos = 0; s_wrong = -1;
    s_state = S_WAIT; s_next = blks + 600;
}
static void s_press(int32_t key_pad)
{
    if (s_state != S_INPUT) { if (s_state == S_IDLE || s_state == S_OVER) s_play(key_pad, 300); return; }
    if (key_pad == s_seq[s_pos]) {
        s_play(key_pad, 300);
        if (++s_pos == s_n) {                    /* round done: one more */
            if (s_n > smn_hi) { smn_hi = s_n; save_wait = 60; }
            s_add(); s_pos = 0; s_state = S_WAIT; s_next = blks + 900;
        }
    } else {
        s_wrong = key_pad; s_state = S_OVER;
        pad_sound(smn_mode ? active() : 0, 36, 127, 600);       /* a low thud */
    }
}
static void s_tick(void)
{
    uint32_t now = blks;
    if (s_lit >= 0 && (int32_t)(now - s_lit_until) >= 0) s_lit = -1;
    if ((s_state == S_WAIT || s_state == S_SHOW) && (int32_t)(now - s_next) >= 0) {
        if (s_state == S_WAIT) { s_state = S_SHOW; s_pos = 0; }
        if (s_pos < s_n) { int32_t on = s_on(); s_play(s_seq[s_pos++], on); s_next = now + (uint32_t)(on + on / 3); }
        else { s_state = S_INPUT; s_pos = 0; }
    }
}
static void smn_draw(void *view, void *bmp)
{
    char s[32], *p;
    (void)view;
    fw_fillrect(bmp, 0, 0, 127, 52, 0);
    int32_t n = s_pads(), w = 128 / n;
    for (int32_t i = 0; i < n; i++) {                              /* the pads, laid out like the trig keys */
        int32_t x0 = i * w + 1, x1 = x0 + w - 3;
        if (i == s_lit) fw_fillrect(bmp, x0, 14, x1, 36, 1);
        else fw_framerect(bmp, x0, 14, x1, 36, 1);
        if (i == s_wrong && s_state == S_OVER) { ui_line(bmp, x0, 14, x1, 36); ui_line(bmp, x0, 36, x1, 14); }
    }
    static const char *const say[5] = { "YES: START", "WATCH AND LISTEN", "GET READY", "YOUR TURN", "WRONG PAD" };
    ui_centred(bmp, 0, 128, 44, say[s_state]);
    p = put(s, "ROUND "); ui_num(p, s_n); ui_text(bmp, 2, 3, s);
    p = put(s, "BEST "); ui_num(p, smn_hi); ui_text(bmp, 50, 3, s);
    ui_text(bmp, 100, 3, smn_mode ? "A:NOT" : "A:TRK");
}
static int32_t smn_enc(void *view, void *event)
{
    int32_t knob = *(int32_t *)((char *)event + 12), turn = *(int32_t *)((char *)event + 16);
    if (knob != 1 || s_state == S_SHOW || s_state == S_INPUT || s_state == S_WAIT) return 0;
    int32_t d = detents(turn);
    if (d) { smn_mode = d > 0; save_wait = 60; }
    fw_invalidate(view);
    return 1;
}
static int32_t smn_key(void *view, void *event)
{
    int32_t key = *(int32_t *)((char *)event + 12), fl = *(int32_t *)((char *)event + 16);
    int32_t press = (fl & F_DOWN) && !(fl & (F_RELEASE | F_REPEAT));
    if (key == K_YES) { if (press && (s_state == S_IDLE || s_state == S_OVER)) s_start(); return 1; }
    if (key == K_NO && s_state != S_IDLE && s_state != S_OVER) { if (press) s_state = S_IDLE; return 1; }
    return mp_key(view, event);
}

/* ==== BRK: Breakout ========================================================================================= */
#define BCOLS 8
#define BROWS 5
#define PW 22
static uint8_t b_bricks[BROWS];                 /* a bit a column, row 0 the lowest */
static int32_t b_state = G_IDLE, b_score, b_lives, b_level, b_stuck;
static int32_t b_px = 53 << 8, b_bx, b_by, b_vx, b_vy, b_spd, b_left, b_right;
static uint32_t b_last;
static int32_t b_row_y(int32_t r) { return 24 + r * 4; }
static void b_fill(void) { for (int32_t r = 0; r < BROWS; r++) b_bricks[r] = 0xff; }
static void b_serve(void) { b_stuck = 1; b_vx = 0; b_vy = 0; }
static void b_start(void)
{
    b_score = 0; b_lives = 3; b_level = 1; b_spd = 300;
    b_fill(); b_serve(); b_state = G_RUN; b_last = blks;
}
static void b_launch(void)
{
    b_stuck = 0;
    b_vx = (int32_t)rnd(2) ? b_spd / 2 : -b_spd / 2;
    b_vy = b_spd;
}
static void b_frame(void)                        /* a 60th of a second */
{
    if (b_left) b_px -= 2 << 8;
    if (b_right) b_px += 2 << 8;
    b_px = ui_clamp(b_px, 0, (127 - PW) << 8);
    if (b_stuck) { b_bx = b_px + ((PW / 2) << 8); b_by = 3 << 8; return; }
    b_bx += b_vx; b_by += b_vy;
    if (b_bx < 0) { b_bx = -b_bx; b_vx = -b_vx; }
    if (b_bx > 126 << 8) { b_bx = (252 << 8) - b_bx; b_vx = -b_vx; }
    if (b_by > 43 << 8) { b_by = (86 << 8) - b_by; b_vy = -b_vy; }
    if (b_vy < 0 && b_by <= 3 << 8 && b_by >= 1 << 8) {           /* the paddle: where it lands steers it */
        int32_t off = (b_bx + (1 << 8)) - (b_px + ((PW / 2) << 8));
        if (iabs(off) <= ((PW / 2 + 2) << 8)) {
            b_vx = off * b_spd / ((PW / 2) << 8);
            b_vx = ui_clamp(b_vx, -b_spd * 4 / 5, b_spd * 4 / 5);
            b_vy = b_spd - iabs(b_vx) / 2;
            b_by = 3 << 8;
        }
    }
    if (b_by < 0) {                              /* missed */
        if (--b_lives <= 0) { b_state = G_OVER; if (b_score > brk_hi) { brk_hi = b_score; save_wait = 60; } }
        else b_serve();
        return;
    }
    int32_t cx = (b_bx >> 8) + 1, cy = (b_by >> 8) + 1;
    for (int32_t r = 0; r < BROWS; r++) {
        int32_t y = b_row_y(r);
        if (cy < y || cy > y + 2) continue;
        int32_t c = cx / 16;
        if (c >= 0 && c < BCOLS && (b_bricks[r] >> c) & 1) {
            b_bricks[r] &= (uint8_t)~(1u << c);
            b_vy = -b_vy;
            b_score += (r + 1) * 10;
            if (brk_snd) pad_sound(brk_snd - 1, 48 + r * 3, 100, 120);
            int32_t left = 0;
            for (int32_t k = 0; k < BROWS; k++) left |= b_bricks[k];
            if (!left) { b_level++; b_spd += 40; b_fill(); b_serve(); }
        }
        break;
    }
}
static void b_tick(void)
{
    uint32_t now = blks;
    if (b_state != G_RUN) { b_last = now; return; }
    int32_t n = 0;
    while ((int32_t)(now - b_last) >= 25 && n++ < 8) { b_last += 25; b_frame(); if (b_state != G_RUN) break; }
    if (n >= 8) b_last = now;
}
static void brk_draw(void *view, void *bmp)
{
    char s[24], *p;
    (void)view;
    fw_fillrect(bmp, 0, 0, 127, 52, 0);
    for (int32_t r = 0; r < BROWS; r++)
        for (int32_t c = 0; c < BCOLS; c++)
            if ((b_bricks[r] >> c) & 1) fw_fillrect(bmp, c * 16 + 1, b_row_y(r), c * 16 + 14, b_row_y(r) + 2, 1);
    fw_fillrect(bmp, 0, 45, 127, 45, 1);
    int32_t px = b_px >> 8;
    if (b_state != G_IDLE) fw_fillrect(bmp, px, 1, px + PW, 2, 1);
    if (b_state == G_RUN || b_state == G_PAUSE) fw_fillrect(bmp, b_bx >> 8, b_by >> 8, (b_bx >> 8) + 1, (b_by >> 8) + 1, 1);
    p = ui_num(s, b_score); ui_text(bmp, 2, 47, s);
    p = put(s, "LV "); ui_num(p, b_level); ui_text(bmp, 52, 47, s);
    for (int32_t i = 0; i < b_lives && i < 5; i++) fw_fillrect(bmp, 122 - i * 5, 48, 124 - i * 5, 50, 1);
    const char *msg = b_state == G_IDLE ? "YES: START" : b_state == G_OVER ? "GAME OVER" : b_state == G_PAUSE ? "PAUSED" :
                      b_stuck ? "YES: LAUNCH" : 0;
    if (msg) {
        int32_t w = 4 * ui_len(msg) + 3, x = 64 - w / 2;
        fw_fillrect(bmp, x, 10, x + w, 18, 0); fw_framerect(bmp, x, 10, x + w, 18, 1); ui_text(bmp, x + 2, 12, msg);
        if (b_state == G_IDLE || b_state == G_OVER) {
            p = put(s, "BEST "); p = ui_num(p, brk_hi); p = put(p, "   D: SOUND ");
            if (brk_snd) { *p++ = 'T'; *p++ = (char)('0' + brk_snd); *p = 0; } else put(p, "OFF");
            ui_centred(bmp, 0, 128, 4, s);
        }
    }
}
static int32_t brk_enc(void *view, void *event)
{
    int32_t knob = *(int32_t *)((char *)event + 12), turn = *(int32_t *)((char *)event + 16);
    if (knob == 1 && b_state == G_RUN) { b_px = ui_clamp(b_px + turn * 64, 0, (127 - PW) << 8); return 1; }
    if (knob == 4) { brk_snd = ui_clamp(brk_snd + detents(turn), 0, 8); save_wait = 60; fw_invalidate(view); return 1; }
    return 0;
}
static int32_t brk_key(void *view, void *event)
{
    int32_t key = *(int32_t *)((char *)event + 12), fl = *(int32_t *)((char *)event + 16);
    int32_t down = (fl & F_DOWN) && !(fl & F_RELEASE), press = down && !(fl & F_REPEAT);
    if (key == K_YES) {
        if (press) {
            if (b_state == G_PAUSE) b_state = G_RUN;
            else if (b_state != G_RUN) b_start();
            else if (b_stuck) b_launch();
        }
        return 1;
    }
    if (key == K_NO && b_state == G_RUN) { if (press) { b_state = G_PAUSE; b_left = b_right = 0; } return 1; }
    if (b_state == G_RUN && (key == K_LEFT || key == K_RIGHT)) { if (key == K_LEFT) b_left = down; else b_right = down; return 1; }
    return mp_key(view, event);
}

/* ==== SNK and PNG: Snake and Pong (moved here from VISUALS 0.3) =============================================== */
/* ---- the games: shared ------------------------------------------------------------------------------ */
static void blip_sound(int32_t vel)
{
    if (png_snd > 0) { fw_note_off(png_snd - 1, NOTE, PANEL); fw_note_on(png_snd - 1, NOTE, vel, PANEL, 0, -1, -1); }
}

/* ---- Snake: 4-pixel cells, 32 x 11 -------------------------------------------------------------------- */
#define SCOLS 32
#define SROWS 11
#define SMAX 64
static uint8_t snx[SMAX], sny[SMAX];
static int32_t sn_head, sn_len, sn_dir, sn_next_dir, sn_fx, sn_fy, sn_state = G_IDLE, sn_score, sn_wait;
static const int8_t ddx[4] = { 1, 0, -1, 0 }, ddy[4] = { 0, 1, 0, -1 };   /* right, up, left, down */

static int32_t sn_on(int32_t x, int32_t y)
{
    for (int32_t i = 0; i < sn_len; i++) {
        int32_t k = (sn_head - i + SMAX) % SMAX;
        if (snx[k] == x && sny[k] == y) return 1;
    }
    return 0;
}
static void sn_food(void)
{
    do { sn_fx = (int32_t)rnd(SCOLS); sn_fy = (int32_t)rnd(SROWS); } while (sn_on(sn_fx, sn_fy));
}
static void sn_start(void)
{
    sn_len = 4; sn_head = 3; sn_dir = sn_next_dir = 0; sn_score = 0;
    for (int32_t i = 0; i < 4; i++) { snx[i] = (uint8_t)(10 + i); sny[i] = 5; }
    sn_food();
    sn_state = G_RUN;
}
static void sn_move(void)
{
    if ((sn_next_dir ^ sn_dir) != 2) sn_dir = sn_next_dir;          /* never straight back */
    int32_t x = snx[sn_head] + ddx[sn_dir], y = sny[sn_head] + ddy[sn_dir];
    if (x < 0 || x >= SCOLS || y < 0 || y >= SROWS || sn_on(x, y)) {
        sn_state = G_OVER;
        if (sn_score > sn_hi) { sn_hi = sn_score; save_wait = 60; }
        return;
    }
    sn_head = (sn_head + 1) % SMAX;
    snx[sn_head] = (uint8_t)x; sny[sn_head] = (uint8_t)y;
    if (x == sn_fx && y == sn_fy) {
        if (sn_len < SMAX - 1) sn_len++;
        sn_score++;
        blip_sound(110);
        sn_food();
    }
}
static void snk_tick(int32_t step_now)
{
    if (sn_state != G_RUN) return;
    int32_t playing = blks - r_last_any < SEC;
    if (playing ? step_now : ++sn_wait >= 8) { sn_wait = 0; sn_move(); }
}
static void sn_cell(void *bmp, int32_t x, int32_t y, int32_t full)
{
    int32_t px = x * 4, py = y * 4;
    if (full) fw_fillrect(bmp, px, py, px + 2, py + 2, 1);
    else fw_framerect(bmp, px, py, px + 2, py + 2, 1);
}
static void sp_status(void *bmp, const char *name, int32_t score, int32_t hi, int32_t state)
{
    char s[24], *p = put(s, name);
    p = put(p, " "); p = ui_num(p, score);
    p = put(p, " HI "); ui_num(p, hi);
    ui_text(bmp, 2, 47, s);
    p = put(s, "SND "); if (png_snd) ui_num(p, png_snd); else put(p, "OFF");
    ui_text(bmp, 98, 47, s);
    if (state != G_RUN) {
        const char *m = state == G_OVER ? "GAME OVER  YES: AGAIN" : state == G_PAUSE ? "PAUSED  YES: GO ON" : "YES: START";
        int32_t n = 0; while (m[n]) n++;
        int32_t w = n * 4 + 6, x = 64 - w / 2;
        fw_fillrect(bmp, x, 18, x + w, 28, 0); fw_framerect(bmp, x, 18, x + w, 28, 1);
        ui_text(bmp, x + 4, 21, m);
    }
}
static void snk_draw(void *view, void *bmp)
{
    (void)view;
    fw_fillrect(bmp, 0, 0, 127, 52, 0);
    fw_framerect(bmp, 0, 0, 127, 45, 1);
    if (sn_len) {
        for (int32_t i = 0; i < sn_len; i++) {
            int32_t k = (sn_head - i + SMAX) % SMAX;
            sn_cell(bmp, snx[k], sny[k], 1);
        }
        if ((g_ticks / 10) & 1 || sn_state != G_RUN) sn_cell(bmp, sn_fx, sn_fy, 0);   /* the food blinks */
    }
    sp_status(bmp, "SNAKE", sn_score, sn_hi, sn_state);
}

/* ---- Pong ----------------------------------------------------------------------------------------------- */
#define PH 10                                   /* paddle height */
static int32_t p_you = 18 << 8, p_cpu = 18 << 8, p_bx, p_by, p_vx, p_vy, p_s1, p_s2, p_state = G_IDLE;
static int32_t view_is_pong;
static int32_t up_held, down_held;
static void p_serve(int32_t dir)
{
    p_bx = 64 << 8; p_by = 22 << 8;
    p_vx = dir * 230; p_vy = (int32_t)rnd(300) - 150;
}
static void png_tick(void)
{
    if (p_state != G_RUN) return;
    if (up_held) p_you += 2 << 8;
    if (down_held) p_you -= 2 << 8;
    p_you = ui_clamp(p_you, 1 << 8, (44 - PH) << 8);
    if (!p_two) {                                                    /* 1P: the Digitakt, a little slow */
        int32_t target = p_by - (PH / 2 << 8), d = target - p_cpu;
        p_cpu += d > 180 ? 180 : d < -180 ? -180 : d;
    }
    p_cpu = ui_clamp(p_cpu, 1 << 8, (44 - PH) << 8);
    p_bx += p_vx; p_by += p_vy;
    if (p_by < (1 << 8)) { p_by = 1 << 8; p_vy = -p_vy; }
    if (p_by > (42 << 8)) { p_by = 42 << 8; p_vy = -p_vy; }
    int32_t by = p_by >> 8;
    if (p_vx < 0 && p_bx <= (5 << 8) && p_bx > (2 << 8) && by + 1 >= p_you >> 8 && by <= (p_you >> 8) + PH) {
        p_vx = -p_vx * 17 / 16; p_vy += (by - (p_you >> 8) - PH / 2) * 40;
        blip_sound(110);
    }
    if (p_vx > 0 && p_bx >= (122 << 8) && p_bx < (125 << 8) && by + 1 >= p_cpu >> 8 && by <= (p_cpu >> 8) + PH) {
        p_vx = -p_vx * 17 / 16; p_vy += (by - (p_cpu >> 8) - PH / 2) * 40;
        blip_sound(80);
    }
    if (p_vx > 600) p_vx = 600;
    if (p_vx < -600) p_vx = -600;
    if (p_bx < 0) { p_s2++; p_serve(1); }
    if (p_bx > (127 << 8)) { p_s1++; p_serve(-1); }
    if (p_s1 >= 9 || p_s2 >= 9) p_state = G_OVER;
}
static void png_draw(void *view, void *bmp)
{
    (void)view;
    fw_fillrect(bmp, 0, 0, 127, 52, 0);
    fw_fillrect(bmp, 0, 45, 127, 45, 1); fw_fillrect(bmp, 0, 0, 127, 0, 1);
    for (int32_t y = 2; y < 44; y += 4) fw_fillrect(bmp, 63, y, 63, y + 1, 1);   /* the net */
    fw_fillrect(bmp, 3, p_you >> 8, 4, (p_you >> 8) + PH, 1);
    fw_fillrect(bmp, 123, p_cpu >> 8, 124, (p_cpu >> 8) + PH, 1);
    if (p_state == G_RUN) fw_fillrect(bmp, p_bx >> 8, p_by >> 8, (p_bx >> 8) + 1, (p_by >> 8) + 1, 1);
    char s[4];
    ui_num(s, p_s1); ui_text(bmp, 52, 38, s);
    ui_num(s, p_s2); ui_text(bmp, 70, 38, s);
    if (p_state == G_OVER) ui_text(bmp, 2, 47, p_two ? (p_s1 > p_s2 ? "P1 WINS" : "P2 WINS") : p_s1 > p_s2 ? "YOU WIN" : "DIGITAKT WINS");
    else ui_text(bmp, 2, 47, p_two ? "PONG 2P" : "PONG 1P");
    if (p_state != G_RUN) {                                          /* who moves what */
        fw_fillrect(bmp, 1, 1, 126, 7, 0);
        ui_text(bmp, 2, 2, p_two ? "P1: A  P2: H  B: 1P/2P" : "YOU: A OR UP/DOWN  B: 1P/2P");
    }
    char t[12], *p = put(t, "SND "); if (png_snd) ui_num(p, png_snd); else put(p, "OFF");
    ui_text(bmp, 98, 47, t);
    if (p_state != G_RUN) {
        const char *m = p_state == G_OVER ? "YES: AGAIN" : p_state == G_PAUSE ? "PAUSED  YES: GO ON" : "YES: START";
        int32_t n = 0; while (m[n]) n++;
        int32_t w = n * 4 + 6, x = 64 - w / 2;
        fw_fillrect(bmp, x, 18, x + w, 28, 0); fw_framerect(bmp, x, 18, x + w, 28, 1);
        ui_text(bmp, x + 4, 21, m);
    }
}

/* ---- the game pages' keys and knobs ----------------------------------------------------------------- */
static int32_t sp_key(void *view, void *event, int32_t is_snake)
{
    int32_t key = *(int32_t *)((char *)event + 12), fl = *(int32_t *)((char *)event + 16);
    int32_t down = (fl & F_DOWN) && !(fl & F_RELEASE), press = down && !(fl & F_REPEAT);
    int32_t *st = is_snake ? &sn_state : &p_state;
    if (!is_snake && key == K_UP) up_held = down && *st == G_RUN;
    if (!is_snake && key == K_DOWN) down_held = down && *st == G_RUN;
    if (key == K_YES) {
        if (press) {
            if (*st == G_PAUSE) *st = G_RUN;
            else if (*st != G_RUN) {
                if (is_snake) sn_start();
                else { p_s1 = p_s2 = 0; p_serve(-1); p_state = G_RUN; }
            }
            fw_invalidate(view);
        }
        return 1;
    }
    if (key == K_NO && *st == G_RUN) {
        if (press) { *st = G_PAUSE; up_held = down_held = 0; fw_invalidate(view); }
        return 1;
    }
    if (*st != G_RUN)
        return mp_key(view, event);                  /* not playing: LEFT / RIGHT step the pages */
    if (key >= K_UP && key <= K_RIGHT) {
        if (press && is_snake) {
            static const int8_t dir_of[4] = { 1, 3, 2, 0 };   /* UP, DOWN, LEFT, RIGHT */
            sn_next_dir = dir_of[key - K_UP];
        }
        return 1;
    }
    return 0;
}
static int32_t snk_key(void *view, void *event) { return sp_key(view, event, 1); }
static int32_t sp_enc(void *view, void *event);
static int32_t snk_enc(void *view, void *event) { view_is_pong = 0; return sp_enc(view, event); }
static int32_t png_enc(void *view, void *event) { view_is_pong = 1; return sp_enc(view, event); }
static int32_t png_key(void *view, void *event) { return sp_key(view, event, 0); }
static int32_t sp_enc(void *view, void *event)
{
    int32_t knob = *(int32_t *)((char *)event + 12), turn = *(int32_t *)((char *)event + 16);
    static int32_t acc;
    if (knob == 2 && view_is_pong && p_state != G_RUN) {   /* B: 1P / 2P (between games) */
        acc += turn;
        while (acc >= 16 || acc <= -16) { int32_t d = acc > 0 ? 1 : -1; acc -= d * 16; p_two = d > 0; save_wait = 60; }
        fw_invalidate(view);
        return 1;
    }
    if (knob == 8 && view_is_pong && p_two && p_state == G_RUN) {   /* H: player 2's paddle */
        p_cpu = ui_clamp(p_cpu + turn * 64, 1 << 8, (44 - PH) << 8);
        return 1;
    }
    if (knob == 4) {                                    /* D: the sound */
        acc += turn;
        while (acc >= 16 || acc <= -16) { int32_t d = acc > 0 ? 1 : -1; acc -= d * 16; png_snd = ui_clamp(png_snd + d, 0, 8); save_wait = 60; }
        fw_invalidate(view);
        return 1;
    }
    if (knob == 1 && p_state == G_RUN) {               /* A: your paddle */
        p_you = ui_clamp(p_you + turn * 64, 1 << 8, (44 - PH) << 8);
        return 1;
    }
    return 0;
}
static void snk_clear(void *view) { sn_hi = 0; save_wait = 60; sn_state = G_IDLE; sn_len = 0; fw_invalidate(view); }
static void png_clear(void *view) { p_s1 = p_s2 = 0; p_state = G_IDLE; fw_invalidate(view); }


/* ==== the menu: one master page, every game behind it ====================================================== */
static int32_t rhy_busy(void) { return r_state == G_RUN; }
static int32_t smn_busy(void) { return s_state == S_SHOW || s_state == S_WAIT || s_state == S_INPUT; }
static int32_t brk_busy(void) { return b_state == G_RUN; }
static int32_t snk_busy(void) { return sn_state == G_RUN; }
static int32_t png_busy(void) { return p_state == G_RUN; }
static void rhy_trig(int32_t i) { r_press(i & 7, r_two ? i >> 3 : 0); }
/* the menu's two ways in: the same game, one player or two */
static void r_mode(int32_t two) { if (r_state != G_RUN) r_two = two; }
static void rhy1_draw(void *v, void *b) { r_mode(0); rhy_draw(v, b); }
static void rhy2_draw(void *v, void *b) { r_mode(1); rhy_draw(v, b); }
static int32_t rhy1_key(void *v, void *e) { r_mode(0); return rhy_key(v, e); }
static int32_t rhy2_key(void *v, void *e) { r_mode(1); return rhy_key(v, e); }
static void smn_trig(int32_t i) { s_press(smn_mode ? i : i & 7); }
struct game {
    const char *name;                            /* the menu's line and the title bar */
    void (*draw)(void *view, void *bmp);
    int32_t (*key)(void *view, void *event);
    int32_t (*enc)(void *view, void *event);
    void (*trig)(int32_t key);                   /* the trig keys, 0-15; 0: the game leaves them alone */
    int32_t (*busy)(void);                       /* playing: NO is the game's, not back to the menu */
    int32_t *hi;
    void (*clear)(void *view);
    void (*trig_up)(int32_t key);                /* a trig key let go, for a game that holds them (Pinball) */
};
#include "games2.h"                              /* the second batch */
#include "games3.h"                              /* the third */
#include "games4.h"                              /* HELLTAKT */
#include "games5.h"                              /* Drum Hero, Pattern Copy, Pinball, the Sequencer Puzzle, the Dungeon */
static const struct game games[] = {
    { "RHYTHM GAME", rhy1_draw, rhy1_key, rhy_enc, rhy_trig, rhy_busy, &rhy_hi, 0 },
    { "SIMON", smn_draw, smn_key, smn_enc, smn_trig, smn_busy, &smn_hi, 0 },
    { "BREAKOUT", brk_draw, brk_key, brk_enc, 0, brk_busy, &brk_hi, 0 },
    { "SNAKE", snk_draw, snk_key, snk_enc, 0, snk_busy, &sn_hi, snk_clear },
    { "PONG", png_draw, png_key, png_enc, 0, png_busy, 0, png_clear },
    GAMES2
    GAMES3
    GAMES4
    GAMES5
};
#define NGAMES ((int32_t)(sizeof games / sizeof games[0]))
static int32_t g_cur = -1, g_sel;                /* the game open (-1: the menu), the menu's cursor */
static void menu_draw(void *bmp)
{
    char s[24];
    fw_fillrect(bmp, 0, 0, 127, 52, 0);
    int32_t top = g_sel - 2;                     /* six lines, the cursor kept in view */
    if (top > NGAMES - 6) top = NGAMES - 6;
    if (top < 0) top = 0;
    for (int32_t i = 0; i < 6 && top + i < NGAMES; i++) {
        int32_t k = top + i, y = 45 - i * 8;
        if (k < 16) { ui_num(s, k + 1); ui_text(bmp, 6 + (k < 9 ? 4 : 0), y, s); }   /* its trig key */
        ui_text(bmp, 16, y, games[k].name);
        if (games[k].hi && *games[k].hi) { ui_num(s, *games[k].hi); ui_text(bmp, 124 - 4 * ui_len(s), y, s); }
        if (k == g_sel) fw_fillrect(bmp, 2, y - 2, 125, y + 6, -1);
    }
    if (top > 0) ui_text(bmp, 0, 45, "+");
    if (top + 6 < NGAMES) ui_text(bmp, 0, 0, "+");
}
static void gm_draw(void *view, void *bmp)
{
    if (g_cur < 0) menu_draw(bmp);
    else games[g_cur].draw(view, bmp);
}
struct core_mpage gm_page;
static void gm_enter(int32_t g) { g_cur = g; gm_page.title = g < 0 ? "DigiARCADE" : games[g].name; }
static int32_t gm_pkey(void *view, void *event)
{
    int32_t key = *(int32_t *)((char *)event + 12), fl = *(int32_t *)((char *)event + 16);
    int32_t press = (fl & F_DOWN) && !(fl & (F_RELEASE | F_REPEAT)), rep = (fl & F_DOWN) && !(fl & F_RELEASE);
    if (g_cur < 0) {
        if (key == K_UP || key == K_DOWN) { if (rep) g_sel = ui_clamp(g_sel + (key == K_UP ? -1 : 1), 0, NGAMES - 1); fw_invalidate(view); return 1; }
        if (key == K_YES) { if (press) gm_enter(g_sel); fw_invalidate(view); return 1; }
        return mp_key(view, event);
    }
    if (key == K_NO && !games[g_cur].busy()) { if (press) gm_enter(-1); fw_invalidate(view); return 1; }
    return games[g_cur].key(view, event);
}
static int32_t gm_penc(void *view, void *event)
{
    if (g_cur < 0) {                              /* any knob scrolls the menu */
        int32_t d = detents(*(int32_t *)((char *)event + 16));
        if (d) { g_sel = ui_clamp(g_sel + d, 0, NGAMES - 1); fw_invalidate(view); }
        return 1;
    }
    return games[g_cur].enc ? games[g_cur].enc(view, event) : 0;
}
static void gm_clear(void *view) { if (g_cur >= 0 && games[g_cur].clear) games[g_cur].clear(view); }
struct core_mpage gm_page = { 99, "ARC", "DigiARCADE", gm_draw, gm_pkey, gm_penc, 0, 0, gm_clear };
#include "leds.h"                                /* the trig lights */
static volatile int32_t shown;                   /* our page is on screen */

/* the trig keys, before anything else sees them: the game's, while a game that uses them is open */
int32_t gm_key(void *brain, void *ev)
{
    int32_t key = *(int32_t *)((uint8_t *)ev + 12), fl = *(int32_t *)((uint8_t *)ev + 16);
    (void)brain;
    if (!shown) return 0;
    if (key >= K_UP && key <= K_RIGHT) {             /* the arrows: the stock master view would take them first */
        if (g_cur < 0) {                              /* the menu: UP / DOWN scroll; LEFT / RIGHT change pages */
            if (key == K_UP || key == K_DOWN) {
                if ((fl & F_DOWN) && !(fl & F_RELEASE)) { g_sel = ui_clamp(g_sel + (key == K_UP ? -1 : 1), 0, NGAMES - 1); if (core_mview) fw_invalidate(core_mview); }
                return 1;
            }
            return 0;
        }
        return games[g_cur].key(core_mview, ev);     /* a game's own: it hands back the page arrows it doesn't use */
    }
    if (key < K_TRIG1 || key > K_TRIG1 + 15) return 0;
    if (g_cur < 0) {                               /* the menu: a trig key opens its game */
        int32_t i = key - K_TRIG1;
        if ((fl & F_DOWN) && !(fl & (F_RELEASE | F_REPEAT)) && i < NGAMES) { g_sel = i; gm_enter(i); }
        return 1;
    }
    if (!games[g_cur].trig) return 0;
    if ((fl & F_DOWN) && !(fl & (F_RELEASE | F_REPEAT))) games[g_cur].trig(key - K_TRIG1);
    else if ((!(fl & F_DOWN) || (fl & F_RELEASE)) && games[g_cur].trig_up) games[g_cur].trig_up(key - K_TRIG1);
    return 1;
}

void gm_tick(void *ctrl)
{
    static uint32_t draws, drawn_at, now;
    (void)ctrl;
    now = ++g_ticks;
    if (!loaded && now > 45) { loaded = 1; load(); }
    if (save_wait > 0 && --save_wait == 0) save();
    sound_tick();
    r_tick(); s_tick(); b_tick();
    int32_t step_now = snk_step_req; snk_step_req = 0;
    snk_tick(step_now);
    png_tick();
    GAMES2_TICK(step_now);
    GAMES3_TICK(step_now);
    GAMES4_TICK(step_now);
    GAMES5_TICK();
    if (core_mview && core_mview_draws != draws) { draws = core_mview_draws; drawn_at = now; }
    int32_t on = core_mview && now - drawn_at < 10 && core_mpage_shown(core_mview) == &gm_page;
    if (!on && shown) { GAMES2_LEAVE(); GAMES3_LEAVE(); GAMES4_LEAVE(); GAMES5_LEAVE(); }             /* left the page: the games that pause, pause */
    if (!on && shown && b_state == G_RUN) { b_state = G_PAUSE; b_left = b_right = 0; }
    shown = on;
    leds_update(on);
    if (on) fw_invalidate(core_mview);
}
