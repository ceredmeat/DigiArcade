/* SPDX-License-Identifier: GPL-2.0-or-later */
/* GAMES, the second batch (included by games.c, after the first): Tetris, Flappy, Space Invaders,
 * Whack-a-mole and Lights Out. Knob D picks the track their sounds play on (OFF, 1-8), shared with
 * Breakout's. Each runs in frames of a 60th of a second (25 audio blocks). */


/* ---- shared ------------------------------------------------------------------------------------------- */
static void g_snd(int32_t note, int32_t vel, int32_t len) { if (brk_snd) pad_sound(brk_snd - 1, note, vel, len); }
static int32_t frames(uint32_t *last)            /* the 60ths of a second since last time (at most 8) */
{
    uint32_t now = blks;
    int32_t n = 0;
    while ((int32_t)(now - *last) >= 25 && n < 8) { *last += 25; n++; }
    if (n == 8) *last = now;
    return n;
}
static void box_msg(void *bmp, int32_t y, const char *m)
{
    int32_t w = 4 * ui_len(m) + 3, x = 64 - w / 2;
    fw_fillrect(bmp, x, y, x + w, y + 8, 0); fw_framerect(bmp, x, y, x + w, y + 8, 1); ui_text(bmp, x + 2, y + 2, m);
}
static int32_t g_press(void *event) { int32_t fl = *(int32_t *)((char *)event + 16); return (fl & F_DOWN) && !(fl & (F_RELEASE | F_REPEAT)); }
static int32_t g_down(void *event) { int32_t fl = *(int32_t *)((char *)event + 16); return (fl & F_DOWN) && !(fl & F_RELEASE); }
static int32_t g_keyid(void *event) { return *(int32_t *)((char *)event + 12); }
static void new_hi(int32_t score, int32_t *hi) { if (score > *hi) { *hi = score; save_wait = 60; } }
/* knob D, the sound, on any of these games; B their own switch (between games) */
static int32_t snd_knob(void *view, void *event)
{
    int32_t knob = *(int32_t *)((char *)event + 12), turn = *(int32_t *)((char *)event + 16);
    if (knob != 4) return 0;
    brk_snd = ui_clamp(brk_snd + detents(turn), 0, 8); save_wait = 60; fw_invalidate(view);
    return 1;
}
static void snd_text(void *bmp, int32_t x, int32_t y)
{
    char s[12], *p = put(s, "D:");
    if (brk_snd) { *p++ = 'T'; *p++ = (char)('0' + brk_snd); *p = 0; } else put(p, "OFF");
    ui_text(bmp, x, y, s);
}

/* ==== TET: Tetris ========================================================================================== */
#define TW 10
#define TH 16
static uint16_t tb[TH + 4];                      /* a bit a column, row 0 the bottom */
static const uint16_t t_shape[7] = {             /* 4 x 4, bit y * 4 + x, y up: I O T S Z J L */
    0x00f0, 0x0066, 0x0072, 0x0036, 0x0063, 0x0071, 0x0074 };
