# System-98

Partial implementation of Four-Nine's System-98 visual novel engine. Startup
now reaches Mugen Yasoukyoku's interactive main menu, including its Options
screen, and the new-game transition to its initial background. Pi graphics,
patterned transitions, palette fades, basic Japanese text, startup script
instructions, P.M.D. FM music and effects, MIDI music and effects, and the
`ENDING.EXE` staff roll are implemented. Full gameplay and game save/load remain unsupported. Launching
displays a warning about this limited support.

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
`PMD.COM`, `PMDB2.COM` and `SE.EFC` are read as data for FM playback.

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

## Staff Roll

Every ending scene (`c_01.s` to `c_08.s`) fades the music, waits, then runs
opcode 0x97 and returns to `open.s`. The original runs `ENDING.EXE くりあ`; the
program refuses to start without that argument. ScummVM reimplements the
program in `ending.cpp`, reading `ENDING.GLB` and `ENDING.FNT` as data. A
linear disassembly finds only INT 18h and INT 21h calls, so the program makes
no sound-driver calls; music is left to the scripts. If either file is
missing, the staff roll is skipped with a warning.

`ENDING.EXE` (9553 bytes, SHA-256
`7ea977e3ee5e17700ae7acab0264abf8bf2012684518c4fa2198e997dffc5587`) is a
Turbo C++ program packed with LZEXE 0.91, whose header signature has been
cleared. The unpacked image was rebuilt as a plain MZ executable (SHA-256
`bae1a757bc2744b6b4ab0ed1e90019b1cd982f876ad1315825f1eaf9f5987418`) and
decompiled with REA/Ghidra. Addresses below are Ghidra linear addresses with
load segment 0x1000; the data segment is 0x12ef. Evidence:
`ev_d66f2ceb5555353d212bb9d046c248fecf7a9cb268d1ef8eb026c9f1b5960ac2` (main),
`ev_94ddfc27faea7b669348bf677c998c2500b748ece6f8536fe0ac1a5efb81d8eb`,
`ev_c9a124691d64f9875728a7f3e94e76d499fe73eb834b28d928ac370486d2b490`,
`ev_1cf2070f75ca81c3d005d0ebb5be3918a8b1393d3ca0cc63e4062e75cec38495`.

### Data

- Both files start with a LE16 entry count. Each entry is a LE16 length, then
  that many bytes: a LE32 decoded size and System-98 LZSS data (0x1046a,
  0x1053f; same window and flags as the archives).
- `ENDING.GLB` (205023 bytes) holds 13 planar pictures. Each stores its B, R,
  G, and I planes one after another, each `width` bytes by `height` rows.
  Entries 0 to 10 are 304 rows tall, the scrolling pictures. Entry 11 is a
  38x96-byte title card and entry 12 an 8x40-byte "Fin." mark.
- `ENDING.FNT` (7948 bytes) holds 10 text blocks of 16x16 glyphs, 32 bytes
  each, two bytes per row with the left half first. Glyphs are uploaded as
  user-defined characters from JIS 7621h (0x1033c, 0x1042c) and laid out left
  to right, top to bottom (0x10390).
- Every decoded size matches the dimensions in the program's tables exactly.

### Sequence

`main` (0x11227) loads both files (0x10ff5), then installs a vertical-sync
handler at 0x10bea. Each tick, a divider that starts at 0x80ff and gains 0x100
per tick applies the posted scroll line on every second tick, after an initial
128 ticks. The handler also steps palette fades and counts down timers. Palette
words are `G << 8 | R << 4 | B`. A fade has 15 steps. A falling component
drops by one per step; a rising one starts only when its distance to the
target equals the steps left (0x10d3d).

Each of 10 sections (picture and text block `n`) runs as follows:

1. Text VRAM is blanked. Row 0 shows a glyph whose top four lines are black,
   hiding the line where the scroll wraps (0x10604).
2. The text block appears as black-attribute glyphs at its table position
   (0x11192). The palette is still black.
3. The display fades in over 10 ticks per step to a sepia palette in which
   color 11 is cream (0xdec).
4. One picture line is drawn per scroll line (0x1086d, 0x10c6c). For the first
   304 lines, the whole VRAM row is filled with color 11 before the picture row
   is copied at its byte column. Lines 400 and later are cleared to color 0.
   The GDC scroll (0x1027d) brings each new row in at the bottom. The black text
   is therefore only visible against the cream band.
5. After 560 lines, the display fades to black over 11 ticks per step while
   the remaining 144 of 704 lines scroll.

In the finale, picture 10 scrolls 368 lines over black, with no cream fill or
text. It is then copied to page 1 at row 32, and from there back to page 0
with the scroll reset to 0. The title card and "Fin." are staged on page 1
below row 304. After 58 and 87 ticks respectively, each dissolves into page 0
at (176, 136) and (456, 344) (0x1099e). A dissolve has 14 stages of 8 ticks,
using an 8-byte mask per stage selected by `row & 7` (DS:0x1b7e). The program
then waits 580 ticks and for the Return key, fades to black over 9 ticks per
step, and blanks text VRAM, clears graphics page 0, and resets the scroll.

At 56.4 Hz the sequence lasts 15892 ticks, about 4 minutes 42 seconds, before the Return
prompt. ScummVM accepts its accept action or the left button for Return. The
skip action runs ticks without waiting.

ScummVM draws into the same display pages and text overlay as the
interpreter. It keeps the decoded data in memory instead of decompressing into
page 1 VRAM as the original does. Its drawing takes no time between ticks. In
the original, the handler could apply the last posted scroll line around the
reset to 0; ScummVM always resets the scroll after one tick.

A headless run from `c_01.s:0302`, without tick delays and with an injected
accept, played the whole staff roll and returned to `open.s`. The section 1
screenshot matched a Python prototype of the format. Screenshots of section 6,
the final picture, and the finale showed the expected layout, with the title
card aligned to the final picture behind it. No comparison with an emulator
recording has been made, and real-time playback has not been watched.

## Audio

The game offers four music settings, chosen on its first-run prompt and Options
screen and stored in register 0x400: 0 no music, 1 mono FM (`n*.m` through
`PMD.COM` on a PC-9801-26K), 2 stereo FM (`b*.m` through `PMDB2.COM` on a
PC-9801-86), and 3 GS MIDI (`g*.m` through `MMD.COM`). The script chooses the
file set from the mode. New configurations default to stereo FM, mode 2; a
saved configuration can select any other mode.

