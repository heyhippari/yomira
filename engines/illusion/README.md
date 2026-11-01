# Illusion

ScummVM engine for *Sexy Beach 2* (game ID `sexybeach2`), the Japanese Windows release. The engine is experimental, is enabled by default, and currently covers startup, the title menu, character selection, and swimsuit selection (the first two steps of the new game). Gameplay, the generic script VM, and most scenes are not implemented.

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
| Character selection (menu choice 0, new-game step 0) | Implemented as an interactive scene. Confirm starts swimsuit selection; back returns to the title menu |
| Swimsuit selection (new-game step 1) | Implemented as an interactive scene. Confirm stops with an error because accessory selection is not implemented; back returns to character selection |
| Menu choice 4 (fifth button) | Ends startup without error |
| Menu choices 1, 2, 3 | Stop with a "not implemented" error |
| Accessories, fitting, 3D previews other than the swimsuit, map, day events | Not implemented |
| Scene logic, gameplay, generic script VM | Not implemented |
| Save and load | Not implemented (`hasFeature` only reports return-to-launcher) |
| Debugger | Placeholder `test` command only |

### Menu Choices

The menu reports the selected button index. Scene state values are the ones requested by the code, and their meaning in the original game is a REA observation.

| Choice | Scene state requested | ScummVM behavior |
| --- | --- | --- |
| 0 | `0x2` (new game; character selection is its first step, REA observation) | Runs character selection, then swimsuit selection. Confirming the swimsuit stops with "The accessory selection is not implemented yet". Back on the swimsuit screen returns to character selection with the chosen girl. Back on character selection returns to the title menu, which replays the title reveal and restarts `BG0000` |
| 1 | `0xc` | Stops with an error |
| 2 | `0x11` (audio settings, REA observation) | Stops with an error |
| 3 | `0xd` | Stops with an error |
| 4 | None | Ends startup without error |

Pressing Escape on the title menu ends startup. Escape has no effect in character selection.

## Build And Setup

`configure.engine` requires the `highres` and `3d` features, where `3d` means
that TinyGL or OpenGL (classic) game rendering is built; configure cannot express
"either" directly, so `3d` stands for it. `tinygl` is listed as a component, so
it stays enabled when available. TinyGL renders at 640x480 or higher in 16 or 32
bits per pixel. The OpenGL renderer needs `USE_OPENGL_GAME`; both renderer
sources are guarded by their define and the build works with only one of them.

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

`icon_17.tga` is drawn at 64x64 layout units at the mouse position on the ready title menu, in character selection and in swimsuit selection. The engine uses that size without checking the image dimensions. The engine does not hide the system cursor (there is no `showMouse` call). The original hides it, as described under REA Observations.

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

- The scene fades in from white over 1000 ms. `BG0002` is started by the new-game flow, not by this scene, and keeps playing into swimsuit selection.
- The pointer starts at the centre of the confirm button. Confirm is highlighted at the start.
- Hovering a button highlights it and plays `SE0004`. The highlight stays on the last hovered button. Hovering the button that is already highlighted plays nothing.
- Clicking a grid cell plays `SE0003`, even when it is the girl already shown. The selection frame and name label move to the clicked cell. If the girl changed, her portrait and profile reload, and the portrait slides from x = -512 to 0 over 200 ms while fading in over 400 ms.
- Confirm plays `SE0000` and stores the shown girl as the current girl. Back plays `SE0002` and leaves the current girl unchanged. Either one fades out to white over 1000 ms.
- Confirm moves on to swimsuit selection. Back returns to the title menu.
- Only the mouse is read in this scene. Keyboard input is ignored.

### Defaults

The engine starts at day 1 with girl 0. There is one day counter and one current girl. No per-girl day counts or Clear flags are stored yet, so all six cells draw `time01.tga` ("Day 1"). Save games are not supported.

## Swimsuit Selection

Implemented in [newgame.cpp](newgame.cpp). Entered from the confirm button of character selection and left by confirm or back. Coordinates are in 1024x768 layout units, scaled by `menuRectangle()`. The preview model is described under 3D Models, in Swimsuit Preview.

### Assets

From `sb2_001` unless noted:

- `swimsuit.bmp`: background.
- `tatiNN.tga`: standing picture of the current girl (NN is her girl number), 270x690 at (17,63).
- `waku.tga`: selection frame, 275x39 at (306, 84+35*item). Drawn only on the page that holds the current swimsuit.
- `new.tga`: 40x40 at (285, 84+35*item). Blinks, shown 1500 ms out of every 2000 ms, for entries in state "new".
- `hatenaNN.tga` (one per page, NN = 01 to 04): bar 260x27 at (314, 91+35*item), drawn over locked entries.
- `list_{a|m|b|c}NN.tga` (NN = 01 to 04): list pages, 314x706 at (301,54). The letter comes from the body type: girls 0-1 `a`, girl 2 `m`, girls 3-5 `b`, girls 6-8 `c`.
- `sb2_000`: `botan00_00/01` (confirm) and `botan01_00/01` (back), 248x59 at (708,626) and (708,700). `_00` is the highlighted variant.
- `sb2_028`: day digits, time-slot digit and the girl's name label `time03_NN` (NN = girl number + 1), at the same positions as on character selection.

### Hit Areas

- List items: (312, 89+35*i), 264x31, for i = 0 to 17. Swimsuit number = page * 18 + i.
- Page tabs: (583, 123+110*i), 30x106, for i = 0 to 3.
- Buttons: the rectangles given above.

### Flow

- The scene fades in from white over 1000 ms. The pointer is warped to the centre of the confirm button and drawn after the fade. The list opens on the page of the current swimsuit.
- Hovering a button highlights it and plays `SE0004`. Hovering the highlighted button plays nothing. Confirm is highlighted at the start.
- Confirm plays `SE0000` and back plays `SE0002`. Both fade out to white over 1000 ms. Confirm stops startup with an error; back returns to character selection.
- A page tab click plays `SE0003` and shows that page, even when it is already shown.
- A click on an owned or new entry selects it and plays `SE0003`. The preview model is reloaded only when the swimsuit changes.
- Locked and missing entries ignore clicks.
- Only the mouse is read. Keys, including Escape, have no effect.
- While a preview model is loaded, the screen redraws every frame, because the turntable keeps turning. Without a model it redraws only on events.
- Entering character selection resets the current swimsuit (`loadCharacterSelection()`). Swimsuit selection therefore always starts on the girl's default, including after a back from it. The chosen girl is kept.

### Swimsuit States

Implemented in `resetNewGame()`. The values were taken from the game's new-game init (REA observation at `0x4c2f80`). Each girl has 72 entries, one state each:

- -1 missing, 0 locked, 1 new, 2 owned.
- Entries 0 to 59 start locked and 60 to 71 start missing.
- Also missing: girl 2 {9, 34, 59}; girl 5 {7, 8, 15, 16, 17, 25, 26, 30, 39, 40, 54, 55, 57, 58}; girls 7 and 8 {8, 13, 18, 19, 26, 27, 38, 54, 57}.
- Each girl owns one default swimsuit: girls 0 to 8 start with 13, 41, 20, 20, 44, 11, 14, 31, 0.

`resetNewGame()` runs whenever New game is chosen on the title menu, so every new game starts from these states, girl 0 and day 1. There are no save games. Nothing in the engine sets an entry to "new" or changes an entry's state by selecting it, so the "new" marker is never shown and only the default swimsuits are owned.

## Rendering Notes