static int32_t t_state = G_IDLE, t_type, t_rot, t_x, t_y, t_next, t_score, t_lines, t_level, t_flash;
static uint32_t t_last, t_fall;
static uint16_t t_mask(int32_t type, int32_t rot)
{
    uint16_t m = t_shape[type];
    if (type == 1) return m;                     /* O doesn't turn */
    for (int32_t r = 0; r < (rot & 3); r++) {    /* a quarter turn: (x, y) -> (y, 3 - x), in a 3 x 3 for all but I */
        uint16_t o = 0;
        int32_t n = type == 0 ? 4 : 3;
        for (int32_t y = 0; y < 4; y++) for (int32_t x = 0; x < 4; x++)
            if ((m >> (y * 4 + x)) & 1) { int32_t nx = y, ny = n - 1 - x; if (ny >= 0) o |= (uint16_t)(1u << (ny * 4 + nx)); }
        m = o;
    }
    return m;
}
static int32_t t_fits(int32_t type, int32_t rot, int32_t x, int32_t y)
{
    uint16_t m = t_mask(type, rot);
    for (int32_t i = 0; i < 16; i++) {
        if (!((m >> i) & 1)) continue;
        int32_t bx = x + (i & 3), by = y + (i >> 2);
        if (bx < 0 || bx >= TW || by < 0) return 0;
        if (by < TH + 4 && ((tb[by] >> bx) & 1)) return 0;
    }
    return 1;
}
static void t_spawn(void)
{
    t_type = t_next; t_next = (int32_t)rnd(7); t_rot = 0; t_x = 3; t_y = TH - 2;
    if (!t_fits(t_type, t_rot, t_x, t_y)) { t_state = G_OVER; new_hi(t_score, &tet_hi); }
}
static void t_start(void)
{
    for (int32_t i = 0; i < TH + 4; i++) tb[i] = 0;
    t_score = t_lines = t_level = 0; t_next = (int32_t)rnd(7);
    t_spawn(); t_state = G_RUN; t_last = t_fall = blks;
}
static void t_lock(void)
{
    uint16_t m = t_mask(t_type, t_rot);
    for (int32_t i = 0; i < 16; i++)
        if ((m >> i) & 1) { int32_t by = t_y + (i >> 2); if (by < TH + 4) tb[by] |= (uint16_t)(1u << (t_x + (i & 3))); }
    int32_t n = 0;
    for (int32_t y = 0; y < TH + 4;) {
        if (tb[y] == (1u << TW) - 1) { for (int32_t k = y; k < TH + 3; k++) tb[k] = tb[k + 1]; tb[TH + 3] = 0; n++; }
        else y++;
    }
    static const uint16_t pts[5] = { 0, 40, 100, 300, 1200 };
    if (n) { t_score += pts[n] * (t_level + 1); t_lines += n; t_level = t_lines / 10; t_flash = 8; g_snd(60 + n * 4, 120, 200); }
    else g_snd(48, 80, 60);
    for (int32_t x = 0; x < TW; x++) if (tb[TH] & (1u << x)) { t_state = G_OVER; new_hi(t_score, &tet_hi); return; }
    t_spawn();
}
static void t_drop(void) { if (t_fits(t_type, t_rot, t_x, t_y - 1)) t_y--; else t_lock(); }
static void tet_tick(int32_t step_now)
{
    if (t_state != G_RUN) { t_last = t_fall = blks; return; }
    frames(&t_last);
    if (t_flash > 0) t_flash--;
    int32_t playing = blks - r_last_any < SEC;
    if (tet_sync && playing) { if (step_now) t_drop(); t_fall = blks; return; }    /* on the active track's steps */
    int32_t every = 750 - t_level * 60;
    if (every < 75) every = 75;
    if ((int32_t)(blks - t_fall) >= every) { t_fall = blks; t_drop(); }
}
static void tet_draw(void *view, void *bmp)
{
    char s[24], *p;
    (void)view;
    fw_fillrect(bmp, 0, 0, 127, 52, 0);
    fw_framerect(bmp, 3, 1, 34, 50, 1);
    uint16_t m = t_state == G_RUN || t_state == G_PAUSE ? t_mask(t_type, t_rot) : 0;
    for (int32_t y = 0; y < TH; y++)
        for (int32_t x = 0; x < TW; x++) {
            int32_t on = (tb[y] >> x) & 1, rx = x - t_x, ry = y - t_y;
            if (rx >= 0 && rx < 4 && ry >= 0 && ry < 4 && ((m >> (ry * 4 + rx)) & 1)) on = 1;
            if (on) fw_fillrect(bmp, 5 + x * 3, 2 + y * 3, 6 + x * 3, 3 + y * 3, 1);
        }
    if (t_flash & 2) fw_fillrect(bmp, 4, 2, 33, 49, -1);
    ui_text(bmp, 42, 45, "NEXT");
    uint16_t nm = t_mask(t_next, 0);
    for (int32_t i = 0; i < 16; i++) if ((nm >> i) & 1) fw_fillrect(bmp, 60 + (i & 3) * 3, 43 + (i >> 2) * 3, 61 + (i & 3) * 3, 44 + (i >> 2) * 3, 1);
    p = put(s, "SCORE "); ui_num(p, t_score); ui_text(bmp, 42, 33, s);
    p = put(s, "LINES "); ui_num(p, t_lines); ui_text(bmp, 42, 26, s);
    p = put(s, "LEVEL "); ui_num(p, t_level); ui_text(bmp, 42, 19, s);
    ui_text(bmp, 42, 11, tet_sync ? "B: FALL ON STEPS" : "B: FALL ON TIME");
    snd_text(bmp, 42, 3);
    p = put(s, "HI "); ui_num(p, tet_hi); ui_text(bmp, 84, 3, s);
    if (t_state != G_RUN) box_msg(bmp, 22, t_state == G_OVER ? "GAME OVER" : t_state == G_PAUSE ? "PAUSED" : "YES: START");
}
static int32_t tet_key(void *view, void *event)
{
    int32_t key = g_keyid(event), press = g_press(event), down = g_down(event);
    if (key == K_YES) {
        if (press) { if (t_state == G_PAUSE) t_state = G_RUN; else if (t_state != G_RUN) t_start(); else { while (t_fits(t_type, t_rot, t_x, t_y - 1)) t_y--; t_lock(); } }
        return 1;
    }
    if (key == K_NO && t_state == G_RUN) { if (press) t_state = G_PAUSE; return 1; }
    if (t_state != G_RUN) return mp_key(view, event);
    if (!down) return 1;                          /* the arrows repeat while held */
    if (key == K_LEFT && t_fits(t_type, t_rot, t_x - 1, t_y)) t_x--;
    if (key == K_RIGHT && t_fits(t_type, t_rot, t_x + 1, t_y)) t_x++;
    if (key == K_DOWN) t_drop();
    if (key == K_UP && press) {                   /* turn, nudged off a wall if it must */
        static const int8_t kick[3] = { 0, -1, 1 };
        for (int32_t k = 0; k < 3; k++) if (t_fits(t_type, t_rot + 1, t_x + kick[k], t_y)) { t_rot = (t_rot + 1) & 3; t_x += kick[k]; break; }
    }
    return 1;
}
static int32_t tet_enc(void *view, void *event)
{
    int32_t knob = *(int32_t *)((char *)event + 12);
    if (knob == 2) { int32_t d = detents(*(int32_t *)((char *)event + 16)); if (d) { tet_sync = d > 0; save_wait = 60; } fw_invalidate(view); return 1; }   /* any time */
    return snd_knob(view, event);
}
static int32_t tet_busy(void) { return t_state == G_RUN; }