FM modes play the P.M.D. songs and effects through ScummVM's FM-TOWNS/PC-98
OPN/OPNA emulation (`PC98AudioCore`), emulating a 26K board in mode 1 and an 86
board in mode 2. `pmd.cpp` is a sequencer for compiled P.M.D. 4.8 data written
for this engine; no DOS driver code is run. Timer B drives music and Timer A,
at its longest period, drives effects and fades, as in the original driver.
FM effects come from `SE.EFC` and play on FM channel 3. SSG effects and the SSG
drums used by the K/R rhythm parts come from the effect table in the selected
driver file. In mode 2 the rhythm parts also trigger the OPNA rhythm source.
The FM, SSG, and rhythm volume adjustments match the options `SYS98.EXE`
passes to each driver (`/DF20`; `/DF10 /DS0 /DR16`).

In MIDI mode, M.M.D. sequences are decoded into Standard MIDI Files for
ScummVM's existing MIDI parser. The five length-prefixed M.M.D. effects in
`SE.MSE` use a separate parser and MIDI channel state, allowing effects over
music. No DOS driver is run.

Both paths support looping music, stop, fade-out, pause/resume, music/effect
volume and mute, and script music-status/measure queries. In FM modes, effects
on FM channel 3 and SSG channel C follow ScummVM's effects volume while they
play.

All 12 MIDI arrangements and all five effects from the complete extraction
passed conversion and ScummVM's SMF loader. Decoder tests cover note timing,
compressed commands, finite loops, malformed data, and effect-bank bounds;
run `./test/runner System98MmdTestSuite` after building `test/runner`.
MIDI runtime playback was audibly confirmed by the user. A separate attempted
loopback capture was silent, so it is not evidence of playback failure or a
verified recording of fade/effect output.

All 24 FM songs (both board variants) run through the sequencer and loop; each
`n`/`b` pair reaches its loop point on the same tick. `SE.EFC` effects 1 to 4 and
the 11 drum effects run to completion with both drivers. A headless capture
of the title screen recorded `b01.m` in mode 2 and `n01.m` in mode 1, with
spectral peaks on equal-tempered pitches and no clipping in stereo mode.
The emulated 26K mixes mono near full scale; a few samples clipped in that
capture. Run `./test/runner System98PmdTestSuite` for the sequencer tests.

## Architecture

Source files are in `engines/system98/`:

- `system98.cpp`, `system98.h`: lifecycle, startup validation, the run loop,
  input and menu-event handling, and shared state.
- `resource.cpp`: loose-file and CAT/LIB lookup, catalog and member decoding,
  LZSS, and two-line string-file helpers.
- `script.cpp`: opcode dispatch, registers, strings, the return stack, scene
  bank selection, menus, and window-style setup.
- `graphics.cpp`: palette and fades, display pages, the text overlay and glyph
  mask, text boxes, and window drawing.
- `image.cpp`, `image.h`: stripped-header Pi image decoding.
- `pmd.cpp`, `pmd.h`: P.M.D. song, FM effect and SSG effect sequencer, writing
  OPN/OPNA registers through a small chip interface.
- `audio.cpp`, `audio.h`: the `PC98AudioCore` FM wrapper, and the MIDI player
  for music and effects, with fades,
  pause, and volume.
- `mmd.cpp`, `mmd.h`: M.M.D.-to-SMF conversion and effect extraction.
- `dialogs.cpp`, `dialogs.h`: the name dialog that replaces `NAMCHG.EXE`.
- `ending.cpp`, `ending.h`: the staff roll that replaces `ENDING.EXE`.
- `detection.cpp`, `metaengine.cpp`: game detection and engine registration.

Runtime state:

- Startup indexes the archives and requires `MUGEN.FNT` to be exactly 6016
  bytes and `SE.MSE` to be at most 0x800 bytes. The first opcode in `run.s`
  must be 0x97 or lower.
- `run()` executes instructions in a loop and sleeps for 1 ms every 128
  instructions. If an instruction is invalid or unimplemented, the run ends with
  an error.
- Two 0x4000-byte scene banks are available. Opcode 0x8F loads the secondary
  bank (see Version Differences).
- Registers are 1040 16-bit values. There are 50 string slots of 80 bytes each,
  and an eight-entry return stack of 16-bit offsets.
- Graphics use three 640x400 display pages, a 640x400 text overlay, a 640x400
  glyph mask, and a 640x400 screen surface. The palette holds 16 RGB entries.
- `present()` composes the display page, the text overlay (value 255 is
  transparent), and menu window tiles, sets the palette, then copies the result
  to the screen.
- The pointer is clamped to (0, 0) through (625, 385). A menu selection writes
  its ID and action to two script registers and jumps to the offset from opcode
  0x40. Coordinate menus (opcode 0x41) write the pointer position and action to
  registers instead.

## Data and Script Formats

The notes below restate the Four-Nine System-98 format research compiled by
ValleyBell in PC98VNResearch (see References), written in local wording and
checked against this build. That research centers on the engine used in
Gao Gao! 4th: Canaan, System-98 v3.10. Earlier revisions and the related DIANA
engine are noted where relevant.

### Archives

- Each disk has a `DISK_A` through `DISK_D` catalog (`.CAT`) and library
  (`.LIB`) pair.
- A catalog begins with a big-endian tag, `Cat0` (plain table) or `Cat1`
  (LZSS-compressed table), followed by a 16-bit little-endian entry count. A
  compressed table decompresses to count x 22 bytes. Each 22-byte record holds
  a 12-byte name, a 16-bit little-endian flags word, a 32-bit size, and a 32-bit
  offset. Bit 0 of the flags marks a compressed member. Any other bit makes the
  catalog invalid here.
- A library begins with the big-endian tag `Lib0` and the same count. Member
  offsets are relative to the byte after its 6-byte header.
- A compressed member starts with a 32-bit little-endian decompressed size, then
  an LZSS stream.
- Loose files in the game directory take precedence over archive members.

### LZSS

- The window is 4 KiB and starts zeroed, with the write position at 1.
- Each flag byte covers eight items, least significant bit first. A set bit
  means one literal byte. A clear bit means a 16-bit little-endian reference:
  the low 4 bits give the length minus 3, and the upper 12 bits give the
  window read position.
- A zero reference word ends the stream. The decoded output must match the
  stored size exactly. This build also rejects output overruns and members
  above 16 MiB.

### Scene Scripts

- Bytes 0x000 to 0x0FF are a header kept unencrypted. From 0x100 on, every
  byte is XORed with 0x01.
- A script is a sequence of 16-bit little-endian words: an opcode followed by
  operands. Operand counts and widths depend on the opcode.
- The reference documents opcodes 0x00 through 0x8E for v3.10. The Mugen
  executable dispatches further; see Version Differences.
- Compare instructions store 0 for equal, 1 for less, and 2 for greater.
  Conditional jumps (0x0D to 0x12) test that stored value.
- Registers are 16-bit, as in the Mugen executable. The reference describes a
  32-bit form for IDs of 0x400 and above, which Mugen does not use.

#### Opcode reference

