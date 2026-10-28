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
Load uses ScummVM's chooser, Settings opens a reconstruction of the original
three-page options screen (see Options Screen), and Quit asks for confirmation. Extras presents the three mode choices, but the gallery,
scene replay, and music room themselves are not implemented. Start feeds the
original menu-result variable and resumes the scenario. One Lover has been
exercised through opening scenes with Japanese text pages, character poses,
and an event image. Later story execution, choices, and transitions remain
incomplete; unsupported commands report their position and restore the title
checkpoint. Message voice/effect playback and voice-token timing are not
implemented. Screen transitions follow the recovered 16-step fades (see
Transitions); the effect `0x411350` and its callers are not implemented. Windows installation paths and INI preferences are not imported or
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

Message frame style and transparency are ScummVM engine settings. The title
menu's Settings screen edits the transparency and the other original options
(see Options Screen). Game-specific keys are recommended in `scummvm.ini`:

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
- On the options screen, the mouse behaves as in the original. Up/Down moves a
  focus outline between controls, Left/Right moves it sideways or steps a
  focused slider, Select activates the focused control, and Back / Cancel
  closes the screen without saving the current page.

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
- `options.cpp` holds the recovered options-page tables and the
  hit-test, radio, slider, and page-switch behaviour, independent of the
  engine. `optionsmenu.cpp` loads the page artwork, draws the screen, routes
  input, and stores settings through `ConfMan`.
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
| `0x26`, `0xb0` | Load a full-screen image with a transition (see Transitions) |
| `0xb1` | Same as `0xb0`, plus a foreground layer set unless its second name is `TACLEAR` |
| `0x2c` | Show up to three character slots over a background (see Character Slots) |
| `0x0d` | Clear the message window and restore the background behind it |
| `0x19`, `0xa4` | Fade a scene rectangle to black (`0x19`) or white (`0xa4`) (see Transitions) |
| `0xa5` | Clear the screen to black immediately |
| `0xae` | Fade the screen to black (see Transitions) |
| `0x21`, `0x8a` | Bounded script calls and returns |
| `0x3a`, `0x3b`, `0x3e`, `0x3f`, `0x61` | Event flags, checkpoint counters, and title strings |
| `0x4b`, `0x54` | `0x4b` presents message tokens that contain text; `0x54` waits for input on a visible page |
| `0xa2` | Decode a three-word sound command; audio playback is not implemented |
| `0x99` | A nonzero argument 0 arms a deadline of argument 1 x 100 ms without waiting; zero waits until the most recently armed deadline passes |
| `0x67` | Enter the title menu and suspend scenario execution |

The dispatcher at `0x401952` indexes the table with `opcode - 1`. Seen-event
command `0x3b` (handler `0x403d29`) reads one word, the event number. It stores
1 or 2 (by `(event - 1) % 2`, from the byte table at `0x466af0`) in the event
byte array, then calls `0x418630`, which writes the global data. Command `0x3a`
(`0x403cfe`) reads the event and a variable and stores whether the event byte is
non-zero. PHM uses `0x3b` in this one-word form at script offset `0x137f`.

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

`CG/EVINFO.BIN` maps a foreground name to its layer names. In One Lover,
`0x410590` looks the name up there and loads each listed layer through the
generic loader `0x40ddf0`. A name missing from `EVINFO.BIN` goes to `0x40ddf0`
unchanged. That loader searches `CG/EV.CGF` (`0x40d020`), then `CG/ST.CGF`
(`0x40d490`), then `CG/EVB.CGF` (`0x40cd50`), then a loose `name.IAF`. The
engine resolves every layer from EV.CGF and then ST.CGF in the same order. PHM
uses unlisted base poses directly, for example `SHSA20011` from `CG/ST.CGF` at
script offset `0xec5`. Neither supplied game has an EV.CGF `X_D` record
without an `EVINFO.BIN` entry for `X`, and the two banks share no names.
Evidence: `ev_7529f427c7abcc51c8cc9309ad2ef357fab9b3f3142959c3ee5ac2762ac8ccd5`
(`0x410590`) and `ev_08686a7273e89cde61f0e9c0c8d29c1635e9526e53bec8f2257f80e997b99253`
(`0x40ddf0`). Layer masks are decoded in the active
screen format (RGB565), whereas message-frame masks keep 24-bit precision, so
mask values that depend on the finer precision may differ from the original.

