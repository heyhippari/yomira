/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef ENGINES_COMPLET_COMPLET_H
#define ENGINES_COMPLET_COMPLET_H

#include "common/array.h"
#include "common/events.h"
#include "common/rect.h"
#include "common/ustr.h"
#include "engines/engine.h"
#include "graphics/font.h"
#include "graphics/pixelformat.h"

#include "complet/audio.h"
#include "complet/gd.h"
#include "complet/script.h"

namespace Graphics {
struct Surface;
}

namespace Complet {

struct CompletMenuStyle;
struct CompletGameDescription;
struct CompletGallery;

enum {
	kMaxCgCount = 256
};

enum CompletAction {
	kActionNone,
	kActionAdvance,
	kActionSkip,
	kActionHide,
	kActionBacklog,
	kActionBacklogForward,
	kActionUp,
	kActionDown,
	kActionLeft,
	kActionRight,
	kActionQuickSave,
	kActionQuickLoad,
	kActionAutoMode
};

class CompletEngine : public Engine, public Common::EventObserver {
public:
	CompletEngine(OSystem *system, const CompletGameDescription &description);
	~CompletEngine() override;

	Common::Error run() override;
	bool hasFeature(EngineFeature f) const override;
	void applyGameSettings() override;
	void syncSoundSettings() override;
	void pauseEngineIntern(bool pause) override;
	bool notifyEvent(const Common::Event &event) override;

	bool canLoadGameStateCurrently(Common::U32String *msg = nullptr) override;
	bool canSaveGameStateCurrently(Common::U32String *msg = nullptr) override;
	Common::Error loadGameStream(Common::SeekableReadStream *stream) override;
	Common::Error saveGameStream(Common::WriteStream *stream, bool isAutosave = false) override;

	// Called before each command that may wait for the player; savegames restart from there
	void markResumePoint();

	// Runs a script text, including its % commands, until it has finished
	void runText(Script &script, const byte *text);
	// Shows the selection window; returns the chosen item, -2 to go back from a submenu or -1 on quit
	int selectMenu(const Common::Array<MenuItem> &items, bool nested);

	Script &getScript() { return _script; }
	void unlockAllCgs();
	void setPersistentFlag(int number);
	// The retail scripts leave the title music playing into the story
	void leaveTitle() { _audio.stopMusic(); }

private:
	struct MenuLayout {
		int left, top, width, height, rows;
	};

	int scaleX(int value) const { return _xexLayout ? value : value * _screenWidth / kScreenWidth; }
	int scaleY(int value) const { return _xexLayout ? value : value * _screenHeight / kScreenHeight; }

	enum MessageButton {
		kButtonNone,
		kButtonLoad,
		kButtonSave,
		kButtonLog
	};

	void initGame();
	void initCursor();
	void loadFont();
	bool loadFrameImage();
	void loadCommonData();
	void saveCommonData();
	// Games share the save directory, so the original "common" file gets the target as prefix
	Common::String getCommonFileName() const { return _targetName + ".common"; }

	bool interrupted();
	bool applyPendingLoad();
	void rebuildScene();
	void quickSave();
	void quickLoad();

	void recordSeenCg(const Common::String &name);
	bool runGallery();
	bool runGroupedGallery(int group);
	bool runMap();
	bool runMhtMap();
	void blitFrame(Common::Array<byte> &dst, int sx, int sy, int w, int h, int dx, int dy) const;
	bool waitAnyInput();

	void presentBuffer(const byte *bgr, int brightness = 255);
	void present();
	void drawOverlays(Graphics::Surface &screen);
	void drawMessage(Graphics::Surface &screen);
	void drawMenu(Graphics::Surface &screen);
	void drawText(Graphics::Surface &screen, const Common::U32String &text, int x, int y, uint32 color, int advance);
	void drawFrameRect(Graphics::Surface &screen, int sx, int sy, int w, int h, int dx, int dy, bool blackKey = false);
	MenuLayout getMenuLayout() const;
	MessageButton messageButtonAt(const Common::Point &pos) const;
	void refreshCursor();