Each command is a LE16 opcode followed by LE16 operands unless noted. Operand
count and meaning depend on the opcode, and scene files also hold embedded
strings and tables, so a word after an opcode is not always an operand.
`reg` is a 16-bit register ID, `string` is a string slot, `file name` is a
scene offset to a NUL-terminated name, and `offset` is a scene offset.
Summaries are local; names and v3.10 status follow the
[System-98 scene format](https://github.com/ValleyBell/PC98VNResearch/blob/master/four-nine_system98/SceneFormat.txt)
note in References.

Status key:

- Implemented: handled with the reference's operands and meaning, as far as checked.
- Partial: handled, but narrower, interpreted, or different from the reference.
- Not implemented: defined in v3.10, with no handler.
- Undefined (v3.10): not defined by the v3.10 reference.
- Unknown (Mugen): dispatched by the Mugen executable; no behavior in this build.

Not-implemented and undefined IDs both reach the default handler. It logs the
ID, marks the script invalid, and ends the run with an error. It does not skip
operands. Opcode 0x00 is the only ID that ends a run without an error.

| ID | Name (v3.10) | Operands | Summary | Status |
| --- | --- | --- | --- | --- |
| 0x00 | DOSEXIT | none | Ends the run without an error. | Implemented |
| 0x01 | - | - | Undefined in v3.10 (v1.xx text box command). | Undefined (v3.10) |
| 0x02 | IMGLOAD | x/8, y, palette flag, plane, file name, transparent flag | Loads an image. Planes 0 and 1 are the VRAM pages; 2 and above decode into the software page (page 2). | Implemented |
| 0x03 | TBOPEN | box ID, x/16, y/16, w/16, h/16, style | Opens one of 16 text boxes. IDs 0x10 and above fail. | Implemented |
| 0x04 | TBCLOSE | box ID | Closes a text box and restores the overlay it covered. | Implemented |
| 0x05 | TBCLEAR | box ID | Clears a text box's contents. | Implemented |
| 0x06 | PALAPL | none | Copies the image palette to the displayed one and redraws. The native handler (0x14690) copies 0x30 words, twice the palette, overwriting the return-stack pointer; no script reaches it. | Implemented |
| 0x07 | PALBW | direction, frames per step | Sets unlocked colors to black (0) or white (nonzero), then steps each 4-bit component by one toward the image palette, waiting before each upload, until all arrive (0x146aa). | Implemented |
| 0x08 | PALFADE | direction, frames per step | Steps unlocked colors of the displayed palette by one per step to black (0) or white (nonzero), as 0x07 does (0x14781). | Implemented |
| 0x09 | PALLCKT | colour ID (0-15) | Toggles a palette lock bit; 0x07 and 0x08 leave locked colors alone (0x1480e). | Implemented |
| 0x0A | JP | offset | Jumps to an offset. | Implemented |
| 0x0B | CMPR | reg, reg | Compares two registers. Stores 0 for equal, 1 for less, 2 for greater. | Implemented |
| 0x0C | CMPI | reg, value | Compares a register with a value, using the same result codes. | Implemented |
| 0x0D-0x12 | JEQ, JLT, JGT, JGE, JLE, JNE | offset | Conditional jumps on the stored compare result. | Implemented |
| 0x13 | JTBL | reg, table | Jumps to the table entry the register selects. The table follows the operand. | Implemented |
| 0x14 | PALCSET | index, red, green, blue regs | Sets one 4-bit palette colour in both the image and displayed palettes, then redraws. | Implemented |
| 0x15 | PRINT | box ID, text offset | Prints scene text in a text box. | Implemented |
| 0x16 | PRINTXY | x, y, text offset | Prints text at a screen position. | Not implemented |
| 0x17 | - | - | Undefined in v3.10. | Undefined (v3.10) |
| 0x18 | MOVI | reg, value | Sets a register to a 16-bit value, for any ID (0x13f72). | Implemented |
| 0x19 | MOVR | dst reg, src reg | Copies one register to another. | Implemented |
| 0x1A | BGMPLAY | file name | Plays the music file with the current driver: P.M.D. in modes 1 and 2, M.M.D. in mode 3. Silent in mode 0. | Implemented |
| 0x1B | BGMFADE | none | Fades out the music. P.M.D. uses the low byte of register 0x403 as its fade speed; MIDI mode derives a fade length from it. | Implemented |
| 0x1C | BGMSTOP | none | Stops the music. | Implemented |
| 0x1D | BGMMODE | reg | Stores the current music mode. | Implemented |
| 0x1E | WAITKEY | reg | Waits for input, then for one pressed bit to be released. Stores the pressed bits in the low byte and the state at release in the high byte. | Implemented |
| 0x1F | LDSCENE | file name | Loads a scene into bank 0 and restarts at 0x100. | Implemented |
| 0x20 | GV02 | none | Writes a fixed pattern to graphics memory. | Not implemented |
| 0x21 | WAIT | reg | Waits the number of frames in the register. | Implemented |
| 0x22 | CHRDLY | reg | Sets the delay between printed characters. | Implemented |
| 0x23 | - | - | Undefined in v3.10. | Undefined (v3.10) |
| 0x24 | ADDI | reg, value | Adds a value, wrapping at 16 bits. | Implemented |
| 0x25 | SUBI | reg, value | Subtracts a value. On a borrow, stores the magnitude and sets register 0 to 1 (0x1408d). | Implemented |
| 0x26 | TXCLR1 | x, y, width, height, flag | Fills 8x16 text cells with spaces, or with white full blocks when the flag is set. | Implemented |
| 0x27 | ADDR | dst reg, src reg | Adds one register to another, wrapping at 16 bits. | Implemented |
| 0x28 | SUBR | dst reg, src reg | Register form of 0x25, with the same borrow handling (0x14075). | Implemented |
| 0x29 | XYREAD | two words | Dispatched to the 0x4a handler in Mugen (0x13fc1). | Implemented |
| 0x2A | XYWRT | two words | Dispatched to the 0x4a handler in Mugen (0x13fc1). | Implemented |
| 0x2B-0x2C | - | - | Undefined in v3.10 (v1.xx register file save and load). | Undefined (v3.10) |
| 0x2D | REGCLR | first reg, last reg | Zeroes an inclusive register range. | Implemented |
| 0x2E | - | - | Undefined in v3.10. | Undefined (v3.10) |
| 0x2F | BLIT1I | eight words | Opaque rectangle copy between pages. | Implemented |
| 0x30-0x31 | - | - | Undefined in v3.10. | Undefined (v3.10) |
| 0x32 | PRINTR | box ID, reg | Prints a register as text. | Not implemented |
| 0x33 | GPDGET | reg | Shows the display page in the register's low bit (port A4h, 0x14395). | Implemented |
| 0x34 | GPDSET | reg | Stores the current display page. | Implemented |
| 0x35 | TXFILL | six words | Fills a text buffer area. | Not implemented |
| 0x36 | GFX36 | none | Copies eight 48x48 nine-tile window styles from the top of page 1 (0x1422a). | Implemented |
| 0x37 | XYFLOAD | file name | Loads the XY buffer from a file. | Not implemented |
| 0x38 | ANDR | dst reg, src reg | Bitwise AND of two registers. | Implemented |
| 0x39 | ORR | dst reg, src reg | Bitwise OR of two registers. | Implemented |
| 0x3A | BLIT1R | eight reg IDs | Register-indirect form of 0x2F. | Implemented |
| 0x3B | BLIT1RL | eight reg IDs | Long-width variant of 0x3A. | Not implemented |
| 0x3C | TXCLR2 | x, y, width, height, flag | Like 0x26, but the full blocks are black. | Implemented |
| 0x3D | MULR | dst reg, src reg | Multiplies two registers, keeping the low 16 bits. | Implemented |
| 0x3E | DIVR | dst reg, src reg | Stores the quotient in the first register and the remainder in the second. A zero divisor sets both to 0 (0x140ba). | Implemented |
| 0x3F | CMD3F | x reg, y reg | Moves the pointer to two register coordinates (0x14b28). | Implemented |
| 0x40 | MENUSEL | id reg, action reg, menu offset, jump offset | Runs a region menu. Any input selects; see Menus (0x149f4, 0x11c51). | Implemented |
| 0x41 | CMD41 | x reg, y reg, action reg, jump offset | Coordinate menu. Stores the pointer position and action, then jumps (0x14a3a). | Implemented |
| 0x42 | CMD42 | flag | Enables or disables the last menu 0x40 or 0x41 set up; does nothing before one. ScummVM also yields a frame when enabling (0x14a77). | Implemented |
| 0x43 | CMD43 | x reg, y reg | Stores the pointer position in two registers (0x14b01). | Implemented |
| 0x44-0x49 | - | - | Undefined in v3.10 (v1.xx dummy and GFX commands). | Undefined (v3.10) |
| 0x4A | REGFSAV | buffer, file name | Saves registers to a ScummVM save file: buffer byte 0 is registers 0-511, 1 is 512-1023, anything else 1024-1031. ScummVM appends the DOS time and date for 0x60 (0x13fc1). | Implemented |
| 0x4B | REGFLD | buffer, file name | Loads at most the buffer's registers from a save file, or from the game file of that name (0x1400f). | Implemented |
| 0x4C | CMD4C | two words | Stores two values. Reference purpose unknown. | Not implemented |
| 0x4D | BGMMEAS | reg | Stores the current music measure. | Implemented |
| 0x4E | SFXSSG | effect number | FM modes play the driver's built-in SSG effect. MIDI mode 3 plays an SE.MSE effect. | Implemented |
| 0x4F | SFXFM | effect number | FM modes play an SE.EFC effect. MIDI mode 3 plays an SE.MSE effect. | Implemented |
| 0x50 | BGMSTAT | reg | FM modes store P.M.D.'s status word: the song's status byte, then its loop count (0xFF when stopped). MIDI mode stores 1 while music plays, otherwise 0. | Implemented |
| 0x51 | ANDI | reg, value | Bitwise AND of a register with a value. | Implemented |
| 0x52 | ORI | reg, value | Bitwise OR of a register with a value. | Implemented |
| 0x53 | STRCAT | dst string, src string | Appends a string, including its terminator. | Implemented |
| 0x54 | CALL | offset | Calls a subroutine. The return stack holds eight levels. | Implemented |
| 0x55 | RET | none | Returns from a call. Fails if the stack is empty. | Implemented |
| 0x56 | STRCMPR | dst string, src string | Compares two strings. | Not implemented |
| 0x57 | STRNCPY | dst string, src string, count | Copies a fixed number of characters. | Not implemented |
| 0x58 | STRCPYC | dst string, src string, index | Appends one character from a string. | Not implemented |
| 0x59 | STRCPYI | dst string, scene offset | Appends a scene string. Truncated to 79 bytes. | Implemented |
| 0x5A | STRCLR | string | Clears a string slot. | Implemented |
| 0x5B | STRCPY | dst string, src string | Copies a string. | Not implemented |
| 0x5C-0x5D | - | - | Undefined in v3.10. | Undefined (v3.10) |
| 0x5E | CMD5E | page, x/8, y, w/8, h | Clears a rectangle on a page to colour 0. | Implemented |
| 0x5F | STRTIME | string | Writes the current date and time into a string. | Not implemented |
| 0x60 | FILETM | reg, string, file name | Stores 1 and the save's date as NEC half-width `YYYY-MM-DD hh:mm:ss` if the file exists; otherwise 0 and an empty string (0x13e95). | Implemented |
| 0x61 | XYFSAVE | file name | Saves the XY buffer to a file. | Not implemented |
| 0x62 | CMD62 | word reg, line reg | Sets the graphics display start (GDC SCROLL). Shown as a wrapping offset of the display page. | Implemented |
| 0x63 | BLIT2I | eight words | Like 0x2F, but zero source pixels keep the destination. | Implemented |
| 0x64 | BLIT2R | eight reg IDs | Register-indirect form of 0x63. | Implemented |
| 0x65-0x66 | - | - | Undefined in v3.10. | Undefined (v3.10) |
| 0x67 | GFX67 | four words | GFX command. | Not implemented |
| 0x68 | GFX68 | two regs | GFX command. | Not implemented |
| 0x69 | GFX69 | two regs, dst reg | Bit-mask GFX command. | Not implemented |
| 0x6A | CMD6A | four values | Sets inclusive pointer bounds, capped at (624, 384) and sorted per axis. The pointer is clamped when it next moves (0x14a89). | Implemented |
| 0x6B | CMD6B | four regs | GFX command. | Not implemented |
| 0x6C-0x6D | - | - | Undefined in v3.10. | Undefined (v3.10) |
| 0x6E | STRLEN | reg, string | Stores the string length, excluding the terminator. | Implemented |
| 0x6F-0x71 | - | - | Undefined in v3.10 (v1.xx register width conversions). | Undefined (v3.10) |
| 0x72 | FRMSTI | value | Sets the script frame counter. | Implemented |
| 0x73 | FRMGTR | reg | Stores the script frame counter. | Implemented |
| 0x74 | FRMJPI | value, offset | Jumps when the frame counter is below the value. Also waits one frame when taken. | Implemented |
| 0x75 | FONTCHR | JIS code, file offset | Loads a custom font glyph. | Not implemented |
| 0x76 | IDLECHR | eight words | Sets the JIS codes of the text-wait marker (0x1492b). | Implemented |
| 0x77 | IDLEDLY | eight words | Sets each marker frame's duration in vertical syncs (0x14930). | Implemented |
| 0x78 | IDLEXY | x, y | Offsets the marker from its box position, in text cells (0x14946). | Implemented |
| 0x79 | CMD79 | offset | Copies a data block to the end of the script. | Not implemented |
| 0x7A | FRMSTR | reg | Sets the frame counter from a register. | Implemented |
| 0x7B | FRMJPR | reg, offset | Register form of 0x74. Also waits one frame when taken. | Implemented |
| 0x7C-0x7E | - | - | Undefined in v3.10. | Undefined (v3.10) |
| 0x7F | DOSRETR | reg | Exits to DOS with a return code from a register. | Not implemented |
| 0x80 | STRFLD | string, file name | Loads two CRLF-separated lines into consecutive strings. | Implemented |
| 0x81 | STRFSAV | string, file name | Writes two strings to a file. | Not implemented |
| 0x82-0x85 | GFX82, GFX83, GFX84, GFX85 | varies | GFX and transition commands. | Not implemented |
| 0x86 | PTDATA | offset | Loads all portrait data. | Not implemented |
| 0x87 | PTDAT1 | index, plane, source x/y, two RGB colours | Sets one portrait's source and palettes (10 words total). | Not implemented |
| 0x88 | PTDSTI | plane, x/8, y | Sets the portrait destination. | Not implemented |
| 0x89 | PTPOSA | offset | Sets all portrait positions. | Not implemented |
| 0x8A | PTPOS1R | four regs | Sets one portrait position from registers. | Not implemented |
| 0x8B | PTDSTR | three regs | Register form of 0x88. | Not implemented |
| 0x8C-0x8E | GFX8C, GFX8D, GFX8E | none | GFX commands. | Not implemented |

#### Mugen extension IDs

These IDs are not in the v3.10 reference. Their facts come from REA notes on
the Mugen Yasoukyoku executable and do not describe System-98 in general.

| ID | Name | Operands | Summary | Status |
| --- | --- | --- | --- | --- |
| 0x8F | Mugen extension | bank, file name | Loads a scene into bank 1, the secondary 0x4000-byte bank. Bank 0 reloads the main scene. | Implemented |
| 0x90 | Mugen extension | none | Drops the newest return-stack entry without jumping (0x14222). An empty stack fails the run. | Implemented |
| 0x91 | Mugen extension | mode reg, result reg | Music mode request for modes 0 to 3. An unavailable mode stores 0xFFFD. | Implemented |
| 0x92 | Mugen extension | none | Runs `NAMCHG.EXE` to change the protagonist's name. Replaced by a ScummVM name dialog. | Partial |
| 0x93 | Mugen extension | reg | Stores the sign-extended input-state byte (DS:0x4119) that WAITKEY polls (0x149e8). | Not implemented |
| 0x94 | Mugen extension | none | Dispatches straight back to the interpreter loop (0x113b6). | Implemented |
| 0x95 | Mugen extension | eight words | Patterned blit from page 1 to page 0 in stages, using a captured pattern slot and a per-stage delay. | Implemented |
| 0x96 | Mugen extension | slot, byte columns | Captures eight rows of page 1 intensity bits as a pattern. | Implemented |
| 0x97 | Mugen extension | none | Runs `ENDING.EXE` with the command line `くりあ` ("clear") through the exec helper at 0x1175e (0x15076). Reimplemented; see Staff Roll. | Implemented |

#### Opcode coverage in Mugen's scripts

The dispatch table at 0x113b8 sends these IDs to the invalid-opcode exit at
0x13d09: 0x01, 0x17, 0x23, 0x2b, 0x2c, 0x30, 0x31, 0x37, 0x44-0x49, 0x5c, 0x5d,
0x61, 0x65-0x67, 0x6c, 0x6d, 0x6f-0x71, 0x7c-0x7e, 0x84-0x8c, and 0x8e. The
portrait commands 0x86-0x8b are therefore absent. Opcodes 0x29 and 0x2a share
0x4a's register-file save handler (0x13fc1) instead of the reference's XY
buffer commands.

Operand counts for every valid handler come from the native handlers. A
control-flow trace with these counts decoded all 613 scenes (archives plus the
three loose `.S` overrides) from 0x100, and `Mug_Sub.s` from every bank-1 call
target. It followed jumps, calls, menu targets, frame jumps, and JTBL tables
(zero entries are unused slots) without reaching an invalid word. A linear
sweep of the unreached bytes left two short code-like runs (`1_61.s:076a`,
`8_32.s:0748`), both using implemented opcodes only.

The scripts reach 72 opcodes. Three were missing and are now implemented:

| ID | Sites | Use |
| --- | --- | --- |
| 0x3c | 12 in `c_01.s`-`c_08.s` | `TXCLR2 0, 0, 80, 25, 0` right after 0x97: fills the whole 80x25 text VRAM with spaces, attribute 0xe1. |
| 0x90 | 1, `Mug_Sub.s:56ff` | Drops the caller's return entry, then 0x1f loads a scene. |
| 0x97 | 12 in `c_01.s`-`c_08.s` | Runs the ending credits program, then returns to `open.s`. |

`ENDING.EXE` is a separate program; see Staff Roll.

0x3c (0x14867) and 0x26 (0x1485b) both call 0x12d00 with five words: x, y,
width, and height in 80x25 text cells, then a flag. A zero flag writes
character 0x20 with attribute 0xe1. A nonzero flag writes character 0x87, the
full block, with attribute 0x01 (black) for 0x3c or 0xe1 (white) for 0x26.
ScummVM applies this to the hardware-text pixels of its text overlay. Clearing
leaves graphics-colored overlay pixels, such as text-box frames, in place,
since the original keeps those in graphics VRAM. The flagged form is not used
by the game.

Valid opcodes no script reaches: 0x06, 0x09, 0x10, 0x11, 0x16, 0x1d, 0x20,
0x26, 0x28, 0x29, 0x2a, 0x2e, 0x32, 0x34, 0x35, 0x39, 0x3a, 0x3b, 0x3d, 0x4c,
0x4d, 0x4e, 0x50, 0x51, 0x56-0x58, 0x5b, 0x5f, 0x68, 0x69, 0x6b, 0x73, 0x75,
0x79, 0x7a, 0x7f, 0x81-0x83, 0x8d, and 0x93. Some are implemented anyway.

The input byte at DS:0x4119 is rebuilt by 0x11db2 from the mouse port (0x7fd9)
and the BIOS key-state map at 0:0x52a. Bit 0 is cancel: the right button, Esc,
Z, keypad /, or keypad 0. Bit 1 is accept: the left button, Return, X, or Space.
These match the menu action values 1 and 2. WAITKEY (0x149cc) waits for a
nonzero byte, keeps it as the pressed bits, and waits until one of them is
released. ScummVM also remembers presses released within one event batch, so
a quick click is not lost. All 79 traced uses write register 0, and none is
followed by a compare. Opcode 0x93 reads the same byte. Reached text
control bytes are 01h, 02h, 03h, 04h, 0Dh, and 0Fh with subcodes 00h-03h; all
are handled.

Every handler previously marked partial was then compared instruction by
instruction with its native handler. Only 0x92, which replaces `NAMCHG.EXE`
with a ScummVM dialog, remains partial. The ScummVM differences that remain are
deliberate: no floppy prompts or file-not-found exits, the skip action, and a
frame of yield when 0x42 enables a menu. A headless run from `run.s` reached the
first-run prompt, the title menu, and `0_00.s`, showing the page-turn marker in
the box frames and saving the configuration with its date. Fades, hiding the
text, and FILETM dates were checked against the disassembly only.

### Text

- Text is Shift-JIS and NUL-terminated. It can include NEC-specific JIS
  characters and custom glyphs. In this build, `MUGEN.FNT` supplies the custom
  glyphs.
- Control bytes handled by this build:

| Byte | Meaning | Handling here |
| --- | --- | --- |
| 00h | End of string | Ends the text |
| 01h, 02h | Wait for a key; 01h also clears the box | Handled |
| 03h | Set the text color for all later text | Handled |
| 04h | Insert string register | Handled |
| 05h | Insert integer register | Handled |
| 06h-08h, 0Ah-0Ch | One operand byte, which switches the 0x79 palette animation | Operand skipped |
| 0Eh | One operand byte, nothing drawn | Operand skipped |
| 09h | Advance 64 pixels | Handled |
| 0Dh | Return to the box's left text edge and advance one row | Handled |
| 0Fh 00h xx | Set menu highlight style | Handled |
| 0Fh 01h | Toggle whether the skip key may drop character delays | Handled |
| 0Fh 02h, 03h | Allow or forbid hiding the text during waits | Handled |

- Bytes from 10h up are characters. Lead bytes 81h to 9Fh and E0h to FFh start
  two-byte characters, drawn 16 pixels wide except Shift-JIS 8540h to 869Eh
  (JIS rows 29h to 2Bh), which are 8 pixels wide. Other characters are 8 pixels
  wide.
- There is no automatic wrapping. Text starts one 16-pixel tile inside the
  box's left and top edges, and rows are register 0x402 pixels apart (16 when
  zero). Script text is hard-wrapped at 64 bytes (32 full-width characters);
  closing punctuation may hang past that as a 33rd character, and lines are
  not clipped to the box.
