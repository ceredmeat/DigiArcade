/* SPDX-License-Identifier: GPL-2.0-or-later */
/* DIGIARCADE, the third batch (included after games2.h): the ear trainer, beat match, lunar lander, Tron
 * and Minesweeper. */

/* ==== EAR: the ear trainer ===================================================================================== */
/* INT: two notes on the active track, one after the other; name the gap on the trig keys (key 1 the same
 * note, key 2 a semitone ... key 13 an octave). TRK: one of tracks 1-8 plays; which? (keys 1-8).
 * Ten questions a round; YES plays the question again. */
static const char *const e_int[13] = { "UNISON", "MINOR 2ND", "MAJOR 2ND", "MINOR 3RD", "MAJOR 3RD", "4TH", "TRITONE",
                                       "5TH", "MINOR 6TH", "MAJOR 6TH", "MINOR 7TH", "MAJOR 7TH", "OCTAVE" };
static int32_t e_state = G_IDLE, e_q, e_right, e_answer, e_root, e_said = -1, e_play_step, e_streak;
static uint32_t e_at;
static void e_play(void) { e_play_step = 1; e_at = blks; }
static void e_new(void)
{
    e_said = -1;
    e_answer = ear_mode ? (int32_t)rnd(8) : (int32_t)rnd(13);
    e_root = 48 + (int32_t)rnd(13);
    e_play();
}
static void e_start(void) { e_q = 0; e_right = 0; e_streak = 0; e_state = G_RUN; e_new(); }
static void ear_tick(void)
{
    if (e_play_step && (int32_t)(blks - e_at) >= 0) {
        if (ear_mode) { pad_sound(e_answer, 60, 110, 500); e_play_step = 0; }
        else if (e_play_step == 1) { pad_sound(active(), e_root, 110, 600); e_play_step = 2; e_at = blks + 750; }
        else { pad_sound(active(), e_root + e_answer, 110, 600); e_play_step = 0; }
    }
    if (e_state == G_PAUSE && (int32_t)(blks - e_at) >= 0) {           /* the answer shown: on to the next */
        if (++e_q >= 10) { e_state = G_OVER; new_hi(e_right, &ear_hi); } else { e_state = G_RUN; e_new(); }
    }
}
static void ear_trig(int32_t i)
{
    if (e_state != G_RUN || e_play_step) { if (e_state == G_IDLE || e_state == G_OVER) e_start(); return; }
    if (i > (ear_mode ? 7 : 12)) return;
    e_said = i;
    if (i == e_answer) { e_right++; e_streak++; } else e_streak = 0;
    e_state = G_PAUSE; e_at = blks + 1800;
}
static void ear_draw(void *view, void *bmp)
{
    char s[28], *p;
    (void)view;
    fw_fillrect(bmp, 0, 0, 127, 52, 0);
    ui_text(bmp, 2, 46, ear_mode ? "WHICH TRACK" : "WHICH INTERVAL");
    p = put(s, "Q "); p = ui_num(p, e_q + (e_state == G_OVER ? 0 : 1)); p = put(p, "/10  RIGHT "); ui_num(p, e_right);
    ui_text(bmp, 2, 38, s);
    if (e_state == G_PAUSE) {
        int32_t ok = e_said == e_answer;
        box_msg(bmp, 22, ok ? "RIGHT" : "NO");
        p = put(s, ok ? "" : "IT WAS ");
        if (ear_mode) { p = put(p, "TRACK "); *p++ = (char)('1' + e_answer); *p = 0; } else put(p, e_int[e_answer]);
        ui_centred(bmp, 0, 128, 12, s);
    } else if (e_state == G_RUN) {
        ui_centred(bmp, 0, 128, 24, e_play_step ? "LISTEN" : ear_mode ? "PRESS TRIG 1-8" : "PRESS TRIG 1-13");
        ui_centred(bmp, 0, 128, 14, "YES: HEAR IT AGAIN");
        if (!ear_mode) ui_text(bmp, 2, 3, "1 SAME  2 SEMITONE ... 13 OCTAVE");
    } else {
        box_msg(bmp, 22, e_state == G_OVER ? "ROUND OVER" : "YES: START");
        p = put(s, "BEST "); p = ui_num(p, ear_hi); put(p, "/10"); ui_text(bmp, 2, 3, s);
        ui_text(bmp, 56, 3, ear_mode ? "B: TRACKS" : "B: INTERVALS");
    }
}
static int32_t ear_key(void *view, void *event)
{
    int32_t key = g_keyid(event), press = g_press(event);
    if (key == K_YES) { if (press) { if (e_state == G_RUN) e_play(); else if (e_state != G_PAUSE) e_start(); } return 1; }
    return mp_key(view, event);
}
static int32_t ear_enc(void *view, void *event)
{
    int32_t knob = *(int32_t *)((char *)event + 12);
    if (knob == 2 && e_state != G_RUN && e_state != G_PAUSE) { int32_t d = detents(*(int32_t *)((char *)event + 16)); if (d) { ear_mode = d > 0; save_wait = 60; } fw_invalidate(view); return 1; }
    return 0;
}
static int32_t ear_busy(void) { return e_state == G_RUN || e_state == G_PAUSE; }