### Character Slots

Command `0x2c` (handler `0x403121`, PHM raw `0x2b`) reads three character
names, a background name and six words. Each of the first three words, `w`,
gives a draw-order index `3 - w`: index 0 is drawn first, so a slot with `w` = 3
is at the back. The fourth word is a transition effect and the fifth its
parameter (`0xff` becomes -1), both passed to the transition routine `0x40edc0`.
The sixth names a variable that receives the renderer's return value, which is
always 0. In the handler, background `SAME` is replaced by the current
background name (`0x7eee7c`, also written by the image and fade commands), and
the result, `NULL` included, becomes the new current background name.

Setup `0x422880` treats `NULL` as an empty slot. A background of `NULL` keeps
the current background and updates only the non-`NULL` slots. Otherwise the
background and all three slots are replaced. Slot X positions depend on how many
of the command's names are present:

| Present slots | Slot 0 | Slot 1 | Slot 2 |
| --- | --- | --- | --- |
| 1 or 3 | 152 | 400 | 648 |
| 0 and 1 | 200 | 580 | - |
| 0 and 2 | 200 | - | 580 |
| 1 and 2 | - | 200 | 580 |

The slot records start at `0x7eea88`, `0x150` bytes apart, with X at `+0x148`.
Renderer `0x416740` draws the background, then each non-`NULL` slot in draw
order. Each slot image is loaded through `0x410ab0`, which uses the `EVINFO.BIN`
lookup described above and records the base sprite's metadata
`(left, top, anchor, bottom)`. Compositor `0x40ea30` moves the destination by
`left - anchor`, so a character is drawn at slot X - anchor + stored left, at
its stored top. Every sprite in both supplied games has anchor 400. The
compositor's separate handling of sprites lying entirely left of, or starting
right of, the anchor is not reproduced.

The engine reproduces the slot layout, draw order, `SAME`/`NULL` handling,
result variable, and the `0x40edc0` cross-fade (see Transitions). Slot state is not stored in save games. All 150 uses in PHM's
scenario name characters in slots 0 and 1, leave slot 2 `NULL`, give an
explicit background, and use the words `2 1 3 1 5 1`.

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

## Transitions

Targeted disassembly of One Lover's `OLV_DL.EXE` recovers one family of
screen effects. REA's decompiler did not respond during this investigation, so
the evidence is objdump listings of the routines named below. Each effect runs
16 steps over 24-bit DIB data with MMX, weighting every 8-bit channel in
sixteenths (`pmullw`, then `psrlw 4`). For step i = 0 to 15:

| Routine | Effect | Channel value at step i |
| --- | --- | --- |
| `0x40edc0`, `0x412f50` | Cross-fade | `new*(i+1)>>4 + old*(15-i)>>4`; unchanged 4-byte groups are copied |
| `0x411880` | Fade in from black | `new*(i+1)>>4` |
| `0x411fa0` | Fade in from white | `new + ((255-new)*(16-i)>>4)` |
| `0x411c00` | Fade to black | `old*(16-i)>>4` |
| `0x412420` | Fade to white | `old + ((255-old)*(i+1)>>4)` |

Timing is shared. A routine multiplies the duration word by 100 and divides it
by the `EFFECT` setting (`0x67c474`: 1 normal, 2 fast, written by the display
apply `0x42e150`). Step i is due at start + D(i+1)/17 (the `0x78787879`
reciprocal), and a step whose time has already passed is skipped. Before a
step, `0x418230(0, delay)` arms a deadline, and after rendering it,
`0x418230(1, mode)` waits for it. Modes 1 and 2 return early once an input
handler (`0x42b7e0`, `0x42c8e0`, `0x42ca20`) sets `0x67c34c` to 99; modes 3
and 4 depend on skip state. An early return draws the final image, and the
routine returns 1; otherwise it returns 0. `0x40edc0` always returns 0.
Handlers pass a zero duration (an instant cut) when `EFFECT` is 0 or a
skip/replay state is active (`0x7eeff8`, `0x7ef1a0`, `0x7e8370`,
`0x67bbf4`). A duration of `0xff` becomes -1, which draws without a fade.