/* ==== FLP: Flappy =========================================================================================== */
#define NPIPE 3
static int32_t f_state = G_IDLE, f_y, f_vy, f_score, f_px[NPIPE], f_gap[NPIPE], f_scored[NPIPE];
static uint32_t f_last;
static void f_start(void)
{
    f_y = 26 << 8; f_vy = 0; f_score = 0;
    for (int32_t i = 0; i < NPIPE; i++) { f_px[i] = (128 + i * 48) << 8; f_gap[i] = 12 + (int32_t)rnd(24); f_scored[i] = 0; }
    f_state = G_RUN; f_last = blks;
}
static void f_flap(void)
{
    if (f_state == G_RUN) { f_vy = 300; g_snd(72, 70, 40); }
    else if (f_state != G_PAUSE) f_start();
}
static int32_t f_speed(void)                     /* pixels a frame x 256: steady, or from the tempo */
{
    if (!flp_bpm || blks - r_last_step > SEC) return 256;
    int32_t steplen = (int32_t)(r_len_q8[0] >> 8);           /* 187.5 blocks a step at 120 BPM */
    return ui_clamp(256 * 188 / (steplen > 0 ? steplen : 188), 128, 640);
}
static void f_frame(void)
{
    f_vy -= 22; f_y += f_vy;
    int32_t sp = f_speed();
    for (int32_t i = 0; i < NPIPE; i++) {
        f_px[i] -= sp;
        if (f_px[i] < -(8 << 8)) {                /* off the left: round again, a new gap */
            int32_t far = 0;
            for (int32_t k = 0; k < NPIPE; k++) if (f_px[k] > far) far = f_px[k];
            f_px[i] = far + (48 << 8); f_gap[i] = 12 + (int32_t)rnd(24); f_scored[i] = 0;
        }
        int32_t x = f_px[i] >> 8, by = f_y >> 8;
        if (x < 27 && x + 7 > 22 && (by < f_gap[i] - 8 || by + 3 > f_gap[i] + 8)) { f_state = G_OVER; new_hi(f_score, &flp_hi); g_snd(36, 127, 300); return; }
        if (!f_scored[i] && x + 7 < 22) { f_scored[i] = 1; f_score++; }
    }
    if (f_y < 0 || f_y > (49 << 8)) { f_state = G_OVER; new_hi(f_score, &flp_hi); g_snd(36, 127, 300); }
}
static void flp_tick(void)
{
    int32_t n = frames(&f_last);
    while (f_state == G_RUN && n-- > 0) f_frame();
}
static void flp_draw(void *view, void *bmp)
{
    char s[16];
    (void)view;
    fw_fillrect(bmp, 0, 0, 127, 52, 0);
    for (int32_t i = 0; i < NPIPE; i++) {
        int32_t x = f_px[i] >> 8;
        if (x > 127 || x < -8) continue;
        fw_fillrect(bmp, x, 0, x + 7, f_gap[i] - 8, 1);
        fw_fillrect(bmp, x, f_gap[i] + 8, x + 7, 52, 1);
    }
    int32_t by = f_y >> 8;
    fw_fillrect(bmp, 22, by, 26, by + 3, 1);               /* the bird, an eye and a wing */
    fw_fillrect(bmp, 25, by + 2, 25, by + 2, 0);
    fw_fillrect(bmp, 21, by + (f_vy > 0 ? 2 : 0), 21, by + (f_vy > 0 ? 3 : 1), 1);
    ui_num(s, f_score); ui_text(bmp, 62, 46, s);
    if (f_state != G_RUN) {
        box_msg(bmp, 24, f_state == G_OVER ? "GAME OVER" : f_state == G_PAUSE ? "PAUSED" : "YES OR A TRIG KEY: FLAP");
        char t[20], *p = put(t, "BEST "); ui_num(p, flp_hi); ui_text(bmp, 2, 2, t);
        ui_text(bmp, 44, 2, flp_bpm ? "B: TEMPO" : "B: STEADY");
        snd_text(bmp, 100, 2);
    }
}
static int32_t flp_key(void *view, void *event)
{
    int32_t key = g_keyid(event), press = g_press(event);
    if (key == K_YES) { if (press) { if (f_state == G_PAUSE) { f_state = G_RUN; f_last = blks; } else f_flap(); } return 1; }
    if (key == K_NO && f_state == G_RUN) { if (press) f_state = G_PAUSE; return 1; }
    return mp_key(view, event);
}
static int32_t flp_enc(void *view, void *event)
{
    int32_t knob = *(int32_t *)((char *)event + 12);
    if (knob == 2) { int32_t d = detents(*(int32_t *)((char *)event + 16)); if (d) { flp_bpm = d > 0; save_wait = 60; } fw_invalidate(view); return 1; }
    return snd_knob(view, event);
}
static void flp_trig(int32_t i) { (void)i; if (f_state != G_PAUSE) f_flap(); }
static int32_t flp_busy(void) { return f_state == G_RUN; }