/* ==== BPM: beat match ========================================================================================== */
/* A hidden tempo plays (on the knob D track, or flashes if that's OFF); tap along on YES or any trig key.
 * After 8 taps your tempo is compared: 100 points dead on, 10 fewer for each BPM off. Five rounds. */
static int32_t m_state = G_IDLE, m_bpm10, m_round, m_total, m_got10, m_pts, m_taps;
static uint32_t m_next, m_beat_at, m_tap[8];
static void m_new(void) { m_bpm10 = 800 + (int32_t)rnd(800); m_taps = 0; m_next = blks + 300; m_state = G_RUN; }
static void m_start(void) { m_round = 0; m_total = 0; m_new(); }
static uint32_t m_period(void) { return (uint32_t)(900000 / m_bpm10); }     /* blocks a beat */
static void bpm_tick(void)
{
    if (m_state == G_RUN && (int32_t)(blks - m_next) >= 0) {
        m_beat_at = blks; m_next += m_period();
        g_snd(60, 110, 60);
    }
    if (m_state == G_PAUSE && (int32_t)(blks - m_next) >= 0) {
        if (++m_round >= 5) { m_state = G_OVER; new_hi(m_total, &bpm_hi); } else m_new();
    }
}
static void m_tap_now(void)
{
    if (m_state != G_RUN) { if (m_state == G_IDLE || m_state == G_OVER) m_start(); return; }
    m_tap[m_taps & 7] = blks;
    if (++m_taps >= 8) {
        uint32_t span = m_tap[7] - m_tap[0];
        m_got10 = span ? (int32_t)(900000u * 7 / span) : 0;
        int32_t off = iabs(m_got10 - m_bpm10);
        m_pts = 100 - off;                        /* 10 points a BPM: off is in tenths */
        if (m_pts < 0) m_pts = 0;
        m_total += m_pts;
        m_state = G_PAUSE; m_next = blks + 2400;
    }
}
static void bpm10_text(char *p, int32_t v) { p = ui_num(p, v / 10); *p++ = '.'; ui_num(p, v % 10); }
static void bpm_draw(void *view, void *bmp)
{
    char s[28], *p;
    (void)view;
    fw_fillrect(bmp, 0, 0, 127, 52, 0);
    p = put(s, "ROUND "); p = ui_num(p, m_round + (m_state == G_OVER ? 0 : 1)); p = put(p, "/5  TOTAL "); ui_num(p, m_total);
    ui_text(bmp, 2, 46, s);
    if (m_state == G_RUN) {
        int32_t lit = brk_snd == 0 && blks - m_beat_at < 150;      /* no sound: watch the flash */
        if (lit) fw_fillrect(bmp, 54, 18, 74, 38, 1); else fw_framerect(bmp, 54, 18, 74, 38, 1);
        for (int32_t k = 0; k < 8; k++) if (k < m_taps) fw_fillrect(bmp, 34 + k * 8, 8, 38 + k * 8, 12, 1); else fw_framerect(bmp, 34 + k * 8, 8, 38 + k * 8, 12, 1);
        ui_centred(bmp, 0, 128, 0, "TAP ALONG: YES OR TRIGS");
    } else if (m_state == G_PAUSE) {
        p = put(s, "IT WAS "); bpm10_text(p, m_bpm10); ui_centred(bmp, 0, 128, 32, s);
        p = put(s, "YOU TAPPED "); bpm10_text(p, m_got10); ui_centred(bmp, 0, 128, 24, s);
        p = put(s, "+"); p = ui_num(p, m_pts); put(p, " POINTS"); ui_centred(bmp, 0, 128, 14, s);
    } else {
        box_msg(bmp, 24, m_state == G_OVER ? "GAME OVER" : "YES: START");
        p = put(s, "BEST "); ui_num(p, bpm_hi); ui_text(bmp, 2, 3, s);
        snd_text(bmp, 100, 3);
    }
}
static int32_t bpm_key(void *view, void *event)
{
    int32_t key = g_keyid(event), press = g_press(event);
    if (key == K_YES) { if (press) m_tap_now(); return 1; }
    return mp_key(view, event);
}
static void bpm_trig(int32_t i) { (void)i; m_tap_now(); }
static int32_t bpm_busy(void) { return m_state == G_RUN || m_state == G_PAUSE; }

