/* SPDX-License-Identifier: GPL-2.0-or-later */
/* DIGIARCADE: the trig lights (included after the menu). */

/* ==== the trig lights ============================================================================================ */
/* LED COLORS 1.0+ paints these on the trig keys while a game page is shown: per key 0 dark, else the colour
 * family + 1 (bits 0-3) and how dim, 0-4 (bits 4-6); [16] nonzero: paint them. */
volatile uint8_t digiarcade_leds[17];
enum { LR, LO, LY, LG, LT, LC, LS, LB, LL, LM, LW };      /* the families, as LED COLORS has them */
static uint8_t L(int32_t fam, int32_t dim) { return (uint8_t)((fam + 1) | (ui_clamp(dim, 0, 4) << 4)); }
static void leds_all(uint8_t c) { for (int32_t i = 0; i < 16; i++) digiarcade_leds[i] = c; }
static void leds_bar(int32_t n, int32_t max)     /* a gauge: green, then yellow, then red as it empties */
{
    int32_t lit = max ? n * 16 / max : 0, fam = lit > 8 ? LG : lit > 4 ? LY : LR;
    for (int32_t i = 0; i < 16; i++) digiarcade_leds[i] = i < lit ? L(fam, 0) : 0;
}
static uint32_t led_flash_until;
static uint8_t led_flash_c;
static void leds_flash(uint8_t c, int32_t len) { led_flash_c = c; led_flash_until = blks + (uint32_t)len; }
static void leds_update(int32_t on)
{
    uint32_t now = blks;
    if (!on) { digiarcade_leds[16] = 0; return; }
    static const uint8_t pad_fam[8] = { LR, LO, LY, LG, LC, LB, LL, LM };
    leds_all(0);
    switch (g_cur) {
    case -1:                                      /* the menu: a key for each game, the chosen one bright */
        for (int32_t i = 0; i < 16 && i < NGAMES; i++) digiarcade_leds[i] = L(pad_fam[i & 7], i == g_sel ? 0 : 3);
        break;
    case 0: case 16:                              /* rhythm, Drum Hero: the next hit in each lane glows as it nears */
        for (int32_t t = 0; t < 8; t++) {
            int32_t len = r_len(t), steplen = (int32_t)(r_len_q8[t] >> 8), s = (r_cur[t] + 1) % len;
            if (r_state == G_RUN && steplen > 8 && r_has(t, s)) {
                int32_t until = steplen - (int32_t)(now - r_at[t]);
                digiarcade_leds[t] = digiarcade_leds[t + 8] = L(LB, until * 4 / steplen);
            }
            for (int32_t pl = 0; pl < 2; pl++)        /* how you did: the key you hit, or both halves in 1P */
                if ((int32_t)(r_flash[pl][t] - now) > 0 && r_judge[pl] && now - r_judge_at[pl] < 150) {
                    uint8_t c = r_judge[pl][0] == 'P' ? L(LG, 0) : r_judge[pl][0] == 'G' ? L(LY, 0) : L(LR, 0);
                    if (r_two) digiarcade_leds[t + pl * 8] = c; else digiarcade_leds[t] = digiarcade_leds[t + 8] = c;
                }
        }
        break;
    case 1:                                       /* Simon: the pads in their colours */
        for (int32_t i = 0; i < (smn_mode ? 16 : 8); i++) {
            uint8_t c = L(pad_fam[i & 7], i == s_lit ? 0 : 4);
            if (s_state == S_OVER && i == s_wrong) c = L(LR, 0);
            digiarcade_leds[i] = c;
            if (!smn_mode) digiarcade_leds[i + 8] = c;
        }
        break;
    case 2:                                       /* Breakout: the bricks left above each key pair */
        for (int32_t c = 0; c < BCOLS; c++) {
            int32_t n = 0;
            for (int32_t r = 0; r < BROWS; r++) n += (b_bricks[r] >> c) & 1;
            digiarcade_leds[c * 2] = digiarcade_leds[c * 2 + 1] = n ? L(pad_fam[n], 5 - n) : 0;
        }
        if (b_state == G_RUN && !b_stuck) digiarcade_leds[ui_clamp((b_bx >> 8) / 8, 0, 15)] = L(LW, 0);   /* the ball */
        break;
    case 3:                                       /* Snake: where the food is, and your head */
        if (sn_len) { digiarcade_leds[ui_clamp(sn_fx / 2, 0, 15)] = L(LG, 0); digiarcade_leds[ui_clamp(snx[sn_head] / 2, 0, 15)] = L(LO, 0); }
        break;
    case 4:                                       /* Pong: the ball crossing the keys */
        if (p_state == G_RUN) digiarcade_leds[ui_clamp((p_bx >> 8) / 8, 0, 15)] = L(LW, 0);
        for (int32_t i = 0; i < p_s1 && i < 8; i++) digiarcade_leds[i] |= 0;
        break;
    case 5:                                       /* Tetris: each column's height */
        for (int32_t x = 0; x < TW; x++) {
            int32_t h = 0;
            for (int32_t y = 0; y < TH; y++) if ((tb[y] >> x) & 1) h = y + 1;
            digiarcade_leds[x + 3] = h ? L(h > 12 ? LR : h > 8 ? LO : h > 4 ? LY : LG, 1) : 0;
        }
        if (t_flash) leds_all(L(LW, 0));
        break;
    case 6:                                       /* Flappy: how high you fly */
        if (f_state == G_RUN) digiarcade_leds[ui_clamp((f_y >> 8) * 16 / 52, 0, 15)] = L(LY, 0);
        break;
    case 7:                                       /* Invaders: aliens left in each column */
        for (int32_t c = 0; c < ICOLS; c++) {
            int32_t n = 0;
            for (int32_t r = 0; r < IROWS; r++) n += (i_al[r] >> c) & 1;
            digiarcade_leds[c * 2] = digiarcade_leds[c * 2 + 1] = n ? L(LG, 4 - n) : 0;
        }
        if (i_boom) leds_all(L(LR, 0));
        break;
    case 8:                                       /* Whack-a-mole: green when up, red as it's about to go */
        for (int32_t i = 0; i < 16; i++)
            if (w_up[i]) { int32_t left = (int32_t)(w_up[i] - now) * 4 / w_stay(); digiarcade_leds[i] = L(left > 2 ? LG : left > 0 ? LY : LR, 0); }
        if (w_hit_at >= 0 && (int32_t)(w_hit_until - now) > 0) digiarcade_leds[w_hit_at] = L(LW, 0);
        break;
    case 9:                                       /* Lights Out: the lights themselves */
        for (int32_t i = 0; i < 16; i++) digiarcade_leds[i] = (l_on >> i) & 1 ? L(LY, 0) : L(LB, 4);
        break;
    case 10:                                      /* ear trainer: the keys you can answer with; then right or wrong */
        for (int32_t i = 0; i < (ear_mode ? 8 : 13); i++) digiarcade_leds[i] = L(ear_mode ? pad_fam[i] : LS, 3);
        if (e_state == G_PAUSE) { digiarcade_leds[e_answer] = L(LG, 0); if (e_said != e_answer && e_said >= 0) digiarcade_leds[e_said] = L(LR, 0); }
        break;
    case 11:                                      /* beat match: every hidden beat flashes; your taps fill up */
        if (m_state == G_RUN && now - m_beat_at < 120) leds_all(L(LM, 1));
        for (int32_t k = 0; k < m_taps && k < 8; k++) digiarcade_leds[k + 4] = L(LG, 0);
        break;
    case 12: leds_bar(d_fuel, 900); if (d_crash) leds_all(L(LR, 0)); break;      /* lander: the fuel */
    case 13:                                      /* Tron: whose keys are whose */
        for (int32_t i = 0; i < 4; i++) { digiarcade_leds[i] = L(LO, 1); if (p_two) digiarcade_leds[i + 12] = L(LC, 1); }
        break;
    case 14:                                      /* Minesweeper: the column you're on */
        digiarcade_leds[n_cx] = L(LW, 0);
        if (n_state == G_OVER) leds_all(L(LR, 0));
        break;
    case 17:                                      /* Pattern Copy: the beat as it plays; then your taps against it */
        if (pc_state == PC_PLAY && pc_pos >= 16 && ((pc_target >> ((pc_pos - 16) & 15)) & 1)) digiarcade_leds[(pc_pos - 16) & 15] = L(LC, 0);
        if (pc_state == PC_YOU || pc_state == PC_RESULT) for (int32_t i = 0; i < 16; i++) if ((pc_mine >> i) & 1) digiarcade_leds[i] = L(LG, 0);
        if (pc_state == PC_RESULT) for (int32_t i = 0; i < 16; i++) if (((pc_mine ^ pc_target) >> i) & 1) digiarcade_leds[i] = L((pc_mine >> i) & 1 ? LR : LY, 0);
        if ((pc_state == PC_PLAY || pc_state == PC_YOU) && pc_pos >= 0 && pc_pos < 16 && (pc_pos & 3) == 0) leds_all(L(LW, 2));   /* the count-in */
        break;
    case 18:                                      /* Pinball: the flippers you hold; a bumper flash */
        for (int32_t i = 0; i < 8; i++) { digiarcade_leds[i] = pb_up[0] ? L(LO, 0) : L(LO, 4); digiarcade_leds[i + 8] = pb_up[1] ? L(LO, 0) : L(LO, 4); }
        if (pb_flash) leds_all(L(LM, 0));
        break;
    case 19:                                      /* the Sequencer Puzzle: your steps, and the playhead */
        if (sq_state == G_RUN) {
            for (int32_t i = 0; i < 16; i++) if ((sq_mine[sq_layer] >> i) & 1) digiarcade_leds[i] = L(sq_layer ? LM : LG, 1);
            digiarcade_leds[sq_step & 15] = L(sq_bar ? LW : LB, 0);
        }
        break;
    case 20:                                      /* the Dungeon: your health */
        leds_bar(dg_hp, dg_max ? dg_max : 1);
        if (dg_hurt) leds_all(L(LR, 0));
        break;
    case 15:                                      /* HELLTAKT: your health, red when you're hit, white when you fire */
        leds_bar(h_hp, 100);
        if (h_hurt) leds_all(L(LR, 0));
        else if (h_flash > 3) leds_all(L(LW, 0));
        break;
    }
    if ((int32_t)(led_flash_until - now) > 0) leds_all(led_flash_c);
    digiarcade_leds[16] = 1;
}

