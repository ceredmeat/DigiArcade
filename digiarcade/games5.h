/* SPDX-License-Identifier: GPL-2.0-or-later */
/* DIGIARCADE, the fifth part (included after games4.h): Drum Hero (the rhythm game for two), Pattern Copy,
 * Pinball, the Sequencer Puzzle and the Dungeon. */

/* ---- drums: each track held one note at a time, as the trig keys do --------------------------------------- */
static int8_t drum_held[8];
static void drum(int32_t t, int32_t vel)
{
    if (drum_held[t]) fw_note_off(t, NOTE, PANEL);
    fw_note_on(t, NOTE, vel, PANEL, 0, -1, -1);
    drum_held[t] = 1;
}
static void drums_off(void) { for (int32_t t = 0; t < 8; t++) if (drum_held[t]) { fw_note_off(t, NOTE, PANEL); drum_held[t] = 0; } }
static int32_t snd_track(void) { return brk_snd ? brk_snd - 1 : 0; }   /* knob D's track, or track 1 */

/* ==== PCP: Pattern Copy ======================================================================================== */
/* The Digitakt plays a bar twice after a count-in; then, after another count-in, you tap it back (YES or any
 * trig key). Taps land on the nearest 16th. All right: the next, longer one. Three lives. */
#define PSTEP 225                                /* blocks a 16th at 100 BPM */
enum { PC_IDLE, PC_PLAY, PC_YOU, PC_RESULT, PC_OVER };
static int32_t pc_state = PC_IDLE, pc_level, pc_lives, pc_score, pc_pos, pc_ok;
static uint16_t pc_target, pc_mine;
static uint32_t pc_t0;
static void pc_new(void)
{
    int32_t n = 3 + pc_level;
    if (n > 11) n = 11;
    pc_target = 1;                               /* always on the one */
    while (1) { int32_t c = 0; for (int32_t i = 0; i < 16; i++) c += (pc_target >> i) & 1; if (c >= n) break; pc_target |= (uint16_t)(1u << (rnd(4) ? rnd(8) * 2 : rnd(16))); }   /* mostly on the 8ths */
    pc_mine = 0; pc_pos = -1; pc_state = PC_PLAY; pc_t0 = blks + 300;
}
static void pc_start(void) { pc_level = 0; pc_lives = 3; pc_score = 0; pc_new(); }
static int32_t pc_step_now(void) { return (int32_t)(blks - pc_t0) / PSTEP; }   /* 16ths since the count-in began */
static void pcp_tick(void)
{
    if (pc_state != PC_PLAY && pc_state != PC_YOU) return;
    if ((int32_t)(blks - pc_t0) < 0) return;
    int32_t s = pc_step_now();
    if (s == pc_pos) return;
    pc_pos = s;
    int32_t t = snd_track();
    if (s < 16) { if ((s & 3) == 0) pad_sound(t, 84, s ? 50 : 80, 60); }   /* the count-in: four clicks */
    else if (pc_state == PC_PLAY && s < 48) { if ((pc_target >> ((s - 16) & 15)) & 1) drum(t, 110); }
    else if (pc_state == PC_PLAY) { pc_state = PC_YOU; pc_t0 = blks; pc_pos = -1; }
    else if (pc_state == PC_YOU && s >= 33) {    /* the bar is over: how did you do */
        pc_ok = pc_mine == pc_target;
        if (pc_ok) { pc_level++; pc_score += pc_level * 100; new_hi(pc_score, &pc_hi); g_snd(84, 110, 300); }
        else { pc_lives--; g_snd(36, 127, 300); }
        pc_state = PC_RESULT; pc_t0 = blks + 2400;
    }
}
static void pc_tap(void)
{
    if (pc_state == PC_IDLE || pc_state == PC_OVER) { pc_start(); return; }
    if (pc_state == PC_RESULT) { if (pc_lives <= 0) { pc_state = PC_OVER; new_hi(pc_score, &pc_hi); } else if (pc_ok) pc_new(); else { pc_mine = 0; pc_state = PC_PLAY; pc_t0 = blks + 300; pc_pos = -1; } return; }
    if (pc_state != PC_YOU) return;
    int32_t at = (int32_t)(blks - pc_t0) - 16 * PSTEP + PSTEP / 2;   /* after the count-in, rounded */
    if (at < 0 || at >= 16 * PSTEP) return;
    pc_mine |= (uint16_t)(1u << (at / PSTEP));
    drum(snd_track(), 100);
}
static void pc_grid(void *bmp, int32_t y, uint16_t bits, uint16_t other, int32_t mark)
{
    for (int32_t i = 0; i < 16; i++) {
        int32_t x = i * 8 + 1;
        if ((bits >> i) & 1) fw_fillrect(bmp, x, y, x + 5, y + 5, 1); else fw_framerect(bmp, x, y, x + 5, y + 5, 1);
        if (mark && (((bits ^ other) >> i) & 1)) { ui_line(bmp, x, y - 2, x + 5, y - 2); }   /* a wrong one: underlined */
    }
}
static void pcp_draw(void *view, void *bmp)
{
    char s[32], *p;
    (void)view;
    fw_fillrect(bmp, 0, 0, 127, 52, 0);
    p = put(s, "LEVEL "); p = ui_num(p, pc_level + 1); p = put(p, "  SCORE "); ui_num(p, pc_score); ui_text(bmp, 2, 46, s);
    for (int32_t k = 0; k < pc_lives; k++) fw_fillrect(bmp, 122 - k * 5, 47, 124 - k * 5, 49, 1);
    int32_t step = pc_step_now();
    if (pc_state == PC_PLAY || pc_state == PC_YOU) {
        int32_t counting = (int32_t)(blks - pc_t0) < 0 || step < 16;
        ui_centred(bmp, 0, 128, 36, pc_state == PC_PLAY ? "LISTEN" : counting ? "GET READY" : "PLAY IT BACK");
        if (counting && step >= 0 && (int32_t)(blks - pc_t0) >= 0) { char c[2] = { (char)('1' + step / 4), 0 }; box_msg(bmp, 14, c); }
        else if (pc_state == PC_YOU) pc_grid(bmp, 16, pc_mine, 0, 0);
        if (!counting) { int32_t i = (step - 16) & 15; fw_fillrect(bmp, i * 8 + 1, 8, i * 8 + 6, 9, 1); }   /* the playhead */
    } else if (pc_state == PC_RESULT) {
        ui_centred(bmp, 0, 128, 38, pc_ok ? "RIGHT" : "NOT QUITE");
        ui_text(bmp, 2, 30, "IT WAS"); pc_grid(bmp, 22, pc_target, pc_mine, 0);
        ui_text(bmp, 2, 14, "YOU"); pc_grid(bmp, 6, pc_mine, pc_target, 1);
    } else {
        box_msg(bmp, 24, pc_state == PC_OVER ? "GAME OVER" : "YES: START");
        ui_centred(bmp, 0, 128, 12, "TAP ON YES OR THE TRIG KEYS");
        p = put(s, "BEST "); ui_num(p, pc_hi); ui_text(bmp, 2, 2, s);
        snd_text(bmp, 100, 2);
    }
}
static int32_t pcp_key(void *view, void *event)
{
    if (g_keyid(event) == K_YES) { if (g_press(event)) pc_tap(); return 1; }
    return mp_key(view, event);
}
static void pcp_trig(int32_t i) { (void)i; pc_tap(); }
static int32_t pcp_busy(void) { return pc_state == PC_PLAY || pc_state == PC_YOU; }

