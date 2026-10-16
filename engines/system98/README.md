# System-98

Partial implementation of Four-Nine's System-98 visual novel engine. Startup
now reaches Mugen Yasoukyoku's interactive main menu, including its Options
screen, and the new-game transition to its initial background. Pi graphics,
patterned transitions, palette fades, basic Japanese text, startup script
instructions, and MIDI music and effects are implemented. Full gameplay,
game save/load, and PMD/FM playback remain unsupported. Launching displays a
warning about this limited support.

## Initial Release

- Game ID: `mugenyasoukyoku`
- Title: Mugen Yasoukyoku (Nocturnal Illusion)
- Developer/publisher: Apricot
- Release: Japanese PC-98, April 22, 1995, four floppies
- Engine version: System-98 3.10

Point ScummVM at a directory containing the extracted floppy contents together.
Detection requires all four `DISK_A` through `DISK_D` CAT/LIB pairs, matching
each file by its complete size and MD5, using
ScummVM's default first-5000-byte hashing (the entire file for shorter files).
It does not depend on the executable, which is shared with other System-98 games.

## Initialization

The original `AUTOEXEC.BAT` and `MUGEN.BAT` both run:

```bat
font /k mugen.fnt
SYS98.EXE run.s 0
font /r
```

ScummVM indexes the four `DISK_A` through `DISK_D` CAT/LIB pairs, supporting
uncompressed `Cat0` catalogs and System-98 LZSS-compressed `Cat1` catalogs and
members. Loose files take precedence over archive members, as in the original.
Member ranges and decoded sizes are checked before use.

Startup loads the 6016-byte `MUGEN.FNT` custom character bank, `SE.MSE` into
the original 0x800-byte limit, and `run.s` with this build's 0x4000-byte limit.
The first 0x100 scene bytes are preserved; remaining bytes are XORed with 1.
The script entry offset is 0x100. Integer/string storage, the return stack,
palette state, and music mode are initialized. Music mode 0 corresponds to
the batch file's second argument; no DOS resident driver is launched.

The display uses a 640x400 indexed framebuffer, two hardware-style graphics
pages plus a software page, and a text overlay. Styled textbox glyphs use the
16-color graphics palette; hardware-style text retains its eight-color palette.
The initial mouse position is (320, 200). DOS/BIOS vectors and PC-98 I/O ports
are not accessed on the host. The renderer uses the game's custom font bank
for its replacement characters rather than installing it into hardware.

System-98 images use a stripped-header Pi variant, not a complete `Pi` file.
The PC-98 analog palette has four significant bits per component, and its
hardware upload order is green, red, blue rather than Pi's RGB triplets.
The Pi decoder supports the move-to-front color table and all five repetition
locations, checking bitstream and output bounds.

The initial extraction had LIB files truncated to 102400 bytes, with 602 of
850 catalog entries extending past EOF. A second extraction provides complete
libraries: A is 693902 bytes, B is 1082613, C is 869244, and D is 940274.
Detection requires these complete sizes and rejects the truncated extraction.

## Main Menu

Startup follows the original scripts, not a hardcoded menu: `run.s` loads
`oyapat.g` and captures four graphics patterns; `open.s` loads `Mug_Sub.s` into
the secondary script bank, then draws `winopn.g`, `backtmp1.g`, and
`backtmp2.g`. The title menu runs its coordinate-input loop at `open.s:071d`.
Options uses the script's rectangular hit regions.

Mouse movement selects entries. All implemented commands and mouse buttons
are remappable through ScummVM's Keymaps settings, with these defaults:

| Command | Keyboard | Gamepad |
| --- | --- | --- |
| Next message / Select | Enter, Space, keypad Enter, X | A |
| Back / Cancel | Escape, Z, keypad Divide, keypad 0 | B |
| Up / Down | Arrow keys, keypad 8 / 2 | D-pad up / down |
| Left / Right | Arrow keys, keypad 4 / 6 | D-pad left / right |
| Left / Right click | Mouse left / right buttons | Unassigned |

These include modern equivalents of the original PC-98 confirm/cancel and
cursor/keypad inputs. Navigation uses the same script-defined menu regions;
left/right also moves the logical pointer within coordinate-driven screens.
Commands not yet implemented, such as game quick-save/load or backlog, are
not exposed as nonfunctional mappings.

