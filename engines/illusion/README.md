# Illusion

ScummVM engine for *Sexy Beach 2* (game ID `sexybeach2`), the Japanese Windows release. The engine is experimental, is enabled by default, and currently covers startup, the title menu, and the character-selection scene. Gameplay, the generic script VM, and most scenes are not implemented.

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
| Character selection (menu choice 0) | Implemented as an interactive scene. Confirm stops with an error because the swimsuit step is not implemented |
| Menu choice 4 (fifth button) | Ends startup without error |
| Menu choices 1, 2, 3 | Stop with a "not implemented" error |
| Swimsuit, accessories, fitting, 3D previews, map, day events | Not implemented |
| Scene logic, gameplay, generic script VM | Not implemented |
| Save and load | Not implemented (`hasFeature` only reports return-to-launcher) |
| Debugger | Placeholder `test` command only |

### Menu Choices

The menu reports the selected button index. Scene state values are the ones requested by the code, and their meaning in the original game is a REA observation.

| Choice | Scene state requested | ScummVM behavior |
| --- | --- | --- |
| 0 | `0x2` (new game; character selection is its first step, REA observation) | Runs the character-selection scene. Confirm stops with "The swimsuit selection is not implemented yet". Back returns to the title menu, which replays the title reveal and restarts `BG0000` |
| 1 | `0xc` | Stops with an error |
| 2 | `0x11` (audio settings, REA observation) | Stops with an error |
| 3 | `0xd` | Stops with an error |
| 4 | None | Ends startup without error |

Pressing Escape on the title menu ends startup. Escape has no effect in character selection.

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

- `loadStartupResources()` reads `sb2_007` and decodes:
  - `fadeW.bmp`, uploaded as the white scene fader. It is drawn only in the fallback path described under Rendering Notes.
  - `icon_17.tga`, uploaded as the software pointer (see Pointer below).
  - `loading.bmp` and `f1omake_01.tga`. These are decoded for debug output only and are not displayed.
- `loadMenuResources()` reads menu images from `sb2_029` and audio from the embedded package `sb2_032`.
- `loadCharacterSelection()` reads one BMP and 15 TGA images from `sb2_000` and 25 TGA images from `sb2_028`. It then loads the portrait and profile of the girl being shown. Sounds come from the embedded `sb2_032` package.

### Pointer

`icon_17.tga` is drawn at 64x64 layout units at the mouse position on the ready title menu and in character selection. The engine uses that size without checking the image dimensions. The engine does not hide the system cursor (there is no `showMouse` call). The original hides it, as described under REA Observations.

## Character Selection

Implemented. Entered from title menu choice 0. Coordinates are in 1024x768 layout units, scaled to the screen by `menuRectangle()`.

### Assets

- `sb2_000`:
  - `character.bmp`: background.
  - `select00.tga`: title, at (388,30), 420x56.
  - `select01.tga`: "Profile" label, at (680,165), 160x72.
  - `kyarawaku.tga`: grid, at (494,433), 516x238.
  - `kyarasen.tga`: selection frame, 104x118.
  - `botan00_00/01` (confirm) and `botan01_00/01` (back): 248x59 at x = 495 + 265*i, y = 699. `_00` is the highlighted variant.
  - `time00.tga` to `time06.tga`: badges.
  - `kaoNN.tga`: portrait, drawn at 1024x768. `profileNN.tga`: profile, at (495,235), 513x190. NN is the girl number of the shown girl.
- `sb2_028`:
  - `time_moji00` to `time_moji09`: day digits. The tens digit is at (833,26) and the ones digit at (853,26), each 29x35.
  - `time2_moji01` to `time2_moji06`: time-slot digit, at (977,70), 19x29.
  - `time03_01` to `time03_09`: girl-name labels, at (875,74), 84x26.

### Grid

- Cells: top row at x = 598 + 102*i, y = 435. Bottom row at x = 649 + 102*i, y = 551. Each cell is 104x118.
- Slots 0 to 5 map to girl numbers 0, 1, 2, 5, 7, and 8. The girl numbers are used in resource names.
- Badge positions: (626 + 102*i, 527) and (677 + 102*i, 643), 48x20.

### Flow

- The scene fades in from white over 1000 ms and loops `BG0002`.
- The pointer starts at the centre of the confirm button. Confirm is highlighted at the start.
- Hovering a button highlights it and plays `SE0004`. The highlight stays on the last hovered button. Hovering the button that is already highlighted plays nothing.
- Clicking a grid cell plays `SE0003`, even when it is the girl already shown. The selection frame and name label move to the clicked cell. If the girl changed, her portrait and profile reload, and the portrait slides from x = -512 to 0 over 200 ms while fading in over 400 ms.
- Confirm plays `SE0000` and stores the shown girl as the current girl. Back plays `SE0002` and leaves the current girl unchanged. Either one fades out to white over 1000 ms.
- Confirm stops startup with the swimsuit error. Back returns to the title menu.
- Only the mouse is read in this scene. Keyboard input is ignored.

### Defaults

The engine starts at day 1 with girl 0. There is one day counter and one current girl. No per-girl day counts or Clear flags are stored yet, so all six cells draw `time01.tga` ("Day 1"). Save games are not supported.

## Rendering Notes

Implemented in [renderer.cpp](renderer.cpp).