/* ==== PIN: Pinball =============================================================================================== */
/* Trig keys 1-8 (or LEFT) the left flipper, 9-16 (or RIGHT) the right; YES puts a ball in play. Three bumpers,
 * four lights across the top (all lit: double points), three balls. Positions in 1/16 pixels. */
static int32_t pb_state = G_IDLE, pb_x, pb_y, pb_vx, pb_vy, pb_balls, pb_score, pb_fl[2], pb_up[2], pb_live, pb_lights, pb_mult = 1, pb_flash, pb_bump = -1;
static uint32_t pb_last;
static const int8_t pb_bx[3] = { 38, 64, 90 }, pb_by[3] = { 32, 40, 32 };
#define PBR 5                                     /* bumper radius */
static int32_t isqrt(int32_t v) { int32_t r = 0; while ((r + 1) * (r + 1) <= v) r++; return r; }
static void pb_flipper(int32_t side, int32_t *x0, int32_t *y0, int32_t *x1, int32_t *y1)   /* in pixels */
{
    int32_t up = pb_fl[side];                    /* 0 down .. 8 up */
    *x0 = side ? 84 : 44; *y0 = 8;
    *x1 = side ? 67 : 61; *y1 = 3 + up * 11 / 8;       /* a 6-pixel gap between the tips */
}
/* bounce off a segment (pixels): every wall on this table (the funnel, the flippers) keeps the ball above it,
 * so a ball found within 1.5 pixels above one, or up to 4 below (it slipped through, or a flipper swept past
 * it), is put back on top and bounced. kick: an extra push upward off it (a flipper on its way up). */
