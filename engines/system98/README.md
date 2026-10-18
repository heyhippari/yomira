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
- `audio.cpp`, `audio.h`: MIDI player for music and effects, with fades,
  pause, and volume.
- `mmd.cpp`, `mmd.h`: M.M.D.-to-SMF conversion and effect extraction.
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
- Registers are 16-bit in this build. The reference describes a 32-bit form for
  IDs of 0x400 and above, which this build does not implement.

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
| 0x02 | IMGLOAD | x/8, y, palette flag, plane, file name, transparent flag | Loads an image. Planes 0 and 1 select the draw page; other values keep it. | Implemented |
| 0x03 | TBOPEN | box ID, x/16, y/16, w/16, h/16, style | Opens one of 16 text boxes. IDs 0x10 and above fail. | Implemented |
| 0x04 | TBCLOSE | box ID | Closes a text box and restores the overlay it covered. | Implemented |
| 0x05 | TBCLEAR | box ID | Clears a text box's contents. | Implemented |
| 0x06 | PALAPL | none | Applies the target palette and redraws. | Implemented |
| 0x07 | PALBW | direction, frames per step | Fades from black (0) or white (nonzero) to the loaded image palette. The reference describes a black/white fade instead. | Partial |
| 0x08 | PALFADE | direction, frames per step | Fades the current palette to black (0) or white (nonzero). | Implemented |
| 0x09 | PALLCKT | colour ID (0-15) | Toggles a palette lock bit. Only 0x07 and 0x08 fades honor locks. | Partial |
| 0x0A | JP | offset | Jumps to an offset. | Implemented |
| 0x0B | CMPR | reg, reg | Compares two registers. Stores 0 for equal, 1 for less, 2 for greater. | Implemented |
| 0x0C | CMPI | reg, value | Compares a register with a value, using the same result codes. | Implemented |
| 0x0D-0x12 | JEQ, JLT, JGT, JGE, JLE, JNE | offset | Conditional jumps on the stored compare result. | Implemented |
| 0x13 | JTBL | reg, table | Jumps to the table entry the register selects. The table follows the operand. | Implemented |
| 0x14 | PALCSET | four regs | Sets one palette colour from registers. | Not implemented |
| 0x15 | PRINT | box ID, text offset | Prints scene text in a text box. | Implemented |
| 0x16 | PRINTXY | x, y, text offset | Prints text at a screen position. | Not implemented |
| 0x17 | - | - | Undefined in v3.10. | Undefined (v3.10) |
| 0x18 | MOVI | reg, value | Sets a register to a 16-bit value. The 32-bit form for IDs 0x400 and above is absent. | Partial |
| 0x19 | MOVR | dst reg, src reg | Copies one register to another. 16-bit registers only. | Partial |
| 0x1A | BGMPLAY | file name | In MIDI mode 3, plays the music file. b and n names use the matching g file. Silent in mode 0. | Partial |
| 0x1B | BGMFADE | none | Fades out the music. Fade length comes from register 0x403 here. | Partial |
| 0x1C | BGMSTOP | none | Stops the music. | Implemented |
| 0x1D | BGMMODE | reg | Stores the current music mode. | Implemented |
| 0x1E | WAITKEY | reg | Waits for a key or click. Stores 1 instead of the key code. | Partial |
| 0x1F | LDSCENE | file name | Loads a scene into bank 0 and restarts at 0x100. | Implemented |
| 0x20 | GV02 | none | Writes a fixed pattern to graphics memory. | Not implemented |
| 0x21 | WAIT | reg | Waits the number of frames in the register. | Implemented |
| 0x22 | CHRDLY | reg | Sets the delay between printed characters. | Implemented |
| 0x23 | - | - | Undefined in v3.10. | Undefined (v3.10) |
| 0x24 | ADDI | reg, value | Adds a value. 16-bit registers only. | Partial |
| 0x25 | SUBI | reg, value | Subtracts a value. 16-bit registers only. | Partial |
| 0x26 | TXCLR1 | five words | Clears a text buffer area. | Not implemented |
| 0x27 | ADDR | dst reg, src reg | Adds one register to another. 16-bit registers only. | Partial |
| 0x28 | SUBR | dst reg, src reg | Subtracts one register from another. 16-bit registers only. | Partial |
| 0x29 | XYREAD | dst reg, x reg, y reg | Reads a value from the XY buffer. | Not implemented |
| 0x2A | XYWRT | src reg, x reg, y reg | Writes a value to the XY buffer. | Not implemented |
| 0x2B-0x2C | - | - | Undefined in v3.10 (v1.xx register file save and load). | Undefined (v3.10) |
| 0x2D | REGCLR | first reg, last reg | Zeroes an inclusive register range. | Implemented |
| 0x2E | - | - | Undefined in v3.10. | Undefined (v3.10) |
| 0x2F | BLIT1I | eight words | Opaque rectangle copy between pages. | Implemented |
| 0x30-0x31 | - | - | Undefined in v3.10. | Undefined (v3.10) |
| 0x32 | PRINTR | box ID, reg | Prints a register as text. | Not implemented |
| 0x33 | GPDGET | reg | Sets the display page from the register's low bit. No port I/O. | Partial |
| 0x34 | GPDSET | reg | Stores the current display page. | Partial |
| 0x35 | TXFILL | six words | Fills a text buffer area. | Not implemented |
| 0x36 | GFX36 | none | Copies page 1 rows into the window-style set used by Options. Reference meaning is unclear. | Partial |
| 0x37 | XYFLOAD | file name | Loads the XY buffer from a file. | Not implemented |
| 0x38 | ANDR | dst reg, src reg | Bitwise AND of two registers. | Implemented |
| 0x39 | ORR | dst reg, src reg | Bitwise OR of two registers. | Implemented |
| 0x3A | BLIT1R | eight reg IDs | Register-indirect form of 0x2F. | Implemented |
| 0x3B | BLIT1RL | eight reg IDs | Long-width variant of 0x3A. | Not implemented |
| 0x3C | TXCLR2 | five words | Clears a text buffer area. | Not implemented |
| 0x3D | MULR | dst reg, src reg | Multiplies two registers. 16-bit registers only. | Partial |
| 0x3E | DIVR | dst reg, src reg | Divides two registers. 16-bit registers only. A zero divisor fails the run. | Partial |
| 0x3F | CMD3F | x reg, y reg | Warps the pointer to two register coordinates. Reference purpose unknown. | Partial |
| 0x40 | MENUSEL | id reg, action reg, menu offset, jump offset | Runs a region menu. Writes the selected ID and action, then jumps. | Implemented |
| 0x41 | CMD41 | x reg, y reg, action reg, jump offset | Coordinate menu. Stores pointer position and action, then jumps. Reference purpose unknown. | Partial |
| 0x42 | CMD42 | flag | Enables or disables the menu. Enabling waits one frame. Reference purpose unknown. | Partial |
| 0x43 | CMD43 | x reg, y reg | Stores the pointer position in two registers. Reference purpose unknown. | Partial |
| 0x44-0x49 | - | - | Undefined in v3.10 (v1.xx dummy and GFX commands). | Undefined (v3.10) |
| 0x4A | REGFSAV | buffer, file name | Saves a register buffer to a ScummVM save file. Buffer ranges differ from the reference. | Partial |
| 0x4B | REGFLD | buffer, file name | Loads a register buffer from a save file, or from a scene resource if none exists. Buffer ranges differ. | Partial |
| 0x4C | CMD4C | two words | Stores two values. Reference purpose unknown. | Not implemented |
| 0x4D | BGMMEAS | reg | Stores the current music measure. | Implemented |
| 0x4E | SFXSSG | effect number | In MIDI mode 3, plays an effect from SE.MSE by number. No separate SSG playback. | Partial |
| 0x4F | SFXFM | effect number | Same handler as 0x4E here. No separate FM playback. | Partial |
| 0x50 | BGMSTAT | reg | Stores 1 if music is playing, otherwise 0. | Implemented |
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
| 0x60 | FILETM | reg, string, file name | Stores 1 if a save file exists, otherwise 0. Clears the string. No date. | Partial |
| 0x61 | XYFSAVE | file name | Saves the XY buffer to a file. | Not implemented |
| 0x62 | CMD62 | two regs | Scroll-register command. Reference marks details as TODO. | Not implemented |
| 0x63 | BLIT2I | eight words | Like 0x2F, but zero source pixels keep the destination. | Implemented |
| 0x64 | BLIT2R | eight reg IDs | Register-indirect form of 0x63. | Implemented |
| 0x65-0x66 | - | - | Undefined in v3.10. | Undefined (v3.10) |
| 0x67 | GFX67 | four words | GFX command. | Not implemented |
| 0x68 | GFX68 | two regs | GFX command. | Not implemented |
| 0x69 | GFX69 | two regs, dst reg | Bit-mask GFX command. | Not implemented |
| 0x6A | CMD6A | four values | Sets inclusive pointer bounds. Reference purpose unknown. | Partial |
| 0x6B | CMD6B | four regs | GFX command. | Not implemented |
| 0x6C-0x6D | - | - | Undefined in v3.10. | Undefined (v3.10) |
| 0x6E | STRLEN | reg, string | Stores the string length, excluding the terminator. | Implemented |
| 0x6F-0x71 | - | - | Undefined in v3.10 (v1.xx register width conversions). | Undefined (v3.10) |
| 0x72 | FRMSTI | value | Sets the script frame counter. | Implemented |
| 0x73 | FRMGTR | reg | Stores the script frame counter. | Implemented |
| 0x74 | FRMJPI | value, offset | Jumps when the frame counter is below the value. Also waits one frame when taken. | Implemented |
| 0x75 | FONTCHR | JIS code, file offset | Loads a custom font glyph. | Not implemented |
| 0x76 | IDLECHR | eight words | Stores idle character IDs. No idle animation uses them. | Partial |
| 0x77 | IDLEDLY | eight words | Stores idle delays. No idle animation uses them. | Partial |
| 0x78 | IDLEXY | x, y | Consumes two operands with no effect. | Partial |
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
| 0x90 | Mugen extension | unknown | Dispatched by the executable. Semantics unknown. | Unknown (Mugen) |
| 0x91 | Mugen extension | mode reg, result reg | Music mode request. Modes 0 and 3 succeed; other modes store 0xFFFD. | Partial |
| 0x92-0x93 | Mugen extension | unknown | Dispatched by the executable. Semantics unknown. | Unknown (Mugen) |
| 0x94 | Mugen extension | none consumed | No-op here. The executable's operand count is unknown. | Partial (no-op) |
| 0x95 | Mugen extension | eight words | Patterned blit from page 1 to page 0 in stages, using a captured pattern slot and a per-stage delay. | Implemented |
| 0x96 | Mugen extension | slot, byte columns | Captures eight rows of page 1 intensity bits as a pattern. Resets the draw page to 0. | Implemented |
| 0x97 | Mugen extension | unknown | Dispatch recorded by REA. No behavior in this build. | Unknown (Mugen) |