- Once the portrait finishes fading in, the scene behind the buttons is cached with `cacheScene()` and restored with a `memcpy` on later frames. A cell click invalidates the cache, and it is rebuilt after the next full draw.
- The frame is redrawn only when the pointer moves, a mouse button is pressed, an animation or fade runs, or a focus or screen event requests it.
- In 32-bit RGB modes, the fade to white is applied directly to the framebuffer. In other modes, `drawFadeToWhite()` draws `fadeW.bmp` stretched over the screen.
- `drawImage` clips destinations that are partly off-screen and maps only the visible part of the source image.

## Architecture

- [illusion.cpp](illusion.cpp) and [illusion.h](illusion.h): engine core. Startup, data validation, display setup, menu flow, character selection, input, and resource loading.
- [archive.cpp](archive.cpp) and [archive.h](archive.h): package reader for both formats above.
- [renderer.cpp](renderer.cpp) and [renderer.h](renderer.h): TinyGL renderer. Creates textures from surfaces, draws backgrounds and images with opacity, supports fade-from-black and fade-to-white, caches and restores a scene, and flips the buffer.
- [detection.cpp](detection.cpp), [detection_tables.h](detection_tables.h), and [detection.h](detection.h): game detection.
- [metaengine.cpp](metaengine.cpp) and [metaengine.h](metaengine.h): meta engine. Creates the engine instance and reports no extra features.
- [console.cpp](console.cpp) and [console.h](console.h): debugger with a placeholder command.
- [module.mk](module.mk): build rules. Also supports plugin builds through `ENABLE_ILLUSION`.

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

## Testing

The archive unit suite is in [test/engines/illusion/archive.h](../../test/engines/illusion/archive.h). It covers table decoding, out-of-bounds directory entries, short reads, and embedded byte negation. Run it with:

```sh
make test
```

## Unverified REA Observations

These are not implemented or checked against ScummVM behavior. Addresses are in `SexyBeach2DVD.exe`.

- The `.ix` index range in the notes runs from `sb2_000.ix` to `sb2_3423.ix`. The engine checks only `sb2_000.ix` through `sb2_002.ix`, and it does not parse `.ix` contents.
- Package `sb2_029` has a decoded table with 3424 record slots, of which the notes report 14 as valid. The engine skips records with empty names, but it does not validate the rest of the table.
- Package `sb2_032` has an unknown value at offset 4 in its header.
- The meanings of scene states `0xc`, `0x11`, and `0xd` are not verified.

### Scene Dispatch And New Game

- The global scene state is at `0x8b34b0`. Five tables switch on it: init `0x4c3b6f`, release `0x4c3c35`, draw `0x4c3a26`, update `0x4c38ce`, and a 3D pass at `0x4c37fb` that is used only by states 2, 4, 5 and 8.
- Scene state 2 is the new game. Its sub-step byte is at `0x8858c0`:
  - Step 0, character select: init `0x4833a6`, release `0x483fd6`, draw `0x484030`, update `0x484759`. The implemented character-selection scene corresponds to this step.
  - Step 1, swimsuit (`sb2_001`): init `0x484e6b`, update `0x4861aa`.
  - Step 2, accessories (`sb2_002`): init `0x4867b9`, update `0x4879ad`.
  - Step 3, fitting confirm (`sb2_003`): init `0x488173`, update `0x488b7c`.
  - Steps 1 to 3 render a 3D preview of the girl from DirectX `.x` meshes with `.xan` animations (for example `sb2_006` `e20_xx.xan` and `sb2_018`). Each step's back button returns to the previous step.
- The character-select back button returns to scene 0 (title) when the previous scene state at `0x8b34b4` was 0. Otherwise it returns to scene `0xf`. The engine always returns to the title.
- After step 3, if the girl's "met" flag is 0, the game goes to scene 3, the 2D map (`sb2_019` map images, `BG0003` to `BG0005`; init `0x48e0c3`, release `0x48ece4`, draw `0x48ed55`, update `0x48f006`). A fresh new game takes this path. The flag is field `+0x10` of the per-girl records, which are 0x204 bytes each at `0x899520`. The check is at `0x4c0e10`. If the flag is set, the game goes to scene 4 (init `0x4a2e0c`, plays `m0001.mpg`, 3D beach) when the girl's day count (record `+0x8`) is below 2. Otherwise it goes to scene `0x12`.
- Day 1 script: `sb2_031` contains event script text. `t01_00.txt` starts with a Day 1 morning event that fires on map selection after the dress-up finishes. Lines are tagged such as `#110000#K00#PT1000` (voice and expression codes).

### Presentation And API

- The original hides the system cursor with `ShowCursor(0)` and draws the pointer `icon_17.tga` instead. The engine does not hide the system cursor.
- `fadeW.bmp` is a uniformly white 128x128 image that the original stretches to 1024x768. The engine does not check its size or colour.
- `f1omake_01.tga` is a 468x48 overlay at (540,710), drawn together with `loading.bmp`. The engine does not draw it.
- Sprite API: `0x4c1ca7` creates a sprite from (path, w, h, x, y, origin x/y, scale x/y, alpha, visible), in layout units multiplied by the screen scale at `0x8b349c`. The button-group hit test at `0x4c2473` returns 1 on a hover change and 2 or 3 on a click, and keeps the last hovered index.
- Tooling limitation: the Ghidra decompiler times out on most scene functions (x87-heavy code). These findings come from annotated disassembly.
