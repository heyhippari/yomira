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
modified. No game files are written.

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
| `0x23`, `0x26`, `0xb0`, `0xb1` | Load backgrounds and layered foregrounds |
| `0x19`, `0xa4` | Clear a bounded scene rectangle toward black or white |
| `0x21`, `0x8a` | Bounded script calls and returns |
| `0x3a`, `0x3b`, `0x3e`, `0x3f`, `0x61` | Event flags, checkpoint counters, and title strings |
| `0x4b`, `0x54` | Decode message tokens and wait for text-page input |
| `0xa2` | Decode a three-word sound command; audio playback is not implemented |
| `0x99` | Arm a deadline in 100 ms units or wait across frames |
| `0xae` | Clear/fade toward black |
| `0x67` | Enter the title menu and suspend scenario execution |

The runner processes at most 64 commands per frame, keeping events responsive
even for a script loop. Unsupported commands and invalid operands stop with a
diagnostic containing the script position. Conditions and jumps determine the
startup path; no offsets are scanned heuristically to find the menu.

`CG/EVB.CGF` contains a 32-bit entry count followed by 20-byte records: a
16-byte filename and a 32-bit absolute offset. Records are bounded by the next
offset or EOF and contain the same IAF wrapper as loose files. One Lover's
`WARNING.IAF`, `TITLE.IAF`, and `MENU.IAF` are read this way. EV/ST color-mask
banks support recovered character and event layers, including partial images
with stored origins. Event-specific effects and animation timing remain
incomplete.

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

Following the game-developer agent's design, Delta caches separate narration
and named-dialogue frame surfaces, preserves the masks at 24-bit precision,
and composites onto the clean scene snapshot on every page. It uses style 0
and transparency 255, matching the supplied One Lover preferences
(`WINDWALLP=0`, `WINDWPNTR=255`), without importing or changing the Windows
INI. Speaker zero does not draw a name. Transparent framing alone is not the
original text presentation: the first implementation omitted the text-shadow
pass and consequently left white glyphs without sufficient contrast.

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
  Thus an invisible window interior is not evidence that plain white text is
  faithful to this configuration.

Following the game-developer agent's correction design, both speaker names
and dialogue now use a shared black (2, 2) shadow pass followed by white text,
without moving the foreground, changing wrapping, or darkening the scene.
Noto's antialiasing, metrics, and regular weight remain approximations of the
Windows glyph rasterizer and fonts. Alternate outline mode, per-state text
colors, preference controls, alternate window styles, window animation, and
wait-indicator behavior remain unimplemented. This investigation is static
evidence from One Lover, not a
runtime comparison with the original Windows renderer or an independent
verification of the packed second game's drawing code.

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

## Menu And Save Integration

Both supplied scripts use menu opcode `0x67` (PHM raw `0x66`) with destination
variable 6, followed by the one-based `0x95` branch table. Its first four
results enter Start, Load, Settings, and Quit branches. One Lover's result 5
enters Extras and result 6 returns to the title loop. PHM uses result 5 for the
title return and result 6 for Extras. The UI uses ScummVM dialogs for the host
operations instead of recreating the original Windows save/options screens.

The native handler entries inspected for these branches are `0x40407b` (load),
`0x408747` (options), and `0x401e2f` (exit). The original save/load implementation
and original save files are not reused.

Delta implements AdvancedMetaEngine extended saves, including launcher loading,
listing, thumbnails, timestamps, playtime, descriptions, and deletion. Saving
and in-game loading are currently restricted to stable title-menu checkpoints;
they do not imply support for saving unfinished story execution. The global
ScummVM menu exposes Save and Load, and the original title Load option opens the
same restore chooser.

The engine payload starts with `DLTA` and format version 1. It stores the game
ID, scenario size and number, random seed, interpreter dialect/cursor and 1024
variables, and title-menu resource/geometry/focus state. Invalid versions,
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
