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

#include "common/config-manager.h"
#include "common/debug.h"
#include "common/events.h"
#include "common/file.h"
#include "common/formats/winexe.h"
#include "common/fs.h"
#include "common/savefile.h"
#include "common/system.h"
#include "engines/util.h"
#include "graphics/fontman.h"
#include "graphics/fonts/ttf.h"
#include "graphics/cursorman.h"
#include "graphics/wincursor.h"
#include "graphics/surface.h"

#include "complet/complet.h"
#include "complet/console.h"
#include "complet/detection.h"

namespace Complet {

enum {
	kFontSize = 24,
	kLogoFadeSteps = 32,
	kLogoFadeDelay = 8,
	kLogoHoldMs = 4000,
	kFadeSteps = 32,
	kFadeDelay = 8,

	// Message window geometry (client coordinates)
	kMessageLeft = 30,
	kMessageTop = 350,
	kMessageLineHeight = 24,
	kMessageMaxLines = 4,
	kMessageLineChars = 20,
	kMessageCharAdvance = 28,
	kMessageCharDelay = 16,
	// The original's 16 ms timer moves the indicator every 8 ticks on NT
	kIndicatorFrameDelay = 128,
	kAutoCharDelay = kIndicatorFrameDelay,
	kMessageBoxX = 16,
	kMessageBoxY = 355,
	kMessageBoxWidth = 608,
	kMessageBoxHeight = 113,

	// Selection window
	// Glyph sheet inside the frame image (GDS\MESFRAME.GD)
	kGlyphSourceRow = 472,

	// LOG, Q-SAVE and Q-LOAD buttons above the message window
	kMessageButtonHeight = 15,
	kLogButtonLeft = 674,
	kLogButtonRight = 699,
	kSaveButtonLeft = 701,
	kSaveButtonRight = 737,
	kLoadButtonLeft = 739,
	kLoadButtonRight = 776,