- After each character the printer (0x12e9a) waits the 0x22 delay in vertical
  syncs, except after a full-width space. Holding cancel cuts each delay short.
  The skip key drops it unless 0Fh 01h has toggled that off; ScummVM's skip
  action stands in for that key. A click while text prints is not remembered.
- Waits 01h and 02h (0x12f50) react to held input. Accept continues after it
  is released. Cancel does nothing unless 0Fh 02h allowed hiding: then it turns
  the text plane off and shows graphics page 1 until the next press (0x12fc3).
  ScummVM's skip action also continues, unless 0Fh 01h is in effect.
- During a wait the vertical-sync handler (0x122ec) cycles a marker of up to
  eight JIS codes from 0x76, each shown for its 0x77 frame count. A zero code
  restarts the cycle. It sits in hardware text at the box's bottom-right inner
  column and last inner row, moved by the 0x78 offset in text cells;
  `run.s`/`open.s` use JIS 7648h-764Dh and (0, -1), which puts it in the bottom
  frame row. Without a box it sits at text cell (72, 22). The executable's
  default table is JIS 7721h-7728h at four frames each. The marker cell is
  blanked after the wait.
- Text boxes are drawn into graphics page 0, so ScummVM shows their frames
  and graphics-colored glyphs only while page 0 is displayed.