The current background name (`0x7eee7c`) selects the effect:

| Command | Operands | Behavior |
| --- | --- | --- |
| `0x26` (`0x402717`) | name, duration, input mode, result variable | After `BLACK`: `0x411880` through `0x421a20`/`0x414990`. After `WHITE`: `0x411fa0`. Otherwise `0x412f50` through `0x422480`/`0x417790`. Clears the character slots. |
| `0xb0` (`0x4076a6`), `0xb1` (`0x4079cb`) | name(s), duration | After `WHITE`: `0x411fa0`. After `BLACK`: a cut. Otherwise `0x421770`. Mode 0, no result. |
| `0x2c` (`0x403121`) | see Character Slots | `0x416740` calls `0x40edc0`. Effect word 0 cuts. Mode 0. |
| `0x19` (`0x401ecd`) | x, y, width, height, duration, input mode, result variable | `0x411c00`; if the background is already `BLACK`, a cut that leaves the variable unchanged. The background becomes `BLACK`. |
| `0xa4` (`0x406e53`) | as `0x19` | `0x412420`. The background becomes `WHITE`. |
| `0xae` (`0x4074b7`) | duration | Full-screen `0x411c00`, mode 0; a cut when already `BLACK`. |
| `0xa5` (`0x406f8b`) | none | `BLACK` at once. |

`transition.cpp` implements the schedule and the five blends. The engine
renders each step when it falls due, decoding RGB565 to 8-bit channels and back,
so values can differ from the 24-bit original by rounding. Unchanged pixels are
copied, matching the native 4-byte comparison as closely as whole pixels
allow. In modes 1 and 2, a left-button release or Select finishes the effect
and stores 1 in the result variable. `0x19` and `0xa4` fade only their
rectangle; what the native redraws outside it afterwards was not traced. The
background name is not saved and is cleared on load.

Limitations:

- `0x421770` and `0x421660` (used by `0xb0`/`0xb1`) were identified only by
  their call sites; the port assumes the `0x412f50` cross-fade and a cut.
- `0x411350`, a five-way effect switch (table `0x41186c`), is called only from
  `0x422fe2`; the owning command was not identified.
- Commands `0x18`, `0x29`, `0xa3`, `0xb7`, and `0xb8` also call these fades
  and are not implemented.
- PHM's executable is packed, so the One Lover routines are assumed to apply to
  it.

Verification: `DeltaTransitionTestSuite` covers the duration scaling, step
schedule, blends, and rectangle clipping. Debug-level-2 runs of both games to
the title menu logged the same startup sequence: `0x26` faded in from `BLACK`
over 2000 ms, a second `0x26` cross-faded over 2000 ms, `0xae` faded to black
over 1000 ms, and `0xb0` after `BLACK` cut. No `0x2c`, `0x19`, or `0xa4`
transition was exercised at runtime, and the rendered frames were not compared
with the original.

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
and dialogue use a shared black (2, 2) shadow pass followed by the foreground
text. Speaker names are white. Dialogue uses the options screen's normal text
color (`CCOLOR`, default white), and its shadow pass follows the shadow option,
without moving the foreground, changing wrapping, or darkening the scene.
Noto's antialiasing, metrics, and regular weight remain approximations of the
Windows glyph rasterizer and fonts. Alternate outline mode, per-state text
colors, WAKU20/WAKU21 override, window animation, and wait-indicator behavior
remain unimplemented. Window styles 0 and 1 are implemented, but the options
screen does not offer a style choice (`WINDWALLP` is not on it). This investigation is static
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
Settings opens the Options Screen below. If its artwork cannot be loaded, it
falls back to ScummVM's options dialog.
One Lover has five buttons, so result 5 enters Extras and there is no result 6.
PHM has six buttons: result 5 has no action (the menu stays open), and result 6
enters Extras. Load and Quit use ScummVM dialogs instead of recreating the original Windows
screens.

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