The startup register/configuration files use ScummVM's SaveFileManager
and do not modify original game files. These files are not full game saves.
Music mode 0 is silent; resident PMD/MMD programs are not executed.

Japanese text prefers `FONT.ROM`, `FONT.BMP`, or ScummVM's `SJIS.FNT` in the
search path. As a fallback, supply `NotoSansCJK-Regular.ttc`,
`NotoSansCJKjp-Regular.otf`, or `NotoSansJP-Regular.otf` plus ScummVM's
`encoding.dat` through the extra path. The Noto fallback approximates the
original glyphs. Building this engine requires high-resolution graphics and
FreeType2. Enable it with `./configure --enable-engine=system98`.

The decoder tests include the published Pi example's exact decoded pixels
and palette, overlapping repeats, truncated streams, and output overruns.
Run them with `make test`, or build `test/runner` and run
`./test/runner System98ImageTestSuite` for the focused image checks.

## Options Rendering

Opcode 0x36 imports eight 48x48 nine-tile window styles from graphics page 1,
after the script loads `winopn.g`; it is not a text-clear operation. Options
opens textbox 1 at (224, 64), size 192x144, using style 1. The native draw path
at 0x1257d repeats the nine 16x16 tiles, with planar copies at 0x12603/0x12622.
Evidence: `ev_eb0c1dcb5a204ddf1a7bf935a7a8314fe0b4b60da00d13cb5226311ae4f2c1b0`,
`ev_0730bac2464cf295cf974006d2bc6ca3e57e4a4688ea799cb7a9823d8086fdf0`.

Options entry color 2 refers to the graphics palette, producing tan glyphs,
not fixed hardware-text red. Row spacing comes from register 0x402 (20 pixels
in `open.s`). Print control `0f 00 05` selects highlight style index 5. The
native highlight at 0x11f6f frames the active row using that atlas style, with
transparent pixels and a glyph-preservation mask. Menu rectangle ends are
exclusive. The overlay is refreshed on hover and removed without trails;
closing a textbox restores its previous overlay and glyph mask.
Evidence: `ev_a4db0af8f314853338361a831dcc100a75c719e9cff0deeaec51b971c20913a2`,
`ev_8c662b0a521f865446abaf8d20bc1fb7f9397eda8dcc8db45186e6cd13197ac8`.

The resulting window, colors, and red selection indicator were compared with
the supplied PC-98 emulator capture. Noto fallback glyphs remain approximate;
a PC-98 font ROM is preferred for original glyph shapes. The placeholder
free-moving pointer is hidden, retaining the script-driven selection graphics.

## Audio

The engine plays the game's original M.M.D. MIDI arrangements (`g*.m`) through
ScummVM's configured MIDI driver. New configurations default to MIDI mode 3;
saved game configuration can override this. Mode 0 remains silent. PMD music
in `b*.m` and `n*.m` and FM effects in `SE.EFC` are not implemented, so FM
driver modes 1 and 2 report unavailable rather than pretending to play.

M.M.D. sequences are decoded into Standard MIDI Files for ScummVM's existing
MIDI parser. In MIDI mode, music requests for the FM arrangements select the
corresponding `g*.m` track. The five length-prefixed M.M.D. effects in `SE.MSE`
use a separate parser and MIDI channel state, allowing effects over music.
Playback supports looping music, stop, fade-out, pause/resume, music/effect
volume and mute, and script music-status/measure queries. No DOS driver is run.

All 12 MIDI arrangements and all five effects from the complete extraction
passed conversion and ScummVM's SMF loader. Decoder tests cover note timing,
compressed commands, finite loops, malformed data, and effect-bank bounds;
run `./test/runner System98MmdTestSuite` after building `test/runner`.
Runtime playback was audibly confirmed by the user. A separate attempted
loopback capture was silent, so it is not evidence of playback failure or a
verified recording of fade/effect output.

## Startup Research

REA/Ghidra static analysis used the supplied LZEXE 0.91-compressed executable
(SHA-256 `339babdce4eb0b04d81ae73edd1d5ca3d16c0eefd47d85c00a727ee56b3a49e6`)
and a separately unpacked copy
(`7038db40f09955e1080952dee24f79ce52792d7626d95fc95d966cfb1be08916`).
Addresses below are linear addresses in Ghidra's load segment 0x1000, not
runtime DOS addresses. The original executable is not needed by ScummVM.

