/* SPDX-License-Identifier: GPL-2.0-or-later */
/* DIGIARCADE, the fourth part (included after games3.h): HELLTAKT, a first-person maze shooter written from
 * scratch for the 1-bit screen (a grid raycaster, not the Doom engine), and the trig lights for every game. */

/* ==== HELLTAKT ================================================================================================= */
/* UP / DOWN walk, LEFT / RIGHT turn (knob A turns finely, knob B steps sideways), YES fires, NO opens a door.
 * The monsters close in on the active track's steps while the pattern plays; your shot plays the knob D
 * track (a kick makes a good gun). Find the exit (the striped wall) on each of three levels. */
static const int16_t h_sin[256] = { 0, 101, 201, 301, 401, 501, 601, 700, 799, 897, 995, 1092, 1189, 1285, 1380, 1474, 1567, 1660, 1751, 1842, 1931, 2019, 2106, 2191, 2276, 2359, 2440, 2520, 2598, 2675, 2751, 2824, 2896, 2967, 3035, 3102, 3166, 3229, 3290, 3349, 3406, 3461, 3513, 3564, 3612, 3659, 3703, 3745, 3784, 3822, 3857, 3889, 3920, 3948, 3973, 3996, 4017, 4036, 4052, 4065, 4076, 4085, 4091, 4095, 4096, 4095, 4091, 4085, 4076, 4065, 4052, 4036, 4017, 3996, 3973, 3948, 3920, 3889, 3857, 3822, 3784, 3745, 3703, 3659, 3612, 3564, 3513, 3461, 3406, 3349, 3290, 3229, 3166, 3102, 3035, 2967, 2896, 2824, 2751, 2675, 2598, 2520, 2440, 2359, 2276, 2191, 2106, 2019, 1931, 1842, 1751, 1660, 1567, 1474, 1380, 1285, 1189, 1092, 995, 897, 799, 700, 601, 501, 401, 301, 201, 101, 0, -101, -201, -301, -401, -501, -601, -700, -799, -897, -995, -1092, -1189, -1285, -1380, -1474, -1567, -1660, -1751, -1842, -1931, -2019, -2106, -2191, -2276, -2359, -2440, -2520, -2598, -2675, -2751, -2824, -2896, -2967, -3035, -3102, -3166, -3229, -3290, -3349, -3406, -3461, -3513, -3564, -3612, -3659, -3703, -3745, -3784, -3822, -3857, -3889, -3920, -3948, -3973, -3996, -4017, -4036, -4052, -4065, -4076, -4085, -4091, -4095, -4096, -4095, -4091, -4085, -4076, -4065, -4052, -4036, -4017, -3996, -3973, -3948, -3920, -3889, -3857, -3822, -3784, -3745, -3703, -3659, -3612, -3564, -3513, -3461, -3406, -3349, -3290, -3229, -3166, -3102, -3035, -2967, -2896, -2824, -2751, -2675, -2598, -2520, -2440, -2359, -2276, -2191, -2106, -2019, -1931, -1842, -1751, -1660, -1567, -1474, -1380, -1285, -1189, -1092, -995, -897, -799, -700, -601, -501, -401, -301, -201, -101 };
static int32_t h_cos(int32_t a) { return h_sin[(a + 64) & 255]; }
#define HM 16
static const char *const h_maps[3][HM] = {
  { "################", "#P....#.......X#", "#.##..#..M.....#", "#.#...D..####..#", "#.#.###..#..#..#",
    "#.#...#..#A.#..#", "#.###.#..#..D..#", "#...#.#..####..#", "###.#.#........#", "#...#.##D###.###",
    "#.###..#...#...#", "#...#..#.M.#.H.#", "#.M.D..#...#...#", "#...#..###D#...#", "#A..#.........M#", "################" },
  { "################", "#P..#....M.....#", "#...#.######.#.#", "#...D.#....#.#.#", "###.#.#.MH.#.#.#",
    "#...#.#....#.#.#", "#.###.###D##.#.#", "#.#.........M#.#", "#.#.#####.#####.#", "#.#.#A..#.....#.#",
    "#...#..M#.###.#.#", "#####..##.#X#.#.#", "#...D...#.#.#...#", "#.M.#.H.#.#D##.##", "#...#...#......M#", "################" },
  { "################", "#P....M.......A#", "#.####.######.#.#", "#.#..#.#....#.#.#", "#.#M.#.#.MM.#.#.#",
    "#.#..D.#....#.#.#", "#.####.###D##.#.#", "#H.....#...#..M.#", "######.#.X.#.####", "#....#.#...#.#..#",
    "#.M..D.##D##.#.M#", "#....#.......D..#", "####.#.#####.#..#", "#A...#.#..M#.#.H#", "#..M...D....#...#", "################" } };
