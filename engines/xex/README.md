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
- Digital: `vmd/<name>.vmd` is MP3 data with every byte XORed with `0xe5`. This requires a ScummVM build with MP3 support (`USE_MAD`).
- Music uses MIDI when it is enabled and falls back to the digital track if the MIDI file cannot be opened or parsed. Voices are always digital.
- Without MP3 support, voices and digital music are silent, and a warning is logged at startup.

### Save Data

- ScummVM save files begin with the big-endian magic `CMPL` and version byte 1, followed by the text position, the resume-state size, and the resume state.
- Quick save uses slot 99. Saving is unavailable in the `startup` and `kaisou` scenarios.
- The `<target>.common` file is 41 bytes. Byte 0 is 1 by default, bytes 1 to 32 hold one bit per gallery CG that has been seen, and bytes 34 to 37 hold the 32 persistent flags.

## Behavior Notes

- The non-framed message advance indicator cycles through 8 frames, one every 128 ms. The framed dialogue used by Case 266 cycles through 48 phases, one every 30 ms.
- Auto mode advances once the voice has finished and at least 128 ms per character shown has passed.
- Pixel-level layout, such as font size, indicator position, and frame artwork, has not yet been verified against every release.

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