**Dispatcher, 0x1137c:** startup calls and dispatch at scene offset 0x100;
opcodes through 0x97, unlike the Canaan research's 0x8e table.
Evidence: `ev_6e240567f0734fc6c39b80a6d0eca6696df0f23a36c089d92adb4cabc5ff3564`.

**Resource open, 0x10258:** loose-file open, followed by archive fallback;
0x10294 reads member length at record +14 and offset at +18, relative to LIB
header +6.
Evidence: `ev_35474c0c1cd966e71b1c1be60ecc427205ba0e1b20207730f134a496d94aaa4a`,
`ev_119ffa4c3ab7e6049b4194a1e38cb3a5426455fd363f0840f7cfe3e483c49531`.

**Sound effects, 0x118ac:** startup load; the string at 0x1bf9d is `SE.MSE`.
Evidence: `ev_ecbbe56d10e2cb9a2e9aa0cb52e6dfeaf5e03f575e729c7365ead80e5c7d71e6`,
`ev_179cb9d3766b0c32eaa7b9deebce49c498d29aabaa35e8c96179a570dd833039`.

**Display, 0x114e8:** PC-98 INT 18h graphics setup and mode writes 0x01/0x41
to port 0x6a. These are PC-98 calls, not IBM-PC video services.
Evidence: `ev_5492d9cc6333e9e59d88d96d16ce5cef2b4c3b1a86a5626d82172883f659292f`.

**Scene load, 0x11936:** 0x4000-byte scene read and XOR decoding after 0x100.
Evidence: `ev_1d38d64762842f2776d0f55501690e94d77412a97065d9cb832bebf0383d99d6`.

**Runtime, 0x11993:** music mode parsing; 0x11b91 sets the mouse center
(320, 200) and original pointer bounds (0, 0) through (625, 385).
Evidence: `ev_586af3c2c605c126eb61550a4b4cb54a2d724e8b4564d53a698fb6b591115b38`,
`ev_54102263f676e04680eb5354322c37cac5d2070018710d5398e3233c6d0a906f`.

REA's loaded-image verification matched the unpacked image's mappings and
relocations: `ev_1f80f91d08e7ec6216e27d2e54a1f62bf431b997621feab2c9d36244eccaa185`.
This is static evidence, not execution of the original on a PC-98 emulator.
ScummVM startup was separately checked with the real data under GDB using
SDL's dummy drivers; it reached PC 0x100 with first opcode 0x0002.

## Main-Menu Research

The linked scene-format document is the baseline, but this executable differs
from Canaan in several important ways. REA observations used the same unpacked
artifact and load segment as the startup investigation above.

The dispatch table at 0x113b8 has entries through opcode 0x97. Evidence:
`ev_5280a2a998c85b0c03bc8b89e398d582fbb7e67dc4b4688cc6c05dd10adefd79`.

Register addressing at 0x12379 is a word-indexed bank. MOVI at 0x13f72 reads
one 16-bit immediate even for IDs above 0x3ff, unlike the reference's 32-bit
register interpretation. Evidence:
`ev_308c74b567f897b4d1e4eee6fb168b5bf5f4fd715b33c595b3fe51fc2bedadb9`.

Opcode 0x8f at 0x13d8c loads the second 0x4000-byte scene bank. Script
addresses above 0x3fff address this bank; `open.s` calls its shared routines
at addresses such as 0x4120. This is established by the native bank-loading
instructions and the decoded game's call targets, not the Canaan table.

Opcode 0x96 captures eight rows of page-1 intensity-plane bytes via 0x11299;
its count is in bytes (eight pixels each), not indexed pixels. The captured
layout is column-byte first, then row; each column supplies one transition
stage. It is not a textbox command. Evidence:
`ev_9fd1635bdadc7fd2377595c6971dbdb1c52197de57c5e9805ea9c84f37c6ba03`.

Opcode 0x95 invokes the patterned blit at 0x110f8. Its eight word arguments are
source byte-column, source row, width in bytes, height, destination byte-column,
destination row, pattern slot, and frame delay. Each stage tiles its mask across
the rectangle, copying page 1 into page 0 only where mask bits are set. Color
zero is copied too. The mask-row index restarts at zero for each stage.
Invalid or uncaptured slots do nothing. Evidence:
`ev_d554152d48709be9edae880b033e89ed8a2bc7e7d15dec26defe1c1482e8e239`,
`ev_a1c2fdbf96a9f50a97e1faf854e416dfa3cf28a0d662a0b86cd3fc8fda0822a4`.
The timer is started before rendering and waited on afterward by 0x122bc and
0x122cf. ScummVM presents each completed stage and uses its frame-delay loop.
Pixel-level regression tests cover bitplane packing, masked replacement,
stage progression, short-height phase reset, clipping, and invalid data.

