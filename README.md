# DigiARCADE

**21 games for the Elektron Digitakt MKI**, on one master page, as an [Elekloader](https://github.com/elekloader/elekloader) mod. Many of them play along with your pattern: notes fall on your steps, enemies march to your tempo, and the games play your own track sounds. With LED COLORS in the build, the trig keys light up for every game.

- **Mod:** `digiarcade` 0.1
- **Digitakt MKI, OS 1.54 only**
- **Needs in the build:** `core` 4.4 and `seq-events`
- **For the trig lights:** `ledcolors` 1.0 or later ([DIGITAKT-LED-COLORS](https://github.com/ceredmeat/DIGITAKT-LED-COLORS)). Without it the games work the same, with the stock lights.

---

## Getting in

1. Press **FUNC + LFO** to open the master pages, then **LEFT / RIGHT** to the **ARC** page.
2. Pick a game with **UP / DOWN**, any knob, or a **trig key** (keys 1–16 open games 1–16; games 17–21 are further down the list).
3. **YES** opens it. **NO** goes back to the list (in a game that's running, NO pauses or ends it first).

While a game is open, the 16 trig keys belong to the game and don't edit your pattern. Leave the page and they're yours again. Games that can pause, pause when you leave.

**Knob D** in most games picks the track (OFF, 1–8) whose sound the game plays. The best scores are saved.

---

## The games

| # | Game | How to play |
|---|---|---|
| 1 | **RHYTHM GAME** | Your own pattern's trigs on tracks 1–8 fall down 8 lanes in time. Hit them on the trig keys (1–8, or 9–16 under them) as they reach the line: PERFECT within 40 ms, GOOD within 100 ms. Start the pattern, then **YES**; **NO** (or STOP) ends it. It learns the pattern as it plays: a trig counts once it has been heard for a loop. **Knob A:** how far ahead you see. |
| 2 | **SIMON** | The pads play a growing sequence; play it back. **YES** starts. **Knob A:** TRK (8 pads with the sounds of tracks 1–8, keys 1–8 or 9–16) or NOT (16 pads playing a pentatonic scale on the active track). |
| 3 | **BREAKOUT** | **Knob A** (or LEFT / RIGHT) moves the paddle, **YES** launches the ball, **NO** pauses. **Knob D:** a track whose sound plays on every brick, higher rows higher notes. |
| 4 | **SNAKE** | The **arrows** steer. **YES** starts, **NO** pauses. |
| 5 | **PONG** | **Knob A** moves your paddle. **YES** serves, **NO** pauses. **Knob B** (between games): 1 player against the Digitakt, or 2 players, with player 2 on **knob H**. |
| 6 | **TETRIS** | **LEFT / RIGHT** move, **UP** turns, **DOWN** drops faster, **YES** starts (and drops the piece straight down while playing). **NO** pauses. **Knob B:** pieces fall on the active track's steps while the pattern plays, or at their own speed. |
| 7 | **FLAPPY** | **YES** (or any trig key) flaps. **NO** pauses. **Knob B:** the pipes move at their own speed, or with the tempo. |
| 8 | **SPACE INVADERS** | **LEFT / RIGHT** (or **knob A**) move, **YES** starts and fires, **NO** pauses. The invaders march on the active track's steps while the pattern plays. |
| 9 | **WHACK-A-MOLE** | Moles pop up in a 4 × 4 grid laid out like the trig keys; hit them on the trig keys. **YES** starts, **NO** ends. **Knob B:** how often they pop up (on the pattern's steps while it plays). |
| 10 | **LIGHTS OUT** | The trig keys are a 4 × 4 grid (keys 1–4 the top row). A key flips its light and its neighbours'. Turn them all off. **YES** starts. |
| 11 | **EAR TRAINER** | **Knob B: INT:** two notes play on the active track; name the gap on the trig keys (key 1 the same note, key 2 a semitone … key 13 an octave). **TRK:** one of tracks 1–8 plays; which one (keys 1–8)? Ten questions a round; **YES** plays the question again. |
| 12 | **BEAT MATCH** | A hidden tempo plays (on the knob D track, or as a flash if that's OFF). Tap along on **YES** or any trig key. After 8 taps you score 100 points dead on, 10 fewer for each BPM off. Five rounds. |
| 13 | **LUNAR LANDER** | **YES** (hold) fires the main engine, **LEFT / RIGHT** the side ones. Land on the flat pad, slowly. |
| 14 | **TRON** | Player 1 steers with the **arrows** (or trig keys 1–4: left, down, up, right). Against the Digitakt, or with **knob B** on 2P, player 2 on trig keys 13–16. The cycles move on the active track's steps while the pattern plays. First to 5. |
| 15 | **MINESWEEPER** | 16 × 5 squares: a **trig key** picks the column, **UP / DOWN** the row, **LEFT / RIGHT** move along it. **YES** opens a square, **NO** flags it. **Knob B** gives up. |
| 16 | **HELLTAKT** | A first-person maze shooter, written from scratch for the 1-bit screen (a raycaster, not the Doom engine). **UP / DOWN** walk, **LEFT / RIGHT** turn (**knob A** turns finely, **knob B** steps sideways), **YES** fires, **NO** opens a door. The monsters close in on the active track's steps while the pattern plays; your shot plays the knob D track (a kick makes a good gun). Find the exit (the striped wall) on each of three levels. |
| 17 | **DRUM HERO 2P** | The rhythm game for two: player 1 on trig keys 1–8, player 2 on 9–16, the same falling notes, scored separately. |
| 18 | **PATTERN COPY** | The Digitakt plays a bar twice after a count-in; then, after another count-in, tap it back (**YES** or any trig key). Taps land on the nearest 16th. Get it right and the next one is longer. Three lives. |
| 19 | **PINBALL** | Trig keys 1–8 (or **LEFT**) the left flipper, 9–16 (or **RIGHT**) the right; **YES** puts a ball in play. Three bumpers, four lights across the top (all lit: double points), three balls. |
| 20 | **SEQUENCER PUZZLE** | A beat plays: one bar the target, the next yours, over and over. Rebuild it on the trig keys (a key sets or clears a step). From level 4 there's a second layer, picked with **knob B**. **YES** checks it. |
| 21 | **DUNGEON** | A turn-based roguelike: the **arrows** walk (walk into a monster to fight it); potions heal, gold scores. **YES** on the stairs goes down. |

---

## The trig lights

With `ledcolors` 1.0 or later in the build, every game lights the trig keys: the rhythm game's lanes, Simon's pads, the moles, the Lights Out grid, Pinball's flippers, the dungeon's surroundings and so on. The other keys glow in a neon theme while a game is open. Close the game (or leave the page) and your own colours come back.

---

## Installing

Add `digiarcade` (with `core` 4.4 and `seq-events`, and `ledcolors` for the lights) to your build in Elekloader, then flash it. Keep a stock OS image and a project backup: firmware modding always carries some risk.

The best scores and settings are kept in one 512-byte block of the Digitakt's internal storage (eMMC block `0x6004d`), outside what the stock firmware writes.

## Building from source

The mod's source is in [`digiarcade/`](digiarcade); it includes two small shared headers from [`common/`](common) (`heapbuf.h`, a heap buffer for saving, and `mpui.h`, the master page helpers). With Elekloader's SDK:

```
python3 -m elekloader.sdk.build digiarcade --stock Digitakt_OS1.54.syx --out build
```

No firmware is included in this repository: the `.syx` contains Elektron's firmware, so only the mod and its source are shared here.

## Tested on a unit

(add yours)

## Licence

GPL-2.0-or-later, the same licence as Elekloader, which the mod is built on. See [LICENSE](LICENSE). The licence does not cover Elektron firmware, trademarks or other third-party materials.

Copyright (c) 2026 CERELYDAT