## Options Screen

### Recovered Original Behaviour

The original options screen is drawn in the game window, not built from Win32
dialog controls. `OLV_DL.EXE` still contains a `SYSTEMオプション` dialog
template (resource 403). Most of its controls have no `WS_VISIBLE` style, and
no code path to it was found in this investigation. It is not the screen
players see. Static REA/Ghidra analysis of `OLV_DL.EXE` (SHA-256
`031aac508a173f698151ac8c35198c1dc1879ed6b61901e88e511bb3fe73eb81`) recovered
the following. No native game was run.

- Three pages are separate objects with constructors `0x42ee20` (display,
  mode 0x16), `0x431510` (sound, mode 0x17), and `0x431d90` (controls,
  mode 0x15). Each loads `MENUS_x1` (normal), `MENUS_x2` (selected), and
  `MENUS_x3` (hover) for x = `D`, `N`, or `Y`. These are full 800x600 images
  from `CG/EVB.CGF`. PHM ships them as loose IAF files, and its sound page as
  `MENUS_N1A`-`MENUS_N3A` (an alternative `B` set is also present).
- `0x42d540` builds a list of 36-byte control records from three static
  tables: rectangles (x, y, width, height), types, and (count, first) groups.
  Type 1 is a push button, 2 a radio button within its group, and 4 a slider.
  One Lover's tables are display `0x465848`/`0x465ab8`/`0x465b38` (31
  controls, 17 groups), sound `0x465e68`/`0x466200`/`0x4662b8` (46, 34), and
  controls `0x4663e0`/`0x466628`/`0x4666a0` (29, 12). They are transcribed in
  `options.cpp`.
- Drawing (`0x42d3b0`-`0x42d450`): a selected radio button copies its
  rectangle from the selected image. Hovering a push button or an unselected
  radio button copies it from the hover image. Otherwise the normal image is
  shown.
- Input: `0x42dbe0` is the first-match hit test. `0x42dc80` (button down)
  starts a slider drag only on the 16-pixel thumb. `0x42ddc0` (move) drags the
  slider and updates hover. `0x42dcd0` (button up) ends a drag, selects an
  unselected radio button (a selected one does nothing), returns push buttons,
  or steps a slider when the click lands beside its thumb.
- Sliders (`0x432570`-`0x4329b0`): the thumb is the `CURSOR` sprite in
  `CG/ST.CGF`, drawn at x + (value - min) x (width - 16) / (max - min). The
  step is range / 16 when that divides evenly (a range of 255 counts as 256),
  otherwise range / 8, otherwise range / 10. A click left of the thumb steps
  down to the previous multiple of the step. A click right of it steps up with
  the original's rounding, which can skip a step from an unaligned value.
- Each page's click handler (`0x42f200`, `0x431b60`, `0x4323e0`) maps a control
  back to its group. Tabs apply the page and open another page (routed by
  `0x42b7e0`). 決定 (OK) applies and closes, 取り消し (Cancel) closes, and
  初期化 (Reset) restores only that page's defaults (`0x42ec70`, `0x430e30`,
  `0x432160`). Applying (`0x42e150`, `0x431270`, `0x432250`) writes changed
  values to the `[OPTION]` INI keys through `0x418810`.
- Display-page extras: `0x42e9d0` fills three 32x24 color swatches and shows
  "文字色/テスト" in the selected color, in 18-pixel heavy type in the
  four-diagonal outline mode. `0x42e4c0` shows "文字に影有無テスト" with the
  (2, 2) shadow. `0x42e5a0` draws the transparency percentage
  (T x 100 / 255) with glyphs from the `MENUSYS1B1` sheet. The native code
  addresses that sheet bottom-up, so digit d starts at top-down row
  252 + 24d. `0x4143b0` darkens the sample picture by T / 256 per channel,
  which is how a black window interior blends at transparency T.
- The controls page keeps at least one of the wheel and right-click bindings
  on メニュー (`0x4320f0`, `0x432120`). The display page refuses
  `DXSHOW` changes during backlog or rewind. The port has no backlog or
  rewind, so that restriction never applies.