### Menus

- Opcode 0x40 takes two registers, one for the chosen ID and one for the action,
  then the offset of the menu data and a jump offset. Opcode 0x41 is a coordinate
  menu: it writes the pointer X and Y to two registers and the action to a third.
- Menu data is a list of 10-byte records: a 16-bit ID, then the left, top,
  right, and bottom coordinates as 16-bit values. Right and bottom are exclusive.
  Only the ID's low byte counts: a zero byte ends the list, and the selected ID
  is that byte sign-extended (0x11e59, 0x11c51).
- The action value is the input byte: 2 for a left click or the advance action,
  1 for a right click or the cancel action. Any input selects, with ID 0
  outside every region. A selection writes the ID and action to the two
  registers, waits for the input to change, and then jumps to the supplied
  offset.

### Images (Stripped Pi)

- Header: a 16-bit big-endian width, a 16-bit big-endian height, and 48 bytes of
  palette (16 entries of R, G, B, one byte each). Compressed data starts at
  byte 52.
- The stored form is a Pi-style stream with its header stripped. Compare the
  research for the full Pi layout.
- This build accepts even widths and heights up to 4096. The decoder reads an
  MSB-first bit stream, codes pixels as ranks in a per-color move-to-front
  table, and supports all five repeat locations.

### Font Bank