static int32_t pb_ox, pb_oy;
static int32_t pb_seg(int32_t ax, int32_t ay, int32_t bx, int32_t by, int32_t kick, int32_t below)
{
    ax *= 16; ay *= 16; bx *= 16; by *= 16;
    int32_t dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy, len = isqrt(l2);
    if (!len) return 0;
    int32_t nx = -dy * 256 / len, ny = dx * 256 / len;             /* a normal, 1/256 ... */
    if (ny < 0) { nx = -nx; ny = -ny; }          /* ... pointing up */
    int32_t t = ((pb_x - ax) * dx + (pb_y - ay) * dy) * 64 / l2;
    if (t < -3 || t > 67) return 0;              /* off its ends */
    int32_t sn = ((pb_x - ax) * nx + (pb_y - ay) * ny) / 256;      /* above it: positive, 1/16 px */
    if (sn > 24 || sn < -below) return 0;
    int32_t vn = (pb_vx * nx + pb_vy * ny) / 256;
    if (vn >= 0 && sn >= 0 && !kick) return 0;    /* above and moving away */
    if (vn < 0) { pb_vx -= 2 * vn * nx / 256 * 7 / 8; pb_vy -= 2 * vn * ny / 256 * 7 / 8; }
    pb_vx += kick * nx / 256; pb_vy += kick * ny / 256;
    if (sn < 26) { pb_x += (26 - sn) * nx / 256; pb_y += (26 - sn) * ny / 256; }   /* back on top */
    return 1;
}
static void pb_frame(void)
{
    for (int32_t s = 0; s < 2; s++) {
        int32_t was = pb_fl[s];
        pb_fl[s] = ui_clamp(pb_fl[s] + (pb_up[s] ? 1 : -1), 0, 8);
        if (!pb_live) continue;
        int32_t x0, y0, x1, y1;
        pb_flipper(s, &x0, &y0, &x1, &y1);
        pb_ox = pb_x; pb_oy = pb_y;
        pb_seg(x0, y0, x1, y1, pb_fl[s] > was ? 32 : 0, 12 * 16);   /* a rising flipper hits hard, and catches what it sweeps */
    }
    if (!pb_live) return;
    static int32_t g;
    if (++g % 3 == 0) pb_vy -= 1;                /* gravity: a third of a 16th of a pixel a step: floaty */
    pb_vx = ui_clamp(pb_vx, -24, 24); pb_vy = ui_clamp(pb_vy, -24, 24);   /* slower: never faster than a wall is thick */
    pb_ox = pb_x; pb_oy = pb_y;
    pb_x += pb_vx; pb_y += pb_vy;
    if (pb_x < 16) { pb_x = 16; pb_vx = -pb_vx; }
    if (pb_x > 126 * 16) { pb_x = 126 * 16; pb_vx = -pb_vx; }
    if (pb_y > 50 * 16) { pb_y = 50 * 16; pb_vy = -pb_vy / 2; }
    pb_seg(0, 22, 44, 8, 0, 64); pb_seg(127, 22, 84, 8, 0, 64);     /* the funnel, right into the flippers */
    for (int32_t b = 0; b < 3; b++) {             /* bumpers: a kick and points */
        int32_t ex = pb_x - pb_bx[b] * 16, ey = pb_y - pb_by[b] * 16, d2 = ex * ex + ey * ey, r = (PBR + 1) * 16;
        if (d2 < r * r) {
            int32_t d = isqrt(d2); if (!d) d = 1;
            pb_vx = ex * 24 / d; pb_vy = ey * 24 / d;
            pb_x = pb_bx[b] * 16 + ex * r / d; pb_y = pb_by[b] * 16 + ey * r / d;
            pb_score += 100 * pb_mult; pb_flash = 8; pb_bump = b;
            g_snd(60 + b * 5, 110, 100);
        }
    }
    if (pb_y > 46 * 16 && pb_vy > 0) {            /* the lights across the top */
        int32_t k = ui_clamp((pb_x / 16 - 16) / 24, 0, 3);
        if (!((pb_lights >> k) & 1)) { pb_lights |= 1 << k; pb_score += 50; g_snd(72 + k * 2, 90, 60); }
        if (pb_lights == 15) { pb_lights = 0; pb_mult++; g_snd(84, 127, 300); }
    }
    if (pb_y < -32) {                            /* drained */
        pb_live = 0; pb_mult = 1;
        if (--pb_balls <= 0) { pb_state = G_OVER; new_hi(pb_score, &pb_hi); }
        g_snd(30, 127, 300);
    }
}
static void pin_tick(void)
{
    if (pb_flash > 0) pb_flash--;
    if (pb_state != G_RUN) { pb_last = blks; return; }
    int32_t n = frames(&pb_last);
    while (n-- > 0 && pb_state == G_RUN) for (int32_t k = 0; k < 4 && pb_state == G_RUN; k++) pb_frame();   /* four small steps a frame */
}
static void pb_launch(void)
{
    if (pb_state != G_RUN) { pb_score = 0; pb_balls = 3; pb_lights = 0; pb_mult = 1; pb_state = G_RUN; pb_last = blks; }
    if (pb_live) return;
    pb_x = (20 + (int32_t)rnd(88)) * 16; pb_y = 49 * 16; pb_vx = (int32_t)rnd(24) - 12; pb_vy = 0; pb_live = 1;
}
static void pin_draw(void *view, void *bmp)
{
    char s[24], *p;
    (void)view;
    fw_fillrect(bmp, 0, 0, 127, 52, 0);
    ui_line(bmp, 0, 22, 44, 8); ui_line(bmp, 127, 22, 84, 8);
    for (int32_t s2 = 0; s2 < 2; s2++) {
        int32_t x0, y0, x1, y1;
        pb_flipper(s2, &x0, &y0, &x1, &y1);
        ui_line(bmp, x0, y0, x1, y1); ui_line(bmp, x0, y0 + 1, x1, y1 + 1);
    }
    for (int32_t b = 0; b < 3; b++) {
        int32_t cx = pb_bx[b], cy = pb_by[b];
        for (int32_t dy = -PBR; dy <= PBR; dy++) {
            int32_t hw = isqrt(PBR * PBR - dy * dy);
            if (b == pb_bump && pb_flash) fw_fillrect(bmp, cx - hw, cy + dy, cx + hw, cy + dy, 1);
            else { fw_fillrect(bmp, cx - hw, cy + dy, cx - hw, cy + dy, 1); fw_fillrect(bmp, cx + hw, cy + dy, cx + hw, cy + dy, 1); }
        }
        if (!(b == pb_bump && pb_flash)) { fw_fillrect(bmp, cx - 3, cy - PBR, cx + 3, cy - PBR, 1); fw_fillrect(bmp, cx - 3, cy + PBR, cx + 3, cy + PBR, 1); }
    }
    for (int32_t k = 0; k < 4; k++) {             /* the lights */
        int32_t x = 22 + k * 24;
        if ((pb_lights >> k) & 1) fw_fillrect(bmp, x, 48, x + 5, 50, 1); else fw_framerect(bmp, x, 48, x + 5, 50, 1);
    }
    if (pb_live) { int32_t x = pb_x / 16, y = pb_y / 16; fw_fillrect(bmp, x - 1, y - 1, x + 1, y + 1, 1); fw_fillrect(bmp, x - 1, y - 1, x - 1, y - 1, 0); }
    p = ui_num(s, pb_score); ui_text(bmp, 2, 2, s);
    if (pb_mult > 1) { p = put(s, "X"); ui_num(p, pb_mult); ui_text(bmp, 2, 9, s); }
    for (int32_t k = 0; k < pb_balls && k < 5; k++) fw_fillrect(bmp, 122 - k * 5, 3, 124 - k * 5, 5, 1);
    if (pb_state != G_RUN) {
        box_msg(bmp, 14, pb_state == G_OVER ? "GAME OVER" : "YES: BALL");
        p = put(s, "BEST "); ui_num(p, pb_hi); ui_centred(bmp, 0, 128, 0, s);
    } else if (!pb_live) box_msg(bmp, 14, "YES: NEXT BALL");
}
static int32_t pin_key(void *view, void *event)
{
    int32_t key = g_keyid(event), press = g_press(event), down = g_down(event);
    if (key == K_YES) { if (press) pb_launch(); return 1; }
    if (pb_state == G_RUN && (key == K_LEFT || key == K_RIGHT)) { pb_up[key == K_RIGHT] = down; return 1; }
    return mp_key(view, event);
}
static void pin_trig(int32_t i) { pb_up[i >> 3] = 1; }
static void pin_trig_up(int32_t i) { pb_up[i >> 3] = 0; }
static int32_t pin_busy(void) { return pb_state == G_RUN && pb_live; }

