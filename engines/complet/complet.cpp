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
	kFontSize = 30,
	kLogoFadeSteps = 32,
	kLogoFadeDelay = 8,
	kLogoHoldMs = 4000,
	kFadeSteps = 32,
	kFadeDelay = 8,

	// Message window geometry (client coordinates)
	kMessageLeft = 26,
	kMessageTop = 484,
	kMessageLineHeight = 24,
	kMessageMaxLines = 4,
	kMessageLineChars = 23,
	kMessageCharAdvance = 28,
	kMessageCharDelay = 16,
	// The original's 16 ms timer moves the indicator every 8 ticks on NT
	kIndicatorFrameDelay = 128,
	kAutoCharDelay = kIndicatorFrameDelay,
	kMessageBoxX = 16,
	kMessageBoxY = 475,
	kMessageBoxWidth = 768,
	kMessageBoxHeight = 113,

	// Selection window
	kMenuRowHeight = 32,
	kMenuCharAdvance = 32,
	kMenuRadius = 16,

	// Glyph sheet inside the frame image (GDS\MESFRAME.GD)
	kGlyphSourceRow = 472,

	// LOG, Q-SAVE and Q-LOAD buttons above the message window
	kButtonTop = 0x1ce,
	kButtonBottom = 0x1db,
	kLogButtonLeft = 0x2a2,
	kLogButtonRight = 0x2bb,
	kSaveButtonLeft = 0x2bd,
	kSaveButtonRight = 0x2e1,
	kLoadButtonLeft = 0x2e3,
	kLoadButtonRight = 0x308,

	// Message history
	kBacklogLineChars = 24,
	kBacklogPageLines = 16,
	kBacklogMaxSize = 0x2000,
	kBacklogAdvance = 32
};

static const uint32 kBacklogLine = 0xfffffffe;
static const uint32 kBacklogPage = 0xffffffff;

static const char *const kFontFiles[] = {
	"NotoSansCJK-Regular.ttc",
	"NotoSansCJKjp-Regular.otf",
	"NotoSerifCJK-Regular.ttc"
};

CompletEngine::CompletEngine(OSystem *system) : Engine(system), _audio(_mixer), _script(this) {
}

CompletEngine::~CompletEngine() {
	g_system->getEventManager()->getEventDispatcher()->unregisterObserver(this);
	if (_ownFont)
		delete _font;
}

bool CompletEngine::hasFeature(EngineFeature f) const {
	return f == kSupportsReturnToLauncher || f == kSupportsLoadingDuringRuntime || f == kSupportsSavingDuringRuntime ||
		f == kSupportsChangingOptionsDuringRuntime;
}