The engine draws through the `IllusionRenderer` interface in [renderer.h](renderer.h). `createRenderer()` in [renderer.cpp](renderer.cpp) `selectRendererType()` picks the backend from the standard `renderer` setting (the game's Graphics tab or `renderer=` in the config) among those that the build and the graphics backend support, and `run()` does this before the display is set up: OpenGL needs `initGraphics3d()`, TinyGL a 2D `initGraphics()`. If OpenGL cannot be initialised, the engine falls back to TinyGL. The backends are TinyGL ([renderer_tinygl.cpp](renderer_tinygl.cpp)) and fixed-function OpenGL ([renderer_opengl.cpp](renderer_opengl.cpp), needs `USE_OPENGL_GAME`). Scene caching (`cacheScene()`/`restoreScene()`) is optional: a backend that keeps nothing makes the scenes redraw every frame. The notes below describe the TinyGL backend unless stated otherwise.

The OpenGL backend draws every image as a textured quad in a pixel-sized orthographic projection (NPOT textures are padded when the context lacks them), fades from black by scaling RGB with blending off, and fades to white with an untextured white quad. It keeps no scene cache. It re-establishes the whole 2D state once per frame and after each model pass, because the graphics backend changes GL state when it presents. For models it follows the TinyGL matrices and ordering, and improves on it with `GL_ZERO / GL_ONE_MINUS_SRC_COLOR` for darken meshes, a local viewer for specular, and `GL_SEPARATE_SPECULAR_COLOR` (GL 1.2) so that specular is added after texturing as in Direct3D. The per-light range is still not applied. This backend has not been run on a GPU yet.

- Once the portrait finishes fading in, the scene behind the buttons is cached with `cacheScene()` and restored with a `memcpy` on later frames. A cell click invalidates the cache, and it is rebuilt after the next full draw.
- The frame is redrawn only when the pointer moves, a mouse button is pressed, an animation or fade runs, or a focus or screen event requests it.
- In 32-bit RGB modes, the fade to white is applied directly to the framebuffer. In other modes, `drawFadeToWhite()` draws `fadeW.bmp` stretched over the screen.
- `drawImage` clips destinations that are partly off-screen and maps only the visible part of the source image.

## 3D Models

Implemented in [model.cpp](model.cpp) and the `drawModel()` part of [renderer_tinygl.cpp](renderer_tinygl.cpp). The format is specified in the reverse-engineering notes (`model-format.md`); only a subset is used so far.

- `Model` parses OBJM version 8 `.x` files: MATE, TEXT, LIGH, MESH, FRAM and ANIM. FOG and the chunks that are not used yet (ENVL, MORP, MATA, TEXA, PART, CNST, BLND) are skipped by size, and an unknown tag stops parsing like the game does. Every count and size is checked against the buffer, and a malformed file fails the load. Skinning and morphs are not implemented.
- Frame matrices keep the game's convention (Direct3D, row vectors, `world = local * parentWorld`). ANIM keys are completed by the "prepare" step, evaluated with slerp and lerp, and replace the frame's local matrix.
- `ModelClips` reads the fixed-size `.xan` file (model name at 0x100, 128 clips at 0x390) and `ModelClipPlayer` follows the game's clip player: animation time is `start + tick * (end - 0.1 - start) / ticks`, 60 ticks per second of game time.
- The renderer uses the game's projection and the left-handed data as it is: the model view is `Scale(1, 1, -1) * Translate(-camera)` and clockwise triangles are front faces. Row-major Direct3D matrices are loaded into TinyGL without transposing. Rigid meshes use the GL model view matrix, with TinyGL's per-vertex lighting.
- Lighting: material colours from MATE, point lights from the LIGH records positioned by their frames (att0/1/2 as in GL), directional lights supported, spot cones and the light range are ignored. A light of type `0xFFFFFFFF` ("Light_AMB") adds its diffuse colour to the global ambient colour; this is an inference.
- Material diffuse alpha selects the mode: 0 hides the mesh, (0, 1] blends, (1, 2] is additive and [-1, 0) darkens. TinyGL has no `ONE_MINUS_SRC_COLOR`, so the darken mode scales the destination by alpha instead. Translucent meshes (alpha not 1, or a `.tga`/`.dds` texture) are drawn after the opaque ones without depth writes, ordered by alpha group, then back to front.
- The 3D pass clears only the depth buffer and runs after the cached 2D scene is restored. TinyGL queues draw calls, so the buttons, pointer and fade drawn afterwards land on top. The draw-call memory is raised to 16 MB because every draw call copies its vertices.

### Swimsuit Preview

- Body type from the girl: girls 0 and 1 are `a`, girl 2 is `m`, girls 3 to 5 are `b`, girls 6 to 8 are `c` (types 0 to 3).
- The model number of each swimsuit is read from the table at `0x4eda80`, embedded in [newgame.cpp](newgame.cpp) (-1 and 3 mean no preview). The file is `e20_<type * 3 + model>` from `sb2_006`. The texture index equals the swimsuit number, and the file name is `<letter><model>_<n>.bmp|tga` from `sb2_027`, replacing the model's `zzz.bmp` entry. The `.tga` choice follows the name table at `0x4ec840`.
- The camera is at (-6.3, 3.7, -30) looking along +Z. Clip 0 (the 600-tick turntable) loops, and time advances by 0.002 s per millisecond, so one turn takes 5 s.
- A swimsuit texture stored as `.tga` is drawn in the alpha pass (an inference: the original classifies by the texture file name at load time and the swap behaviour was not traced).

## Architecture

- [illusion.cpp](illusion.cpp) and [illusion.h](illusion.h): engine core. Startup, data validation, display setup, menu flow, character selection, input, and resource loading.
- [archive.cpp](archive.cpp) and [archive.h](archive.h): package reader for both formats above.
- [renderer.h](renderer.h) and [renderer.cpp](renderer.cpp): renderer interface and backend selection.
- [renderer_opengl.cpp](renderer_opengl.cpp) and [renderer_opengl.h](renderer_opengl.h): fixed-function OpenGL renderer with the same interface, without scene caching.
- [renderer_tinygl.cpp](renderer_tinygl.cpp) and [renderer_tinygl.h](renderer_tinygl.h): TinyGL renderer. Creates textures from surfaces, draws backgrounds and images with opacity, supports fade-from-black and fade-to-white, caches and restores a scene, draws posed 3D models, and flips the buffer.
- [model.cpp](model.cpp) and [model.h](model.h): `.x` model and `.xan` clip parsing, frame animation and the clip player.
- [newgame.cpp](newgame.cpp): new-game flow, swimsuit selection and the swimsuit preview model.
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

The archive unit suite is in [test/engines/illusion/archive.h](../../test/engines/illusion/archive.h). It covers table decoding, out-of-bounds directory entries, short reads, and embedded byte negation. The model suite in [test/engines/illusion/model.h](../../test/engines/illusion/model.h) parses a synthetic in-memory `.x` model, checks animation, truncated and malformed input, and the `.xan` clip player. Run them with:

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
  - Step 1, swimsuit (`sb2_001`): init `0x484e6b`, draw `0x485b76`, update `0x4861aa`.
  - Step 2, accessories (`sb2_002`): init `0x4867b9`, update `0x4879ad`.
  - Step 3, fitting confirm (`sb2_003`): init `0x488173`, update `0x488b7c`.
  - Steps 1 to 3 render a 3D preview of the girl from DirectX `.x` meshes with `.xan` animations (for example `sb2_006` `e20_xx.xan` and `sb2_018`). Each step's back button returns to the previous step.
- The character-select back button returns to scene 0 (title) when the previous scene state at `0x8b34b4` was 0. Otherwise it returns to scene `0xf`. The engine always returns to the title.
- After step 3, if the girl's "met" flag is 0, the game goes to scene 3, the 2D map (`sb2_019` map images, `BG0003` to `BG0005`; init `0x48e0c3`, release `0x48ece4`, draw `0x48ed55`, update `0x48f006`). A fresh new game takes this path. The flag is field `+0x10` of the per-girl records, which are 0x204 bytes each at `0x899520`. The check is at `0x4c0e10`. If the flag is set, the game goes to scene 4 (init `0x4a2e0c`, plays `m0001.mpg`, 3D beach) when the girl's day count (record `+0x8`) is below 2. Otherwise it goes to scene `0x12`.
- Swimsuit selection (step 1), as implemented under Swimsuit Selection:
  - The hit-test groups for the buttons, page tabs and list items have no addresses recorded yet. The button-group hit test at `0x4c2473` may be the one used, but this was not checked.
  - The item click check at `0x483375` treats an entry as usable when its state is neither 0 nor -1.
  - The blink timer for the "new" marker is at `0x4c2876`.
  - Swimsuit states are at +0x2c of the per-girl record. The default swimsuit table is at `0x509db0`. The new-game init at `0x4c2f80` fills the states.
- Day 1 script: `sb2_031` contains event script text. `t01_00.txt` starts with a Day 1 morning event that fires on map selection after the dress-up finishes. Lines are tagged such as `#110000#K00#PT1000` (voice and expression codes).

### Presentation And API

- The original hides the system cursor with `ShowCursor(0)` and draws the pointer `icon_17.tga` instead. The engine does not hide the system cursor.
- `fadeW.bmp` is a uniformly white 128x128 image that the original stretches to 1024x768. The engine does not check its size or colour.
- `f1omake_01.tga` is a 468x48 overlay at (540,710), drawn together with `loading.bmp`. The engine does not draw it.
- Sprite API: `0x4c1ca7` creates a sprite from (path, w, h, x, y, origin x/y, scale x/y, alpha, visible), in layout units multiplied by the screen scale at `0x8b349c`. The button-group hit test at `0x4c2473` returns 1 on a hover change and 2 or 3 on a click, and keeps the last hovered index.
- Tooling limitation: the Ghidra decompiler times out on most scene functions (x87-heavy code). These findings come from annotated disassembly.
