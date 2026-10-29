# XEX

Implementation of Complet's XEX and C4 visual novel engines, registered in
ScummVM as `xex`. Gameplay, graphics, audio, message history, save/load,
and CG galleries are implemented. The engine remains experimental; detection
does not imply that every route or feature has been verified.

Existing launcher entries need their engine ID updated to `xex`, or the games
can be added again. Per-game keymapper overrides now use the
`keymap_xex-default_` configuration prefix.

## Detected Releases

All currently detected releases are Japanese Windows versions.

| Game ID | Title | Resolution |
| --- | --- | --- |
| `collection` | Collection ~Shuushuu Sareta Akumu~ | 640x480 |
| `case266` | Case 266 ~266 Gou Jiken~ | 640x480 |
| `bokunohimitsutaiken` | Boku no Himitsu Taiken | 640x480 |
| `theguts` | The Guts! | 640x480 |
| `koisuruscience` | Koi Suru Science | 640x480 |
| `mottohimitsutaiken` | Motto! Himitsu Taiken | 640x480 |
| `boseimamaclub` | Bosei ~Mama Club~ | 640x480 |
| `matty` | Matty | 640x480 |
| `yagai` | Yagai Gakushuu | 800x600 |
| `mamatuma` | Mama! Tsuma? | 800x600 |
| `mousugunatsuyasumi` | Mousugu Natsuyasumi! | 800x600 |
| `thegutsmaximummaternity` | The Guts! -Maximum Maternity!- | 800x600 |

Matty and Mama! Tsuma? each have an additional detected `Updated` variant.
Detection matches `mpx/startup.mpx` by size and MD5, using ScummVM's default
first-5000-byte hashing (the entire file for shorter files). Matty also matches
`mpx/e-home.mpx` to distinguish its variants. Detection does not require the
original Windows executable.

## Game Data

Point ScummVM at the game data root, preserving the original subdirectories,
including `mpx`, `gds`, `vmd`, and `mus` where present. These contain scripts,
graphics, digital audio, and MIDI music respectively.

Japanese text requires ScummVM's `encoding.dat` and a Japanese font available
through the game directory or ScummVM's extra path. The engine tries these
font filenames in order:

- `NotoSansCJK-Regular.ttc`
- `NotoSansCJKjp-Regular.otf`
- `NotoSerifCJK-Regular.ttc`

The build requires 16-bit color, high-resolution graphics, FreeType2, and MP3 support (`mad`).
Enable the engine with `./configure --enable-engine=xex`, then build with
`make -j2`. It is enabled by default.

The original `c4.exe` or `xex.exe` is optional. When one is available, the arrow cursor is read from its resource 104; otherwise a default cursor is used.

## Engine Options

Depending on the game, engine options include animated text, a separate
speaker-name frame, and MIDI music in place of digital tracks. MIDI-only games
use the configured MIDI device. Input mappings can be customized through
ScummVM's keymapper, including advance, skip, auto mode, message history,
quick save, and quick load.

## Architecture

- [XexEngine](xex.cpp) handles startup, text and command dispatch, rendering, the message window, input, and auto mode. Save and load code is in [saveload.cpp](saveload.cpp), and the CG gallery and map code is in [gallery.cpp](gallery.cpp).
- [Script](script.cpp) runs MPX scenarios loaded by [Scenario](scenario.cpp). The game starts at the `startup` scenario.
- [GdImage](gd.cpp) decodes the GD images used for backgrounds, sprites, and the message frame.
- [AudioPlayer](audio.cpp) plays MIDI music, digital music, and digital voices.
- [Detection tables](detection_tables.h) assign each release to an engine family, XEX (640x480) or C4 (800x600). Where the families differ, the source branches on the family: the message advance indicator and the message buttons are drawn from different regions of the frame image, and black keying for sprites applies only to the XEX family.

## Data and File Formats

### Directory Layout

- `mpx/`: scenario scripts, such as `startup.mpx`.
- `gds/`: GD images. `mesframe.gd` holds the message frame and glyph sheet; `null.gd` is its fallback if `mesframe.gd` is missing.
- `vmd/`: digital voices and music.
- `mus/`: MIDI music.

### GD Images

- GD2 images decode at 640x480, with pixel data starting at 0x2346. GD3 images decode at 800x600, with pixel data starting at 0x3756. The legacy untagged GD path also decodes at 800x600, with pixel data starting at offset 2.
- The compression type is the byte two bytes before the pixel data: `b` is raw (GD3 only), `l` is LZSS with a 64 KB ring buffer, and `p` is a predictive sparse pixel stream.
- Decoded images are 24-bit BGR and stored bottom-up.
- If the frame image dimensions do not match the detected game, startup fails with a warning.

### Scenarios (MPX)

- Files begin with the `Mp17` magic. Little-endian 16-bit header fields give the text offset (0x04), the text size (0x06), and the local variable count (0x08). The command stream begins at 0x10.
- The text section is decoded with XOR 0x24 on load. Dialogue is read as Windows-932.
- `%` commands cover graphics (`%g`, `%o`), music (`%m`), voices (`%v`), flags (`%s`), gallery (`%t`), scenario jumps (`%j`), waits and clicks (`%w`, `%k`, `%r`), and quit (`%q`).

### Audio

- MIDI: `mus/<name>.mid` is a standard MIDI file.
- Digital: `vmd/<name>.vmd` is MP3 data with every byte XORed with `0xe5`. `configure.engine` declares `mad` as a dependency, so the engine is not built without MP3 support.
- Music uses MIDI when it is enabled and falls back to the digital track if the MIDI file cannot be opened or parsed. Voices are always digital.