| Page | Original control | INI key | Reset value |
| --- | --- | --- | --- |
| Display | Fullscreen / window | `FULLSCREEN` | window |
| Display | Text colors (normal, read, backlog) | `CCOLOR`, `KCOLOR`, `RCOLOR` | FFFFFF, FFFFC0, 00FFFF (RGB) |
| Display | Text speed, auto-advance speed | `SPEED`, `ASPEED` | normal |
| Display | Cross-section view (OLV) / movie playback (PHM) | `NOMOVIE` | on |
| Display | Screen effects: normal, fast, off | `EFFECT` (1, 2, 0) | normal |
| Display | Antialiasing, text shadow | `FONTSMTH`, `FONTSHADOW` | on, on |
| Display | Window transparency, 0-255 | `WINDWPNTR` | 128 |
| Display | Wheel: backlog/rewind (OLV) / DirectShow (PHM) | `DXSHOW` | first choice |
| Sound | Master, voice-ducking sliders, 0-100 | `VOLUMEALL`, `VOLUMEDWN` | 100, 71 |
| Sound | Music, effect sliders, -30-0 dB | `VOLUMEMUS`, `VOLUMESND` (x100) | -9 dB |
| Sound | Voice slider, 0-100 | `VOLUMEVIC` | 81 |
| Sound | Music, effect, voice on/off | `BGM`, `SE`, `VOICE` | on |
| Sound | Voice interruption, DirectSound | `VOICECUT`, `DSNOUSE` | on, unchanged |
| Sound | Per-character voice on/off and volume | `VOLUMEC01`... | on, 0 dB |
| Controls | Skip unread, skip read | `MIDOKU`, `KIDOKU` | off |
| Controls | Stop skip or auto mode at choices | `SKIPSTOP`, `AUTOSTOP` | on, off |
| Controls | Skip key Ctrl / Shift | `SKIPKEY` (17, 16) | Ctrl |
| Controls | Cursor auto-follow | `AUTOMCUR` | off |
| Controls | Wheel button, right click (6 actions) | `WHEELBTN`, `RBTNCLICK` | settings, menu |

PHM's executable is packed, so its code was not inspected. The port applies
One Lover's tables to PHM after comparing each control's rectangle with the
difference between PHM's normal and hover images. Every PHM control matches
except display buttons 24-25: PHM draws them at (648, 500) and (712, 500),
each 40x20. PHM's sound artwork shows four character-voice rows; One Lover's
shows none. The port therefore enables 4 and 0 rows. The native source of that
count (`DAT_0067bba8`) was not traced.

### Port Implementation

Settings on the title menu opens the display page. Rendering, input and
paging follow the recovered behaviour above, and Reset uses the original
defaults. Values are stored in the active `ConfMan` domain when a tab or OK
applies a page. The original Windows INI is not read or written.

- Applied now: window transparency (`delta_window_transparency`), the normal
  text color (`delta_text_color`), the text shadow (`delta_font_shadow`), and
  fullscreen (the ScummVM `fullscreen` feature). The music, effect and voice
  sliders and their on/off buttons map to ScummVM's `music_volume`,
  `sfx_volume`, and `speech_volume` keys and their `*_mute` counterparts.
  Decibels convert as 256 x 10^(dB / 20). A ScummVM level is rewritten only
  when its whole-decibel step changes, so opening and closing the screen does
  not change it. Screen effects (`delta_effect`) scale or disable
  transitions (see Transitions).
- Stored only, because the engine does not implement the feature yet:
  `delta_read_color`, `delta_backlog_color`, `delta_text_speed`,
  `delta_auto_speed`, `delta_no_movie`,
  `delta_font_smoothing`, `delta_wheel_mode`, `delta_volume_all`,
  `delta_voice_duck`, `delta_voice_cut`, `delta_no_directsound`,
  `delta_character_voice_off_N`, `delta_character_volume_N`,
  `delta_unread_skip`, `delta_read_skip`, `delta_skip_stop`,
  `delta_auto_stop`, `delta_skip_key`, `delta_auto_cursor`,
  `delta_wheel_button`, and `delta_right_click`. Antialiasing does not change
  the Noto rasterizer.
