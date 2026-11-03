# Yomira Compatibility

This page lists the games supported by engines that Yomira adds on top of ScummVM: visual novels, eroge and similar titles. It is derived from each engine's detection table and README under [engines/](engines/). It does not cover the many games inherited from ScummVM; for those, see the [upstream supported games list](https://wiki.scummvm.org/index.php?title=Category:Supported_Games) and the [ScummVM compatibility list](https://www.scummvm.org/compatibility/).

Yomira does not include game data. You need your own copy of each game; detection matches files by size and MD5 checksum, so other releases, translations, and re-extractions may not be recognized.

## Status Labels

| Label | Meaning |
| --- | --- |
| Completable | User testing confirmed that the game can be played to the end. |
| Experimental | Core playback is implemented, but end-to-end completion and full feature coverage are not established by the engine documentation. |
| Partial | Only part of the game runs. Missing features are listed in the notes. |
| Startup only | The game boots to its title menu. Story, gameplay, or most menu options are not implemented. |

Only games labeled Completable have been played to the end, and only by user testing. "Detected" means the files match a detection entry, not that the game is playable.

## Supported Games

All detected releases are Japanese. The detection flag column shows what Yomira reports for the entry: `unstable` is `ADGF_UNSTABLE`, `warning` is `ADGF_WARNING`, and `none` means no flag is set.

| Game | Game ID | Engine | Platform | Detection flag | Status | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| Collection ~Shuushuu Sareta Akumu~ | `collection` | [XEX](engines/xex/README.md) | Windows | none | Experimental | 640x480. |
| Case 266 ~266 Gou Jiken~ | `case266` | [XEX](engines/xex/README.md) | Windows | none | Experimental | 640x480. |
| Boku no Himitsu Taiken | `bokunohimitsutaiken` | [XEX](engines/xex/README.md) | Windows | none | Experimental | 640x480. |
| The Guts! | `theguts` | [XEX](engines/xex/README.md) | Windows | none | Completable | 640x480. Voiced release. Includes the timed clicking minigame. |
| Koi Suru Science | `koisuruscience` | [XEX](engines/xex/README.md) | Windows | none | Experimental | 640x480. Digital music only (no MIDI option). |
| Motto! Himitsu Taiken | `mottohimitsutaiken` | [XEX](engines/xex/README.md) | Windows | none | Experimental | 640x480. |
| Bosei ~Mama Club~ | `boseimamaclub` | [XEX](engines/xex/README.md) | Windows | none | Experimental | 640x480. |
| Matty | `matty` | [XEX](engines/xex/README.md) | Windows | none | Experimental | 640x480. Original and `Updated` variants are detected. |
| Yagai Gakushuu | `yagai` | [XEX](engines/xex/README.md) | Windows | none | Experimental | 800x600 (C4 family). |
| Mama! Tsuma? | `mamatuma` | [XEX](engines/xex/README.md) | Windows | none | Experimental | 800x600 (C4 family). Original and `Updated` variants are detected. |
| Mousugu Natsuyasumi! | `mousugunatsuyasumi` | [XEX](engines/xex/README.md) | Windows | none | Experimental | 800x600 (C4 family). |
| The Guts! -Maximum Maternity!- | `thegutsmaximummaternity` | [XEX](engines/xex/README.md) | Windows | none | Experimental | 640x480. |
| One☆Lover!! ~Miku-chan no Koibito~ | `onelover` | [Delta](engines/delta/README.md) | Windows | unstable | Partial | Download Edition. Runs through the logos and warning screens to the title menu and has been exercised through opening scenes. Music, sound effects, message voices, the Settings screen, screen fades, and skipping read text work. Later story and choices remain incomplete. The Extras gallery, scene replay, and music room are not implemented. |
| Onegai Tasukete! ~Soto ni Detara Yararechau~ | `onegaitasukete` | [Delta](engines/delta/README.md) | Windows | unstable | Startup only | Package Edition. Reaches the title menu. The engine README records story execution only for One☆Lover!!, so treat this title as title-menu support. |
| Sexy Beach 2 | `sexybeach2` | [Illusion](engines/illusion/README.md) | Windows | unstable | Partial | Title menu with music and sound. The opening movie is skipped. New game runs character selection and swimsuit selection with a 3D preview, then stops with an error because accessory selection is not implemented. The second to fourth menu buttons stop with a "not implemented" error; the fifth ends startup. No gameplay, save or load. Renders with OpenGL or TinyGL ("Game 3D Renderer" option). |
| Mugen Yasoukyoku (Nocturnal Illusion) | `mugenyasoukyoku` | [System-98](engines/system98/README.md) | PC-98 | warning | Partial | Four-floppy release, System-98 3.10. Main menu, Options, new-game transition to the first background, Pi graphics, basic Japanese text, and MIDI music and effects. No full gameplay, no game save/load, no PMD/FM music or FM effects. Launching shows a warning. |

## Engine Notes

| Engine | Build default | Build requirements (`configure.engine`) | Details |
| --- | --- | --- | --- |
| XEX (Complet's XEX and C4 engines) | Yes | `16bit`, `highres`, `freetype2`, `mad`, `midi` component | [engines/xex/README.md](engines/xex/README.md) |
| Delta (Route Two Ltd.) | Yes | `16bit`, `highres`, `freetype2`, `vorbis` | [engines/delta/README.md](engines/delta/README.md) |
| Illusion | Yes | `highres`, `3d` (TinyGL or OpenGL), `tinygl` component | [engines/illusion/README.md](engines/illusion/README.md) |
| System-98 (Four-Nine) | Yes | `highres`, `freetype2`, `midi` component | [engines/system98/README.md](engines/system98/README.md) |

- XEX, Delta, and System-98 need a Japanese font (a Noto CJK font in the game directory or extra path, or an original font where the engine README lists one) and ScummVM's `encoding.dat`. Check the engine README for the exact filenames.
- XEX was previously registered as `complet`. Existing launcher entries need their engine ID changed to `xex`, or the games must be added again.
- XEX digital voices and music are MP3, so the engine is not built without MP3 support (`mad`). Delta music and voices are Ogg Vorbis.

## Reporting Compatibility

If a game in this table behaves differently from its notes, or a different release of one of these games is not detected, open an issue in this repository. Include the Yomira version, game language and release, platform, and a log. Detection tables and per-engine documentation are the source of truth; update this page when they change.
