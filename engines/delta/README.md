# Delta

Route Two Ltd.'s Windows adventure runtime. Detection covers the Japanese
download edition of One Lover!! ~Miku-chan no Koibito~
([VNDB v5757](https://vndb.org/v5757)) and the Japanese Windows package edition of
Onegai Tasukete! ~Soto ni Detara Yararechau~
([VNDB v5324](https://vndb.org/v5324)).

## Current Status

Both detected games execute their startup scenarios through the logos and
warning screens to the title menu, using their original artwork at 800x600.
Menu button hover states are drawn from the original sprite sheets. Execution
pauses at the menu command; menu selections, gameplay, text, audio playback,
and saves are not implemented. Audio records and nonvisual setup commands on
this startup path are parsed without reproducing all their original effects.
Cross-fades are an interim approximation, not a faithful implementation of the
original transition selectors and timing. Windows installation paths and INI
preferences are not imported or modified. No game files are written.

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
| `0x23`, `0x26`, `0xb0` | Load startup background images |
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
`WARNING.IAF`, `TITLE.IAF`, and `MENU.IAF` are read this way. The separate EV/ST
bank format is not implemented.

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