static char h_map[HM][HM];                       /* the level as it is now: doors open, '#' wall */
#define HE 12
struct hent { int32_t x, y, hp; uint8_t kind, alive; };   /* kind: 1 monster, 2 health, 3 ammo; x, y Q8 */
static struct hent h_e[HE];
static int32_t h_state = G_IDLE, h_px, h_py, h_pa, h_hp, h_ammo, h_lvl, h_score, h_up, h_down, h_left, h_right;
static int32_t h_flash, h_hurt, h_hit_col = -1, h_hit_t, h_cool, h_wait, h_won_t, h_bob;
static uint32_t h_last;
static int16_t h_zbuf[64];                       /* each 2-pixel column's wall distance, Q8 */
#define VH 44                                    /* the view's height: rows 8-51 */
#define VY 8
static int32_t h_solid(int32_t x, int32_t y) { if (x < 0 || y < 0 || x >= HM || y >= HM) return 1; char c = h_map[y][x]; return c == '#' || c == 'D' || c == 'X'; }
static void h_level(int32_t n)
{
    int32_t k = 0;
    for (int32_t i = 0; i < HE; i++) h_e[i].alive = 0;
    for (int32_t y = 0; y < HM; y++) for (int32_t x = 0; x < HM; x++) {
        char c = h_maps[n][y][x];
        h_map[y][x] = c == '#' || c == 'D' || c == 'X' ? c : '.';
        if (c == 'P') { h_px = x * 256 + 128; h_py = y * 256 + 128; h_pa = 0; }
        if ((c == 'M' || c == 'H' || c == 'A') && k < HE) {
            h_e[k].x = x * 256 + 128; h_e[k].y = y * 256 + 128; h_e[k].alive = 1;
            h_e[k].kind = c == 'M' ? 1 : c == 'H' ? 2 : 3; h_e[k].hp = 3 + n; k++;
        }
    }
    h_lvl = n; h_state = G_RUN; h_last = blks;
}
static void h_start(void) { h_hp = 100; h_ammo = 20; h_score = 0; h_level(0); }
static void h_move(int32_t dx, int32_t dy)       /* with a little room kept from the walls */
{
    int32_t nx = h_px + dx, ny = h_py + dy, m = dx > 0 ? 60 : -60, n = dy > 0 ? 60 : -60;
    if (dx && !h_solid((nx + m) >> 8, h_py >> 8)) h_px = nx;
    if (dy && !h_solid(h_px >> 8, (ny + n) >> 8)) h_py = ny;
    if (h_map[h_py >> 8][h_px >> 8] == 'x') {    /* walked into the open exit */
        h_score += 500;
        if (h_lvl == 2) { h_state = G_OVER; h_won_t = 1; new_hi(h_score, &hel_hi); g_snd(84, 127, 600); }
        else { h_level(h_lvl + 1); g_snd(79, 120, 400); }
    }
}
static int32_t h_dist2(struct hent *e) { int32_t dx = (e->x - h_px) >> 4, dy = (e->y - h_py) >> 4; return dx * dx + dy * dy; }   /* (cells x 16)^2 */
static int32_t h_sees(struct hent *e)            /* a clear line between it and you */
{
    int32_t x = e->x, y = e->y, dx = (h_px - x) / 16, dy = (h_py - y) / 16;
    for (int32_t i = 0; i < 16; i++) { x += dx; y += dy; if (h_solid(x >> 8, y >> 8)) return 0; }
    return 1;
}
static void h_monsters(void)                     /* one step of theirs */
{
    for (int32_t i = 0; i < HE; i++) {
        struct hent *e = &h_e[i];
        if (!e->alive || e->kind != 1) continue;
        int32_t d2 = h_dist2(e);
        if (d2 > 64 * 64 || !h_sees(e)) continue;
        if (d2 < 12 * 12) {                       /* close enough to claw */
            h_hp -= 8 + h_lvl * 2; h_hurt = 20; g_snd(31, 127, 200);
            if (h_hp <= 0) { h_hp = 0; h_state = G_OVER; h_won_t = 0; new_hi(h_score, &hel_hi); }
            continue;
        }
        int32_t sx = h_px > e->x ? 40 : -40, sy = h_py > e->y ? 40 : -40;
        if (!h_solid((e->x + sx * 2) >> 8, e->y >> 8)) e->x += sx;
        if (!h_solid(e->x >> 8, (e->y + sy * 2) >> 8)) e->y += sy;
    }
}
static void h_fire(void)
{
    if (h_cool > 0) return;
    if (h_ammo <= 0) { g_snd(84, 40, 30); return; }  /* click */
    h_ammo--; h_flash = 6; h_cool = 12;
    g_snd(36, 127, 150);
    int32_t best = -1, bd = h_zbuf[32];
    int32_t dx = h_cos(h_pa), dy = h_sin[h_pa & 255], plx = -dy * 2 / 3, ply = dx * 2 / 3;
    int32_t det = (plx * dy - dx * ply) / 4096;
    for (int32_t i = 0; i < HE; i++) {           /* the nearest monster in the middle of the view */
        struct hent *e = &h_e[i];
        if (!e->alive || e->kind != 1 || !det) continue;
        int32_t sx = e->x - h_px, sy = e->y - h_py;
        int32_t tx = (dy * sx - dx * sy) / det, ty = (-ply * sx + plx * sy) / det;
        if (ty <= 16 || ty >= bd) continue;
        int32_t col = 32 + 32 * tx / ty, w = 64 * 100 / ty;
        if (iabs(col - 32) <= w / 2 + 1) { best = i; bd = ty; }
    }
    if (best >= 0) {
        struct hent *e = &h_e[best];
        h_hit_col = 1; h_hit_t = 8;
        if (--e->hp <= 0) { e->alive = 2; h_score += 100; g_snd(28, 127, 300); }    /* 2: a body on the floor */
    }
}
static void h_frame(int32_t step_now)
{
    if (h_flash > 0) h_flash--;
    if (h_hurt > 0) h_hurt--;
    if (h_hit_t > 0) h_hit_t--;
    if (h_cool > 0) h_cool--;
    if (h_left) h_pa = (h_pa - 2) & 255;
    if (h_right) h_pa = (h_pa + 2) & 255;
    int32_t dx = h_cos(h_pa), dy = h_sin[h_pa & 255], sp = 14;
    if (h_up) { h_move(dx * sp / 4096, dy * sp / 4096); h_bob++; }
    if (h_down) { h_move(-dx * sp / 4096, -dy * sp / 4096); h_bob++; }
    for (int32_t i = 0; i < HE; i++) {           /* pickups */
        struct hent *e = &h_e[i];
        if (e->alive != 1 || e->kind == 1 || h_dist2(e) > 10 * 10) continue;
        if (e->kind == 2) { if (h_hp >= 100) continue; h_hp += 30; if (h_hp > 100) h_hp = 100; }
        else h_ammo += 12;
        e->alive = 0; g_snd(84, 100, 120);
    }
    int32_t playing = blks - r_last_any < SEC;
    if (playing ? step_now : ++h_wait >= 18) { h_wait = 0; h_monsters(); }
}
static void hel_tick(int32_t step_now)
{
    if (h_state != G_RUN) { h_last = blks; return; }
    int32_t n = frames(&h_last);
    for (int32_t k = 0; k < n && h_state == G_RUN; k++) h_frame(k == 0 ? step_now : 0);
}
static void h_use(void)                          /* NO: open the door or exit in front of you */
{
    int32_t fx = (h_px + h_cos(h_pa) * 200 / 4096) >> 8, fy = (h_py + h_sin[h_pa & 255] * 200 / 4096) >> 8;
    if (fx < 0 || fy < 0 || fx >= HM || fy >= HM) return;
    if (h_map[fy][fx] == 'D') { h_map[fy][fx] = '.'; g_snd(48, 100, 200); }
    else if (h_map[fy][fx] == 'X') { h_map[fy][fx] = 'x'; g_snd(67, 110, 300); }
}
static void hel_draw(void *view, void *bmp)
{
    char s[32], *p;
    (void)view;
    fw_fillrect(bmp, 0, 0, 127, 52, 0);
    if (h_state == G_IDLE || h_state == G_OVER) {
        if (h_state == G_IDLE) {
            ui_centred(bmp, 0, 128, 42, "H E L L T A K T");
            ui_centred(bmp, 0, 128, 32, "UP DOWN: WALK  LEFT RIGHT: TURN");
            ui_centred(bmp, 0, 128, 25, "YES: FIRE  NO: OPEN DOORS");
            ui_centred(bmp, 0, 128, 18, "A TURN  B SIDESTEP  D GUN SOUND");
            box_msg(bmp, 4, "YES: START");
        } else {
            box_msg(bmp, 34, h_won_t ? "YOU ESCAPED" : "YOU DIED");
            p = put(s, "SCORE "); ui_num(p, h_score); ui_centred(bmp, 0, 128, 22, s);
            p = put(s, "BEST "); ui_num(p, hel_hi); ui_centred(bmp, 0, 128, 14, s);
            ui_centred(bmp, 0, 128, 2, "YES: AGAIN");
        }
        return;
    }
    int32_t dx = h_cos(h_pa), dy = h_sin[h_pa & 255], plx = -dy * 2 / 3, ply = dx * 2 / 3;
    int32_t horizon = VY + VH / 2;
    for (int32_t c = 0; c < 64; c++) {           /* the walls: a ray each 2 pixels */
        int32_t cam = c * 2 - 63;                 /* -63 .. 63 */
        int32_t rx = dx + plx * cam / 64, ry = dy + ply * cam / 64;
        int32_t mx = h_px >> 8, my = h_py >> 8;
        int32_t ddx_ = rx ? iabs(4096 * 256 / rx) : 1 << 20, ddy_ = ry ? iabs(4096 * 256 / ry) : 1 << 20;
        if (ddx_ > 1 << 20) ddx_ = 1 << 20;     /* a ray almost along an axis: kept in 32 bits */
        if (ddy_ > 1 << 20) ddy_ = 1 << 20;
        int32_t stx = rx < 0 ? -1 : 1, sty = ry < 0 ? -1 : 1;
        int32_t sdx = ((rx < 0 ? h_px - mx * 256 : (mx + 1) * 256 - h_px) * ddx_) >> 8;
        int32_t sdy = ((ry < 0 ? h_py - my * 256 : (my + 1) * 256 - h_py) * ddy_) >> 8;
        int32_t side = 0, hit = 0;
        for (int32_t n = 0; n < 32 && !hit; n++) {
            if (sdx < sdy) { sdx += ddx_; mx += stx; side = 0; } else { sdy += ddy_; my += sty; side = 1; }
            hit = h_solid(mx, my);
        }
        int32_t perp = side ? sdy - ddy_ : sdx - ddx_;
        if (perp < 16) perp = 16;
        h_zbuf[c] = (int16_t)(perp > 32767 ? 32767 : perp);
        int32_t h = VH * 256 / perp, x = c * 2;
        if (h > VH) h = VH;
        int32_t y0 = horizon - h / 2, y1 = horizon + h / 2;
        char kind = mx >= 0 && my >= 0 && mx < HM && my < HM ? h_map[my][mx] : '#';
        if (perp < 3 * 256 && !side) fw_fillrect(bmp, x, y0, x + 1, y1, 1);           /* near, lit face: solid */
        else if (perp < 6 * 256) fw_fillrect(bmp, x + (side ? (c & 1) : 0), y0, x + (side ? (c & 1) : 0), y1, 1);   /* shaded */
        else { fw_fillrect(bmp, x, y0, x + 1, y0, 1); fw_fillrect(bmp, x, y1, x + 1, y1, 1); if (!(c & 3)) fw_fillrect(bmp, x, y0, x, y1, 1); }   /* far: edges */
        if (kind == 'D' && h > 6) fw_fillrect(bmp, x, horizon - 1, x + 1, horizon + 1, -1);           /* a door: a band */
        if (kind == 'X' && h > 6) for (int32_t yy = y0 + 2; yy < y1; yy += 4) fw_fillrect(bmp, x, yy, x + 1, yy, -1);   /* the exit: stripes */
    }
    for (int32_t y = VY; y < horizon - 4; y += 4)  /* the floor: dots getting closer */
        for (int32_t x = ((y >> 2) & 1) * 4 + 2; x < 128; x += 8) fw_fillrect(bmp, x, y, x, y, 1);
    int32_t det = (plx * dy - dx * ply) / 4096;
    int32_t order[HE], n = 0;                     /* the things, far to near */
    for (int32_t i = 0; i < HE; i++) if (h_e[i].alive) order[n++] = i;
    for (int32_t a = 0; a < n; a++) for (int32_t b = a + 1; b < n; b++)
        if (h_dist2(&h_e[order[b]]) > h_dist2(&h_e[order[a]])) { int32_t t = order[a]; order[a] = order[b]; order[b] = t; }
    for (int32_t k = 0; k < n && det; k++) {
        struct hent *e = &h_e[order[k]];
        int32_t sx = e->x - h_px, sy = e->y - h_py;
        int32_t tx = (dy * sx - dx * sy) / det, ty = (-ply * sx + plx * sy) / det;
        if (ty <= 32) continue;
        int32_t col = 32 + 32 * tx / ty, size = VH * 256 / ty;
        if (size > VH) size = VH;
        int32_t w = e->kind == 1 ? size * 2 / 3 : size / 3, h = e->kind == 1 ? size : size / 3;
        if (e->alive == 2) h = size / 6;
        int32_t base = horizon - size / 2, c0 = col - w / 4, c1 = col + w / 4;
        for (int32_t c = c0; c <= c1; c++) {
            if (c < 0 || c > 63 || ty >= h_zbuf[c]) continue;
            int32_t x = c * 2;
            fw_fillrect(bmp, x, base, x + 1, base + h, 0);                     /* cut it out of the wall */
            if (e->kind == 1 && e->alive == 1) {    /* a monster: body, horns, eyes, a mouth */
                fw_fillrect(bmp, x, base, x + 1, base + h * 4 / 5, 1);
                if (c == c0 || c == c1) fw_fillrect(bmp, x, base + h * 4 / 5, x + 1, base + h, 1);
                if (h > 10 && (c == col - w / 8 || c == col + w / 8)) fw_fillrect(bmp, x, base + h * 3 / 5, x + 1, base + h * 3 / 5 + 1, 0);
                if (h > 10 && c > c0 && c < c1) fw_fillrect(bmp, x, base + h / 4, x + 1, base + h / 4, 0);
            } else if (e->alive == 2) fw_fillrect(bmp, x, base, x + 1, base + h, (c & 1));   /* a body */
            else {                                  /* a pickup: a box, + for health */
                fw_framerect(bmp, x, base, x + 1, base + h, 1);
                if (e->kind == 2 && c == col) fw_fillrect(bmp, x, base + 1, x + 1, base + h - 1, 1);
                if (e->kind == 3) fw_fillrect(bmp, x, base, x + 1, base + h / 2, 1);
            }
        }
    }
    int32_t bob = (h_bob >> 2) & 1;              /* the gun, bobbing as you walk; a flash when it fires */
    fw_fillrect(bmp, 58, VY, 69, VY + 6 + bob, 1);
    fw_fillrect(bmp, 61, VY + 6 + bob, 66, VY + 12 + bob, 1);
    fw_fillrect(bmp, 63, VY + 12 + bob, 64, VY + 13 + bob, 0);
    if (h_flash > 3) { fw_fillrect(bmp, 60, VY + 14, 67, VY + 18, 1); fw_fillrect(bmp, 62, VY + 18, 65, VY + 21, 1); }
    if (h_hit_t > 0) { ui_line(bmp, 61, horizon - 3, 66, horizon + 2); ui_line(bmp, 61, horizon + 2, 66, horizon - 3); }
    else fw_fillrect(bmp, 63, horizon - 1, 64, horizon, -1);                /* the crosshair */
    if (h_hurt & 4) fw_fillrect(bmp, 0, VY, 127, 51, -1);                  /* hit: the screen flashes */
    fw_fillrect(bmp, 0, 0, 127, VY - 1, 0); fw_fillrect(bmp, 0, VY - 1, 127, VY - 1, 1);
    p = put(s, "HP "); p = ui_num(p, h_hp); p = put(p, "  AMMO "); p = ui_num(p, h_ammo); p = put(p, "  LV "); ui_num(p, h_lvl + 1);
    ui_text(bmp, 2, 0, s);
    if (h_state == G_PAUSE) box_msg(bmp, 30, "PAUSED");
}
static int32_t hel_key(void *view, void *event)
{
    int32_t key = g_keyid(event), press = g_press(event), down = g_down(event);
    if (key == K_YES) {
        if (press) { if (h_state == G_RUN) h_fire(); else if (h_state == G_PAUSE) h_state = G_RUN; else h_start(); }
        return 1;
    }
    if (h_state != G_RUN) return mp_key(view, event);
    if (key == K_NO) { if (press) h_use(); return 1; }
    if (key == K_UP) { h_up = down; return 1; }
    if (key == K_DOWN) { h_down = down; return 1; }
    if (key == K_LEFT) { h_left = down; return 1; }
    if (key == K_RIGHT) { h_right = down; return 1; }
    return 0;
}
static int32_t hel_enc(void *view, void *event)
{
    int32_t knob = *(int32_t *)((char *)event + 12), turn = *(int32_t *)((char *)event + 16);
    if (h_state == G_RUN && knob == 1) { h_pa = (h_pa + turn / 2) & 255; return 1; }
    if (h_state == G_RUN && knob == 2) {        /* sidestep */
        int32_t sx = -h_sin[h_pa & 255], sy = h_cos(h_pa), d = turn * 3;
        h_move(sx * d / 4096, sy * d / 4096);
        return 1;
    }
    return snd_knob(view, event);
}
static int32_t hel_busy(void) { return h_state == G_RUN; }

#define GAMES4 { "HELLTAKT", hel_draw, hel_key, hel_enc, 0, hel_busy, &hel_hi, 0 },
#define GAMES4_TICK(step) hel_tick(step)
#define GAMES4_LEAVE() do { h_up = h_down = h_left = h_right = 0; if (h_state == G_RUN) h_state = G_PAUSE; } while (0)