/* ==== INV: Space Invaders ===================================================================================== */
#define IROWS 4
#define ICOLS 8
static uint8_t i_al[IROWS];
static int32_t i_state = G_IDLE, i_ax, i_ay, i_dir = 1, i_anim, i_px = 60 << 8, i_score, i_lives, i_wave, i_wait, i_beat;
static int32_t i_bx, i_by = -1, i_bomb_x[3], i_bomb_y[3], i_left, i_right, i_boom;
static uint32_t i_last;
static int32_t i_alive(void) { int32_t n = 0; for (int32_t r = 0; r < IROWS; r++) for (int32_t c = 0; c < ICOLS; c++) n += (i_al[r] >> c) & 1; return n; }
static void i_wave_start(void)
{
    for (int32_t r = 0; r < IROWS; r++) i_al[r] = 0xff;
    i_ax = 4; i_ay = 39 - (i_wave < 4 ? i_wave * 3 : 12); i_dir = 1; i_by = -1;
    for (int32_t k = 0; k < 3; k++) i_bomb_y[k] = -1;
}
static void i_start(void) { i_score = 0; i_lives = 3; i_wave = 0; i_wave_start(); i_state = G_RUN; i_last = blks; }
static int32_t i_cell_x(int32_t c) { return i_ax + c * 11; }
static int32_t i_cell_y(int32_t r) { return i_ay - r * 7; }     /* row 0 the top */
static void i_march(void)                        /* one step of the formation, and its four-note bass */
{
    int32_t minc = ICOLS, maxc = -1;
    for (int32_t r = 0; r < IROWS; r++) for (int32_t c = 0; c < ICOLS; c++) if ((i_al[r] >> c) & 1) { if (c < minc) minc = c; if (c > maxc) maxc = c; }
    if (maxc < 0) return;
    int32_t nx = i_ax + 2 * i_dir;
    if (nx + minc * 11 < 1 || nx + maxc * 11 + 7 > 126) { i_dir = -i_dir; i_ay -= 3; }
    else i_ax = nx;
    i_anim ^= 1;
    static const uint8_t bass[4] = { 40, 38, 36, 35 };
    g_snd(bass[i_beat++ & 3], 100, 80);
    int32_t lowest = 0;
    for (int32_t r = 0; r < IROWS; r++) if (i_al[r]) lowest = r;
    if (i_cell_y(lowest) < 8) { i_state = G_OVER; new_hi(i_score, &inv_hi); }   /* they landed */
}
static void i_frame(int32_t step_now)
{
    if (i_left) i_px -= 2 << 8;
    if (i_right) i_px += 2 << 8;
    i_px = ui_clamp(i_px, 0, 118 << 8);
    if (i_boom > 0) i_boom--;
    int32_t playing = blks - r_last_any < SEC;
    if (playing ? step_now : ++i_wait >= 6 + i_alive() / 2) { i_wait = 0; i_march(); }
    if (i_by >= 0) {                              /* your shot */
        i_by += 3;
        if (i_by > 50) i_by = -1;
        for (int32_t r = 0; r < IROWS && i_by >= 0; r++)
            for (int32_t c = 0; c < ICOLS; c++) {
                if (!((i_al[r] >> c) & 1)) continue;
                int32_t x = i_cell_x(c), y = i_cell_y(r);
                if (i_bx >= x && i_bx <= x + 6 && i_by >= y && i_by <= y + 4) {
                    i_al[r] &= (uint8_t)~(1u << c); i_by = -1;
                    i_score += r == 0 ? 30 : r == 1 ? 20 : 10;
                    g_snd(72, 90, 50);
                    if (!i_alive()) { i_wave++; i_wave_start(); }
                    break;
                }
            }
    }
    for (int32_t k = 0; k < 3; k++) {             /* their bombs */
        if (i_bomb_y[k] < 0) {
            if (rnd(90) == 0) {                   /* from the lowest alien of a column */
                int32_t c = (int32_t)rnd(ICOLS);
                for (int32_t r = IROWS - 1; r >= 0; r--) if ((i_al[r] >> c) & 1) { i_bomb_x[k] = i_cell_x(c) + 3; i_bomb_y[k] = i_cell_y(r) << 8; break; }
            }
            continue;
        }
        i_bomb_y[k] -= 384;
        int32_t by = i_bomb_y[k] >> 8, px = i_px >> 8;
        if (by <= 4 && by >= 1 && i_bomb_x[k] >= px && i_bomb_x[k] <= px + 8) {
            i_bomb_y[k] = -1; i_boom = 30; g_snd(30, 127, 300);
            if (--i_lives <= 0) { i_state = G_OVER; new_hi(i_score, &inv_hi); return; }
        } else if (by < 0) i_bomb_y[k] = -1;
    }
}
static void inv_tick(int32_t step_now)
{
    if (i_state != G_RUN) { i_last = blks; return; }
    int32_t n = frames(&i_last);
    for (int32_t k = 0; k < n && i_state == G_RUN; k++) i_frame(k == 0 ? step_now : 0);
}
static void inv_draw(void *view, void *bmp)
{
    char s[16];
    (void)view;
    fw_fillrect(bmp, 0, 0, 127, 52, 0);
    static const uint8_t spr[2][5] = { { 0x22, 0x7f, 0x5d, 0x7f, 0x41 }, { 0x22, 0x7f, 0x5d, 0x7f, 0x14 } };   /* top row first */
    for (int32_t r = 0; r < IROWS; r++)
        for (int32_t c = 0; c < ICOLS; c++) {
            if (!((i_al[r] >> c) & 1)) continue;
            int32_t x = i_cell_x(c), y = i_cell_y(r);
            for (int32_t yy = 0; yy < 5; yy++) for (int32_t xx = 0; xx < 7; xx++)
                if ((spr[i_anim][yy] >> xx) & 1) fw_fillrect(bmp, x + xx, y + 4 - yy, x + xx, y + 4 - yy, 1);
        }
    int32_t px = i_px >> 8;
    if (!(i_boom & 4)) { fw_fillrect(bmp, px, 1, px + 8, 2, 1); fw_fillrect(bmp, px + 3, 3, px + 5, 3, 1); fw_fillrect(bmp, px + 4, 4, px + 4, 4, 1); }
    if (i_by >= 0) fw_fillrect(bmp, i_bx, i_by, i_bx, i_by + 2, 1);
    for (int32_t k = 0; k < 3; k++) if (i_bomb_y[k] >= 0) { int32_t by = i_bomb_y[k] >> 8; fw_fillrect(bmp, i_bomb_x[k], by, i_bomb_x[k], by + 2, 1); }
    ui_num(s, i_score); ui_text(bmp, 2, 47, s);
    for (int32_t k = 0; k < i_lives && k < 5; k++) fw_fillrect(bmp, 122 - k * 5, 48, 124 - k * 5, 50, 1);
    if (i_state != G_RUN) {
        box_msg(bmp, 22, i_state == G_OVER ? "GAME OVER" : i_state == G_PAUSE ? "PAUSED" : "YES: START");
        char t[20], *p = put(t, "BEST "); ui_num(p, inv_hi); ui_text(bmp, 40, 47, t);
        snd_text(bmp, 2, 12);
        ui_text(bmp, 2, 5, "A OR ARROWS: MOVE  YES: FIRE");
    }
}
static int32_t inv_key(void *view, void *event)
{
    int32_t key = g_keyid(event), press = g_press(event), down = g_down(event);
    if (key == K_YES) {
        if (press) {
            if (i_state == G_PAUSE) i_state = G_RUN;
            else if (i_state != G_RUN) i_start();
            else if (i_by < 0) { i_bx = (i_px >> 8) + 4; i_by = 5; }
        }
        return 1;
    }
    if (key == K_NO && i_state == G_RUN) { if (press) { i_state = G_PAUSE; i_left = i_right = 0; } return 1; }
    if (i_state == G_RUN && (key == K_LEFT || key == K_RIGHT)) { if (key == K_LEFT) i_left = down; else i_right = down; return 1; }
    return mp_key(view, event);
}
static int32_t inv_enc(void *view, void *event)
{
    int32_t knob = *(int32_t *)((char *)event + 12), turn = *(int32_t *)((char *)event + 16);
    if (knob == 1 && i_state == G_RUN) { i_px = ui_clamp(i_px + turn * 64, 0, 118 << 8); return 1; }
    return snd_knob(view, event);
}
static int32_t inv_busy(void) { return i_state == G_RUN; }