### Text

- Text is Shift-JIS and NUL-terminated. It can include NEC-specific JIS
  characters and custom glyphs. In this build, `MUGEN.FNT` supplies the custom
  glyphs.
- Control bytes handled by this build:

| Byte | Meaning | Handling here |
| --- | --- | --- |
| 00h | End of string | Ends the text |
| 01h, 02h | Wait for a key; 01h also clears the box | Handled |
| 03h | Set text color | Handled |
| 04h | Insert string register | Handled |
| 05h | Insert integer register | Handled |
| 0Dh | Line break | Handled |
| 0Fh 00h xx | Set menu highlight style | Handled |

- Other control bytes (06h, 07h, 08h, 09h, 0Ah, 0Bh, 0Ch, 0Eh) are ignored.
  Their operand bytes are not consumed.
- Lead bytes 81h to 9Fh and E0h to FFh are drawn 16 pixels wide. Other
  characters are drawn 8 pixels wide.

### Menus

- Opcode 0x40 takes two registers, one for the chosen ID and one for the action,
  then the offset of the menu data and a jump offset. Opcode 0x41 is a coordinate
  menu: it writes the pointer X and Y to two registers and the action to a third.
- Menu data is a list of 10-byte records: a 16-bit ID, then the left, top,
  right, and bottom coordinates as 16-bit values. Right and bottom are exclusive.
  An ID of 0 ends the list.
- The action value is 2 for a left click or the advance action, and 1 for a
  right click or the cancel action. A selection writes the ID and action to
  the two registers and then jumps to the supplied offset.

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
- The reference says v3.10 draws Shift-JIS 8541h to 869Eh at half width. This
  build draws all two-byte characters at full width, so that range is not
  verified for this executable.

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
  gameplay, later dialogue and scene paths, and PMD and FM playback are
  unsupported. Later scenes may reach unsupported instructions.
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