- `MUGEN.FNT` is 6016 bytes. This is consistent with 188 glyphs of 32 bytes
  each, the 16x16, 1-bit glyph size described by the reference. This is an
  inference and has not been checked against glyph use.
- The research notes that the four-nine font stores its quadrants in upper-left,
  upper-right, lower-left, lower-right order. The PC-98 ROM uses upper-left,
  lower-left, upper-right, lower-right order.

### Music and Effects

- A `g*.m` M.M.D. file starts with a tempo byte (beats per minute) and a signed
  global transpose byte. Then come 18 four-byte track entries starting at
  offset 2: a 16-bit track data offset, a transpose byte, and a channel byte.
  In the transpose byte, bit 7 disables transposition for that track. In the
  channel byte, the low nibble is the MIDI channel and 0xFF mutes the track.
- Valid files are 0x4C to 0x4000 bytes. Conversion produces a format 1 SMF with
  48 ticks per quarter note, one tempo track, and 18 note tracks.
- `SE.MSE` begins with a 16-bit effect count. Each effect is a 16-bit length
  followed by M.M.D. data.

### P.M.D. Data and Driver Behavior

These findings come from capstone disassembly of the supplied `PMD.COM` (P.M.D.
4.8b for OPN/OPNA, 23060 bytes) and `PMDB2.COM` (P.M.D. 4.8b for OPNA with
rhythm/ADPCM, 28300 bytes), the unpacked `SYS98.EXE`, and the game's data.
REA could not open these 16-bit DOS images with its configured providers.
Addresses are `.COM` load addresses (file offset + 0x100) in `PMD.COM` unless
noted.