/* ==== SEQ: the Sequencer Puzzle ===================================================================================== */
/* A beat plays: one bar the target, the next yours, over and over. Rebuild it on the trig keys (a key sets or
 * clears a step); from level 4 there's a second layer, picked with knob B. YES checks it. */
static uint16_t sq_target[2], sq_mine[2];
static int32_t sq_level, sq_layers, sq_layer, sq_tries, sq_state = G_IDLE, sq_step = -1, sq_bar, sq_said, sq_score;
static uint32_t sq_t0, sq_said_at;
static const uint8_t sq_trk[2] = { 0, 1 };       /* tracks 1 and 2: usually a kick and a snare */
static void sq_new(void)
{
    sq_layers = sq_level < 3 ? 1 : 2;
    int32_t extra = sq_level % 3 + 1;
    sq_target[0] = 0x0101;                        /* kick on 1 and 3 */
    for (int32_t k = 0; k < extra; k++) sq_target[0] |= (uint16_t)(1u << (rnd(8) * 2 + (sq_level > 5 ? rnd(2) : 0)));
    sq_target[1] = sq_layers > 1 ? (uint16_t)(0x1010 | (sq_level > 4 ? 1u << (rnd(16)) : 0)) : 0;   /* snare on 2 and 4 */
    sq_mine[0] = sq_mine[1] = 0; sq_layer = 0; sq_tries = 0; sq_state = G_RUN; sq_t0 = blks; sq_step = -1;
}
static void sq_start(void) { sq_level = 0; sq_score = 0; sq_new(); }
static void seq_tick(void)
{
    if (sq_state != G_RUN) return;
    int32_t s = (int32_t)(blks - sq_t0) / 188;  /* 16ths at 120 BPM */
    if (s == sq_step) return;
    sq_step = s;
    sq_bar = (s / 16) & 1;                        /* 0: the target, 1: yours */
    for (int32_t l = 0; l < sq_layers; l++) {
        uint16_t bits = sq_bar ? sq_mine[l] : sq_target[l];
        if ((bits >> (s & 15)) & 1) drum(sq_trk[l], sq_bar ? 95 : 115);
    }
}
static void sq_check(void)
{
    int32_t wrong = 0;
    for (int32_t l = 0; l < sq_layers; l++) for (int32_t i = 0; i < 16; i++) wrong += ((sq_mine[l] ^ sq_target[l]) >> i) & 1;
    sq_said = wrong; sq_said_at = blks;
    if (!wrong) {
        sq_score += 100 * (sq_level + 1) - sq_tries * 20 > 10 ? 100 * (sq_level + 1) - sq_tries * 20 : 10;
        sq_level++; new_hi(sq_level, &sq_hi); g_snd(84, 110, 300); sq_new(); sq_said = -1; sq_said_at = blks;
    } else { sq_tries++; g_snd(36, 100, 200); }
}
static void seq_draw(void *view, void *bmp)
{
    char s[32], *p;
    (void)view;
    fw_fillrect(bmp, 0, 0, 127, 52, 0);
    if (sq_state != G_RUN) {
        box_msg(bmp, 30, "YES: START");
        ui_centred(bmp, 0, 128, 18, "REBUILD THE BEAT YOU HEAR");
        ui_centred(bmp, 0, 128, 10, "ON THE TRIG KEYS  YES: CHECK");
        p = put(s, "BEST LEVEL "); ui_num(p, sq_hi); ui_text(bmp, 2, 2, s);
        return;
    }
    p = put(s, "LEVEL "); p = ui_num(p, sq_level + 1); p = put(p, "  TRIES "); ui_num(p, sq_tries); ui_text(bmp, 2, 46, s);
    ui_text(bmp, 88, 46, sq_bar ? "YOURS" : "TARGET");
    for (int32_t l = 0; l < sq_layers; l++) {
        int32_t y = 30 - l * 12;
        ui_text(bmp, 1, y + 1, l ? "S" : "K");
        for (int32_t i = 0; i < 16; i++) {
            int32_t x = 8 + i * 7;
            if ((sq_mine[l] >> i) & 1) fw_fillrect(bmp, x, y, x + 5, y + 6, 1); else fw_framerect(bmp, x, y, x + 5, y + 6, 1);
        }
        if (l == sq_layer && sq_layers > 1) fw_fillrect(bmp, 1, y - 2, 5, y - 2, 1);
    }
    int32_t i = sq_step & 15;                     /* the playhead */
    fw_fillrect(bmp, 8 + i * 7, 39, 13 + i * 7, 40, 1);
    if (blks - sq_said_at < 1500 && sq_said >= 0) { p = ui_num(s, sq_said); put(p, sq_said == 1 ? " STEP WRONG" : " STEPS WRONG"); box_msg(bmp, 2, s); }
    else if (blks - sq_said_at < 1500 && sq_said < 0) box_msg(bmp, 2, "SOLVED");
    else if (sq_layers > 1) ui_text(bmp, 2, 3, "B: K OR S LAYER  YES: CHECK");
    else ui_text(bmp, 2, 3, "YES: CHECK");
}
static int32_t seq_key(void *view, void *event)
{
    if (g_keyid(event) == K_YES) { if (g_press(event)) { if (sq_state == G_RUN) sq_check(); else sq_start(); } return 1; }
    return mp_key(view, event);
}
static int32_t seq_enc(void *view, void *event)
{
    int32_t knob = *(int32_t *)((char *)event + 12);
    if (knob == 2 && sq_state == G_RUN && sq_layers > 1) { int32_t d = detents(*(int32_t *)((char *)event + 16)); if (d) sq_layer = d > 0; fw_invalidate(view); return 1; }
    return 0;
}
static void seq_trig(int32_t i) { if (sq_state == G_RUN) { sq_mine[sq_layer] ^= (uint16_t)(1u << i); if ((sq_mine[sq_layer] >> i) & 1) drum(sq_trk[sq_layer], 90); } else sq_start(); }
static int32_t seq_busy(void) { return 0; }