/* ==== WAK: Whack-a-mole ======================================================================================= */
static uint32_t w_up[16];                        /* when each mole goes back down; 0: down */
static int32_t w_state = G_IDLE, w_score, w_miss, w_steps, w_hit_at = -1;
static uint32_t w_timer, w_hit_until;
static int32_t w_stay(void) { int32_t s = 1100 - w_score * 15; return s < 300 ? 300 : s; }   /* blocks it stays up */
static void w_pop(void)
{
    int32_t i = (int32_t)rnd(16);
    for (int32_t k = 0; k < 16 && w_up[i]; k++) i = (i + 1) & 15;
    if (!w_up[i]) w_up[i] = blks + (uint32_t)w_stay();
}
static void w_start(void) { for (int32_t i = 0; i < 16; i++) w_up[i] = 0; w_score = w_miss = w_steps = 0; w_state = G_RUN; w_timer = blks; }
static void wak_tick(int32_t step_now)
{
    if (w_state != G_RUN) return;
    uint32_t now = blks;
    for (int32_t i = 0; i < 16; i++)
        if (w_up[i] && (int32_t)(now - w_up[i]) >= 0) {
            w_up[i] = 0;
            if (++w_miss >= 10) { w_state = G_OVER; new_hi(w_score, &wak_hi); return; }
        }
    static const uint8_t every[3] = { 1, 2, 4 };
    int32_t playing = now - r_last_any < SEC;
    if (playing) { if (step_now && ++w_steps >= every[wak_rate]) { w_steps = 0; w_pop(); } }   /* on the steps */
    else if ((int32_t)(now - w_timer) >= w_stay() / 2) { w_timer = now; w_pop(); }
}
static void wak_trig(int32_t i)
{
    if (w_state != G_RUN) { if (w_state != G_PAUSE) w_start(); return; }
    static const uint8_t pent[5] = { 0, 2, 4, 7, 9 };
    if (w_up[i]) { w_up[i] = 0; w_score++; w_hit_at = i; w_hit_until = blks + 200; g_snd(60 + 12 * (i / 5) + pent[i % 5], 110, 120); }
    else if (++w_miss >= 10) { w_state = G_OVER; new_hi(w_score, &wak_hi); }
}
static void wak_draw(void *view, void *bmp)
{
    char s[20], *p;
    (void)view;
    fw_fillrect(bmp, 0, 0, 127, 52, 0);
    uint32_t now = blks;
    for (int32_t i = 0; i < 16; i++) {           /* the holes, laid out like the trig keys */
        int32_t x = i * 8 + 1;
        fw_fillrect(bmp, x, 14, x + 5, 14, 1);
        if (w_up[i]) {                            /* a mole, sinking as its time runs out */
            int32_t left = (int32_t)(w_up[i] - now), h = 4 + left * 14 / w_stay();
            fw_fillrect(bmp, x, 15, x + 5, 15 + h, 1);
            fw_fillrect(bmp, x + 1, 13 + h, x + 1, 13 + h, 0); fw_fillrect(bmp, x + 4, 13 + h, x + 4, 13 + h, 0);
        }
        if (i == w_hit_at && (int32_t)(w_hit_until - now) > 0) { ui_line(bmp, x - 1, 16, x + 6, 24); ui_line(bmp, x - 1, 24, x + 6, 16); }
    }
    p = put(s, "HITS "); ui_num(p, w_score); ui_text(bmp, 2, 46, s);
    for (int32_t k = 0; k < 10; k++) {           /* misses left */
        if (k < w_miss) { ui_line(bmp, 76 + k * 5, 46, 79 + k * 5, 49); ui_line(bmp, 76 + k * 5, 49, 79 + k * 5, 46); }
        else fw_framerect(bmp, 76 + k * 5, 46, 79 + k * 5, 49, 1);
    }
    if (w_state != G_RUN) {
        box_msg(bmp, 30, w_state == G_OVER ? "GAME OVER" : "A TRIG KEY: START");
        p = put(s, "BEST "); ui_num(p, wak_hi); ui_text(bmp, 2, 3, s);
        static const char *const rt[3] = { "B: EVERY STEP", "B: EVERY 2", "B: EVERY 4" };
        ui_text(bmp, 40, 3, rt[wak_rate]);
        snd_text(bmp, 100, 3);
    }
}
static int32_t wak_key(void *view, void *event)
{
    int32_t key = g_keyid(event), press = g_press(event);
    if (key == K_YES) { if (press && w_state != G_RUN) w_start(); return 1; }
    if (key == K_NO && w_state == G_RUN) { if (press) { w_state = G_OVER; new_hi(w_score, &wak_hi); } return 1; }
    return mp_key(view, event);
}
static int32_t wak_enc(void *view, void *event)
{
    int32_t knob = *(int32_t *)((char *)event + 12);
    if (knob == 2) { wak_rate = ui_clamp(wak_rate + detents(*(int32_t *)((char *)event + 16)), 0, 2); save_wait = 60; fw_invalidate(view); return 1; }
    return snd_knob(view, event);
}
static int32_t wak_busy(void) { return w_state == G_RUN; }