/* ==== LND: lunar lander ======================================================================================== */
/* YES fires the main engine (hold it), LEFT / RIGHT the side ones. Land on the flat pad, slowly. */
static uint8_t d_ground[33];                     /* the ground's height every 4 pixels */
static int32_t d_state = G_IDLE, d_x, d_y, d_vx, d_vy, d_fuel, d_pad, d_level, d_score, d_up, d_left, d_right, d_crash;
static uint32_t d_last;
static void d_level_start(void)
{
    d_pad = 2 + (int32_t)rnd(26);                 /* the pad: points d_pad .. d_pad + 4 */
    int32_t h = 6 + (int32_t)rnd(10);
    for (int32_t i = 0; i < 33; i++) {
        if (i > d_pad && i <= d_pad + 4) { d_ground[i] = d_ground[d_pad]; continue; }
        h += (int32_t)rnd(9) - 4; h = ui_clamp(h, 2, 26);
        d_ground[i] = (uint8_t)h;
    }
    d_x = (8 + (int32_t)rnd(110)) << 8; d_y = 46 << 8; d_vx = (int32_t)rnd(160) - 80; d_vy = 0;
    d_fuel = 900 - d_level * 60; if (d_fuel < 300) d_fuel = 300;
    d_state = G_RUN; d_last = blks; d_crash = 0;
}
static void d_start(void) { d_level = 0; d_score = 0; d_level_start(); }
static int32_t d_ground_at(int32_t x)             /* the ground's height under pixel x */
{
    int32_t i = ui_clamp(x / 4, 0, 31), f = x - i * 4;
    return d_ground[i] + (d_ground[i + 1] - d_ground[i]) * f / 4;
}
static void d_frame(void)
{
    int32_t grav = 3 + d_level;
    d_vy -= grav;
    if (d_fuel > 0) {
        if (d_up) { d_vy += 9; d_fuel -= 3; }
        if (d_left) { d_vx -= 4; d_fuel--; }
        if (d_right) { d_vx += 4; d_fuel--; }
    }
    d_x += d_vx; d_y += d_vy;
    if (d_x < 0) { d_x = 0; d_vx = 0; }
    if (d_x > 122 << 8) { d_x = 122 << 8; d_vx = 0; }
    int32_t x = d_x >> 8, y = d_y >> 8, gl = d_ground_at(x), gr = d_ground_at(x + 5);
    if (y <= (gl > gr ? gl : gr)) {               /* down: a landing or a crash */
        int32_t on_pad = x >= d_pad * 4 && x + 5 <= (d_pad + 4) * 4;
        if (on_pad && d_vy > -90 && iabs(d_vx) < 60) {
            d_score += 100 + d_fuel / 5; d_level++; g_snd(72, 110, 300); d_state = G_PAUSE; d_last = blks + 1500;
        } else { d_crash = 1; d_state = G_OVER; new_hi(d_score, &lnd_hi); g_snd(30, 127, 400); }
    }
}
static void lnd_tick(void)
{
    if (d_state == G_PAUSE && !d_crash && (int32_t)(blks - d_last) >= 0) { d_level_start(); return; }
    if (d_state != G_RUN) return;
    int32_t n = frames(&d_last);
    while (n-- > 0 && d_state == G_RUN) d_frame();
}
static void lnd_draw(void *view, void *bmp)
{
    char s[24], *p;
    (void)view;
    fw_fillrect(bmp, 0, 0, 127, 52, 0);
    for (int32_t i = 0; i < 32; i++) ui_line(bmp, i * 4, d_ground[i], i * 4 + 4, d_ground[i + 1]);
    fw_fillrect(bmp, d_pad * 4, d_ground[d_pad] - 1, (d_pad + 4) * 4, d_ground[d_pad], 1);   /* the pad */
    int32_t x = d_x >> 8, y = d_y >> 8;
    if (d_crash) { ui_line(bmp, x - 2, y, x + 7, y + 6); ui_line(bmp, x - 2, y + 6, x + 7, y); }
    else {
        fw_framerect(bmp, x + 1, y + 2, x + 4, y + 5, 1);                 /* the lander, its legs */
        fw_fillrect(bmp, x, y, x, y + 1, 1); fw_fillrect(bmp, x + 5, y, x + 5, y + 1, 1);
        if (d_up && d_fuel > 0 && d_state == G_RUN && (g_ticks & 2)) fw_fillrect(bmp, x + 2, y - 2, x + 3, y, 1);
    }
    p = put(s, "FUEL "); ui_num(p, d_fuel); ui_text(bmp, 2, 47, s);
    p = put(s, "SCORE "); ui_num(p, d_score); ui_text(bmp, 70, 47, s);
    int32_t sp = -d_vy * 10 / 256;                                        /* falling speed, a tenth of a pixel a frame */
    p = put(s, "DOWN "); ui_num(p, sp > 0 ? sp : 0); ui_text(bmp, 2, 40, s);
    if (d_state == G_PAUSE && !d_crash) box_msg(bmp, 30, "LANDED");
    else if (d_state == G_OVER) box_msg(bmp, 30, "CRASHED  YES: AGAIN");
    else if (d_state == G_IDLE) { box_msg(bmp, 30, "YES: START"); ui_text(bmp, 30, 22, "HOLD YES: ENGINE"); ui_text(bmp, 30, 15, "LEFT RIGHT: STEER"); }
}
static int32_t lnd_key(void *view, void *event)
{
    int32_t key = g_keyid(event), press = g_press(event), down = g_down(event);
    if (key == K_YES) {
        if (press && (d_state == G_IDLE || d_state == G_OVER)) d_start();
        d_up = down && d_state == G_RUN;
        return 1;
    }
    if (d_state == G_RUN && (key == K_LEFT || key == K_RIGHT)) { if (key == K_LEFT) d_left = down; else d_right = down; return 1; }
    return mp_key(view, event);
}
static int32_t lnd_busy(void) { return d_state == G_RUN; }