- Driver selection (`SYS98.EXE` 0x159c): mode 1 loads `PMD.COM /M8 /V1 /DF20
  /K`, mode 2 `PMDB2.COM /M8 /V1 /DF10 /DS0 /DR16 /K`, mode 3 `MMD.COM /M16 /K`;
  `ESET.COM SE.EFC` loads the FM effects. Register 0x400 holds the mode and
  0x401 the previous one. The shipped 16-byte register file stores 0xFFFF in
  register 0x400, which makes `open.s` show its first-run music prompt.
- Script sound opcodes call INT 60h: 0x1A loads and starts (functions 1, 6,
  0), 0x1B fades (function 2, AL = low byte of register 0x403), 0x1C stops
  (1), 0x4D reads the measure counter (5), 0x50 the status word (0x0A), 0x4E
  starts an SSG effect (3), and 0x4F an FM effect (0x0C).
- A `.M` file has a format byte, then 16-bit offsets relative to file offset 1
  for FM1 to FM6, SSG1 to SSG3, ADPCM, the K rhythm part, the R pattern table,
  and, when the first offset is 0x1A, a table of 26-byte voices. `n*.m` uses
  FM1 to FM3 and SSG; `b*.m` uses all six FM channels. `PMD.COM` maps header
  slots 4 to 6 to FM3 extension parts, which `n*.m` leaves empty. Only `b13.m`
  and `n13.m` have rhythm parts; no song uses ADPCM.
- Command dispatch tables for FM, SSG and rhythm parts are at 0x0ab8, 0x0b50
  and 0x0be8, covering commands 0xFF down to 0xB4. Loop counters live in the
  song data. Notes are `octave << 4 | key`, 0x0F is a rest, and 0x0C repeats
  the previous note. The frequency tables are at 0x2d25 (FM) and 0x2d3d (SSG).
- Timer A, set to 0 (its longest period), runs at 0x2c1d: every eighth tick
  steps the fade, then SSG and FM effects run. Timer B runs music at 0x2bd6:
  SSG parts, the K part, then FM parts, followed by the measure counter
  (default 96 ticks per measure).
- FM volume (0x1a41) adds `127 - volume` to carrier TLs after scaling the
  volume by the `/DF` setting and half the fade level. SSG volume (0x1b79)
  scales by the fade level and adds a software envelope (0x20e2). New parts start
  at FM volume 108, SSG volume 8, with SSG output gated until the first note.
- FM effects (0x2a17, 0x0342) use `SE.EFC` entries 0 to 126 as offsets from
  the file start, take voices from the table at entry 127, play on FM channel
  3, and mask the music's FM3 part until its next note after the effect. The
  game's effect 0 is empty.
- SSG effects (0x0173, 0x021d) use a table of 3-byte entries (priority,
  offset) found at 0x2d65 in `PMD.COM` and 0x36fd in `PMDB2.COM`. Each frame
  holds a duration, tone, noise, mixer bits, channel C volume, envelope, and
  tone/noise sweeps. Entries 0 to 10 are drums with priority 1, so music notes
  on SSG3 interrupt them. The engine finds the table through its lookup code.
- K/R rhythm (0x09b5): R patterns hold 14-bit rhythm masks. `PMD.COM` plays
  the lowest set bit as an SSG drum. `PMDB2.COM` (0x1147) also keys the OPNA
  rhythm source for every set bit using an 11-entry instrument table, unless
  `/N` is given, and skips SSG drums while fading.

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
Shift-JIS text. Loose filenames are tried as provided, as Shift-JIS transcoded
to UTF-8, and as Windows-1252 bytes transcoded to UTF-8, for extracted disks
whose legacy filenames were converted that way. A string file saved by the game
takes precedence over the shipped one. The native handler at 0x14d3a reads the string ID and scene-relative
filename, then copies rows until EOF; row handling at 0x14d79 recognizes CRLF
and 0x1a. Evidence: `ev_c3c164131ce4308b774e5e9fae294cb934f5aafb93c7fd1dcea77b2446ac0780`,
`ev_bccf6b98119000c8202a009c3b0dae1616cf3909be1d7afe979c2c148e2a97fcec`.

Three loose files on disk A have Shift-JIS names: `名前` (`96 bc 91 4f`,
9 bytes, the two-line name file), `設` (`90 dd`, the 16-byte configuration
registers), and `秘` (`94 e9`, 1024 bytes). The D88 directory entries hold
these bytes. One extraction decoded them with an MSX-style 8-bit character set,
producing `かシあO`, `πン`, and `えの`. These names are not supported; rename
the files to the UTF-8 names above.

Opcode 0x92 (dispatch 0x15017) takes no operands. It calls the DOS exec
helper at 0x1175e with `NAMCHG.EXE` and an empty command line, then continues.
`open.s` reaches it from item 3 of the title submenu at 0x0877 (JTBL at
0x08b5) and jumps back to the menu at 0x0851. `NAMCHG.EXE` is packed. Its
visible strings include `名前`, 変更 (change), 入力 (input), 決定 (confirm),
and 取消 (cancel). The name file's second line is the name: `open.s:2d63` loads
it into string 0x1e, and the JTBL at 0x2d6f centres it for lengths up to 8
bytes. The shipped file holds an empty first line and `慎一`. ScummVM shows a
dialog for up to eight Shift-JIS bytes, keeps the first line, and writes the
file with CRLF line ends and a 0x1a terminator to a save file.
`NAMCHG.EXE`'s own screen and any first-line edits have not been reproduced.

Opcode 0x62 (dispatch 0x14484) reads a word offset `w` and a line `l` from two
registers and sends graphics GDC command 0x70 (SCROLL) with two display areas.
Area 1 starts at word `40 * l + w + 0x4000` for `400 - l` lines, and area 2 starts
at word 0x4000 for `l` lines. The image-mode bit comes from port 0x31. Display
rows read 40 consecutive words from each area's start, so a word offset shifts
the picture left by 16 pixels per word, carrying into the next row. ScummVM
models each plane as 0x4000 words. It reads the unused words past 16000 as
color 0 and scrolls only graphics pixels: the page and graphics-colored overlay
pixels. Hardware text and the menu highlight stay in place.
`Mug_Sub.s:5885` uses it as a screen shake: it alternates offsets 0 and 1 word,
two frames each, five times, then restores (0, 0). No other scene calls it.

PRINT (0x1497e) selects the 24-byte box record and calls the printer at
0x12d50. Box records store the text cursor one tile inside the frame: column
`(x + 1) * 2` bytes and row `y + 1`. The glyph path (0x12dd2) only advances the
column by the glyph width; it has no right-edge or bottom-edge check. The
control-byte table at 0x12d90 handles 0Dh (0x13054) by resetting the column
and incrementing the row, and 09h (0x13030) by adding 8 bytes. In the main
dialogue box (`TBOPEN 3, 3, 19, 34, 6`), the 64-byte text area matches the
scripts' hard wrap, so hanging punctuation reaches the frame edge rather than
starting a new line.