void CompletEngine::applyGameSettings() {
	_autoMode = ConfMan.getBool("auto_mode");
	_animateText = ConfMan.getBool("text_animation");
	_audio.setUseMidi(ConfMan.getBool("midi_music"));
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
	// The original uses a fixed 800x600 window with 24bpp BGR DIBs
	_format = Graphics::PixelFormat::createFormatBGRA32();
	initGraphics(kScreenWidth, kScreenHeight, &_format);

	setDebugger(new Console(this));
	ConfMan.registerDefault("auto_mode", false);
	ConfMan.registerDefault("text_animation", false);
	ConfMan.registerDefault("midi_music", false);
	syncSoundSettings();
	applyGameSettings();

	if (!initGame())
		return Common::Error(Common::kReadingFailed, "Missing game data (gds/mpx folders)");

	loadFont();
	initCursor();
#ifndef USE_MAD
	warning("Complet: built without MP3 support, voices and digital music are silent");
#endif

	_scene.resize(kScreenWidth * kScreenHeight * 3);
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

bool CompletEngine::initGame() {
	const Common::FSNode gameDataDir(ConfMan.getPath("path"));
	// The detector already added mpx/, where the detection file lives
	SearchMan.addSubDirectoryMatching(gameDataDir, "gds");
	SearchMan.addSubDirectoryMatching(gameDataDir, "vmd");

	return loadFrameImage();
}

void CompletEngine::loadFont() {
	for (uint i = 0; i < ARRAYSIZE(kFontFiles) && !_font; i++) {
		Common::SeekableReadStream *stream = SearchMan.createReadStreamForMember(kFontFiles[i]);
		if (stream)
			_font = Graphics::loadTTFFont(stream, DisposeAfterUse::YES, kFontSize, Graphics::kTTFSizeModeCell);
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
	_xexLayout = loaded && _frameImage.isGd2();
	return loaded;
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
		for (int i = 0; i < kCgCount; i++)
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
	for (int i = 0; i < kCgCount; i++) {
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
	for (int y = 0; y < kScreenHeight; y++) {
		const byte *row = bgr + (kScreenHeight - 1 - y) * kScreenWidth * 3;
		uint32 *dst = (uint32 *)screen->getBasePtr(0, y);
		for (int x = 0; x < kScreenWidth; x++, row += 3)
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
	for (int y = 0; y < kScreenHeight; y++) {
		const byte *row = bgr + (kScreenHeight - 1 - y) * kScreenWidth * 3;
		uint32 *dst = (uint32 *)screen->getBasePtr(0, y);
		for (int x = 0; x < kScreenWidth; x++, row += 3)
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
		if (_xexLayout) {
			const int nativeX = 0x40 + _indicatorFrame * 0x1a;
			const int sourceX = nativeX * kScreenWidth / 640;
			const int sourceRight = (nativeX + 0x1a) * kScreenWidth / 640;
			const int sourceY = _autoMode ? 0x0c : 0;
			const int destX = 0x244 * kScreenWidth / 640;
			const int destY = (0x1e0 - 0x22) * kScreenHeight / 480;
			drawFrameRect(screen, sourceX, sourceY, sourceRight - sourceX, 0x0c * kScreenHeight / 480,
					destX, destY, true);
		} else {
			// Animated "CLICK" (or "AUTO") marker from the top left of the C4 frame image
			drawFrameRect(screen, (_indicatorFrame + (_autoMode ? 8 : 0)) * 32, 0, 32, 32,
					0x2e7, kScreenHeight - 0x34, true);
		}
	}
	if (_menu)
		drawMenu(screen);
}

// Copies a rectangle of the frame image, given in top-down coordinates; optionally black is transparent
void CompletEngine::drawFrameRect(Graphics::Surface &screen, int sx, int sy, int w, int h, int dx, int dy, bool blackKey) {
	const byte *frame = _frameImage.getPixels();
	for (int y = 0; y < h; y++) {
		const byte *src = frame + ((kScreenHeight - 1 - (sy + y)) * kScreenWidth + sx) * 3;
		uint32 *dst = (uint32 *)screen.getBasePtr(dx, dy + y);
		for (int x = 0; x < w; x++, src += 3, dst++) {
			if (!blackKey || src[0] || src[1] || src[2])
				*dst = _format.RGBToColor(src[2], src[1], src[0]);
		}
	}
}

CompletEngine::MessageButton CompletEngine::messageButtonAt(const Common::Point &pos) const {
	if (!_messageVisible || _messageHidden || _menu || pos.y < kButtonTop || pos.y >= kButtonBottom)
		return kButtonNone;
	if (pos.x >= kLoadButtonLeft && pos.x < kLoadButtonRight)
		return kButtonLoad;
	if (pos.x >= kSaveButtonLeft && pos.x < kSaveButtonRight)
		return kButtonSave;
	if (pos.x >= kLogButtonLeft && pos.x < kLogButtonRight)
		return kButtonLog;
	return kButtonNone;
}

void CompletEngine::drawText(Graphics::Surface &screen, const Common::U32String &text, int x, int y, uint32 color, int advance) {
	for (uint i = 0; i < text.size(); i++) {
		_font->drawChar(&screen, text[i], x, y, color);
		x += text[i] < 0x100 ? advance / 2 : advance;
	}
}

void CompletEngine::drawMessage(Graphics::Surface &screen) {
	if (!_messageVisible)
		return;

	// Dim the area behind the text
	for (int y = kMessageBoxY; y < kMessageBoxY + kMessageBoxHeight; y++) {
		uint32 *p = (uint32 *)screen.getBasePtr(kMessageBoxX, y);
		for (int x = 0; x < kMessageBoxWidth; x++, p++) {
			byte r, g, b;
			_format.colorToRGB(*p, r, g, b);
			*p = _format.RGBToColor(r >> 2, g >> 2, b >> 1);
		}
	}

	const uint32 shadow = _format.RGBToColor(0x3f, 0x3f, 0x7f);
	const uint32 white = _format.RGBToColor(0xff, 0xff, 0xff);
	for (uint i = 0; i < _messageLines.size(); i++) {
		int y = kMessageTop + i * kMessageLineHeight;
		drawText(screen, _messageLines[i], kMessageLeft + 1, y + 1, shadow, kMessageCharAdvance);
		drawText(screen, _messageLines[i], kMessageLeft, y, white, kMessageCharAdvance);
	}

	// Q-SAVE/Q-LOAD and LOG buttons from the top right of the frame image
	if (!_menu && !_xexLayout) {
		drawFrameRect(screen, 0x200, 0, 0x4d, 0xf, kSaveButtonLeft - 1, kButtonTop - 1);
		drawFrameRect(screen, 0x200, 0x10, 0x1b, 0xf, kLogButtonLeft - 1, kButtonTop - 1);
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
	layout.left = 0x170 - 7 * longest;
	layout.width = 14 * longest + 0x30;
	layout.top = 0x11c - 16 * layout.rows;
	layout.height = kMenuRowHeight * layout.rows + 6;
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

	const uint32 fill = _format.RGBToColor(0x00, 0x00, 0x30);
	const uint32 border = _format.RGBToColor(0xb0, 0xc0, 0x00);
	for (int y = 0; y < layout.height; y++) {
		int sy = layout.top + y;
		if (sy < 0 || sy >= screen.h)
			continue;
		for (int x = 0; x < layout.width; x++) {
			int sx = layout.left + x;
			if (sx < 0 || sx >= screen.w || !insideRoundRect(x, y, layout.width, layout.height, kMenuRadius))
				continue;
			bool inner = insideRoundRect(x - 2, y - 2, layout.width - 4, layout.height - 4, kMenuRadius - 2);
			*(uint32 *)screen.getBasePtr(sx, sy) = inner ? fill : border;
		}
	}

	const uint32 idle = _format.RGBToColor(0x40, 0x40, 0xff);
	const uint32 shadow = _format.RGBToColor(0x3f, 0x3f, 0x7f);
	const uint32 white = _format.RGBToColor(0xff, 0xff, 0xff);
	for (int i = 0; i < layout.rows; i++) {
		Common::U32String text((*_menu)[i].text, Common::kWindows932);
		int inkLeft = INT_MAX;
		int inkRight = INT_MIN;
		for (uint j = 0; j < text.size(); j++) {
			const Common::Rect bounds = _font->getBoundingBox(text[j]);
			inkLeft = MIN(inkLeft, (int)j * kMenuCharAdvance + bounds.left);
			inkRight = MAX(inkRight, (int)j * kMenuCharAdvance + bounds.right);
		}
		int x = layout.left + layout.width / 2 - (inkLeft + inkRight) / 2;
		int y = layout.top + 2 + i * kMenuRowHeight;

		if (i == _hover) {
			drawText(screen, text, x + 1, y + 1, shadow, kMenuCharAdvance);
			drawText(screen, text, x, y, white, kMenuCharAdvance);
		} else {
			drawText(screen, text, x, y, idle, kMenuCharAdvance);
		}
	}

	// Q-SAVE and Q-LOAD below the bottom right of the window
	if (!_xexLayout)
		drawFrameRect(screen, 0x200, 1, 0x4d, 0xe, layout.left + layout.width - 0x5d, layout.top + layout.rows * kMenuRowHeight + 2);
}

int CompletEngine::selectMenu(const Common::Array<MenuItem> &items, bool nested) {
	_menu = &items;
	_hover = -1;
	_messageVisible = false;
	const MenuLayout layout = getMenuLayout();

	auto rowAt = [&](const Common::Point &pos) {
		if (pos.x < layout.left || pos.x >= layout.left + layout.width)
			return -1;
		int row = (pos.y - layout.top) / kMenuRowHeight;
		if (pos.y < layout.top || row >= layout.rows)
			return -1;
		return row;
	};

	_hover = rowAt(g_system->getEventManager()->getMousePos());
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
		int x = pos.x - layout.left - layout.width;
		int y = pos.y - layout.top - layout.rows * kMenuRowHeight;
		if (y < 2 || y >= 0x10)
			return kButtonNone;
		if (x > -0x5d && x < -0x39)
			return kButtonSave;
		if (x >= -0x38 && x < -0x12)
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
		uint32 periods = (g_system->getMillis() - start) / kIndicatorFrameDelay;
		int frame = periods ? periods % 8 : -1;
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
		int oldWeight = 0x1e - i > 0 ? 0x1e - i : 0;
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

	out.resize(kScreenWidth * kScreenHeight * 3);
	memcpy(out.data(), image.getPixels(), out.size());
	return true;
}

void CompletEngine::overlaySprite(Common::Array<byte> &dst, const Common::Array<byte> &sprite) {
	const byte transparentGreen = _xexLayout ? 0 : 0xff;
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
	_messageVisible = false;
}

void CompletEngine::printChar(uint32 ch) {
	if (_messageLines.empty())
		_messageLines.push_back(Common::U32String());

	// Closing punctuation may overflow the line
	bool hangs = ch == 0x3001 || ch == 0x3002 || ch == 0x300d;
	if (_messageLines.back().size() >= kMessageLineChars && !hangs) {
		if (_messageLines.size() >= kMessageMaxLines) {
			present();
			if (!waitClick())
				return;
			_messageLines.clear();
		}
		_messageLines.push_back(Common::U32String());
	}

	_messageLines.back() += ch;
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
	if (++_backlogLineChars >= kBacklogLineChars)
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
	if (++_backlogLines >= kBacklogPageLines) {
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

	present();
}

void CompletEngine::drawBacklogPage(uint start, int pageNumber, int pageCount) {
	Graphics::Surface *screen = g_system->lockScreen();

	screen->fillRect(Common::Rect(10, 10, 790, 590), _format.RGBToColor(0, 0, 0));
	screen->fillRect(Common::Rect(11, 11, 789, 589), _format.RGBToColor(0x10, 0x30, 0x10));

	// Page number as full-width digits followed by a full-width "p"
	if (pageCount < 100) {
		const uint32 color = _format.RGBToColor(0x7f, 0xff, 0xff);
		_font->drawChar(screen, 0xff50, 0x2f6, 0x22a, color);
		_font->drawChar(screen, 0xff10 + pageNumber % 10, 0x2da, 0x22c, color);
		if (pageNumber > 9)
			_font->drawChar(screen, 0xff10 + pageNumber / 10, 0x2ba, 0x22c, color);
	}

	// Full-width angle brackets and double angle quotes switch colours, as in the original
	const uint32 colors[3] = {
		_format.RGBToColor(0xff, 0xff, 0xe8),
		_format.RGBToColor(0xff, 0x9e, 0xc2),
		_format.RGBToColor(0x9e, 0xff, 0xc2)
	};
	int color = 0;
	int x = 0, y = 0x18;
	for (uint i = start + 1; i < _backlog.size() && _backlog[i] != kBacklogPage; i++) {
		const uint32 ch = _backlog[i];
		if (ch == kBacklogLine) {
			x = 0;
			y += 0x20;
			continue;
		}

		if (ch == 0xff1e)
			color = 1;
		else if (ch == 0x226a)
			color = 2;
		_font->drawChar(screen, ch, x + 0x1b, y, colors[color]);
		if (ch == 0xff1c || ch == 0x226b)
			color = 0;

		x += kBacklogAdvance;
		if (x > 0x2fd) {
			x = 0;
			y += 0x20;
		}
	}

	g_system->unlockScreen();
	g_system->updateScreen();
}

void CompletEngine::runText(Script &script, const byte *text) {
	clearMessage();
	_fastText = false;
	_inText = true;

	const byte *p = text;
	while (*p && !shouldQuit() && script.isRunning()) {
		_textPos = p - script.getData();
		if (_replaying && _textPos >= _replayEnd) {
			_replaying = false;
			present();
		}

		if (*p == '%') {
			p = runCommand(script, p);
			continue;
		}

		int length = ((*p >= 0x81 && *p <= 0x9f) || (*p >= 0xe0 && *p <= 0xfc)) && p[1] ? 2 : 1;
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
const byte *CompletEngine::runCommand(Script &script, const byte *p) {
	const char command = p[1] | 0x20;
	if (!command)
		return p + 1;

	// The argument starts after "%X," ("%gX," for the graphic variants)
	int argStart = 3;
	char variant = 0;
	if (command == 'g') {
		variant = p[2] | 0x20;
		argStart = variant == ',' ? 3 : 4;
	}

	const byte *q = p + 2;
	Common::String args;
	bool hasArgs = Common::String("efgjmosvw").contains(command);
	if (hasArgs) {
		q = p + argStart;
		while (*q && *q != ';')
			args += *q++;
		if (*q == ';')
			q++;
	}
	debugC(1, kDebugScript, "%%%c%c %s", command, variant ? variant : ' ', args.c_str());

	switch (command) {
	case 'e':
		if (_replaying)
			break;
		if (atoi(args.c_str()) == 10)
			showLogo();
		else if (atoi(args.c_str()) == 11)
			runGallery();
		else
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
		else
			waitClick();
		break;
	case 'n':
		addBacklogBreak();
		if (_messageLines.size() >= kMessageMaxLines) {
			waitClick();
			_messageLines.clear();
		}
		_messageLines.push_back(Common::U32String());
		break;
	case 'q':
		script.stop();
		break;
	case 'w':
		present();
		waitTimed(atoi(args.c_str()) * 100);
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