/* ==== TRN: Tron light cycles ==================================================================================== */
/* 64 x 23 cells of 2 pixels. Player 1 steers with the arrows (or trig keys 1-4: left, down, up, right);
 * against the Digitakt, or with knob B on 2P, player 2 with trig keys 13-16. The cycles move on the active
 * track's steps while the pattern plays, on their own when stopped. First to 5. */
#define CW 64
#define CH 23
static uint8_t c_grid[CW * CH / 8];
static int32_t c_state = G_IDLE, c_x[2], c_y[2], c_dir[2], c_next[2], c_s[2], c_wait, c_dead[2];
static uint32_t c_last;
static int32_t c_get(int32_t x, int32_t y) { if (x < 0 || x >= CW || y < 0 || y >= CH) return 1; int32_t i = y * CW + x; return (c_grid[i >> 3] >> (i & 7)) & 1; }
static void c_set(int32_t x, int32_t y) { int32_t i = y * CW + x; c_grid[i >> 3] |= (uint8_t)(1u << (i & 7)); }
static void c_round(void)
{
    for (int32_t i = 0; i < CW * CH / 8; i++) c_grid[i] = 0;
    c_x[0] = 10; c_y[0] = CH / 2; c_dir[0] = c_next[0] = 0;
    c_x[1] = CW - 11; c_y[1] = CH / 2; c_dir[1] = c_next[1] = 2;
    c_set(c_x[0], c_y[0]); c_set(c_x[1], c_y[1]);
    c_dead[0] = c_dead[1] = 0; c_state = G_RUN; c_last = blks;
}
static void c_start(void) { c_s[0] = c_s[1] = 0; c_round(); }
static int32_t c_free_run(int32_t x, int32_t y, int32_t d)        /* how far it's clear that way */
{
    int32_t n = 0;
    while (n < 20) { x += ddx[d]; y += ddy[d]; if (c_get(x, y)) break; n++; }
    return n;
}
static void c_step(void)
{
    if (!p_two) {                                 /* the Digitakt: keep going, turn toward the most room */
        int32_t d = c_dir[1], best = d, room = c_free_run(c_x[1], c_y[1], d);
        if (room < 4 || rnd(14) == 0)
            for (int32_t t = 1; t <= 3; t += 2) { int32_t nd = (d + t) & 3, r = c_free_run(c_x[1], c_y[1], nd); if (r > room) { room = r; best = nd; } }
        c_next[1] = best;
    }
    for (int32_t p = 0; p < 2; p++) {
        if ((c_next[p] ^ c_dir[p]) != 2) c_dir[p] = c_next[p];
        c_x[p] += ddx[c_dir[p]]; c_y[p] += ddy[c_dir[p]];
    }
    for (int32_t p = 0; p < 2; p++) c_dead[p] = c_get(c_x[p], c_y[p]) || (c_x[0] == c_x[1] && c_y[0] == c_y[1]);
    for (int32_t p = 0; p < 2; p++) if (!c_dead[p]) c_set(c_x[p], c_y[p]);
    if (c_dead[0] || c_dead[1]) {
        if (!c_dead[0]) c_s[0]++;
        if (!c_dead[1]) c_s[1]++;
        g_snd(c_dead[0] ? 36 : 72, 120, 300);
        if (c_s[0] >= 5 || c_s[1] >= 5) { c_state = G_OVER; if (!p_two && c_s[0] >= 5) { trn_hi++; save_wait = 60; } }
        else { c_state = G_PAUSE; c_last = blks + 1500; }
    }
}
static void trn_tick(int32_t step_now)
{
    if (c_state == G_PAUSE && (int32_t)(blks - c_last) >= 0) { c_round(); return; }
    if (c_state != G_RUN) return;
    int32_t playing = blks - r_last_any < SEC;
    if (playing) { if (step_now) c_step(); c_last = blks; return; }
    int32_t n = frames(&c_last);
    while (n-- > 0 && c_state == G_RUN) if (++c_wait >= 5) { c_wait = 0; c_step(); }
}
static void trn_draw(void *view, void *bmp)
{
    char s[24], *p;
    (void)view;
    fw_fillrect(bmp, 0, 0, 127, 52, 0);
    for (int32_t y = 0; y < CH; y++)
        for (int32_t x = 0; x < CW; x++)
            if (c_get(x, y)) fw_fillrect(bmp, x * 2, y * 2, x * 2 + 1, y * 2 + 1, 1);
    for (int32_t pl = 0; pl < 2; pl++) if (c_state != G_IDLE) fw_framerect(bmp, c_x[pl] * 2 - 1, c_y[pl] * 2 - 1, c_x[pl] * 2 + 2, c_y[pl] * 2 + 2, 1);
    p = put(s, p_two ? "P1 " : "YOU "); p = ui_num(p, c_s[0]); p = put(p, p_two ? "  P2 " : "  DIGITAKT "); ui_num(p, c_s[1]);
    ui_text(bmp, 2, 47, s);
    if (c_state == G_OVER) box_msg(bmp, 24, c_s[0] > c_s[1] ? (p_two ? "P1 WINS" : "YOU WIN") : (p_two ? "P2 WINS" : "DIGITAKT WINS"));
    else if (c_state == G_IDLE) {
        box_msg(bmp, 30, "YES: START");
        ui_centred(bmp, 0, 128, 20, "P1: ARROWS OR TRIGS 1-4");
        ui_centred(bmp, 0, 128, 12, p_two ? "P2: TRIGS 13-16  B: 2P" : "B: 1P");
    }
}
static int32_t trn_key(void *view, void *event)
{
    int32_t key = g_keyid(event), press = g_press(event);
    if (key == K_YES) { if (press && c_state != G_RUN && c_state != G_PAUSE) c_start(); return 1; }
    if (c_state == G_RUN && key >= K_UP && key <= K_RIGHT) {
        static const int8_t dir_of[4] = { 1, 3, 2, 0 };     /* UP, DOWN, LEFT, RIGHT */
        if (press) c_next[0] = dir_of[key - K_UP];
        return 1;
    }
    return mp_key(view, event);
}
static void trn_trig(int32_t i)
{
    static const int8_t dir_of[4] = { 2, 3, 1, 0 };         /* left, down, up, right */
    if (c_state == G_IDLE || c_state == G_OVER) { c_start(); return; }
    if (i < 4) c_next[0] = dir_of[i];
    else if (i >= 12 && p_two) c_next[1] = dir_of[i - 12];
}
static int32_t trn_enc(void *view, void *event)
{
    int32_t knob = *(int32_t *)((char *)event + 12);
    if (knob == 2 && c_state != G_RUN) { int32_t d = detents(*(int32_t *)((char *)event + 16)); if (d) { p_two = d > 0; save_wait = 60; } fw_invalidate(view); return 1; }
    return snd_knob(view, event);
}
static int32_t trn_busy(void) { return c_state == G_RUN || c_state == G_PAUSE; }