	bool pollTextInput();
	bool handleSkipAction(const Common::Event &event);
	void hideMessageWindow();
	bool waitTyping(uint32 ms);
	bool waitTimed(uint32 ms);

	void addBacklogChar(uint32 ch);
	void addBacklogBreak();
	void resetBacklog();
	void showBacklog();
	void drawBacklogPage(uint start, int pageNumber, int pageCount);

	bool showLogo();
	bool showFullImage(const Common::String &name, bool fade);
	void stageBackground(const Common::String &name);
	bool showForeground(const Common::String &args);
	void composeSprites(Common::Array<byte> &dst, const Common::String &args);
	bool loadImage(const Common::String &name, Common::Array<byte> &out);
	void overlaySprite(Common::Array<byte> &dst, const Common::Array<byte> &sprite);
	bool fadeTo(const Common::Array<byte> &target);
	bool delay(uint32 ms);
	bool waitClick();

	const byte *runCommand(Script &script, const byte *p);
	void printChar(uint32 ch);
	void clearMessage();
	void newMessageLine();
	int messageLineChars() const;
	int messageMaxLines() const;

	Graphics::PixelFormat _format;
	GdImage _frameImage;
	// Image currently on screen, same layout as GD pixel data
	Common::Array<byte> _scene;
	// Latest %gb background and whether it is still waiting to be shown
	Common::Array<byte> _bg;
	Common::String _bgName;
	bool _bgPending = false;
	const CompletMenuStyle *_menuStyle = nullptr;
	const CompletGallery *_gallery = nullptr;
	bool _case266Map = false;
	bool _mhtMap = false;
	bool _xexLayout = false;
	bool _fullScreenMessages = false;
	bool _framedMessages = false;
	uint16 _waitUnitMillis = 100;
	bool _midiOnly = false;
	bool _blackKeySprites = false;
	int _screenWidth = kScreenWidth;
	int _screenHeight = kScreenHeight;
	// What _scene shows: a full-screen image, or a background with sprites
	Common::String _sceneFull;
	Common::String _sceneBg;
	Common::String _sceneFg;
	const Graphics::Font *_font = nullptr;
	const Graphics::Font *_menuFont = nullptr;
	bool _ownFont = false;

	bool _messageVisible = false;
	// Right click hides the message window and pauses the script
	bool _messageHidden = false;
	// A click while typing shows the rest of the text at once
	bool _fastText = false;
	// Held Ctrl skips text, waits and voices
	bool _skipMode = false;
	bool _cursorRefreshPending = false;
	// Engine options
	bool _autoMode = false;
	bool _animateText = false;
	// Characters printed since the last click, for the auto mode delay
	uint _autoChars = 0;
	Common::Array<Common::U32String> _messageLines;
	uint _messageIndent = 0;
	bool _readingSpeaker = false;

	// Message history, characters mixed with line and page markers
	Common::Array<uint32> _backlog;
	int _backlogLineChars = 0;
	int _backlogLines = 0;

	const Common::Array<MenuItem> *_menu = nullptr;
	int _hover = -1;

	// Script and scene state from the last markResumePoint()
	Common::Array<byte> _resumePoint;
	// Offset in the scenario data of the text being shown, while in a text
	uint32 _textPos = 0;
	bool _inText = false;
	// Set while waiting for a click or a menu choice, the points where saving is allowed
	bool _inputWait = false;
	// Frame of the click indicator while waiting for a click, -1 when hidden
	int _indicatorFrame = -1;
	// Loaded state applied once the interpreter has unwound
	bool _loadPending = false;
	Common::Array<byte> _pendingState;
	uint32 _pendingTextPos = 0;
	// After loading, the text is run silently up to this offset
	bool _replaying = false;
	uint32 _replayEnd = 0;

	AudioPlayer _audio;
	Script _script;
	// Layout of the original "common" file: settings, seen-CG bits, font, flag bits, volumes
	byte _commonData[41];
	bool _seenCg[kMaxCgCount];
};

} // End of namespace Complet

#endif // ENGINES_COMPLET_COMPLET_H