Text color is one engine-wide value, not a per-box one. Control byte 03h
(0x12ff0) stores its operand at DS 0xc05:0x172, the only writer. The value
starts at 15, and the glyph routine at 0x1101a loads it into the GRCG tile
registers. Only `open.s` sets colors (2, 14, and 15). Gameplay command menus
such as `0_00.s:0176` therefore draw their labels in color 15. Their highlight
(style 5 in `WIN.G`) fills the row with color 7, the same brown that a per-box
default of 7 gave the labels, which hid the selected entry.

The menu hover handler (0x11e83) restores the previous row (0x11f27), saves the
new one (0x11ede), and draws the style tiles at (left - 16, top - 16), 48
pixels tall. The flag at 0x3ede selects the masked blitter at 0x12777, which
keeps destination pixels wherever the glyph plane (0x4184) or a transparent tile
pixel is set. The selected label therefore stays on top of the highlight.
A headless ScummVM capture of `0_00.s:0176` showed color-15 label pixels inside
the color-7 highlight after this change.

Opcode 0x14 (`PALCSET`, 0x14652) reads four registers: index, red, green, and
blue. Each is masked to 4 bits. It writes the hardware palette through ports
0xa8 (index), 0xac (red), 0xaa (green), and 0xae (blue). It stores the color in
both the loaded-image palette (0x3f6a) and the displayed palette (0x3f9a).
Opcode 0x06 copies the first to the second, so a later fade or PALAPL keeps the
change. `0_00.s:03e0` uses it to set color 15 to white.

IMGLOAD (0x143ae) passes its x/8 and y to the decoder (0x13139, 0x13184),
which positions the image only from these operands. The stripped Pi header has
no offset. The plane operand selects one of four writers at 0x13586. Planes 0
and 1 select a VRAM page through port 0xa6 and use the opaque or
color-0-transparent writer. Planes 2 and above write the off-screen software
page in the segments at 0x408a and 0x408c, using an opaque (0x1394c) or
transparent (0x13b12) writer. The handler then writes 0 to port 0xa6.

Gameplay scenes compose this way. `Mug_Sub.s` loads a 480x304 room (`011.G`)
into page 2 and a transparent character (`43A.G`) into page 1 at (0, 0).
`Mug_Sub.s:4c44` then uses opcode 0x95 to copy that area from page 1 to (112, 8)
on page 0, and `4c5e` restores page 1 from page 2. Treating plane 2 as "keep
the current page" drew rooms at the screen's top-left and left characters on
black. A headless capture of `1_01.s` shows the room inside the viewport with
the character over it.

Opcode 0x6e (`STRLEN`) writes the bounded byte length of a string into a
register. Opcode 0x53 (`STRCAT`) appends one string to another, keeping the
destination NUL-terminated within its 80-byte slot. These support the first
scene's script-driven menu setup.

Opcode 0x91 calls 0x1159c with a mode register and result register. The original
launches DOS sound drivers only when needed. This implementation supports
silent mode 0, FM modes 1 and 2, and MIDI mode 3.
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

## Version Differences and Research Limits

### Version Differences

The reference documents System-98 v3.10 as used in Canaan. The REA findings
below come from the Mugen executable and describe that executable only, not
System-98 in general.

| Area | Reference (Canaan, v3.10) | Mugen Yasoukyoku executable (REA, static) |
| --- | --- | --- |
| Opcode range | 0x00 to 0x8E | Dispatch through 0x97; 0x95 and 0x96 lie outside the reference table |
| Second scene bank | Not described | Opcode 0x8F loads a second 0x4000-byte bank |
| Register width | 32-bit for IDs 0x400 and above | MOVI reads one 16-bit immediate for any ID |
| Opcodes 0x07, 0x08 | Fades to or from black or white | 0x07 fades toward the loaded image palette; 0x08 fades the current palette to black or white |
| Opcode 0x36 | Described as a graphics clear, unclear | Imports eight 48x48 nine-tile window styles on the Options path; full arguments not established |

- The reference lists v1.00 and v1.10 with narrower opcode ranges (0x00 to 0x88
  and 0x00 to 0x8C). Nothing here implements or tests those revisions.
- DIANA, the related engine, reuses many command IDs but is not binary
  compatible. Nothing here implements or tests DIANA data.
- The reference says v3.10 draws Shift-JIS 8541h to 869Eh at half width. The
  Mugen glyph fetch at 0x1105e agrees: JIS codes 2921h to 2B7Eh return a
  one-byte advance.

### Research Limits

- REA findings come from static analysis of the packed executable (SHA-256
  `339babdce4eb0b04d81ae73edd1d5ca3d16c0eefd47d85c00a727ee56b3a49e6`) and its
  unpacked copy (`7038db40f09955e1080952dee24f79ce52792d7626d95fc95d966cfb1be08916`).
  They do not show runtime behavior on PC-98 hardware or in an emulator.
- Evidence IDs and hashes in this README are preserved static-analysis
  findings. During this documentation pass, the live REA evidence-bundle index
  returned zero records for both SHA-256 values. The IDs are therefore not
  currently retrievable bundles, and this README does not claim that current
  REA records exist.
- Opcode semantics not described here are unverified. The full argument meaning
  of opcode 0x36 is not established. The window-style reading covers only the
  Options path.
- Support is limited to the verified startup path described above. Full
  gameplay and later dialogue and scene paths are unsupported. Later scenes
  may reach unsupported instructions.
- The P.M.D. sequencer implements the commands the game's songs and effects
  use, plus related volume, loop, tempo, and effect commands. Portamento is
  approximated by its destination note; FM3 extension parts, PPSDRV, ADPCM,
  hardware LFO, the second software LFO, and extended SSG envelopes are not
  implemented, and the game data does not use them. FM playback has not been
  compared with a PC-98 emulator recording.
- Noto fallback glyphs are approximate. A PC-98 font ROM is preferred for
  original glyph shapes.

## References

- [System-98 research by ValleyBell](https://github.com/ValleyBell/PC98VNResearch/tree/master/four-nine_system98), the Four-Nine System-98 research summarized in Architecture, Data and Script Formats, and Version Differences. The summaries in this README were written locally.
- [Research game list](https://github.com/ValleyBell/PC98VNResearch/blob/master/four-nine_system98/game-list.md)
- [System-98 scene format](https://github.com/ValleyBell/PC98VNResearch/blob/master/four-nine_system98/SceneFormat.txt)
- [System-98 image format](https://github.com/ValleyBell/PC98VNResearch/blob/master/four-nine_system98/ImageFormat.txt)
- [PC98VNResearch repository](https://github.com/ValleyBell/PC98VNResearch)
- [Pi graphics format](https://mooncore.eu/bunny/txt/pi-pic.htm)
- [Game](https://vndb.org/v725)
- [Target PC-98 release](https://vndb.org/r18109)