/* ==== MIN: Minesweeper ========================================================================================== */
/* 16 x 5 squares: a trig key picks the column, UP / DOWN the row, YES opens, NO flags. Knob B gives up. */
#define MW 16
#define MH 5
#define MINES 12
static uint8_t n_cell[MW * MH];                   /* bits: 0-3 mines around, 4 a mine, 5 open, 6 flagged */
static int32_t n_state = G_IDLE, n_cx, n_cy = 2, n_placed, n_open, n_flags;
static uint32_t n_t0, n_t1;
static void n_start(void) { for (int32_t i = 0; i < MW * MH; i++) n_cell[i] = 0; n_placed = 0; n_open = 0; n_flags = 0; n_state = G_RUN; n_t0 = blks; }
static void n_place(int32_t safe)                 /* after the first square: never a mine there or beside it */
{
    int32_t sx = safe % MW, sy = safe / MW, k = 0;
    while (k < MINES) {
        int32_t i = (int32_t)rnd(MW * MH), x = i % MW, y = i / MW;
        if ((n_cell[i] & 16) || (iabs(x - sx) <= 1 && iabs(y - sy) <= 1)) continue;
        n_cell[i] |= 16; k++;
    }
    for (int32_t i = 0; i < MW * MH; i++) {
        int32_t x = i % MW, y = i / MW, n = 0;
        for (int32_t dy = -1; dy <= 1; dy++) for (int32_t dx = -1; dx <= 1; dx++) {
            int32_t xx = x + dx, yy = y + dy;
            if ((dx || dy) && xx >= 0 && xx < MW && yy >= 0 && yy < MH && (n_cell[yy * MW + xx] & 16)) n++;
        }
        n_cell[i] = (uint8_t)((n_cell[i] & 0xf0) | n);
    }
    n_placed = 1;
}
static void n_reveal(int32_t start)
{
    static uint8_t stack[MW * MH];
    int32_t sp = 0;
    stack[sp++] = (uint8_t)start;
    while (sp > 0) {
        int32_t i = stack[--sp];
        if (n_cell[i] & (32 | 64)) continue;
        n_cell[i] |= 32; n_open++;
        if (n_cell[i] & 15) continue;             /* a zero opens its neighbours */
        int32_t x = i % MW, y = i / MW;
        for (int32_t dy = -1; dy <= 1; dy++) for (int32_t dx = -1; dx <= 1; dx++) {
            int32_t xx = x + dx, yy = y + dy;
            if (xx >= 0 && xx < MW && yy >= 0 && yy < MH && !(n_cell[yy * MW + xx] & 32) && sp < MW * MH) stack[sp++] = (uint8_t)(yy * MW + xx);
        }
    }
}
static void n_open_here(void)
{
    int32_t i = n_cy * MW + n_cx;
    if (n_cell[i] & (32 | 64)) return;
    if (!n_placed) n_place(i);
    if (n_cell[i] & 16) {                         /* a mine */
        for (int32_t k = 0; k < MW * MH; k++) if (n_cell[k] & 16) n_cell[k] |= 32;
        n_state = G_OVER; n_t1 = blks; g_snd(30, 127, 400); return;
    }
    n_reveal(i);
    g_snd(60 + (n_cell[i] & 15) * 2, 90, 60);
    if (n_open == MW * MH - MINES) {              /* cleared: the time counts if it's the best */
        n_state = G_PAUSE; n_t1 = blks;
        int32_t secs = (int32_t)((n_t1 - n_t0) / SEC);
        if (secs < 1) secs = 1;
        if (!min_hi || secs < min_hi) { min_hi = secs; save_wait = 60; }
        g_snd(84, 120, 400);
    }
}
static void min_draw(void *view, void *bmp)
{
    char s[24], *p;
    (void)view;
    fw_fillrect(bmp, 0, 0, 127, 52, 0);
    for (int32_t y = 0; y < MH; y++)
        for (int32_t x = 0; x < MW; x++) {
            int32_t c = n_cell[y * MW + x], px = x * 8, py = 38 - y * 8;
            if (c & 32) {
                if (c & 16) { fw_fillrect(bmp, px + 2, py + 2, px + 5, py + 5, 1); }
                else if (c & 15) { char d[2] = { (char)('0' + (c & 15)), 0 }; ui_text(bmp, px + 2, py + 1, d); }
            } else {
                fw_framerect(bmp, px, py, px + 6, py + 6, 1);
                if (c & 64) fw_fillrect(bmp, px + 2, py + 2, px + 4, py + 4, 1);
            }
            if (x == n_cx && y == n_cy && n_state == G_RUN) fw_fillrect(bmp, px - 1, py - 1, px + 7, py + 7, -1);
        }
    int32_t flags = 0;
    for (int32_t i = 0; i < MW * MH; i++) if (n_cell[i] & 64) flags++;
    uint32_t t = (n_state == G_RUN ? blks : n_t1) - n_t0;
    p = put(s, "MINES "); ui_num(p, MINES - flags); ui_text(bmp, 2, 46, s);
    p = put(s, "TIME "); ui_num(p, n_state == G_IDLE ? 0 : (int32_t)(t / SEC)); ui_text(bmp, 50, 46, s);
    p = put(s, "BEST "); ui_num(p, min_hi); ui_text(bmp, 92, 46, s);
    if (n_state == G_IDLE) { box_msg(bmp, 20, "YES: START"); ui_centred(bmp, 0, 128, 2, "ARROWS OR TRIGS: MOVE"); }
    else if (n_state == G_OVER) box_msg(bmp, 20, "BOOM");
    else if (n_state == G_PAUSE) box_msg(bmp, 20, "CLEARED");
    else ui_centred(bmp, 0, 128, 0, "YES: OPEN  NO: FLAG  B: GIVE UP");
}
static int32_t min_key(void *view, void *event)
{
    int32_t key = g_keyid(event), press = g_press(event), down = g_down(event);
    if (key == K_YES) { if (press) { if (n_state == G_RUN) n_open_here(); else n_start(); } return 1; }
    if (n_state == G_RUN) {
        if (key == K_NO) { if (press && !(n_cell[n_cy * MW + n_cx] & 32)) n_cell[n_cy * MW + n_cx] ^= 64; return 1; }
        if (key == K_UP || key == K_DOWN) { if (down) n_cy = ui_clamp(n_cy + (key == K_UP ? -1 : 1), 0, MH - 1); return 1; }
        if (key == K_LEFT || key == K_RIGHT) { if (down) n_cx = ui_clamp(n_cx + (key == K_LEFT ? -1 : 1), 0, MW - 1); return 1; }
    }
    return mp_key(view, event);
}
static void min_trig(int32_t i) { if (n_state == G_RUN) n_cx = i; else if (n_state != G_RUN) n_start(); }
static int32_t min_enc(void *view, void *event)
{
    int32_t knob = *(int32_t *)((char *)event + 12);
    if (knob == 2 && n_state == G_RUN && detents(*(int32_t *)((char *)event + 16))) { n_state = G_IDLE; fw_invalidate(view); return 1; }
    return snd_knob(view, event);
}
static int32_t min_busy(void) { return n_state == G_RUN; }

#define GAMES3 \
    { "EAR TRAINER", ear_draw, ear_key, ear_enc, ear_trig, ear_busy, &ear_hi, 0 }, \
    { "BEAT MATCH", bpm_draw, bpm_key, snd_knob, bpm_trig, bpm_busy, &bpm_hi, 0 }, \
    { "LUNAR LANDER", lnd_draw, lnd_key, snd_knob, 0, lnd_busy, &lnd_hi, 0 }, \
    { "TRON", trn_draw, trn_key, trn_enc, trn_trig, trn_busy, &trn_hi, 0 }, \
    { "MINESWEEPER", min_draw, min_key, min_enc, min_trig, min_busy, &min_hi, 0 },
#define GAMES3_TICK(step) do { ear_tick(); bpm_tick(); lnd_tick(); trn_tick(step); } while (0)
#define GAMES3_LEAVE() do { if (d_state == G_RUN) { d_state = G_IDLE; d_up = d_left = d_right = 0; } } while (0)