	// Message history
	kBacklogLineChars = 24,
	kBacklogPageLines = 16,
	kBacklogMaxSize = 8192,
	kBacklogAdvance = 32
};

static const uint32 kBacklogLine = 0xfffffffe;
static const uint32 kBacklogPage = 0xffffffff;

static const char *const kFontFiles[] = {
	"NotoSansCJK-Regular.ttc",
	"NotoSansCJKjp-Regular.otf",
	"NotoSerifCJK-Regular.ttc"
};

CompletEngine::CompletEngine(OSystem *system, const CompletGameDescription &description) :
		Engine(system), _menuStyle(&description.menuStyle), _gallery(&description.gallery),
		_case266Map(description.mapLayout == kCompletMapCase266),
		_mhtMap(description.mapLayout == kCompletMapMht),
		_xexEngine(description.engineFamily == kCompletEngineXex),
		_fullScreenMessages(description.messageLayout == kMessageFullScreen),
		_framedMessages(description.messageLayout == kMessageFramedDialogue), _waitUnitMillis(description.waitUnitMillis),
		_midiOnly(description.midiOnly), _screenWidth(description.screenWidth), _screenHeight(description.screenHeight),
		_audio(_mixer), _script(this) {
}

CompletEngine::~CompletEngine() {
	g_system->getEventManager()->getEventDispatcher()->unregisterObserver(this);
	if (_ownFont)
		delete _font;
	delete _menuFont;
}

bool CompletEngine::hasFeature(EngineFeature f) const {
	return f == kSupportsReturnToLauncher || f == kSupportsLoadingDuringRuntime || f == kSupportsSavingDuringRuntime ||
		f == kSupportsChangingOptionsDuringRuntime;
}

void CompletEngine::applyGameSettings() {
	_animateText = ConfMan.getBool("text_animation");
	_nameFrameEnabled = !_fullScreenMessages && !_framedMessages && ConfMan.getBool("name_frame");
	_audio.setUseMidi(_midiOnly || ConfMan.getBool("midi_music"));
}

void CompletEngine::syncSoundSettings() {
	Engine::syncSoundSettings();
	_audio.syncVolume();
}

void CompletEngine::pauseEngineIntern(bool pause) {
	Engine::pauseEngineIntern(pause);
	_audio.pause(pause);
}

// Like the original, the pointer hides on keyboard input and comes back when the mouse moves
bool CompletEngine::notifyEvent(const Common::Event &event) {
	if (event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START && event.customType == kActionAutoMode) {
		_autoMode = !_autoMode;
		if (_indicatorFrame >= 0 && !_showingBacklog)
			present();
		return true;
	}

	bool show = CursorMan.isVisible();
	if (event.type == Common::EVENT_MOUSEMOVE)
		show = true;
	else if (event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START &&
			event.customType != kActionQuickSave && event.customType != kActionQuickLoad)
		show = false;

	if (show != CursorMan.isVisible()) {
		CursorMan.showMouse(show);
		_cursorRefreshPending = true;
	}
	if (event.type == Common::EVENT_MOUSEMOVE)
		_cursorRefreshPending = true;
	return false;
}

void CompletEngine::refreshCursor() {
	if (!_cursorRefreshPending)
		return;

	_cursorRefreshPending = false;
	g_system->updateScreen();
}

void CompletEngine::initCursor() {
	// Both engine executables store the arrow cursor as resource 104
	Common::WinResources *exe = Common::WinResources::createFromEXE("c4.exe");
	if (!exe)
		exe = Common::WinResources::createFromEXE("xex.exe");
	Graphics::WinCursorGroup *group = exe ? Graphics::WinCursorGroup::createCursorGroup(exe, 104) : nullptr;
	if (group && !group->cursors.empty()) {
		CursorMan.replaceCursor(group->cursors[0].cursor);
	} else {
		Graphics::Cursor *cursor = Graphics::makeDefaultWinCursor();
		CursorMan.replaceCursor(cursor);
		delete cursor;
	}
	delete group;
	delete exe;

	CursorMan.showMouse(true);
	g_system->getEventManager()->getEventDispatcher()->registerObserver(this, 1, false);
}

Common::Error CompletEngine::run() {
	_format = Graphics::PixelFormat::createFormatBGRA32();
	initGame();
	initGraphics(_screenWidth, _screenHeight, &_format);
	if (!loadFrameImage())
		return Common::Error(Common::kReadingFailed, "Missing or incompatible game frame image");

	setDebugger(new Console(this));
	ConfMan.registerDefault("text_animation", false);
	ConfMan.registerDefault("midi_music", false);
	ConfMan.registerDefault("name_frame", true);
	syncSoundSettings();
	applyGameSettings();

	loadFont();
	initCursor();
#ifndef USE_MAD
	warning("Complet: built without MP3 support, voices and digital music are silent");
#endif

	_scene.resize(_screenWidth * _screenHeight * 3);
	memset(_scene.data(), 0, _scene.size());
	_bg = _scene;
	resetBacklog();
	loadCommonData();
	present();

	if (!_script.start("startup"))
		return Common::Error(Common::kReadingFailed, "Cannot load the startup scenario");

	if (ConfMan.hasKey("save_slot"))
		loadGameState(ConfMan.getInt("save_slot"));

	while (!shouldQuit()) {
		if (_loadPending && !applyPendingLoad())
			return Common::Error(Common::kReadingFailed, "Cannot restore the saved game");
		if (!_script.isRunning())
			break;
		_script.step();
	}

	// The game's own exit (%q) goes back to the launcher instead of closing ScummVM
	if (!shouldQuit())
		ConfMan.setBool("gui_return_to_launcher_at_exit", true, Common::ConfigManager::kTransientDomain);

	return Common::kNoError;
}

bool CompletEngine::interrupted() {
	return shouldQuit() || _loadPending;
}

void CompletEngine::initGame() {
	const Common::FSNode gameDataDir(ConfMan.getPath("path"));
	// The detector already added mpx/, where the detection file lives
	SearchMan.addSubDirectoryMatching(gameDataDir, "gds");
	SearchMan.addSubDirectoryMatching(gameDataDir, "vmd");
	SearchMan.addSubDirectoryMatching(gameDataDir, "mus");
}

void CompletEngine::loadFont() {
	for (uint i = 0; i < ARRAYSIZE(kFontFiles) && !_font; i++) {
		Common::SeekableReadStream *stream = SearchMan.createReadStreamForMember(kFontFiles[i]);
		if (stream) {
			const int fontSize = scaleMessageY(_menuStyle->menuFontSize ? _menuStyle->menuFontSize : static_cast<int>(kFontSize));
			if (_menuStyle->plainMenu && _menuStyle->menuFontWidth) {
				const int xdpi = _menuStyle->menuFontWidth * 72 / _menuStyle->menuFontSize;
				_font = Graphics::loadTTFFont(stream, DisposeAfterUse::YES, fontSize, Graphics::kTTFSizeModeCharacter, xdpi, 72);
			} else {
				_font = Graphics::loadTTFFont(stream, DisposeAfterUse::YES, fontSize, Graphics::kTTFSizeModeCell);
			}
		}
		if (_font && (_menuStyle->outlinedMenu || _menuStyle->plainMenu)) {
			stream = SearchMan.createReadStreamForMember(kFontFiles[i]);
			if (stream) {
				if (_menuStyle->plainMenu) {
					const int xdpi = _menuStyle->menuFontWidth ? _menuStyle->menuFontWidth * 72 / _menuStyle->menuFontSize : 72;
					_menuFont = Graphics::loadTTFFont(stream, DisposeAfterUse::YES, _menuStyle->menuFontSize, Graphics::kTTFSizeModeCharacter, xdpi, 72);
				} else {
					_menuFont = Graphics::loadTTFFont(stream, DisposeAfterUse::YES, 32, Graphics::kTTFSizeModeCell, 100, 72);
				}
			}
		}
	}

	if (_font) {
		_ownFont = true;
		return;
	}

	warning("Complet: no Japanese font found, put one of the Noto CJK fonts in the extra path");
	_font = FontMan.getFontByUsage(Graphics::FontManager::kGUIFont);
}

bool CompletEngine::loadFrameImage() {
	// MESFRAME.GD holds the message frame and glyph sheet, NULL.GD is the fallback
	const bool loaded = _frameImage.loadFromName("mesframe.gd") || _frameImage.loadFromName("null.gd");
	if (!loaded)
		return false;

	_blackKeySprites = isXexEngine();
	if (_frameImage.getWidth() != _screenWidth || _frameImage.getHeight() != _screenHeight) {
		warning("Complet: frame image dimensions do not match the detected game");
		return false;
	}
	return true;
}

enum {
	kCommonSize = 41,
	kCommonSeenBits = 1,
	kCommonFlagBits = 34
};

void CompletEngine::loadCommonData() {
	memset(_commonData, 0, sizeof(_commonData));
	_commonData[0] = 1;
	memset(_seenCg, 0, sizeof(_seenCg));

	Common::InSaveFile *file = getSaveFileManager()->openForLoading(getCommonFileName());
	if (!file)
		return;

	if (file->size() >= kCommonSize && file->read(_commonData, kCommonSize) == kCommonSize) {
		for (int i = 0; i < 32; i++) {
			if (_commonData[kCommonFlagBits + i / 8] & (1 << (i & 7)))
				_script.setFlag(i);
		}
		for (int i = 0; i < _gallery->count; i++)
			_seenCg[i] = _commonData[kCommonSeenBits + i / 8] & (1 << (i & 7));
	}
	delete file;
}

void CompletEngine::saveCommonData() {
	memset(_commonData + kCommonFlagBits, 0, 4);
	for (int i = 0; i < 32; i++) {
		if (_script.getFlag(i))
			_commonData[kCommonFlagBits + i / 8] |= 1 << (i & 7);
	}
	memset(_commonData + kCommonSeenBits, 0, 32);
	for (int i = 0; i < _gallery->count; i++) {
		if (_seenCg[i])
			_commonData[kCommonSeenBits + i / 8] |= 1 << (i & 7);
	}

	Common::OutSaveFile *file = getSaveFileManager()->openForSaving(getCommonFileName(), false);
	if (!file) {
		warning("Complet: cannot write the common data file");
		return;
	}

	file->write(_commonData, kCommonSize);
	file->finalize();
	delete file;
}

void CompletEngine::presentBuffer(const byte *bgr, int brightness) {
	Graphics::Surface *screen = g_system->lockScreen();

	// Source rows are bottom-up
	for (int y = 0; y < _screenHeight; y++) {
		const byte *row = bgr + (_screenHeight - 1 - y) * _screenWidth * 3;
		uint32 *dst = (uint32 *)screen->getBasePtr(0, y);
		for (int x = 0; x < _screenWidth; x++, row += 3)
			*dst++ = _format.RGBToColor(row[2] * brightness / 255, row[1] * brightness / 255, row[0] * brightness / 255);
	}

	g_system->unlockScreen();
	g_system->updateScreen();
	_cursorRefreshPending = false;
}

void CompletEngine::present() {
	// A loaded game shows up once its text has caught up
	if (_replaying)
		return;

	Graphics::Surface *screen = g_system->lockScreen();

	const byte *bgr = _scene.data();
	for (int y = 0; y < _screenHeight; y++) {
		const byte *row = bgr + (_screenHeight - 1 - y) * _screenWidth * 3;
		uint32 *dst = (uint32 *)screen->getBasePtr(0, y);
		for (int x = 0; x < _screenWidth; x++, row += 3)
			*dst++ = _format.RGBToColor(row[2], row[1], row[0]);
	}
	drawOverlays(*screen);

	g_system->unlockScreen();
	g_system->updateScreen();
	_cursorRefreshPending = false;
}

void CompletEngine::drawOverlays(Graphics::Surface &screen) {
	if (!_messageHidden)
		drawMessage(screen);
	if (_indicatorFrame >= 0 && !_messageHidden) {
		if (_fullScreenMessages) {
			const int line = MAX(0, (int)_messageLines.size() - 1);
			const int columns = _messageLines.empty() ? 0 : _messageLines.back().size();
			_font->drawChar(&screen, _indicatorFrame & 2 ? 0x25bd : 0x25bc,
					scaleMessageX(26 + columns * 28), scaleMessageY(25 + line * 24), _format.RGBToColor(0xff, 0xff, 0xff));
		} else if (_framedMessages) {
			const byte level = (_indicatorFrame % 8 + 1) * 31;
			byte red = 0, green = 0, blue = 0;
			switch (_indicatorFrame / 8) {
			case 0: red = 248; blue = level; break;
			case 1: red = 248 - level; blue = 248; break;
			case 2: green = level; blue = 248; break;
			case 3: green = 248; blue = 248 - level; break;
			case 4: red = level; green = 248; break;
			default: red = 248; green = 248 - level; break;
			}
			_font->drawChar(&screen, _indicatorFrame & 2 ? 0x25bd : 0x25bc,
					scaleMessageX(580), scaleMessageY(446), _format.RGBToColor(red, green, blue));
		} else if (isXexEngine()) {
			drawFrameRect(screen, 64 + _indicatorFrame * 26, _autoMode ? 12 : 0,
					26, 12, 588, 480 - 34);
		} else {
			// Animated "CLICK" (or "AUTO") marker from the top left of the C4 frame image
			drawFrameRect(screen, scaleX((_indicatorFrame + (_autoMode ? 8 : 0)) * 32), 0,
					scaleX(32), scaleY(32), scaleX(743), scaleY(kScreenHeight - 52), true);
		}
	}
	if (_menu)
		drawMenu(screen);
}

// Copies a rectangle of the frame image, given in top-down coordinates; optionally black is transparent
void CompletEngine::drawFrameRect(Graphics::Surface &screen, int sx, int sy, int w, int h, int dx, int dy, bool blackKey) {
	const byte *frame = _frameImage.getPixels();
	for (int y = 0; y < h; y++) {
		const byte *src = frame + ((_screenHeight - 1 - (sy + y)) * _screenWidth + sx) * 3;
		uint32 *dst = (uint32 *)screen.getBasePtr(dx, dy + y);
		for (int x = 0; x < w; x++, src += 3, dst++) {
			if (!blackKey || src[0] || src[1] || src[2])
				*dst = _format.RGBToColor(src[2], src[1], src[0]);
		}
	}
}

CompletEngine::MessageButton CompletEngine::messageButtonAt(const Common::Point &pos) const {
	const int buttonY = scaleMessageY(kMessageBoxY) - scaleY(kMessageButtonHeight);
	if (_fullScreenMessages || isXexEngine() || !_messageVisible || _messageHidden || _menu ||
			pos.y < buttonY + scaleY(1) || pos.y >= buttonY + scaleY(kMessageButtonHeight - 1))
		return kButtonNone;
	if (pos.x >= scaleX(kLoadButtonLeft) && pos.x < scaleX(kLoadButtonRight))
		return kButtonLoad;
	if (pos.x >= scaleX(kSaveButtonLeft) && pos.x < scaleX(kSaveButtonRight))
		return kButtonSave;
	if (pos.x >= scaleX(kLogButtonLeft) && pos.x < scaleX(kLogButtonRight))
		return kButtonLog;
	return kButtonNone;
}

void CompletEngine::drawText(Graphics::Surface &screen, const Common::U32String &text, int x, int y, uint32 color, int advance) {
	for (uint i = 0; i < text.size(); i++) {
		_font->drawChar(&screen, text[i], x, y, color);
		x += text[i] < 0x100 ? advance / 2 : advance;
	}
}

void CompletEngine::drawOutlinedText(Graphics::Surface &screen, const Common::U32String &text, int x, int y, int advance, uint32 outline, uint32 fill) {
	for (int offsetY = -1; offsetY <= 1; offsetY++) {
		drawText(screen, text, x - 1, y + offsetY, outline, advance);
		drawText(screen, text, x + 1, y + offsetY, outline, advance);
	}
	drawText(screen, text, x + 1, y, fill, advance);
	drawText(screen, text, x, y, fill, advance);
}

void CompletEngine::drawMessage(Graphics::Surface &screen) {
	if (!_messageVisible)
		return;

	// Dim the area behind the text
	const int boxX = scaleMessageX(_fullScreenMessages ? 20 : kMessageBoxX);
	const int boxY = scaleMessageY(_fullScreenMessages ? 15 : kMessageBoxY);
	const int boxWidth = scaleMessageX(_fullScreenMessages ? 600 : kMessageBoxWidth);
	const int boxHeight = scaleMessageY(_fullScreenMessages ? 450 : kMessageBoxHeight);
	for (int y = boxY; y < boxY + boxHeight; y++) {
		uint32 *p = (uint32 *)screen.getBasePtr(boxX, y);
		for (int x = 0; x < boxWidth; x++, p++) {
			byte r, g, b;
			_format.colorToRGB(*p, r, g, b);
			*p = _format.RGBToColor(r >> 2, g >> 2, b >> 1);
		}
	}

	if (_framedMessages)
		drawFrameRect(screen, 0, scaleMessageY(318), scaleMessageX(640), scaleMessageY(162), 0, scaleMessageY(318), true);
	const bool showNameFrame = _nameFrameEnabled && !_messageSpeaker.empty();
	const int advance = scaleMessageX(kMessageCharAdvance);
	const int messageX = scaleMessageX(_fullScreenMessages ? 24 : kMessageLeft);
	if (showNameFrame) {
		const int frameX = boxX;
		const int frameY = boxY - scaleMessageY(33);
		const int frameHeight = scaleMessageY(33);
		const int frameWidth = scaleMessageX(128);
		const int bevel = scaleMessageX(34);
		for (int row = 0; row < frameHeight; row++) {
			const int right = frameX + frameWidth - bevel + bevel * row / (frameHeight - 1);
			uint32 *pixel = (uint32 *)screen.getBasePtr(frameX, frameY + row);
			for (int column = 0; column < right - frameX; column++, pixel++) {
				byte red, green, blue;
				_format.colorToRGB(*pixel, red, green, blue);
				*pixel = _format.RGBToColor(red >> 2, green >> 2, blue >> 1);
			}
		}

		const int speakerX = messageX - scaleMessageX(7);
		const int speakerY = frameY - scaleMessageY(2);
		const uint32 speakerOutline = _format.RGBToColor(0x50, 0x50, 0x7f);
		drawOutlinedText(screen, _messageSpeaker, speakerX, speakerY, advance, speakerOutline, _format.RGBToColor(0xff, 0xff, 0xff));
	}

	const uint32 shadow = (_fullScreenMessages || _framedMessages) ? _format.RGBToColor(0x18, 0x18, 0x30) : _format.RGBToColor(0x3f, 0x3f, 0x7f);
	const uint32 white = _format.RGBToColor(0xff, 0xff, 0xff);
	for (uint i = 0; i < _messageLines.size(); i++) {
		int y = scaleMessageY(_fullScreenMessages ? 23 + i * 24 : kMessageTop + i * kMessageLineHeight);
		if (!_fullScreenMessages && _screenWidth == 800 && _screenHeight == 600)
			y += 2;
		int x = messageX;
		if (_fullScreenMessages || _framedMessages) {
			for (int offsetY = -1; offsetY < 2; offsetY++) {
				for (int offsetX = -1; offsetX < 3; offsetX++)
					drawText(screen, _messageLines[i], x + offsetX, y + offsetY, shadow, advance);
			}
			drawText(screen, _messageLines[i], x + 1, y, white, advance);
		} else {
			drawOutlinedText(screen, _messageLines[i], x, y, advance, _format.RGBToColor(0x50, 0x50, 0x7f), white);
		}
		if (_fullScreenMessages || _framedMessages)
			drawText(screen, _messageLines[i], x, y, white, advance);
	}

	// Q-SAVE/Q-LOAD and LOG buttons from the top right of the frame image
	if (!_menu && !isXexEngine()) {
		const int buttonY = boxY - scaleY(kMessageButtonHeight);
		drawFrameRect(screen, scaleX(512), 0, scaleX(77), scaleY(kMessageButtonHeight), scaleX(kSaveButtonLeft - 1), buttonY);
		drawFrameRect(screen, scaleX(512), scaleY(16), scaleX(27), scaleY(kMessageButtonHeight), scaleX(kLogButtonLeft - 1), buttonY);
	}
}

CompletEngine::MenuLayout CompletEngine::getMenuLayout() const {
	int longest = 0;
	for (uint i = 0; i < _menu->size(); i++) {
		int length = (*_menu)[i].text.size();
		if (length > longest)
			longest = length;
	}

	MenuLayout layout;
	layout.rows = _menu->size();
	layout.left = scaleX(_menuStyle->centerX - _menuStyle->leftPerCharacter * longest);
	layout.width = scaleX(_menuStyle->widthPerCharacter * longest + _menuStyle->widthPadding);
	layout.top = scaleY(_menuStyle->topBase - _menuStyle->topPerRow * layout.rows);
	layout.height = scaleY(_menuStyle->rowHeight * layout.rows + _menuStyle->heightPadding);
	layout.left += scaleX(_menuStyle->offsetX);
	layout.top += scaleY(_menuStyle->offsetY);
	return layout;
}

static bool insideRoundRect(int x, int y, int w, int h, int r) {
	if (x < 0 || y < 0 || x >= w || y >= h)
		return false;

	int cx = x < r ? r : (x >= w - r ? w - r - 1 : x);
	int cy = y < r ? r : (y >= h - r ? h - r - 1 : y);
	int dx = x - cx, dy = y - cy;
	return dx * dx + dy * dy <= r * r;
}

void CompletEngine::drawMenu(Graphics::Surface &screen) {
	const MenuLayout layout = getMenuLayout();
	const CompletMenuStyle &style = *_menuStyle;
	const int rowHeight = scaleY(style.rowHeight);
	const int charAdvance = scaleX(style.charAdvance);
	const uint32 fill = _format.RGBToColor((style.fillColor >> 16) & 0xff, (style.fillColor >> 8) & 0xff, style.fillColor & 0xff);
	const uint32 border = _format.RGBToColor((style.borderColor >> 16) & 0xff, (style.borderColor >> 8) & 0xff, style.borderColor & 0xff);
	for (int y = 0; y < layout.height; y++) {
		int sy = layout.top + y;
		if (sy < 0 || sy >= screen.h)
			continue;
		for (int x = 0; x < layout.width; x++) {
			int sx = layout.left + x;
			if (sx < 0 || sx >= screen.w || !insideRoundRect(x, y, layout.width, layout.height, scaleX(style.cornerRadius)))
				continue;
			const int borderWidth = style.outlinedMenu ? 1 : 2;
			bool inner = insideRoundRect(x - borderWidth, y - borderWidth, layout.width - 2 * borderWidth, layout.height - 2 * borderWidth, scaleX(style.cornerRadius - borderWidth));
			*(uint32 *)screen.getBasePtr(sx, sy) = inner ? fill : border;
		}
	}

	for (int i = 0; i < layout.rows; i++) {
		Common::U32String text((*_menu)[i].text, Common::kWindows932);
		if (style.plainMenu) {
			const Graphics::Font *font = _menuFont ? _menuFont : _font;
			const int x = layout.left + layout.width / 2 - text.size() * charAdvance / 2;
			const int y = layout.top + 2 + i * rowHeight - MAX(0, (font->getFontHeight() - style.menuFontSize) / 2);
			const uint32 textColor = i == _hover ? style.hoverTextColor : style.idleTextColor;
			const uint32 color = _format.RGBToColor((textColor >> 16) & 0xff, (textColor >> 8) & 0xff, textColor & 0xff);
			const uint32 shadow = _format.RGBToColor((style.textShadowColor >> 16) & 0xff, (style.textShadowColor >> 8) & 0xff, style.textShadowColor & 0xff);
			for (uint j = 0; j < text.size(); j++) {
				const int glyphX = x + j * charAdvance;
				const int glyphY = y + (i == _hover && style.outlineSelectedText ? 1 : 0);
				if (i == _hover) {
					if (style.outlineSelectedText) {
						for (int offsetY = -1; offsetY <= 1; offsetY++) {
							font->drawChar(&screen, text[j], glyphX - 1, glyphY + offsetY, shadow);
							font->drawChar(&screen, text[j], glyphX + 1, glyphY + offsetY, shadow);
						}
					} else {
						font->drawChar(&screen, text[j], glyphX + 1, glyphY + 1, shadow);
					}
				}
				if (style.menuFontBold)
					font->drawChar(&screen, text[j], glyphX + 1, glyphY, color);
				font->drawChar(&screen, text[j], glyphX, glyphY, color);
			}
			continue;
		}
		if (style.outlinedMenu) {
			const Graphics::Font *font = _menuFont ? _menuFont : _font;
			const int textWidth = text.size() * charAdvance;
			const int x = layout.left + layout.width / 2 - textWidth / 2;
			const int y = layout.top + 2 + i * rowHeight;
			if (i == _hover) {
				screen.drawEllipse(x - 8, y, x + textWidth + 7, y + 23, _format.RGBToColor(0xf3, 0x9b, 0x23), true);
				screen.drawEllipse(x - 8, y, x + textWidth + 7, y + 23, _format.RGBToColor(0, 0, 0), false);
			}
			Graphics::Surface glyph;
			for (uint j = 0; j < text.size(); j++) {
				glyph.create(MAX(1, font->getCharWidth(text[j])), font->getFontHeight(), _format);
				glyph.fillRect(Common::Rect(glyph.w, glyph.h), 0);
				font->drawChar(&glyph, text[j], 0, 0, _format.RGBToColor(0xff, 0xff, 0xff));
				Graphics::Surface *scaledGlyph = glyph.scale(32, 32);
				for (int pass = 0; pass < 2; pass++) {
					for (int glyphY = 0; glyphY < 32; glyphY++) {
						for (int glyphX = 0; glyphX < 32; glyphX++) {
							byte alpha, unusedGreen, unusedBlue;
							_format.colorToRGB(*(const uint32 *)scaledGlyph->getBasePtr(glyphX, glyphY), alpha, unusedGreen, unusedBlue);
							if (!alpha)
								continue;
							const int radius = pass == 0 ? 1 : 0;
							const int shade = pass == 0 ? 0x60 : 0xff;
							for (int offsetY = -radius; offsetY <= radius; offsetY++) {
								for (int offsetX = -radius; offsetX <= radius; offsetX++) {
									const int sx = x + j * charAdvance + glyphX + offsetX;
									const int sy = y + glyphY + offsetY - 6;
									if (sx < 0 || sx >= screen.w || sy < 0 || sy >= screen.h)
										continue;
									uint32 *pixel = (uint32 *)screen.getBasePtr(sx, sy);
									byte red, green, blue;
									_format.colorToRGB(*pixel, red, green, blue);
									*pixel = _format.RGBToColor((red * (255 - alpha) + shade * alpha) / 255,
											(green * (255 - alpha) + shade * alpha) / 255, (blue * (255 - alpha) + shade * alpha) / 255);
								}
							}
						}
					}
				}
				scaledGlyph->free();
				delete scaledGlyph;
				glyph.free();
			}
			continue;
		}
		int inkLeft = INT_MAX;
		int inkRight = INT_MIN;
		for (uint j = 0; j < text.size(); j++) {
			const Common::Rect bounds = _font->getBoundingBox(text[j]);
			inkLeft = MIN(inkLeft, (int)j * charAdvance + bounds.left);
			inkRight = MAX(inkRight, (int)j * charAdvance + bounds.right);
		}
		int x = layout.left + layout.width / 2 - (inkLeft + inkRight) / 2;
		int y = layout.top + scaleY(2) + i * rowHeight;

		const uint32 idle = _format.RGBToColor((style.idleTextColor >> 16) & 0xff, (style.idleTextColor >> 8) & 0xff, style.idleTextColor & 0xff);
		const uint32 shadow = _format.RGBToColor((style.textShadowColor >> 16) & 0xff, (style.textShadowColor >> 8) & 0xff, style.textShadowColor & 0xff);
		const uint32 white = _format.RGBToColor((style.hoverTextColor >> 16) & 0xff, (style.hoverTextColor >> 8) & 0xff, style.hoverTextColor & 0xff);
		const uint32 outline = _format.RGBToColor((style.hoverOutlineColor >> 16) & 0xff, (style.hoverOutlineColor >> 8) & 0xff, style.hoverOutlineColor & 0xff);
		if (i == _hover) {
			drawText(screen, text, x + 2, y + 2, shadow, charAdvance);
			drawText(screen, text, x + 1, y + 1, outline, charAdvance);
			drawText(screen, text, x, y, white, charAdvance);
		} else {
			drawText(screen, text, x + 1, y + 1, shadow, charAdvance);
			drawText(screen, text, x, y, idle, charAdvance);
		}
	}

	if (style.showQuickSaveLoad) {
		if (isXexEngine())
			drawFrameRect(screen, 121, 291, 75, 13,
					layout.left + layout.width - 93, layout.top + layout.rows * rowHeight - 1);
		else
			drawFrameRect(screen, scaleX(512), scaleY(1), scaleX(77), scaleY(14),
					layout.left + layout.width - scaleX(93), layout.top + layout.rows * rowHeight + scaleY(2));
	}
}

int CompletEngine::selectMenu(const Common::Array<MenuItem> &items, bool nested) {
	_menu = &items;
	_hover = -1;
	_messageVisible = false;
	const MenuLayout layout = getMenuLayout();

	auto rowAt = [&](const Common::Point &pos) {
		if (pos.x < layout.left || pos.x >= layout.left + layout.width)
			return -1;
		int row = (pos.y - layout.top) / scaleY(_menuStyle->rowHeight);
		if (pos.y < layout.top || row >= layout.rows)
			return -1;
		return row;
	};

	_hover = rowAt(g_system->getEventManager()->getMousePos());
	if (_hover < 0 && !_menuStyle->plainMenu)
		_hover = 0;
	present();

	// Right click or Escape goes back from a submenu and hides the top menu to show the artwork
	bool hidden = false;
	auto cancel = [&]() {
		if (nested)
			return -2;
		hidden = !hidden;
		_menu = hidden ? nullptr : &items;
		present();
		return -3;
	};
	auto backlog = [&]() {
		_menu = nullptr;
		showBacklog();
		_menu = hidden ? nullptr : &items;
		present();
	};
	auto quickButtonAt = [&](const Common::Point &pos) {
		if (!_menuStyle->showQuickSaveLoad)
			return kButtonNone;
		int x = pos.x - layout.left - layout.width;
		int y = pos.y - layout.top - layout.rows * scaleY(_menuStyle->rowHeight);
		if (y < (isXexEngine() ? -1 : scaleY(2)) || y >= (isXexEngine() ? 12 : scaleY(16)))
			return kButtonNone;
		if (x > -scaleX(93) && x < -scaleX(57))
			return kButtonSave;
		if (x >= -scaleX(56) && x < -scaleX(18))
			return kButtonLoad;
		return kButtonNone;
	};

	_inputWait = true;
	int result = -3;
	Common::Event event;
	while (result == -3 && !interrupted()) {
		int previous = _hover;
		while (result == -3 && !interrupted() && g_system->getEventManager()->pollEvent(event)) {
			if (handleSkipAction(event))
				continue;

			switch (event.type) {
			case Common::EVENT_MOUSEMOVE:
				if (_menuStyle->plainMenu || rowAt(event.mouse) >= 0)
					_hover = rowAt(event.mouse);
				break;
			case Common::EVENT_LBUTTONUP:
				if (hidden)
					break;
				if (rowAt(event.mouse) >= 0)
					result = rowAt(event.mouse);
				else if (quickButtonAt(event.mouse) == kButtonSave)
					quickSave();
				else if (quickButtonAt(event.mouse) == kButtonLoad)
					quickLoad();
				break;
			case Common::EVENT_RBUTTONUP:
				result = cancel();
				break;
			case Common::EVENT_WHEELUP:
				backlog();
				break;
			case Common::EVENT_CUSTOM_ENGINE_ACTION_START:
				switch (event.customType) {
				case kActionHide:
					result = cancel();
					break;
				case kActionBacklog:
					backlog();
					break;
				case kActionQuickSave:
					quickSave();
					break;
				case kActionQuickLoad:
					quickLoad();
					break;
				case kActionUp:
					if (!hidden)
						_hover = _hover <= 0 ? layout.rows - 1 : _hover - 1;
					break;
				case kActionDown:
					if (!hidden)
						_hover = (_hover + 1) % layout.rows;
					break;
				case kActionAdvance:
					if (!hidden && _hover >= 0)
						result = _hover;
					break;
				default:
					break;
				}
				break;
			default:
				break;
			}
		}

		refreshCursor();
		if (_hover != previous && !hidden)
			present();
		g_system->delayMillis(10);
	}

	if (result >= 0 && !interrupted()) {
		if (_backlogLineChars)
			addBacklogBreak();
		Common::U32String choice(Common::String::format("\x81\x84\x81\x75%s\x81\x76\x82\xf0\x91\x49\x91\xf0\x81\x83", items[result].text.c_str()), Common::kWindows932);
		for (uint i = 0; i < choice.size(); i++)
			addBacklogChar(choice[i]);
		addBacklogBreak();
	}

	_inputWait = false;
	_menu = nullptr;
	_hover = -1;
	present();
	return interrupted() ? -1 : result;
}

// Skipping lasts while the action is held; returns true if the event was a skip press or release
bool CompletEngine::handleSkipAction(const Common::Event &event) {
	if (event.type != Common::EVENT_CUSTOM_ENGINE_ACTION_START && event.type != Common::EVENT_CUSTOM_ENGINE_ACTION_END)
		return false;
	if (event.customType != kActionSkip)
		return false;

	_skipMode = event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START;
	return true;
}

bool CompletEngine::delay(uint32 ms) {
	uint32 end = g_system->getMillis() + ms;
	Common::Event event;

	while (g_system->getMillis() < end) {
		while (g_system->getEventManager()->pollEvent(event))
			handleSkipAction(event);
		refreshCursor();
		if (interrupted())
			return false;
		g_system->delayMillis(5);
	}
	return !interrupted();
}

// Handles the input available while text is shown; returns true when the player asks to go on
bool CompletEngine::pollTextInput() {
	bool advance = false;
	Common::Event event;

	while (g_system->getEventManager()->pollEvent(event)) {
		if (handleSkipAction(event)) {
			advance |= _skipMode;
			continue;
		}

		switch (event.type) {
		case Common::EVENT_LBUTTONUP:
			switch (messageButtonAt(event.mouse)) {
			case kButtonNone:
				advance = true;
				break;
			case kButtonLog:
				showBacklog();
				break;
			case kButtonSave:
				quickSave();
				break;
			case kButtonLoad:
				quickLoad();
				break;
			}
			break;
		case Common::EVENT_RBUTTONUP:
			if (_skipMode)
				_skipMode = false;
			else
				hideMessageWindow();
			break;
		case Common::EVENT_WHEELUP:
			showBacklog();
			break;
		case Common::EVENT_CUSTOM_ENGINE_ACTION_START:
			switch (event.customType) {
			case kActionAdvance:
				advance = true;
				break;
			case kActionHide:
				hideMessageWindow();
				break;
			case kActionBacklog:
				showBacklog();
				break;
			case kActionQuickSave:
				quickSave();
				break;
			case kActionQuickLoad:
				quickLoad();
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
	}
	refreshCursor();
	return advance;
}

// Keeps the message window hidden, and the script paused, until the next right click
void CompletEngine::hideMessageWindow() {
	_messageHidden = true;
	present();

	Common::Event event;
	while (_messageHidden && !interrupted()) {
		while (_messageHidden && g_system->getEventManager()->pollEvent(event)) {
			if (handleSkipAction(event))
				continue;
			const bool action = event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START;
			if (event.type == Common::EVENT_RBUTTONUP || (action && event.customType == kActionHide))
				_messageHidden = false;
			else if (event.type == Common::EVENT_WHEELUP || (action && event.customType == kActionBacklog))
				showBacklog();
		}
		refreshCursor();
		g_system->delayMillis(10);
	}

	_messageHidden = false;
	present();
}

// Per-character delay; a click or key shows the rest of the text at once
bool CompletEngine::waitTyping(uint32 ms) {
	uint32 end = g_system->getMillis() + ms;
	while (!interrupted() && !_fastText && !_skipMode && g_system->getMillis() < end) {
		if (pollTextInput())
			_fastText = true;
		else
			g_system->delayMillis(5);
	}
	return !interrupted();
}

// %w: clicks do not shorten it, but skipping does
bool CompletEngine::waitTimed(uint32 ms) {
	uint32 end = g_system->getMillis() + ms;
	while (!interrupted() && !_skipMode && !_replaying && g_system->getMillis() < end) {
		pollTextInput();
		g_system->delayMillis(5);
	}
	return !interrupted();
}

bool CompletEngine::waitClick() {
	if (_replaying)
		return true;

	// Auto mode waits for the voice, then one icon frame (128 ms on NT) per character shown
	const uint32 start = g_system->getMillis();
	const uint32 readTime = _autoChars * kAutoCharDelay;

	present();
	_inputWait = true;
	bool advance = false;
	while (!interrupted() && !advance) {
		advance = pollTextInput() || _skipMode ||
			(_autoMode && !_messageHidden && !_audio.isVoicePlaying() && g_system->getMillis() - start >= readTime);

		// The first frame appears after one period, like the original timer
		uint32 periods = (g_system->getMillis() - start) / (_framedMessages ? 30 : kIndicatorFrameDelay);
		int frame = periods ? periods % (_framedMessages ? 48 : 8) : -1;
		if (!advance && frame != _indicatorFrame) {
			_indicatorFrame = frame;
			present();
		}
		if (!advance)
			g_system->delayMillis(10);
	}
	_indicatorFrame = -1;
	_inputWait = false;
	_fastText = false;
	_autoChars = 0;
	return !interrupted();
}

void CompletEngine::setPersistentFlag(int number) {
	if (_script.setFlag(number))
		saveCommonData();
}

// Company logo: fade in, hold four seconds, fade out
bool CompletEngine::showLogo() {
	GdImage logo;
	if (!logo.loadFromName("cplogo.gd"))
		return true;
	if (logo.getWidth() != _screenWidth || logo.getHeight() != _screenHeight) {
		warning("Complet: company logo has incompatible dimensions");
		return true;
	}

	for (int i = 1; i <= kLogoFadeSteps; i++) {
		presentBuffer(logo.getPixels(), i * 255 / kLogoFadeSteps);
		if (!delay(kLogoFadeDelay))
			return false;
	}

	if (!delay(kLogoHoldMs))
		return false;

	for (int i = kLogoFadeSteps - 1; i >= 0; i--) {
		presentBuffer(logo.getPixels(), i * 255 / kLogoFadeSteps);
		if (!delay(kLogoFadeDelay))
			return false;
	}

	present();
	return true;
}

bool CompletEngine::fadeTo(const Common::Array<byte> &target) {
	if (_replaying) {
		_scene = target;
		return true;
	}

	const Common::Array<byte> old = _scene;
	Common::Array<byte> blended(old.size());
	const byte *next = target.data();

	bool ok = true;
	for (int i = 0; i < kFadeSteps && ok; i++) {
		int newWeight = i + 1;
		int oldWeight = 30 - i > 0 ? 30 - i : 0;
		for (uint k = 0; k < blended.size(); k++) {
			int v = (next[k] * newWeight + old[k] * oldWeight) >> 5;
			blended[k] = v > 255 ? 255 : v;
		}
		presentBuffer(blended.data());
		ok = delay(kFadeDelay);
	}

	memcpy(_scene.data(), next, _scene.size());
	return ok;
}

// A missing graphic falls back to NULL.GD, like the original
bool CompletEngine::loadImage(const Common::String &name, Common::Array<byte> &out) {
	debugC(1, kDebugGraphics, "Loading graphic '%s'", name.c_str());
	GdImage image;
	if (!image.loadFromName(name + ".gd") && !image.loadFromName("null.gd")) {
		warning("Complet: cannot load graphic '%s'", name.c_str());
		return false;
	}

	if (image.getWidth() != _screenWidth || image.getHeight() != _screenHeight) {
		warning("Complet: graphic '%s' has incompatible dimensions", name.c_str());
		return false;
	}
	out.resize(_screenWidth * _screenHeight * 3);
	memcpy(out.data(), image.getPixels(), out.size());
	return true;
}

void CompletEngine::overlaySprite(Common::Array<byte> &dst, const Common::Array<byte> &sprite) {
	const byte transparentGreen = _blackKeySprites ? 0 : 0xff;
	for (uint i = 0; i + 2 < dst.size(); i += 3) {
		if (sprite[i] != 0 || sprite[i + 1] != transparentGreen || sprite[i + 2] != 0)
			memcpy(&dst[i], &sprite[i], 3);
	}
}

// %g,name: replaces the whole screen, keeping the background layer untouched
bool CompletEngine::showFullImage(const Common::String &name, bool fade) {
	recordSeenCg(name);
	Common::Array<byte> image;
	if (!loadImage(name, image))
		return true;

	bool ok = true;
	if (fade)
		ok = fadeTo(image);
	else
		_scene = image;
	_sceneFull = name;

	present();
	return ok;
}

// %gb,name: only stages the next background, it appears with the following %gf
void CompletEngine::stageBackground(const Common::String &name) {
	if (name.equalsIgnoreCase(_bgName))
		return;

	if (loadImage(name, _bg)) {
		_bgName = name;
		_bgPending = true;
	}
}

// %gf,sprite[,sprite]: draws sprites over the staged background; "null" means none
bool CompletEngine::showForeground(const Common::String &args) {
	Common::Array<byte> target = _bg;
	composeSprites(target, args);
	_sceneFull.clear();
	_sceneBg = _bgName;
	_sceneFg = args;

	bool ok = true;
	if (_bgPending) {
		_bgPending = false;
		ok = fadeTo(target);
	} else {
		_scene = target;
	}

	present();
	return ok;
}

void CompletEngine::composeSprites(Common::Array<byte> &dst, const Common::String &args) {
	Common::String names[2] = {args, "null"};
	const char *comma = strchr(args.c_str(), ',');
	if (comma) {
		names[0] = Common::String(args.c_str(), comma);
		names[1] = comma + 1;
	}

	for (int i = 0; i < 2; i++) {
		Common::Array<byte> sprite;
		if (!names[i].equalsIgnoreCase("null") && loadImage(names[i], sprite))
			overlaySprite(dst, sprite);
	}
}

void CompletEngine::clearMessage() {
	_messageLines.clear();
	_messageSpeaker.clear();
	_messageVisible = false;
	_messageIndent = 0;
	_readingSpeaker = false;
}

int CompletEngine::messageLineChars() const {
	return _fullScreenMessages ? 21 : kMessageLineChars;
}

int CompletEngine::messageMaxLines() const {
	if (_fullScreenMessages)
		return 18;
	if (_framedMessages)
		return 5;
	return kMessageMaxLines;
}

void CompletEngine::newMessageLine() {
	if (_messageLines.size() >= (uint)messageMaxLines()) {
		present();
		if (!waitClick())
			return;
		_messageLines.clear();
		_messageIndent = 0;
		_readingSpeaker = false;
	}
	_messageLines.push_back(Common::U32String());
	if (_fullScreenMessages || _framedMessages) {
		for (uint column = 0; column < _messageIndent; column++)
			_messageLines.back() += 0x3000;
	}
}

void CompletEngine::printChar(uint32 ch) {
	bool capturedSpeaker = false;
	if (_nameFrameEnabled && ch == 0x3010) {
		_messageSpeaker.clear();
		_readingSpeaker = true;
		capturedSpeaker = true;
	} else if (_nameFrameEnabled && ch == 0x3011 && _readingSpeaker) {
		_readingSpeaker = false;
		capturedSpeaker = true;
	} else if (_nameFrameEnabled && _readingSpeaker) {
		_messageSpeaker += ch;
		capturedSpeaker = true;
	}

	if (!capturedSpeaker) {
		if (_messageLines.empty())
			_messageLines.push_back(Common::U32String());

		// Closing punctuation may overflow the line
		bool hangs = ch == 0x3001 || ch == 0x3002 || ch == 0x300d;
		if (_messageLines.back().size() >= (uint)messageLineChars() && !hangs) {
			newMessageLine();
			if (interrupted())
				return;
		}

		if ((_fullScreenMessages || _framedMessages) && ch == '_')
			ch = 0x3000;
		_messageLines.back() += ch;
		if (_fullScreenMessages || _framedMessages) {
			if (ch == 0x3010) {
				_readingSpeaker = true;
				_messageIndent = 0;
			} else if (ch == 0x3011 && _readingSpeaker) {
				_readingSpeaker = false;
				_messageIndent = MIN(_messageLines.back().size(), (uint)messageLineChars() - 1);
			}
		}
	}
	_messageVisible = true;
	addBacklogChar(ch);
	_autoChars++;
	if (!_animateText || _fastText || _skipMode || _replaying)
		return;
	present();
	waitTyping(kMessageCharDelay);
}

void CompletEngine::addBacklogChar(uint32 ch) {
	// Like the original, a line break is inserted before the 24th character after an explicit break
	if (++_backlogLineChars >= (isXexEngine() ? 22 : kBacklogLineChars))
		addBacklogBreak();
	_backlog.push_back(ch);

	// Drop the oldest pages, keeping a page marker at the start
	if (_backlog.size() > kBacklogMaxSize) {
		uint cut = kBacklogMaxSize / 2;
		while (cut < _backlog.size() && _backlog[cut] != kBacklogPage)
			cut++;
		if (cut < _backlog.size())
			_backlog.erase(_backlog.begin(), _backlog.begin() + cut);
	}
}

void CompletEngine::addBacklogBreak() {
	_backlogLineChars = 0;
	if (++_backlogLines >= (isXexEngine() ? 13 : kBacklogPageLines)) {
		_backlogLines = 0;
		_backlog.push_back(kBacklogPage);
	} else {
		_backlog.push_back(kBacklogLine);
	}
}

void CompletEngine::resetBacklog() {
	_backlog.clear();
	_backlog.push_back(kBacklogPage);
	_backlogLineChars = 0;
	_backlogLines = 0;
}

// Mouse wheel or Page Up/Down browse the message history one page at a time
void CompletEngine::showBacklog() {
	if (_backlog.size() < 2)
		return;

	Common::Array<uint> pages;
	for (uint i = 0; i < _backlog.size(); i++) {
		if (_backlog[i] == kBacklogPage)
			pages.push_back(i);
	}
	const int pageCount = pages.size();

	_showingBacklog = true;
	int back = 1;
	drawBacklogPage(pages[pageCount - back], back, pageCount);

	Common::Event event;
	while (back > 0 && !interrupted()) {
		int previous = back;
		while (back > 0 && g_system->getEventManager()->pollEvent(event)) {
			if (handleSkipAction(event))
				continue;
			const bool action = event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START;
			// The wheel stops at the oldest page, Page Up wraps around like the original
			if (event.type == Common::EVENT_WHEELUP) {
				if (back < pageCount)
					back++;
			} else if (event.type == Common::EVENT_WHEELDOWN) {
				back--;
			} else if (action && event.customType == kActionBacklog) {
				back = back < pageCount ? back + 1 : 1;
			} else if (action && event.customType == kActionBacklogForward) {
				back--;
			}
		}

		refreshCursor();
		if (back > 0 && back != previous)
			drawBacklogPage(pages[pageCount - back], back, pageCount);
		g_system->delayMillis(10);
	}

	_showingBacklog = false;
	present();
}

void CompletEngine::drawBacklogPage(uint start, int pageNumber, int pageCount) {
	Graphics::Surface *screen = g_system->lockScreen();

	screen->fillRect(Common::Rect(scaleX(10), scaleY(10), _screenWidth - scaleX(10), _screenHeight - scaleY(10)), _format.RGBToColor(0, 0, 0));
	screen->fillRect(Common::Rect(scaleX(11), scaleY(11), _screenWidth - scaleX(11), _screenHeight - scaleY(11)), _format.RGBToColor(0x10, 0x30, 0x10));

	// Page number as full-width digits followed by a full-width "p"
	if (pageCount < 100) {
		const uint32 color = _format.RGBToColor(0x7f, 0xff, 0xff);
		_font->drawChar(screen, 0xff50, _screenWidth - scaleX(42), _screenHeight - scaleY(46), color);
		_font->drawChar(screen, 0xff10 + pageNumber % 10, _screenWidth - scaleX(70), _screenHeight - scaleY(44), color);
		if (pageNumber > 9)
			_font->drawChar(screen, 0xff10 + pageNumber / 10, _screenWidth - scaleX(102), _screenHeight - scaleY(44), color);
	}

	// Full-width angle brackets and double angle quotes switch colours, as in the original
	const uint32 colors[3] = {
		_format.RGBToColor(0xff, 0xff, 0xe8),
		_format.RGBToColor(0xff, 0x9e, 0xc2),
		_format.RGBToColor(0x9e, 0xff, 0xc2)
	};
	int color = 0;
	for (uint i = 0; i <= start; i++) {
		if (_backlog[i] == 0xff1e)
			color = 1;
		else if (_backlog[i] == 0x226a)
			color = 2;
		else if (_backlog[i] == 0xff1c || _backlog[i] == 0x226b)
			color = 0;
	}
	int x = 0, y = scaleY(24);
	for (uint i = start + 1; i < _backlog.size() && _backlog[i] != kBacklogPage; i++) {
		const uint32 ch = _backlog[i];
		if (ch == kBacklogLine) {
			x = 0;
			y += scaleY(32);
			continue;
		}

		if (ch == 0xff1e)
			color = 1;
		else if (ch == 0x226a)
			color = 2;
		_font->drawChar(screen, ch, x + scaleX(27), y, colors[color]);
		if (ch == 0xff1c || ch == 0x226b)
			color = 0;

		x += scaleX(kBacklogAdvance);
		if (x > _screenWidth - scaleX(35)) {
			x = 0;
			y += scaleY(32);
		}
	}

	g_system->unlockScreen();
	g_system->updateScreen();
}

void CompletEngine::runText(Script &script, const byte *text, uint32 textSize) {
	clearMessage();
	_fastText = false;
	_inText = true;

	const byte *p = text;
	const byte *end = text + textSize;
	while (p < end && *p && !shouldQuit() && script.isRunning()) {
		_textPos = p - script.getData();
		if (_replaying && _textPos >= _replayEnd) {
			_replaying = false;
			present();
		}

		if (*p == '%') {
			p = runCommand(script, p, end);
			continue;
		}

		int length = ((*p >= 0x81 && *p <= 0x9f) || (*p >= 0xe0 && *p <= 0xfc)) && p + 1 < end && p[1] ? 2 : 1;
		Common::U32String ch((const char *)p, length, Common::kWindows932);
		if (ch.size())
			printChar(ch[0]);
		p += length;
	}
	_inText = false;
	_replaying = false;
	present();
}

// Handles one "%X[,arg;]" command and returns the position after it
const byte *CompletEngine::runCommand(Script &script, const byte *p, const byte *end) {
	if (p + 1 >= end)
		return end;
	if (!p[1])
		return p + 1;

	const char command = p[1] | 0x20;

	// The argument starts after "%X," ("%gX," for the graphic variants)
	char variant = 0;
	const byte *q = p + 2;
	if (command == 'g') {
		if (q >= end || !*q)
			return q;
		variant = *q++ | 0x20;
		if (variant != ',') {
			if (q >= end || !*q)
				return q;
			q++;
		}
	}

	Common::String args;
	bool hasArgs = Common::String("efgjmosvw").contains(command);
	if (hasArgs) {
		if (command != 'g') {
			if (q >= end || !*q)
				return q;
			q++;
		}
		while (q < end && *q && *q != ';')
			args += *q++;
		if (q < end && *q == ';')
			q++;
	} else if (command == 't' && _gallery->groupCount && q < end && *q >= '0' && *q <= '9') {
		args += *q++;
		if (q < end && *q == ';')
			q++;
	}
	debugC(1, kDebugScript, "%%%c%c %s", command, variant ? variant : ' ', args.c_str());

	switch (command) {
	case 'e':
		if (_replaying)
			break;
		if (atoi(args.c_str()) == 1 && (_case266Map || _mhtMap))
			runMap();
		else if (atoi(args.c_str()) == 10)
			showLogo();
		else if (atoi(args.c_str()) == 11)
			runGallery();
		else if (atoi(args.c_str()) != 99)
			warning("Complet: unimplemented effect %%e,%s", args.c_str());
		break;
	case 'g': {
		bool restore = _messageVisible;
		_messageVisible = false;
		switch (variant) {
		case 'b':
			stageBackground(args);
			break;
		case 'f':
			showForeground(args);
			break;
		case 'n':
			showFullImage(args, false);
			break;
		default:
			// %g, and %gc differ only in the transition shape
			showFullImage(args, true);
			break;
		}
		_messageVisible = restore;
		present();
		break;
	}
	case 'o':
		showFullImage(args, false);
		break;
	case 'i':
		script.resetVariables();
		break;
	case 'j':
		script.queueScenario(args);
		break;
	case 'k':
		addBacklogBreak();
		waitClick();
		clearMessage();
		present();
		break;
	case 'r':
		addBacklogBreak();
		// When skipping, the original clears the window instead of waiting
		if (_skipMode)
			clearMessage();
		else {
			waitClick();
			if (_fullScreenMessages && !interrupted())
				newMessageLine();
		}
		break;
	case 'n':
		addBacklogBreak();
		newMessageLine();
		break;
	case 'q':
		script.stop();
		break;
	case 't':
		if (!_replaying && !args.empty())
			runGroupedGallery(atoi(args.c_str()));
		break;
	case 'w':
		present();
		waitTimed(atoi(args.c_str()) * _waitUnitMillis);
		break;
	case 'y':
		addBacklogBreak();
		clearMessage();
		present();
		break;
	case 'f':
		_audio.stopMusic();
		break;
	case 'm':
		// A leading '@' plays the track once instead of looping it
		if (args.hasPrefix("@"))
			_audio.playMusic(args.substr(1), false);
		else
			_audio.playMusic(args, true);
		break;
	case 'v':
		// Skipping silences voices
		if (_skipMode)
			_audio.stopVoice();
		// For voices the '@' prefix means the opposite: loop
		else if (args.hasPrefix("@"))
			_audio.playVoice(args.substr(1), true);
		else
			_audio.playVoice(args, false);
		break;
	case 's':
		setPersistentFlag(atoi(args.c_str()));
		break;
	case 'l':
		// The original loads the slot used last; the ScummVM load dialog lets the player pick
		if (!_replaying) {
			_inputWait = true;
			loadGameDialog();
			_inputWait = false;
		}
		break;
	default:
		break;
	}

	return q;
}

} // End of namespace Complet
