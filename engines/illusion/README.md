# Illusion

ScummVM engine for *Sexy Beach 2* (game ID `sexybeach2`), the Japanese Windows release. The engine is experimental, is enabled by default, and currently covers only startup, the title menu, and resource loading for the character-selection entry. Gameplay, the generic script VM, and most scenes are not implemented.

Labels used below:

- **Implemented**: verified in the engine source.
- **REA observation**: taken from reverse-engineering notes. Not verified against the ScummVM code and not implemented.

## Current Status

| Area | Status |
| --- | --- |
| Game detection | Implemented (`sexybeach2`, Japanese, Windows) |
| Data validation at startup | Implemented |
| Opening movie | Skipped (no playback) |
| Title menu (background, logo, five buttons, title music, selection sound) | Implemented |
| Menu choice 0 (character selection) | Loads resources, then stops with a "not implemented" error |
| Menu choice 4 (fifth button) | Ends startup without error |
| Menu choices 1, 2, 3 | Stop with an "not implemented" error |
| Scene logic, gameplay, generic script VM | Not implemented |
| Save and load | Not implemented (`hasFeature` only reports return-to-launcher) |
| Debugger | Placeholder `test` command only |

### Menu Choices

The menu reports the selected button index. Scene state values are the ones requested by the code, and their meaning in the original game is a REA observation.

| Choice | Scene state requested | ScummVM behavior |
| --- | --- | --- |
| 0 | `0x2` (character selection) | Loads 18 images and `BG0002` music, then stops with an error |
| 1 | `0xc` | Stops with an error |
| 2 | `0x11` (audio settings, REA observation) | Stops with an error |
| 3 | `0xd` | Stops with an error |
| 4 | None | Ends startup without error |

Pressing Escape ends startup.

## Build And Setup

`configure.engine` requires the `16bit`, `highres`, and `tinygl` features: the
engine renders through TinyGL at 640x480 or higher in 16 or 32 bits per pixel.

```sh
./configure --enable-engine=illusion
make -j2
```

Point ScummVM at a directory that contains a `Data/` subfolder with the game files. The engine looks for these files through the search path:

- **Required**
  - `Data/sune.rrr` (12 bytes). Used for detection and existence checks only. It is not parsed as script data.
  - `Data/000_640.FTT` and `Data/000_1024.FTT`
  - `Data/sb2_000.ix` through `Data/sb2_002.ix`
  - `Data/sb2_000.pp` through `Data/sb2_035.pp`
  - `Data/sb2_NNN.tbl` for every package except `031`, `032`, and `033`
- **Optional**
  - `setting.cfg` in the game directory. Files shorter than 0x44 bytes are ignored. Otherwise the engine reads these little-endian words:
    - `0x10`: texture filtering (non-zero selects linear filtering).
    - `0x20`, `0x24`, `0x28`: width, height, and depth. Width must be 320 to 4096, height 200 to 2160, and depth 16, 24, or 32. Otherwise the engine uses 640x480 at 32 bits per pixel.
    - `0x40`: UI scale index.

Detection is in [detection.cpp](detection.cpp) and [detection_tables.h](detection_tables.h). It matches `Data/sb2_000.tbl`, `Data/sb2_000.pp`, and `Data/sune.rrr` by exact size and MD5 checksum.

## Startup And Menu Flow

The startup state machine is `runStartup()` in [illusion.cpp](illusion.cpp), with phases defined in [illusion.h](illusion.h):

1. `kMoviePending`: the opening movie is skipped. No video is decoded.
2. `kMenuBackground`: the menu background fades in over about one second.
3. `kMenuButtons`: five buttons appear in a staggered sequence over about 900 ms.
4. `kMenuReady`: the menu accepts mouse and keyboard input. Title music (`BG0000`) loops, and a selection sound (`SE0000`) plays on confirmation.
5. `kMenuLeaving`: the menu fades out, then the requested choice is handled as in the table above.

Resource loading:

- `loadStartupResources()` reads `sb2_007` and decodes `fadeW.bmp`, `loading.bmp`, and `f1omake_01.tga`. The decoded images are not displayed.
- `loadMenuResources()` reads menu images from `sb2_029` and audio from the embedded package `sb2_032`.
- `loadCharacterSelection()` reads 17 TGA sprites and one BMP background from `sb2_000`, plus `BG0002` audio.

## Archive Format

Implemented in [archive.cpp](archive.cpp) and [archive.h](archive.h). `IllusionArchive` handles two package layouts.

**Table-indexed packages (`.pp` with `.tbl`)**

- The table starts with a little-endian `uint32` size, XORed with `0xa67f54cb`.
- Complete 32-bit words in the packed body are XORed with `0x35d8ca2f` before the body is decoded with LZSS. Trailing partial bytes are not XORed.
- The first 0x800 bytes are a 256-entry directory of (offset, count) pairs, indexed by the lowercased first byte of the member name.
- Each member record is 0x50 bytes: data offset, size, and a 64-byte name.
- Lookups are case-insensitive and scan only the matching directory group.
- `open()` checks directory group spans and the bounds of nonempty records through the highest referenced index. A short member read clears the output buffer.

**Embedded packages (`openEmbedded()`, used by `sb2_032.pp`)**

- A `uint32` count is at offset 0. The parser does not read offset 4. (REA observation: its meaning is unknown.)
- Names follow at offset 8, as fixed 32-byte fields. Sizes follow the names as `uint32` values. Payloads follow the size table in order.
- All names and payloads are byte-negated.
- The total of all payload sizes must match the file size exactly, or the open fails.

## Architecture

- [illusion.cpp](illusion.cpp) and [illusion.h](illusion.h): engine core. Startup, data validation, display setup, menu flow, input, and resource loading.
- [archive.cpp](archive.cpp) and [archive.h](archive.h): package reader for both formats above.
- [renderer.cpp](renderer.cpp) and [renderer.h](renderer.h): TinyGL renderer. Creates textures from surfaces, draws backgrounds and images with opacity, supports fade-from-black, and flips the buffer.
- [detection.cpp](detection.cpp), [detection_tables.h](detection_tables.h), and [detection.h](detection.h): game detection.
- [metaengine.cpp](metaengine.cpp) and [metaengine.h](metaengine.h): meta engine. Creates the engine instance and reports no extra features.
- [console.cpp](console.cpp) and [console.h](console.h): debugger with a placeholder command.
- [module.mk](module.mk): build rules. Also supports plugin builds through `ENABLE_ILLUSION`.

## Testing

The archive unit suite is in [test/engines/illusion/archive.h](../../test/engines/illusion/archive.h). It covers table decoding, out-of-bounds directory entries, short reads, and embedded byte negation. Run it with:

```sh
make test
```

## Unverified REA Observations

These are not implemented or checked against ScummVM behavior:

- The `.ix` index range in the notes runs from `sb2_000.ix` to `sb2_3423.ix`. The engine checks only `sb2_000.ix` through `sb2_002.ix`, and it does not parse `.ix` contents.
- Package `sb2_029` has a decoded table with 3424 record slots, of which the notes report 14 as valid. The engine skips records with empty names, but it does not validate the rest of the table.
- Package `sb2_032` has an unknown value at offset 4 in its header.
- The meanings of scene states `0xc`, `0x11`, and `0xd` are not verified.
