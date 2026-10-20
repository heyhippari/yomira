# Delta

Route Two Ltd.'s Windows adventure runtime. Detection covers the Japanese
download edition of One Lover!! ~Miku-chan no Koibito~
([VNDB v5757](https://vndb.org/v5757)) and the Japanese Windows package edition of
Onegai Tasukete! ~Soto ni Detara Yararechau~
([VNDB v5324](https://vndb.org/v5324)).

## Current Status

Both detected games execute their startup scenarios through the logos and
warning screens to the title menu, using their original artwork at 800x600.
Menu button hover states are drawn from the original sprite sheets. Mouse
release on the pressed row and mapped Select actions activate menu options.
Load uses ScummVM's chooser, Settings uses its in-game options dialog, and Quit
asks for confirmation. Extras presents the three mode choices, but the gallery,
scene replay, and music room themselves are not implemented. Start feeds the
original menu-result variable and resumes the scenario. One Lover has been
exercised through opening scenes with Japanese text pages, character poses,
and an event image. Later story execution, choices, and transitions remain
incomplete; unsupported commands report their position and restore the title
checkpoint. Message voice/effect playback and voice-token timing are not
implemented. Cross-fades remain an approximation of the original effects and
timing. Windows installation paths and INI preferences are not imported or
modified. No game files are written. Both detection entries carry the
`ADGF_UNSTABLE` flag.

## Build And Setup

The engine is built by default. To enable it explicitly, use:

```sh
./configure --enable-engine=delta
make
```

`configure.engine` requires the `16bit`, `highres`, and `freetype2` features.
The engine can also be built as a dynamic plugin; `./configure --help` lists
the matching option. It requests an 800x600 RGB565 screen and rejects 8-bit
color modes.

Game files are read through ScummVM's search path (`SearchMan`). The code
opens:

- `ms.mhu` and the scenario file named in its header (`OLV_DLN.SD` for One
  Lover, `PHMN.SD` for PHM).
- `CG/EVB.CGF`, `CG/EV.CGF`, and `CG/ST.CGF`, the image banks. A missing bank
  causes a load error when an image needs it.
- `CG/EVINFO.BIN`, which foreground command `0xb1` requires.
- Loose `IAF` images in the game root or `CG/`, when present.

Message text uses the first available Noto CJK font from
`NotoSansCJK-Regular.ttc`, `NotoSansCJKjp-Regular.otf`, or
`NotoSerifCJK-Regular.ttc`, at size 24. Place one of these in the game
directory or in an added search path.

Message frame style and transparency are ScummVM engine settings. Game-specific
keys are recommended in `scummvm.ini`:

```ini
[onelover]
delta_window_style=0
delta_window_transparency=0

[onegaitasukete]
delta_window_style=0
delta_window_transparency=0
```

`delta_window_style` is clamped to 0-1 (default 0). `delta_window_transparency`
is clamped to 0-255 (default 0). The original Windows INI is not imported.

## Controls

The `delta-default` keymap maps:

- Select: Return, Space, Keypad Enter, or joystick A. Activates the focused menu
  row or advances a waiting message page.
- Back / Cancel: Escape or joystick B. Clears menu focus.
- Focus movement: arrow keys, keypad 8/2/4/6, the joystick D-pad, and both
  analog sticks.
- Game menu: F5 or joystick Start. Opens ScummVM's main menu.
- Left mouse: a press and release on the same menu row activates it; a left
  click advances a waiting message page.

Keyboard and controller input hide the pointer, and mouse input shows it again.
Joystick axes use a ±8000 deadzone.

## Architecture

- `delta.cpp` owns `DeltaEngine`: startup, resource lookup, the per-frame script
  runner, transitions, message presentation, title-menu interaction, and input
  dispatch.
- `data.cpp` reads the startup configuration and scenario data, decompresses
  packed blocks, decodes IAF and sprite-bank images, and provides masked
  compositing and shadowed text drawing.
- `script.cpp` implements the bounds-checked bytecode reader and interpreter.
  It converts operands into `ScriptCommand` records and owns variables, event
  flags, checkpoint counters, and the bounded return stack. The PHM dialect
  adjustment is selected when the engine creates the interpreter.
- `saveload.cpp` serializes and validates ScummVM save payloads. The runtime
  currently applies them only at supported title-menu checkpoints.
- `input.h` tracks pointer activity; `metaengine.cpp` registers engine
  features and keymaps; `detection.cpp` and `detection_tables.h` identify game
  releases; `console.cpp` provides the engine debugger command.

## Recovered Startup

Static REA/Ghidra analysis of the supplied download executable
(SHA-256 `031aac508a173f698151ac8c35198c1dc1879ed6b61901e88e511bb3fe73eb81`):

- `0x426d90`: load startup configuration and Windows preferences, initialize
  runtime state, then create an 800x600 window.
- `0x4246c0`: read `ms.mhu`, decompress it, and split the Shift-JIS text at
  commas. `//SinNo` introduces the startup header. Its first value is a scenario
  number, not a confirmed engine-version identifier. The header supplies the
  title, resource prefix, scenario and INI filenames, save metadata, sound
  prefixes, default names, message rectangle, and text position. Semicolons
  introduce comments in these header values. Named string tables follow.
- `0x418c40` / `0x418c60`: the packed-data header is a little-endian 32-bit
  output size. Bit 31 selects byte-run RLE; otherwise bit 30 selects raw data.
  With neither bit set, use LZSS with a zero-filled 4096-byte ring buffer,
  initial cursor `0xfee`, low-bit-first flags, 12-bit offsets, and lengths 3-18.
- `0x424f30`: reset runtime state, read the configured scenario file, invert
  every byte with XOR `0xff`, and initialize its cursor at the beginning.
  Windows sound-driver checks, system settings, and single-instance mutexes
  are not carried over to ScummVM.

The production decoder/parser also reads the second supplied game's `ms.mhu`:

| Field | One Lover | PHM |
| --- | --- | --- |
| Scenario number | 1012 | 1010 |
| Resource prefix | OLV | PHM |
| Scenario file | OLV_DLN.SD | PHMN.SD |
| Unpacked configuration size | 4770 | 6903 |
| String tables | 9 | 9 |
| Message rectangle (x, y, width, height) | 4, 468, 792, 132 | 4, 468, 792, 132 |

PHM is detected as `onegaitasukete` (Package Edition) using its `PHMN.SD`
scenario file. The supplied directory contains the original disc contents.

### Native Windows Font Check

Static REA analysis of `OLV_DL.EXE` (SHA-256
`031aac508a173f698151ac8c35198c1dc1879ed6b61901e88e511bb3fe73eb81`) shows a
startup check of installed Windows GDI fonts. It is not a game-directory font
or Noto/`encoding.dat` check.

- `0x426d90` gets the window DC, zeroes a 0x3c-byte `LOGFONTA`, sets `lfCharSet`
  to `SHIFTJIS_CHARSET` (0x80), and resets `DAT_0067ba70` to 0.
- For each of the two pointers at `0x466870` and `0x466874`, it calls
  `EnumFontFamiliesExA` with callback `0x423980` and flags 0. The pointers name
  the localized MS Gothic and MS Mincho families (Shift-JIS bytes, decoded as
  full-width names).
- The callback only increments `DAT_0067ba70` and returns 1. It does not check
  `FontType` or which family matched.
- After `ReleaseDC`, a count below 2 shows the message "Font data required for a
  display is not found." (`0x45c850`) through `0x418990` and returns 0 before
  `ShowWindow` and the main loop. The trigger is fewer than two total callbacks,
  not two separate per-family checks, since one family can report several styles.
- The original code has no fallback enumeration and no `CreateFontA` failure check here.

No Windows runtime run was performed, and the host state is unknown. Practical
troubleshooting inference, not a verified fix: install the Windows Optional
Feature "Japanese Supplemental Fonts," which Microsoft lists as providing MS
Gothic and MS Mincho ([Windows missing fonts](https://learn.microsoft.com/windows/deployment/windows-missing-fonts)).
If the check still fails, check the Japanese non-Unicode (ANSI) code page and the
beta UTF-8 option, because the names pass through the A API
([EnumFontFamiliesExA](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-enumfontfamiliesexa)).

Evidence records:

- Startup check: `ev_595f600ea67f890da5643a9786ab3d4a2f6dcdd36901c474f9c39818e5871afa`
- Callback: `ev_c8fcfdc1cfb1f42270544a4ab5fa3f331f1246c7ceba8b6bc50a0f89d08a998b`
- Font-name bytes: `ev_dd6d3a7ac33202b7d73f862a9a93cd0462dd68eb9e2699fd7c5be7acffc9f233`

## IAF Images

The original loose-file loader at `0x40ddf0` (and archive-record loader at
`0x40cd50`) reads this little-endian wrapper:

| Offset | Value |
| --- | --- |
| 0 | One opaque marker byte |
| 1 | 32-bit packed payload size |
| 5 | Packed BMP bytes |
| 5 + packed size | Four 32-bit metadata values |
| 21 + packed size | 32-bit unpacked size and compression-mode flags |

The payload uses the same decompressor as `ms.mhu`, but its size/mode word is
stored after the payload instead of before it. The implementation bounds both
input and output, decodes the BMP with the existing image library, and converts
its pixels to the active screen format. The startup assets in both supplied
games are 800x600, so no scaling or metadata-based placement is needed here.
In One Lover's decoded scenario, the name `DELTA` occurs at byte offset 562
inside image command `0x26`. The logo is now loaded by that command rather than
being hardcoded into initialization. Button sheets can exceed screen height:
One Lover uses 192x585 pixels; PHM uses 192x684 pixels.

## Startup Scenario Execution

REA's instruction inventory identifies the One Lover dispatcher at `0x401830`
and its 187-entry opcode table at `0x40909c`. Targeted assembly inspection of
the table's handlers establishes the implemented startup slice:

| Commands | Implemented behavior |
| --- | --- |
| `0x84`, `0x83`, `0x70`-`0x81` | Decode scenario/header and configuration records |
| `0x91` | Assign the 1024-word variable bank, including variable references |
| `0x3c` | Evaluate high-bit-tagged Boolean/comparison tokens |
| `0x22`, `0x3d` | Jump to a bounded absolute byte offset |
| `0x5e`, `0x95` | Random choice and one-based indexed branch |
| `0x23` | Load a full-screen image with an instant cut |
| `0x26`, `0xb0` | Load a full-screen image with a 300 ms transition |
| `0xb1` | Same as `0x26`, plus a foreground layer set unless its second name is `TACLEAR` |
| `0x0d` | Clear the message window and restore the background behind it |
| `0x19`, `0xa4` | Clear a scene rectangle to black (`0x19`) or white (`0xa4`); duration is argument 4 x 20 ms |
| `0xa5` | Clear the screen to black immediately |
| `0xae` | Fade the screen to black; duration is argument 0 x 20 ms |
| `0x21`, `0x8a` | Bounded script calls and returns |
| `0x3a`, `0x3b`, `0x3e`, `0x3f`, `0x61` | Event flags, checkpoint counters, and title strings |
| `0x4b`, `0x54` | `0x4b` presents message tokens that contain text; `0x54` waits for input on a visible page |
| `0xa2` | Decode a three-word sound command; audio playback is not implemented |
| `0x99` | A nonzero argument 0 arms a deadline of argument 1 x 100 ms without waiting; zero waits until the most recently armed deadline passes |
| `0x67` | Enter the title menu and suspend scenario execution |

The runner processes at most 64 commands per frame, keeping events responsive
even for a script loop. Unsupported commands and invalid operands stop with a
diagnostic containing the script position. Conditions and jumps determine the
startup path; no offsets are scanned heuristically to find the menu.

Parser limits are enforced while reading: 1024 variables, a return stack of 64
entries, up to 127 random-selection options, up to 32 indexed-branch entries,
and up to 256 message tokens per command. Packed and unpacked data blocks are
capped at 16 MiB, decoded BMP images at 4096x4096 pixels and 4 M pixels, and
bank directory entry counts at 65536.

`CG/EVB.CGF` contains a 32-bit entry count followed by 20-byte records: a
16-byte filename and a 32-bit absolute offset. Records are bounded by the next
offset or EOF and contain the same IAF wrapper as loose files. One Lover's
`WARNING.IAF`, `TITLE.IAF`, and `MENU.IAF` are read this way. EV/ST color-mask
banks support recovered character and event layers, including partial images
with stored origins. Event-specific effects and animation timing remain
incomplete.

Foreground and event images resolve in this order: a loose `IAF` in the game
root or `CG/`, then the `CG/EVB.CGF` record, then the `CG/EV.CGF` sprite bank,
then an E-prefixed event composition. For an event composition, the base image
is the first six characters of the name plus `00`, and the layers come from
characters 6 and 7. Backgrounds that are not 800x600 are rejected as unsupported
partial images.

`CG/EVINFO.BIN` maps a foreground name to its layer names. Layers are read from
`CG/ST.CGF`, except names ending in `_D`, which come from `CG/EV.CGF`. A name
missing from `EVINFO.BIN` is treated as a direct layer: an eight-character
`E##` name, or `name_D` from `CG/EV.CGF`. Layer masks are decoded in the active
screen format (RGB565), whereas message-frame masks keep 24-bit precision, so
mask values that depend on the finer precision may differ from the original.

Opcode `0x7c` supplies the menu and button-sheet names, count, position, width,
row height, and row spacing. Normal button rows occupy the first vertical
block of the sheet; hover rows use the next block. The original menu setup is
at `0x41e060`. The title-menu checkpoint is verified in real-data runs:

| Game | Cursor after menu command | Button count |
| --- | --- | --- |
| One Lover | `0x66a9c` | 5 |
| Onegai Tasukete | `0x9d0b2` | 6 |

PHM's executable is packed (REA finds only 27 procedures), so its handlers have
not been independently recovered. Its supported legacy dialect maps raw
opcodes 5 and above to the next One Lover opcode. This mapping is validated
against the supplied scenario's startup operands, branch targets, asset loads,
and menu checkpoint, not asserted to cover all gameplay commands. The dialect
is selected explicitly for the detected game, not inferred from `//SinNo`.

## Message Frames

The original message window is artwork, not a solid rectangle. Static
REA/Ghidra analysis of the One Lover executable above establishes:

- `0x4201a0` selects `WAKU%d0` for narration (speaker zero) and `WAKU%d1`
  for named dialogue. A preference supplies the style digit; a separate override
  selects `WAKU20`/`WAKU21`. The requested destination is the configured message
  rectangle's origin. Speaker names are white, at offset (8, 6), using a
  24-pixel, weight-900 Windows font.
- The supplied `CG/ST.CGF` contains `WAKU00`, `WAKU01`, `WAKU10`, and
  `WAKU11` color records and their preceding masks. These are not loose IAF
  images. `WAKU_S` is used by the separate choice renderer at `0x422db0`.
- `0x4175b0` loads the selected image through `0x410ab0`, then calls
  `0x415f40`. That compositor adds the asset's stored origin to the requested
  destination. Mask 0 replaces the scene pixel; mask 255 preserves it. For
  `WAKU` assets only, mask 1 is replaced by the window-transparency preference
  before blending. Converting the mask to RGB565 first would erase that marker.
- `0x420790` also uses `WAKUB` during refresh. `0x420950` contains window
  movement and text redraw paths; these are not reproduced by static framing.

Following the game-developer agent's design, Delta caches four frame surfaces
(`WAKU00`, `WAKU01`, `WAKU10`, and `WAKU11`), selected by
`style * 2 + (speaker > 0 ? 1 : 0)`. Each is loaded from `CG/ST.CGF` as
`WAKU%u%u` using `frame / 2` and `frame % 2`, keeps its stored origin and
24-bit masks, and is composited onto the clean scene snapshot on every page.
Style and transparency come from the ScummVM keys under Build And Setup, with
port defaults of 0 and 0. Transparency 0 is an intentional, visible, almost
opaque frame; it is not a claimed native default. The supplied One Lover
preferences (`WINDWALLP=0`, `WINDWPNTR=255`) are historical evidence of the
original settings and are not imported. Speaker zero does not draw a name.
Transparent framing alone is not the original text presentation: the first implementation omitted the text-shadow
pass and consequently left white glyphs without sufficient contrast.

Message tokens are read in order. `N` or `n` sets the speaker index, and speaker
zero draws no name. `M`, `m`, and `@` end the current page, but only after text
has been collected on that page. `V`/`v` tokens carry a word and a dword
argument in the scenario and are not displayed, and `W`/`w` tokens are not
displayed either. Other tokens are decoded as Windows-932 (Shift-JIS), and blank
lines are dropped.

The follow-up REA investigation establishes the missing effect:

- `0x40e760` stores the font family, size, weight, foreground color, and
  secondary color used by the text routines.
- `0x414f50` checks the font-shadow flag, then, in normal mode (last argument
  zero), draws the secondary color at (x + 2, y + 2) before the foreground at
  (x, y). Its alternate mode (last argument one) draws the secondary color at
  all four diagonal offsets (+/-1, +/-1) before the foreground.
- With font smoothing enabled, `0x414f50` forwards to `0x413a90`, which also
  draws the secondary glyph at offset (2, 2) before the foreground. Speaker
  names use this smoothed routine directly. Its scene-backed scratch tile is
  font-size plus two pixels on each axis. `0x40f4e0` downsamples a doubled-size
  glyph and adjusts edge coverage; `0x40f180` smooths a native-size glyph.
- The supplied preferences enable `FONTSHADOW=1` and `FONTSMTH=1`.
  `CCOLOR=16777215,0` supplies white foreground and black secondary color.
  Thus a hidden window frame is not evidence that plain white text is
  faithful to this configuration. Transparency hides the whole mask-controlled
  panel, not only its interior.

Following the game-developer agent's correction design, both speaker names
and dialogue now use a shared black (2, 2) shadow pass followed by white text,
without moving the foreground, changing wrapping, or darkening the scene.
Noto's antialiasing, metrics, and regular weight remain approximations of the
Windows glyph rasterizer and fonts. Alternate outline mode, per-state text
colors, WAKU20/WAKU21 override, window animation, and wait-indicator behavior
remain unimplemented. Window styles 0 and 1 are implemented; runtime graphical
preference controls are not. This investigation is static
evidence from One Lover, not a runtime comparison with the original Windows
renderer or an independent verification of the packed second game's drawing
code.

REA evidence records:

- Frame selection and name placement: `ev_d2ad05649600dd4539c3049cec9f210a3216387f52829927b063c7831c35d8cb`
- Frame loading: `ev_f1c3401b28edb477f70bd03fb2b2a7625b23a99602a3a70534be71459301f0c0`
- Stored-origin and mask compositing: `ev_f3290227c44586bac49ff9e77d587d334f767eb5475e83abc420876b20f63153`
- Frame refresh: `ev_7ea6b7ba9c69d3866dd2543ace3f7c6ae5b40ccce77d3db3c2e51283bffae843`
- Choice frame: `ev_a7657ff50cbfd596eb9e03d7eb0ad859d98847c1347d646d6205c7dea2192b9c`
- Font state: `ev_c6487f9878f0daffae9b174661f931665f73878bf4a35b2bf51efa3d98d5f9d2`
- Dialogue shadow and alternate outline: `ev_3235f4b259d9fda77aa3891d2a298de4245836ea327f4126cea59e6d5f6fae22`
- Smoothed text and speaker-name shadow: `ev_b91ee5b74bc8f68a1d7fd3f803b9ea17d6aaa3c942e9243165141a116ce4ddfa`
- Supersampled glyph raster: `ev_6ab7cb56a5c9c06bd83b755e28369f90382aa589049d056a9b44a9c817ea9a62`
- Native-size glyph raster: `ev_b9a13166610380c49cd35c40702ce236e628b419689989071c7063839ce528b5`

### Window Transparency Follow-up

This follows static REA analysis of the One Lover executable (SHA256
`031aac508a173f698151ac8c35198c1dc1879ed6b61901e88e511bb3fe73eb81`) and offline
decoding of the supplied assets. No native game was run and no runtime
screenshot was taken.

- Preference path: the loader at `0x410ab0` sets a special flag when the asset
  basename starts with `WAKU`. `0x423e70` reads the saved transparency with
  `atol` directly, `0x42e150` applies options without inversion, and the slider
  at `0x42ee20` ranges from 0 to 255. The supplied `WINDWPNTR=255` is the value
  saved in the supplied One Lover INI; it is not a native default.
- Compositing: `0x415f40` copies the source for raw mask 0 and the destination
  for raw mask 255. For intermediate mask 1, the mask is replaced by the
  transparency value (`DAT_0068758c`), then blended as
  `floor(D*T/256) + floor(S*(255-T)/256)`, with each product shifted right by 8
  before the sum. D is the scene and S is the frame. `compositeMasked` now
  follows this order: raw 0 copies the source, raw 255 preserves the
  destination, and mask 1 is substituted before the intermediate blend. At
  substituted endpoints this gives T=0 `floor(S*255/256)` and T=255
  `floor(D*255/256)`. Unlike the old port, substituted endpoints are not treated
  as raw-mask copy/preserve shortcuts.
- Whole panel: each game's `WAKU00`, `WAKU01`, `WAKU10`, and `WAKU11` is 792x132
  at origin 0,0. All eight color BMPs from the two supplied `CG/ST.CGF` files are
  byte-identical (SHA256 `f4d83f658d7b6764282da7aea4d0c426e64804b1736fb308b8c280e5884329bd`),
  and all eight masks are byte-identical (SHA256
  `0463178bf173e752e0803384ddad3a988e14f3f0d2dbb97ab9d1fe51d54c6500`). Each mask
  uses an 8-bit identity gray palette, and all 104544 pixels map to index 1,
  RGB(1,1,1). Transparency therefore controls the entire panel, including the thin
  light border and top-right close box, not only the interior. The color art has
  a black interior (100768 black pixels). Hiding the frame blends the art out; it
  does not remove art from the asset.
- Other asset: `CG/WAKUB.IAF` decodes to 560x32. `0x420790` loads it through
  `0x414c60` into buffer 1, while the main masked frame is drawn to destination
  -2. Its artwork is a control-button strip, not the full panel, and its other
  uses are not all known.
- Implementation: `delta.cpp` reads `delta_window_style` and
  `delta_window_transparency` from ScummVM configuration. Original INI import and
  runtime preference controls are not implemented. PHM settings are not inferred
  from One Lover.
- Verification: the current build compiles, and the unit tests
  `testFrameTransparencyUsesNativeBlendBranchesAndRounding`,
  `testFrameTransparencyPreservesMaskPrecision`, and
  `testPartialMaskPlacementAndClipping` pass. `make -j2 test` passes all 519
  configured tests. Real-data One Lover runs reach the first named dialogue
  page and load `WAKU01` at default settings and `WAKU11` with style 1 and
  transparency 128. Screenshots confirm the 792x132 panel at (4,468), with
  visible border and text over opaque and translucent backgrounds respectively.
  Narration variants and PHM dialogue were not checked in these runtime runs.
- PHM: the executable (SHA256 `997ab8f035690302c85410dab4a568c1fa7bc819253c7101f93c67c5e6e56523`)
  is packed and not recognized by the installed UPX (`upx -t` rejected it).
  REA finds 27 procedures. Its
  entry at `0x43efd3` goes to self-modifying code at `0xbac000`, which reaches
  `0xbac111` and a page transform at `0x6e5000`. An offline partial decoding
  probe did not establish unpacked renderers. A recursive inventory of the game
  tree found no PHM INI. The shared art supports a shared-asset inference only;
  PHM's compositor and default transparency are not independently verified.
- Alternate NoDVD PHM: `PHM.EXE` (SHA256
  `99382a9c429b2ce8d4233ad7094f31ef93e8f2928da333f941e9517a85749cb5`, 5287936
  bytes, same size as the original) reports the same 27 procedures. Its entry at
  `0x43efd3` still calls `0xbac000`, and `0xbac111` has the same decoding stub as
  the original. A read-only byte comparison finds exactly 3 differing bytes, at
  file offsets `0x265083`-`0x265085`; all other bytes are identical. It is not an
  unpacked executable, and PHM's compositor and default transparency remain
  unresolved.

Evidence records for this follow-up:

| Finding | Evidence record |
| --- | --- |
| WAKU loader flag | `ev_65ad176e23a64dcb6a08469a4f9e670e090c14ff0c147a5200b3c59feb64c9cb` |
| Saved preference loader | `ev_4074293a557b03f9a7691fe77b128e3aadaaa33b16f356041e62cd794c1191f9` |
| Options apply | `ev_b19ec9498158d7103778c88e8685c6176c2cf9aff03ab6d49b8deb12dbdcd9fc` |
| Transparency slider | `ev_38be6e706ca513178ed9e44694a7a5b2f44d69d0e13f0acf63cde5ac11deb4a2` |
| WAKUB buffer load | `ev_83af6d4d47b015d4306a647b7355d88ac4a55a54ce298e27c6a520af1f9ca26d`, `ev_0aebbfa05a48d7f055fe677c0074fa92dbecaefaeea6e6672c1dec2a000ad041` |
| PHM entry | `ev_9446bd4ffed61469add993a64e2f9312858b932d1ad1c4a6495e3b27ac64780a` |
| Alternate NoDVD PHM overview | `ev_a1bdf7e809b7a3cd8582af9ba62888e1a572ca626986854b1636e9867c6c9b9e` |
| Alternate NoDVD PHM entry and decoding stub | `ev_24111d9e4f17c9b6d31120c70c334162960ed0411b30994b25e18b64eb58ebd4`, `ev_33effcb39e48640d08675b33757b619f4a1582f5ee578891b28afd6b6f2dbfaf` |

The stored-origin and mask compositing record above also applies to the
compositing bullet.

## Menu And Save Integration

Both supplied scripts use menu opcode `0x67` (PHM raw `0x66`) with destination
variable 6. Menu rows are zero-based internally, and the scenario sees the
one-based result. Start sets the result variable to 1 and resumes the scenario.
Load, Settings, and Quit are handled by the port and do not set the variable.
One Lover has five buttons, so result 5 enters Extras and there is no result 6.
PHM has six buttons: result 5 has no action (the menu stays open), and result 6
enters Extras. The UI uses ScummVM dialogs for the host
operations instead of recreating the original Windows save/options screens.

The native handler entries inspected for these branches are `0x40407b` (load),
`0x408747` (options), and `0x401e2f` (exit). The original save/load implementation
and original save files are not reused.

Delta's metaengine derives from `AdvancedMetaEngine`, which reports the extended
save features: extended format, simple save names, save listing, deletion, save
metadata, thumbnails, creation date, and play time. Saving
and in-game loading are currently restricted to stable title-menu checkpoints;
they do not imply support for saving unfinished story execution. The global
ScummVM menu exposes Save and Load, and the original title Load option opens the
same restore chooser.

The engine payload starts with `DLTA` and format version 1. It stores the game
ID, scenario size and number, random seed, interpreter dialect and cursor,
1024 variables, the return stack (up to 64 entries), 2048 checkpoint counters,
2048 seen-event flags, and title-menu resource, geometry, and focus state. Invalid versions,
different games/dialects, truncated state, invalid cursors, resource paths, and
menu geometry are rejected before replacing the active interpreter. Loads are
queued and applied outside dialog/input dispatch, then menu artwork is rebuilt.
Original Windows saves are not compatible with this format.

REA evidence records:

- Startup: `ev_595f600ea67f890da5643a9786ab3d4a2f6dcdd36901c474f9c39818e5871afa`
- Configuration: `ev_06987c3e68f57cf2207bc5fb9b480828b25fd586d3b30ddd18ce72f21120a65a`
- Compression: `ev_a54405f29d4543724450a278a05939ae1463b036193ab6266b5740a4038c6583`
- Scenario: `ev_8c0ad1d5c47c6069bb711ec68bfbd1026a709b89c4bf8589a93b706e64d345f7`
- Loose IAF loader: `ev_0eed3bf52c30687c3308880d5614b3324eefab0de3b96c80c880abf9f55f5ecf`
- Archive IAF loader: `ev_03938785b14397206033b349b087f1cea2cca006e2e31effd17bc555300ce4f0`
- Dispatch table: `ev_205deaea741fef426303f65d5f171110b1fd0551b12121c890f4fb10deb12b8c`
- Script integer reader: `ev_7f7d1100103f75d0a2b6ebff132e5cebe6adaedfad5212dd5fa21f1a8cfd12ea`
- Script string reader: `ev_7ef83d94d9a51e7b14605b4b5edde31c8d91ac2f663bb4978d37eb8447e72f7e`
- Menu asset references: `ev_dc07aa706b7fda6617b35c551866a2c215991f6ec8bad5adcd2f14e5012d9c72`
- Packed PHM overview: `ev_5a637af28131e3cfbf5a2cfc55351273299604f7b48bc88f5e01c791a15ebf77`

## Debugging And Tests

- Console: the engine console provides a `test` command. No other engine-specific
  console commands are defined.
- Logging: `debug(1, ...)` reports major steps such as initialization, image and
  font loads, and menu selections. `debug(2, ...)` traces each script command,
  pointer mode, and menu focus.
- Declared but unused: `detection.cpp` declares the `Graphics`, `Path`,
  `FilePath`, `Scan`, and `Script` debug channels, and
  `GAMEOPTION_ORIGINAL_SAVELOAD`. No code checks them, so enabling them has no
  effect.
- Tests: CxxTest suites are in `test/engines/delta/` (`data.h`, `script.h`,
  `input.h`). `test/module.mk` adds them to `make test` only when `ENABLE_DELTA`
  is `STATIC_PLUGIN`, and links `engines/delta/data.o` and
  `engines/delta/script.o`. There are no runtime tests. The REA-derived runtime
  claims above were not rerun for this documentation update.
