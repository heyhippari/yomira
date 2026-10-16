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
| `koisuruscience` | Koi Suru Science | 640x480 |
| `mottohimitsutaiken` | Motto! Himitsu Taiken | 640x480 |
| `boseimamaclub` | Bosei ~Mama Club~ | 640x480 |
| `matty` | Matty | 640x480 |
| `yagai` | Yagai Gakushuu | 800x600 |
| `mamatuma` | Mama! Tsuma? | 800x600 |
| `mousugunatsuyasumi` | Mousugu Natsuyasumi! | 800x600 |

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

The build requires 16-bit color, high-resolution graphics, and FreeType2.
Enable the engine with `./configure --enable-engine=xex`, then build with
`make -j2`. It is not enabled by default.

## Engine Options

Depending on the game, engine options include animated text, a separate
speaker-name frame, and MIDI music in place of digital tracks. MIDI-only games
use the configured MIDI device. Input mappings can be customized through
ScummVM's keymapper, including advance, skip, auto mode, message history,
quick save, and quick load.

## References

- [Game names and engine registration](detection.cpp)
- [Release fingerprints and per-game settings](detection_tables.h)
- [Engine options and input mappings](metaengine.cpp)
- [Graphics decoders](gd.cpp)
- [Script interpreter](script.cpp)