Opcode 0x80 (STRFLD) reads a two-line text file into consecutive 80-byte
strings, splitting on CRLF and stopping at the DOS 0x1a marker. The opcode's
string ID must leave room for both entries. String bytes are preserved for
Shift-JIS text. Loose filenames are tried as provided and as Windows-1252 bytes
transcoded to UTF-8, for extracted disks whose legacy filenames were converted
that way. The native handler at 0x14d3a reads the string ID and scene-relative
filename, then copies rows until EOF; row handling at 0x14d79 recognizes CRLF
and 0x1a. Evidence: `ev_c3c164131ce4308b774e5e9fae294cb934f5aafb93c7fd1dcea77b2446ac0780`,
`ev_bccf6b98119000c8202a009c3b0dae1616cf3909be1d7afe979c2c148e2a97fcec`.

Opcode 0x6e (`STRLEN`) writes the bounded byte length of a string into a
register. Opcode 0x53 (`STRCAT`) appends one string to another, keeping the
destination NUL-terminated within its 80-byte slot. These support the first
scene's script-driven menu setup.

Opcode 0x91 calls 0x1159c with a mode register and result register. The original
launches DOS sound drivers only when needed. This implementation supports
silent mode 0 and MIDI mode 3, and reports failure for unavailable FM drivers.
Evidence: `ev_6a7ea3e4fd9254c3a8b21d71d91b48cbbccef272be40916ab8e8326f4a1c0ad2`.

The executable's configuration defaults have music mode 0. Evidence:
`ev_68dba5bcf945e98c989d4b719519152e97dcdf2bfd3804d2ab35a24546f5be79`.

Palette handlers at 0x146aa and 0x14781 distinguish fading toward the loaded
image palette (0x07) from fading the current palette to black/white (0x08).
Mouse menus return action 2 for left/accept and 1 for right/cancel. These were
checked against both native handlers and the game's script comparisons.

Page blits 0x2f/0x3a are opaque, while 0x63/0x64 preserve the destination
where the source color index is zero. The native alternate path at 0x129b6
builds a mask at 0x12c29 by ORing all four source bitplanes and inverting it.
The per-plane copy at 0x12a41 applies `(destination & mask) | source`, replacing
nonzero source pixels rather than ORing their color indices with the background.
Evidence: `ev_db5d39556745b0bd86935ca45f0b477c96975e11bbb06eaae18df255d1191266`,
`ev_44d9db1da4390938a6fba1e1fcd7d904ca3a01690d29ab6c0235d98a58930f0c`,
`ev_6d2a824b766adea2b13ebdec3faae93ea3acb1ea33ab2a3335f9464c2c154d8a`.
Native window checks confirmed transparent menu labels, arrow, and Options
heading, with opaque background restoration still working during hover.

Native ScummVM window checks reached the visible title menu from `run.s`,
verified hover changes, and opened Options using mouse and keyboard input.
Options can return to the title menu. Start completes opcode 0x95 at
`Mug_Sub.s:4c9c` using eight stages of pattern 1 with four frames per stage,
then loads `BK0.G`; opcodes 0x80, 0x6e, and 0x53 load, measure, and combine the
two string slots. Runtime reaches `0_00.s` and presents its five-region
first-scene command menu with four visible Japanese options. Text and menu
selection are working at this gameplay-entry point; later dialogue and scene
paths may still encounter unsupported instructions.
These validate this implementation, not execution of the original DOS game.

## References

- [System-98 research by ValleyBell](https://github.com/ValleyBell/PC98VNResearch/tree/master/four-nine_system98)
- [Research game list](https://github.com/ValleyBell/PC98VNResearch/blob/master/four-nine_system98/game-list.md)
- [System-98 scene format](https://github.com/ValleyBell/PC98VNResearch/blob/master/four-nine_system98/SceneFormat.txt)
- [Pi graphics format](https://mooncore.eu/bunny/txt/pi-pic.htm)
- [Game](https://vndb.org/v725)
- [Target PC-98 release](https://vndb.org/r18109)