### Save Data

- ScummVM save files begin with the big-endian magic `CMPL` and version byte 2, followed by the text position, the signed 16-bit click-minigame score, the resume-state size, and the resume state. Version 1 saves remain loadable and default the score to zero.
- Quick save uses slot 99. Saving is unavailable in the `startup` and `kaisou` scenarios.
- The `<target>.common` file is 41 bytes. Byte 0 is 1 by default, bytes 1 to 32 hold one bit per gallery CG that has been seen, and bytes 34 to 37 hold the 32 persistent flags.

## Behavior Notes

- The non-framed message advance indicator cycles through 8 frames, one every 128 ms. The framed dialogue used by Case 266 cycles through 48 phases, one every 30 ms.
- Auto mode advances once the voice has finished and at least 128 ms per character shown has passed.
- Pixel-level layout, such as font size, indicator position, and frame artwork, has not yet been verified against every release.

## The Guts! Clicking Minigame

`%e,12;` starts a modal rapid-click minigame, not a graphics transition.
The engine implements the timed interaction and exposes the completed score
through `Script::getValue()` at variable index `0x1fd`.

### Original Behavior

Static analysis of the supplied x86 `XEX.EXE` established the following
sequence. Times are nominal, derived from the original polling loops:

| Phase | Duration | Behavior |
| --- | --- | --- |
| Ready | 1 second | Draw the CP932 label "youi" at (36, 340). |
| Start | 20 ticks of 250 ms | Draw "hajime!", reset the score to zero, and update the gauge while accepting clicks. |
| Stop | 2 seconds | Draw "sokomade!", retain the score at the end of the scoring phase, then resume the script. |

These labels mean "Ready", "Start!", and "Stop!". The original draws their
double-byte characters at 56-pixel horizontal intervals. The ready and stop
loops sleep for 8 ms per iteration; the scoring loop sleeps for 4 ms.

- Score left-button **releases**, not presses or held-button frames. The
	scoring branch does not test the pointer position.
- Each release adds one point, or two while Shift is held (`MK_SHIFT`,
	`wParam & 4`), capped at 99. Keyboard input does not enter this scoring path.
- The score is held at executable address `0x442868`. The VM variable resolver
	exposes its low 16 bits as index `0x1fd` (encoded reference `0xfe02`).
- The routine snapshots the score before the stop pause and restores that
	snapshot on completion, so releases during the pause do not change the result.
- The gauge uses the existing graphics as a mask, recoloring only BGR
	`(0, 2, 2)` pixels. For nonzero score, it visits 60 rows at screen y 210..269,
	starting at x 13, and `6 * (score + 1)` columns. Its replacement BGR color is
	`(0, g, g)`, where `g = min(255, 127 + 3 * score)`, producing yellow in RGB.
	The invalidated rectangle is `(12, 210, 19 + 6 * score, 270)`.

The six script call sites are in decoded text in `DAY01.MPX` at text-relative
offsets 56568, 56734, and 56900, and `DAY02.MPX` at 24754, 24920, and 25086.
Each follows `%gf,ga_clk1,renw;`, `%gf,ga_clk2,renw;`, or
`%gf,ga_clk3,renw;`, respectively. The script supplies the artwork; the effect
does not select one of these backgrounds itself.

### Evidence and Implementation Boundary

The analyzed executable has SHA-256
`a416bb90fb03afb153d8b6f950e759c7aebe3b3c6cd6d930e6d5a26e312cdddb`.
Addresses below are virtual addresses with image base `0x400000`:

- Effect dispatch: parser `0x4083c0`, `%e` branch `0x408e08`, numeric dispatch
	`0x408e43`, case 12 at `0x408ea4`, callback `0x4074d0`, and main routine
	`0x4026e0`. The callback sets modal mode `0x4428e8` to 3 and resumes the
	script after the routine returns.
- Input: `WM_LBUTTONUP` (`0x202`) at `0x40bbf2` reaches `0x40c9b7`; modal
	jump table `0x40d568` maps mode 3 to score increment code `0x40cb65`.
- Result: resolver `0x4033c0` reads the score for index `0x1fd` at `0x403425`.
- REA/Ghidra 12.1.2 evidence: effect switch
	`ev_d2a21afd25a5d9942eb5cd621c40ca635f1943dba2f1da09a3d16054bffff7f5`,
	main routine
	`ev_e5090ee7ff118466426347fede959c6f7b0434f39526a3d2346224458f4dc36b`,
	and result resolver
	`ev_7527758024da873a5ee382816faea5d58740d4727168601a8b8416993a8e04c5`.
	Undefined callback and window-procedure boundaries were checked against raw
	disassembly of the same executable.

The implementation uses a modal event loop with the existing image buffers
and Japanese font, suppresses normal advance/skip/auto and menu actions, and
supports quit. Save/load preserves the completed score; replay restores the
gauge without playing the minigame again. User testing confirmed that the
minigame works and the game can be completed.

The original-executable investigation is static only and specific to this executable. Original
Windows execution, actual timing, and pixel-for-pixel rendering have not been
validated; these findings should not be generalized to every XEX release.

## References

- [Game names and engine registration](detection.cpp)
- [Release fingerprints and per-game settings](detection_tables.h)
- [Engine options and input mappings](metaengine.cpp)
- [Graphics decoders](gd.cpp)
- [Script interpreter](script.cpp)
- [Scenario loader](scenario.cpp)
- [Audio playback](audio.cpp)
- [Save and load](saveload.cpp)
- [CG gallery and map](gallery.cpp)
- [Engine core](xex.cpp)