/* ==== LTS: Lights Out ========================================================================================= */
/* the 16 trig keys as a 4 x 4 grid (1-4 the top row): a key flips its light and its neighbours'. */
static uint16_t l_on;
static int32_t l_level, l_moves, l_state = G_IDLE, l_par;
static uint32_t l_next_at;
static uint16_t l_flip(int32_t i)
{
    int32_t r = i / 4, c = i % 4;
    uint16_t m = (uint16_t)(1u << i);
    if (r > 0) m |= (uint16_t)(1u << (i - 4));
    if (r < 3) m |= (uint16_t)(1u << (i + 4));
    if (c > 0) m |= (uint16_t)(1u << (i - 1));
    if (c < 3) m |= (uint16_t)(1u << (i + 1));
    return m;
}
static void l_puzzle(void)                        /* presses from all-off: it can always be solved */
{
    uint16_t used = 0;
    l_on = 0; l_par = 0;
    int32_t n = 2 + l_level;
    if (n > 12) n = 12;
    while (l_par < n) { int32_t i = (int32_t)rnd(16); if (used & (1u << i)) continue; used |= (uint16_t)(1u << i); l_on ^= l_flip(i); l_par++; }
    if (!l_on) l_puzzle();
    l_moves = 0; l_state = G_RUN;
}
static void l_start(void) { l_level = 1; l_puzzle(); }
static void lts_trig(int32_t i)
{
    if (l_state != G_RUN) { if (l_state == G_IDLE || l_state == G_OVER) l_start(); return; }
    l_on ^= l_flip(i); l_moves++;
    g_snd(55 + i, 90, 80);
    if (!l_on) { l_state = G_PAUSE; l_next_at = blks + 1200; new_hi(l_level, &lts_hi); g_snd(84, 120, 300); }   /* solved */
}
static void lts_tick(void)
{
    if (l_state == G_PAUSE && (int32_t)(blks - l_next_at) >= 0) { l_level++; l_puzzle(); }
}
static void lts_draw(void *view, void *bmp)
{
    char s[20], *p;
    (void)view;
    fw_fillrect(bmp, 0, 0, 127, 52, 0);
    for (int32_t i = 0; i < 16; i++) {
        int32_t r = i / 4, c = i % 4, x = 4 + c * 12, y = 40 - r * 12;
        if ((l_on >> i) & 1) fw_fillrect(bmp, x, y, x + 9, y + 9, 1);
        else fw_framerect(bmp, x, y, x + 9, y + 9, 1);
    }
    p = put(s, "LEVEL "); ui_num(p, l_level); ui_text(bmp, 60, 44, s);
    p = put(s, "MOVES "); ui_num(p, l_moves); ui_text(bmp, 60, 36, s);
    p = put(s, "BEST "); ui_num(p, lts_hi); ui_text(bmp, 60, 28, s);
    ui_text(bmp, 60, 16, "TRIGS 1-4 TOP ROW");
    ui_text(bmp, 60, 9, "TURN ALL OFF");
    snd_text(bmp, 60, 1);
    if (l_state == G_PAUSE) box_msg(bmp, 20, "SOLVED");
    else if (l_state != G_RUN) box_msg(bmp, 20, "YES: START");
}
static int32_t lts_key(void *view, void *event)
{
    int32_t key = g_keyid(event), press = g_press(event);
    if (key == K_YES) { if (press && l_state != G_RUN) l_start(); return 1; }
    return mp_key(view, event);
}
static int32_t lts_busy(void) { return 0; }

/* ---- into the menu, the tick and leaving the page --------------------------------------------------------- */
#define GAMES2 \
    { "TETRIS", tet_draw, tet_key, tet_enc, 0, tet_busy, &tet_hi, 0 }, \
    { "FLAPPY", flp_draw, flp_key, flp_enc, flp_trig, flp_busy, &flp_hi, 0 }, \
    { "SPACE INVADERS", inv_draw, inv_key, inv_enc, 0, inv_busy, &inv_hi, 0 }, \
    { "WHACK-A-MOLE", wak_draw, wak_key, wak_enc, wak_trig, wak_busy, &wak_hi, 0 }, \
    { "LIGHTS OUT", lts_draw, lts_key, snd_knob, lts_trig, lts_busy, &lts_hi, 0 },
#define GAMES2_TICK(step) do { tet_tick(step); flp_tick(); inv_tick(step); wak_tick(step); lts_tick(); } while (0)
#define GAMES2_LEAVE() do { if (t_state == G_RUN) t_state = G_PAUSE; if (f_state == G_RUN) f_state = G_PAUSE; \
                            if (i_state == G_RUN) { i_state = G_PAUSE; i_left = i_right = 0; } } while (0)
