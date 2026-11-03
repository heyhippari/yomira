For a more comprehensive changelog of the latest experimental code, see:
        https://github.com/heyhippari/yomira/commits/

For changes made before Yomira was forked from ScummVM, see:
        https://github.com/scummvm/scummvm/blob/master/NEWS.md

#### 2026.4.1 (2026-XX-XX)

 New games:
   - Added support for Collection ~Shuushuu Sareta Akumu~.
   - Added support for Case 266 ~266 Gou Jiken~.
   - Added support for Boku no Himitsu Taiken.
   - Added support for The Guts!
   - Added support for The Guts! -Maximum Maternity!-.
   - Added support for Koi Suru Science.
   - Added support for Motto! Himitsu Taiken.
   - Added support for Bosei ~Mama Club~.
   - Added support for Matty.
   - Added support for Yagai Gakushuu.
   - Added support for Mama! Tsuma?
   - Added support for Mousugu Natsuyasumi!

 General:
   - Forked from ScummVM 2026.4.0 and rebranded as Yomira, with a focus on
     visual novels, eroge and similar games.
   - Removed the engines inherited from ScummVM. Use ScummVM for those games.
   - Added automated development and stable release builds for Linux,
     Windows and macOS. The macOS build is a universal app bundle.
   - Added the Yomira GUI theme and made it the default. It replaces the
     ScummVM Modern, Modern Remastered and ResidualVM themes, which are no
     longer included. Configurations that used them switch to Yomira.
   - The GUI now uses Noto Sans JP in every language, so Japanese game titles
     display correctly in the launcher. Characters that Noto Sans JP lacks are
     drawn with Liberation Sans.
   - Start and OK buttons are highlighted in themes that support it.

 Delta:
   - New engine for Route Two Ltd. games.
   - Added detection for One☆Lover!! ~Miku-chan no Koibito~ and
     Onegai Tasukete! ~Soto ni Detara Yararechau~.
   - Implemented the title menu, story playback, message rendering and
     saving and loading.
   - Implemented the options menu and the original scene transitions.
   - Added music, sound effects and voice playback.
   - Added a skip action that only skips text that has already been read.
   - Added message window style and transparency settings.

 Illusion:
   - New engine for Illusion games.
   - Added detection for Sexy Beach 2.
   - Implemented the title menu, character selection and swimsuit selection
     with a 3D preview of the chosen swimsuit.
   - Added an OpenGL renderer next to TinyGL, selectable with the
     "Game 3D Renderer" option. The engine falls back to TinyGL when OpenGL
     is not available.

 System-98:
   - New engine for Four-Nine's System-98 games.
   - Added detection for Mugen Yasoukyoku (Nocturnal Illusion).
   - Implemented the main menu, options and the start of a new game, with
     Pi graphics, Japanese text and MIDI music and effects.

 XEX:
   - New engine for Complet's XEX and C4 visual novel engines.
   - Implemented gameplay, message history, auto mode, CG galleries and maps,
     saving and loading, keymapper support and a per-game choice between MIDI
     and digital music.
   - Implemented the timed clicking minigame in The Guts!
   - The engine was previously named Complet. Existing launcher entries need
     their engine ID changed to "xex", or the games must be added again.