- When `delta_window_transparency` is absent, the port default stays 0; the
  in-game Reset sets 128.
- Not reproduced: the text-speed demonstrations in the two gray preview boxes
  (`0x42e350`, `0x42e3f0`), the sample-playback (●) buttons, and DirectSound
  availability checks that disable the sound sliders. The original's
  "restart required" message boxes are also not reproduced.
- Port additions: keyboard and controller navigation with a white focus
  outline, since the original screen is mouse-only. If the page artwork,
  `CURSOR`, or `MENUSYS1B1` cannot be loaded, Settings falls back to ScummVM's
  options dialog.
- The options screen is reachable only from the title menu, the only
  checkpoint the engine supports. Saving and loading are disabled while it is
  open.

`CURSOR`'s mask in `CG/ST.CGF` decodes one byte past its stated size. The
native RLE decoder (`0x418c60`) writes every run in full and stops once the
output reaches the stated size. `readPackedData` now keeps only the bytes up
to the stated size instead of rejecting the record.

Verification: `make test` passes all 530 tests, including the
`DeltaOptionsTestSuite` cases for the recovered stepping, paging, reset, and
binding rules. A temporary, uncommitted harness ran both games from the
title-menu checkpoint into the options screen. It drove the screen through the
engine's input path and saved screenshots. It confirmed the artwork, hover and
selected states, thumbs, percentage digits, transparency sample, page
switches, keyboard focus, and values persisted through `ConfMan`, with PHM
using `MENUS_N*A`. Message text in the new color was not checked at runtime.

REA evidence records:

| Finding | Evidence record |
| --- | --- |
| Dialog resource inventory | `ev_89c21367117cad3f810cbb7d8c05c1fec594958cd1f7178525ad28bc262c1d0d` |
| Page artwork names | `ev_fc4582bd7ac622da4e647f725e2eda283cbac875dbb9ada2c8442d150843c8fe` |
| Display constructor, apply, INI loader | `ev_2c123236c3dbf85a4a35e1df353f227b394b062218620afc5d97a52051ca885b` |
| Control list, radio state, slider setup, previews | `ev_d00312d72b064d9ef4a6f6c6c4bc1b052ac0916561305435d6846c45251ccbe3` |
| Sound and controls constructors | `ev_c5b83f95f73d210a272b7ad2a5adf80df4dae747c8a10a89192c114b40fad6b3` |
| Hit test, press, release, hover | `ev_e2e628903f98e06061c55432e5e0a645a4cff4c14325700b30fb758dc468cdf1` |
| State drawing and slider class | `ev_69f35cc294d0804bda8c5c875c46e61cef209e553ccfdcd0dbf6f43169ed485f` |
| Display handlers and reset | `ev_cd88af570a12db4894be65951ebbaa3e4886f907758d0f36b3ba8908a23e5fff` |
| Page click handlers | `ev_564c4b069804e13604747fda6d9836e422750268efb753278e205e2d414b6099` |
| Sound and controls apply and reset | `ev_b5cb1fa561f7bdd49c4ef9d6c8077de6aeac03c27acc1faea952df96666f7c6d` |
| Transparency sample, swatch, speed demo | `ev_4a78e6443ef527c2427b3a0499f87fac766aa3885be903956d1f03601772ba9a` |
| Page routing | `ev_00dfa43ddb71bac0ddcf9e083d52a3b5f487f6ee19a4fb57e84dc1e1a95376a0` |
| RLE decoder | `ev_48d919e3c9da05017791f938c5938dfb8965c2c5cedb0145162a6c8b9cb955e1` |

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
  `input.h`, `options.h`, `transition.h`). `test/module.mk` adds them to
  `make test` only when `ENABLE_DELTA` is `STATIC_PLUGIN`, and links
  `engines/delta/data.o`, `engines/delta/script.o`, `engines/delta/options.o`,
  and `engines/delta/transition.o`. There are no runtime tests. The REA-derived runtime
  claims above were not rerun for this documentation update.