/* ==== DGN: the Dungeon ============================================================================================== */
/* Turn-based: the arrows walk (into a monster: a fight); potions heal, gold scores; YES on the stairs goes down. */
#define DW 32
#define DH 16
static uint8_t dg_map[DH][DW];                   /* bit 0 wall, bit 1 seen */
struct dent { int8_t x, y, hp; uint8_t kind; };   /* kind: 1 monster, 2 potion, 3 gold; 0 gone */
#define DE 14
static struct dent dg_e[DE];
static int32_t dg_state = G_IDLE, dg_px, dg_py, dg_hp, dg_max, dg_depth, dg_gold, dg_sx, dg_sy, dg_hurt, dg_hit_at;
static const char *dg_msg;
static void dg_carve(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    for (int32_t y = y0; y <= y1; y++) for (int32_t x = x0; x <= x1; x++) if (x > 0 && y > 0 && x < DW - 1 && y < DH - 1) dg_map[y][x] = 0;
}
static int32_t dg_free(int32_t x, int32_t y) { return x >= 0 && y >= 0 && x < DW && y < DH && !(dg_map[y][x] & 1); }
static struct dent *dg_at(int32_t x, int32_t y) { for (int32_t i = 0; i < DE; i++) if (dg_e[i].kind && dg_e[i].x == x && dg_e[i].y == y) return &dg_e[i]; return 0; }
static void dg_level(void)
{
    int32_t rx[6], ry[6], n = 0;
    for (int32_t y = 0; y < DH; y++) for (int32_t x = 0; x < DW; x++) dg_map[y][x] = 1;
    for (int32_t i = 0; i < DE; i++) dg_e[i].kind = 0;
    for (int32_t tries = 0; tries < 40 && n < 6; tries++) {     /* rooms, then corridors between them */
        int32_t w = 3 + (int32_t)rnd(5), h = 2 + (int32_t)rnd(3), x = 1 + (int32_t)rnd(DW - w - 2), y = 1 + (int32_t)rnd(DH - h - 2), ok = 1;
        for (int32_t k = 0; k < n; k++) if (iabs(x - rx[k]) < w + 3 && iabs(y - ry[k]) < h + 2) ok = 0;
        if (!ok) continue;
        dg_carve(x, y, x + w - 1, y + h - 1);
        rx[n] = x + w / 2; ry[n] = y + h / 2;
        if (n) { dg_carve(rx[n - 1] < rx[n] ? rx[n - 1] : rx[n], ry[n - 1], rx[n - 1] > rx[n] ? rx[n - 1] : rx[n], ry[n - 1]);
                 dg_carve(rx[n], ry[n - 1] < ry[n] ? ry[n - 1] : ry[n], rx[n], ry[n - 1] > ry[n] ? ry[n - 1] : ry[n]); }
        n++;
    }
    dg_px = rx[0]; dg_py = ry[0]; dg_sx = rx[n - 1]; dg_sy = ry[n - 1];
    int32_t k = 0;
    for (int32_t i = 0; i < 3 + dg_depth && k < DE; i++) {      /* monsters, then things to find */
        int32_t x = 1 + (int32_t)rnd(DW - 2), y = 1 + (int32_t)rnd(DH - 2);
        if (!dg_free(x, y) || (iabs(x - dg_px) < 4 && iabs(y - dg_py) < 3) || dg_at(x, y)) continue;
        dg_e[k].x = (int8_t)x; dg_e[k].y = (int8_t)y; dg_e[k].kind = 1; dg_e[k].hp = (int8_t)(2 + dg_depth); k++;
    }
    for (int32_t i = 0; i < 6 && k < DE; i++) {
        int32_t x = 1 + (int32_t)rnd(DW - 2), y = 1 + (int32_t)rnd(DH - 2);
        if (!dg_free(x, y) || dg_at(x, y)) continue;
        dg_e[k].x = (int8_t)x; dg_e[k].y = (int8_t)y; dg_e[k].kind = (uint8_t)(rnd(3) ? 3 : 2); k++;
    }
    dg_msg = 0;
}
static void dg_start(void) { dg_depth = 0; dg_gold = 0; dg_max = dg_hp = 12; dg_level(); dg_state = G_RUN; }
static void dg_say(const char *m) { dg_msg = m; dg_hit_at = (int32_t)g_ticks; }
static void dg_monsters(void)
{
    for (int32_t i = 0; i < DE; i++) {
        struct dent *e = &dg_e[i];
        if (e->kind != 1) continue;
        int32_t dx = dg_px - e->x, dy = dg_py - e->y;
        if (iabs(dx) + iabs(dy) == 1) {           /* next to you: it bites */
            int32_t dmg = 1 + (int32_t)rnd((uint32_t)(1 + dg_depth / 2));
            dg_hp -= dmg; dg_hurt = 12; g_snd(33, 120, 150); dg_say("IT BITES");
            if (dg_hp <= 0) { dg_hp = 0; dg_state = G_OVER; new_hi(dg_gold + dg_depth * 50, &dg_hi); dg_say("YOU DIED"); return; }
            continue;
        }
        if (iabs(dx) > 6 || iabs(dy) > 5) continue;
        int32_t nx = e->x + (dx > 0 ? 1 : dx < 0 ? -1 : 0), ny = e->y;
        if (iabs(dy) > iabs(dx) || !dg_free(nx, ny) || dg_at(nx, ny)) { nx = e->x; ny = e->y + (dy > 0 ? 1 : dy < 0 ? -1 : 0); }
        if (dg_free(nx, ny) && !dg_at(nx, ny) && !(nx == dg_px && ny == dg_py)) { e->x = (int8_t)nx; e->y = (int8_t)ny; }
    }
}
static void dg_walk(int32_t dx, int32_t dy)
{
    int32_t nx = dg_px + dx, ny = dg_py + dy;
    struct dent *e = dg_at(nx, ny);
    if (e && e->kind == 1) {                      /* a fight */
        int32_t dmg = 1 + (int32_t)rnd(3) + dg_depth / 3;
        e->hp = (int8_t)(e->hp - dmg); g_snd(45, 110, 100);
        if (e->hp <= 0) { e->kind = 0; dg_gold += 10 + dg_depth * 5; dg_say("IT DIES"); g_snd(28, 127, 200); } else dg_say("YOU HIT IT");
    } else if (dg_free(nx, ny)) {
        dg_px = nx; dg_py = ny;
        if (e && e->kind == 2) { e->kind = 0; dg_hp = dg_hp + 5 > dg_max ? dg_max : dg_hp + 5; dg_say("A POTION: +5"); g_snd(79, 100, 150); }
        if (e && e->kind == 3) { e->kind = 0; dg_gold += 10 + (int32_t)rnd(20); dg_say("GOLD"); g_snd(88, 90, 80); }
        if (nx == dg_sx && ny == dg_sy) dg_say("STAIRS: YES TO GO DOWN");
    } else return;
    dg_monsters();
}
static void dgn_draw(void *view, void *bmp)
{
    char s[32], *p;
    (void)view;
    fw_fillrect(bmp, 0, 0, 127, 52, 0);
    if (dg_state == G_IDLE) {
        ui_centred(bmp, 0, 128, 40, "THE DUNGEON");
        ui_centred(bmp, 0, 128, 30, "ARROWS: WALK AND FIGHT");
        ui_centred(bmp, 0, 128, 23, "FIND THE STAIRS: YES GOES DOWN");
        box_msg(bmp, 6, "YES: START");
        return;
    }
    for (int32_t y = -3; y <= 3; y++) for (int32_t x = -4; x <= 4; x++) {   /* what you can see from here */
        int32_t mx = dg_px + x, my = dg_py + y;
        if (mx >= 0 && my >= 0 && mx < DW && my < DH && x * x + y * y <= 20) dg_map[my][mx] |= 2;
    }
    int32_t cx = ui_clamp(dg_px - 8, 0, DW - 16), cy = ui_clamp(dg_py - 3, 0, DH - 6);
    for (int32_t ty = 0; ty < 6; ty++) for (int32_t tx = 0; tx < 16; tx++) {
        int32_t mx = cx + tx, my = cy + ty, x = tx * 8, y = 40 - ty * 7;
        uint8_t c = dg_map[my][mx];
        if (!(c & 2)) continue;                   /* never seen */
        int32_t vis = iabs(mx - dg_px) <= 4 && iabs(my - dg_py) <= 3;
        if (c & 1) { fw_fillrect(bmp, x, y, x + 7, y + 6, 1); fw_fillrect(bmp, x, y + 3, x + 7, y + 3, 0); fw_fillrect(bmp, x + (my & 1) * 4 + 1, y, x + (my & 1) * 4 + 1, y + 2, 0); }
        else if (vis) fw_fillrect(bmp, x + 3, y + 3, x + 3, y + 3, 1);
        if (mx == dg_sx && my == dg_sy) { fw_fillrect(bmp, x + 1, y, x + 6, y, 1); fw_fillrect(bmp, x + 3, y + 2, x + 6, y + 2, 1); fw_fillrect(bmp, x + 5, y + 4, x + 6, y + 4, 1); }
        struct dent *e = vis ? dg_at(mx, my) : 0;
        if (e && e->kind == 1) { fw_fillrect(bmp, x + 1, y, x + 6, y + 5, 1); fw_fillrect(bmp, x + 2, y + 3, x + 2, y + 3, 0); fw_fillrect(bmp, x + 5, y + 3, x + 5, y + 3, 0); fw_fillrect(bmp, x + 1, y + 6, x + 1, y + 6, 1); fw_fillrect(bmp, x + 6, y + 6, x + 6, y + 6, 1); }
        if (e && e->kind == 2) { fw_framerect(bmp, x + 2, y, x + 5, y + 3, 1); fw_fillrect(bmp, x + 3, y + 4, x + 4, y + 5, 1); }
        if (e && e->kind == 3) { fw_fillrect(bmp, x + 3, y + 1, x + 4, y + 4, 1); fw_fillrect(bmp, x + 2, y + 2, x + 5, y + 3, 1); }
        if (mx == dg_px && my == dg_py) {          /* you */
            fw_fillrect(bmp, x + 3, y + 5, x + 4, y + 6, 1); fw_fillrect(bmp, x + 3, y + 2, x + 4, y + 4, 1);
            fw_fillrect(bmp, x + 1, y + 4, x + 6, y + 4, 1); fw_fillrect(bmp, x + 2, y, x + 2, y + 1, 1); fw_fillrect(bmp, x + 5, y, x + 5, y + 1, 1);
        }
    }
    if (dg_hurt & 2) fw_fillrect(bmp, 0, 0, 127, 46, -1);
    p = put(s, "HP "); p = ui_num(p, dg_hp); p = put(p, "/"); p = ui_num(p, dg_max); p = put(p, "  DEPTH "); p = ui_num(p, dg_depth + 1);
    p = put(p, "  GOLD "); ui_num(p, dg_gold); ui_text(bmp, 2, 47, s);
    if (dg_state == G_OVER) { box_msg(bmp, 20, "YOU DIED"); p = put(s, "BEST "); ui_num(p, dg_hi); box_msg(bmp, 8, s); }
    else if (dg_msg && (int32_t)g_ticks - dg_hit_at < 90) { int32_t w = 4 * ui_len(dg_msg) + 3; fw_fillrect(bmp, 0, 0, w, 7, 0); ui_text(bmp, 1, 1, dg_msg); }
}
static int32_t dgn_key(void *view, void *event)
{
    int32_t key = g_keyid(event), down = g_down(event), press = g_press(event);
    if (key == K_YES) {
        if (press) {
            if (dg_state != G_RUN) dg_start();
            else if (dg_px == dg_sx && dg_py == dg_sy) { dg_depth++; dg_max += 2; dg_hp = dg_hp + 3 > dg_max ? dg_max : dg_hp + 3; dg_level(); g_snd(55, 110, 300); dg_say("DEEPER"); }
        }
        return 1;
    }
    if (dg_state == G_RUN && key >= K_UP && key <= K_RIGHT) {
        if (down) { static const int8_t mx[4] = { 0, 0, -1, 1 }, my[4] = { -1, 1, 0, 0 }; dg_walk(mx[key - K_UP], my[key - K_UP]); }   /* UP is north: up the screen */
        return 1;
    }
    return mp_key(view, event);
}
static void dgn_tick(void) { if (dg_hurt > 0) dg_hurt--; }
static int32_t dgn_busy(void) { return 0; }

#define GAMES5 \
    { "DRUM HERO 2P", rhy2_draw, rhy2_key, rhy_enc, rhy_trig, rhy_busy, &dh_hi, 0, 0 }, \
    { "PATTERN COPY", pcp_draw, pcp_key, snd_knob, pcp_trig, pcp_busy, &pc_hi, 0, 0 }, \
    { "PINBALL", pin_draw, pin_key, snd_knob, pin_trig, pin_busy, &pb_hi, 0, pin_trig_up }, \
    { "SEQUENCER PUZZLE", seq_draw, seq_key, seq_enc, seq_trig, seq_busy, &sq_hi, 0, 0 }, \
    { "DUNGEON", dgn_draw, dgn_key, snd_knob, 0, dgn_busy, &dg_hi, 0, 0 },
#define GAMES5_TICK() do { pcp_tick(); pin_tick(); seq_tick(); dgn_tick(); } while (0)
#define GAMES5_LEAVE() do { pb_up[0] = pb_up[1] = 0; drums_off(); if (sq_state == G_RUN) sq_state = G_IDLE; \
                            if (pc_state == PC_PLAY || pc_state == PC_YOU) pc_state = PC_IDLE; } while (0)
