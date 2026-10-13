#include "common/config-manager.h"
#include "common/events.h"
#include "common/file.h"
#include "common/fs.h"
#include "common/savefile.h"
#include "common/system.h"
#include "engines/util.h"
#include "graphics/fontman.h"
#include "graphics/fonts/ttf.h"
#include "graphics/surface.h"

#include "complet/complet.h"

namespace Complet {

enum {
	kFontSize = 28,
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
	if (_ownFont)
		delete _font;
}

bool CompletEngine::hasFeature(EngineFeature f) const {
	return f == kSupportsReturnToLauncher;
}

Common::Error CompletEngine::run() {
	// The original uses a fixed 800x600 window with 24bpp BGR DIBs
	_format = Graphics::PixelFormat::createFormatBGRA32();
	initGraphics(kScreenWidth, kScreenHeight, &_format);

	if (!initGame())
		return Common::Error(Common::kReadingFailed, "Missing game data (gds/mpx folders)");

	loadFont();

	_scene.resize(kScreenWidth * kScreenHeight * 3);
	memset(_scene.data(), 0, _scene.size());
	_bg = _scene;
	_backlog.push_back(kBacklogPage);
	loadCommonData();
	present();

	if (!_script.start("startup"))
		return Common::Error(Common::kReadingFailed, "Cannot load the startup scenario");

	while (!shouldQuit() && _script.isRunning())
		_script.step();

	return Common::kNoError;
}

bool CompletEngine::initGame() {
	const Common::FSNode gameDataDir(ConfMan.getPath("path"));
	SearchMan.addSubDirectoryMatching(gameDataDir, "gds");
	SearchMan.addSubDirectoryMatching(gameDataDir, "mpx");
	SearchMan.addSubDirectoryMatching(gameDataDir, "vmd");

	return loadFrameImage();
}

void CompletEngine::loadFont() {
	for (uint i = 0; i < ARRAYSIZE(kFontFiles) && !_font; i++) {
		Common::SeekableReadStream *stream = SearchMan.createReadStreamForMember(kFontFiles[i]);
		if (stream)
			_font = Graphics::loadTTFFont(stream, DisposeAfterUse::YES, kFontSize);
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
	return _frameImage.loadFromName("mesframe.gd") || _frameImage.loadFromName("null.gd");
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

	Common::InSaveFile *file = getSaveFileManager()->openForLoading("common");
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

	Common::OutSaveFile *file = getSaveFileManager()->openForSaving("common", false);
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
}

void CompletEngine::present() {
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
}

void CompletEngine::drawOverlays(Graphics::Surface &screen) {
	if (!_messageHidden)
		drawMessage(screen);
	if (_menu)
		drawMenu(screen);
}

// Copies a rectangle of the frame image, given in top-down coordinates
void CompletEngine::drawFrameRect(Graphics::Surface &screen, int sx, int sy, int w, int h, int dx, int dy) {
	const byte *frame = _frameImage.getPixels();
	for (int y = 0; y < h; y++) {
		const byte *src = frame + ((kScreenHeight - 1 - (sy + y)) * kScreenWidth + sx) * 3;
		uint32 *dst = (uint32 *)screen.getBasePtr(dx, dy + y);
		for (int x = 0; x < w; x++, src += 3)
			*dst++ = _format.RGBToColor(src[2], src[1], src[0]);
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
	if (!_menu) {
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
		int x = layout.left + layout.width / 2 - (int)text.size() * kMenuCharAdvance / 2;
		int y = layout.top + 2 + i * kMenuRowHeight;

		if (i == _hover) {
			drawText(screen, text, x + 1, y + 1, shadow, kMenuCharAdvance);
			drawText(screen, text, x, y, white, kMenuCharAdvance);
		} else {
			drawText(screen, text, x, y, idle, kMenuCharAdvance);
		}
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

	int result = -3;
	Common::Event event;
	while (result == -3 && !shouldQuit()) {
		int previous = _hover;
		while (result == -3 && g_system->getEventManager()->pollEvent(event)) {
			if (handleSkipKey(event))
				continue;

			switch (event.type) {
			case Common::EVENT_MOUSEMOVE:
				_hover = rowAt(event.mouse);
				break;
			case Common::EVENT_LBUTTONUP:
				if (!hidden && rowAt(event.mouse) >= 0)
					result = rowAt(event.mouse);
				break;
			case Common::EVENT_RBUTTONUP:
				result = cancel();
				break;
			case Common::EVENT_WHEELUP:
				backlog();
				break;
			case Common::EVENT_KEYDOWN:
				if (event.kbd.keycode == Common::KEYCODE_ESCAPE)
					result = cancel();
				else if (event.kbd.keycode == Common::KEYCODE_PAGEUP)
					backlog();
				else if (hidden)
					break;
				else if (event.kbd.keycode == Common::KEYCODE_UP)
					_hover = _hover <= 0 ? layout.rows - 1 : _hover - 1;
				else if (event.kbd.keycode == Common::KEYCODE_DOWN)
					_hover = (_hover + 1) % layout.rows;
				else if ((event.kbd.keycode == Common::KEYCODE_RETURN || event.kbd.keycode == Common::KEYCODE_KP_ENTER ||
						event.kbd.keycode == Common::KEYCODE_SPACE) && _hover >= 0)
					result = _hover;
				break;
			default:
				break;
			}
		}

		if (_hover != previous && !hidden)
			present();
		g_system->delayMillis(10);
	}

	_menu = nullptr;
	_hover = -1;
	present();
	return shouldQuit() ? -1 : result;
}

// Ctrl skips while held; returns true if the event was a Ctrl press or release
bool CompletEngine::handleSkipKey(const Common::Event &event) {
	if (event.type != Common::EVENT_KEYDOWN && event.type != Common::EVENT_KEYUP)
		return false;
	if (event.kbd.keycode != Common::KEYCODE_LCTRL && event.kbd.keycode != Common::KEYCODE_RCTRL)
		return false;

	if (!event.kbdRepeat)
		_skipMode = event.type == Common::EVENT_KEYDOWN;
	return true;
}

bool CompletEngine::delay(uint32 ms) {
	uint32 end = g_system->getMillis() + ms;
	Common::Event event;

	while (g_system->getMillis() < end) {
		while (g_system->getEventManager()->pollEvent(event))
			handleSkipKey(event);
		if (shouldQuit())
			return false;
		g_system->delayMillis(5);
	}
	return !shouldQuit();
}

// Handles the input available while text is shown; returns true when the player asks to go on
bool CompletEngine::pollTextInput() {
	bool advance = false;
	Common::Event event;

	while (g_system->getEventManager()->pollEvent(event)) {
		if (handleSkipKey(event)) {
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
			default:
				// Quick save and load need the save system
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
		case Common::EVENT_KEYDOWN:
			if (event.kbdRepeat)
				break;
			switch (event.kbd.keycode) {
			case Common::KEYCODE_RETURN:
			case Common::KEYCODE_KP_ENTER:
			case Common::KEYCODE_SPACE:
				advance = true;
				break;
			case Common::KEYCODE_ESCAPE:
				hideMessageWindow();
				break;
			case Common::KEYCODE_PAGEUP:
				showBacklog();
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
	}
	return advance;
}

// Keeps the message window hidden, and the script paused, until the next right click
void CompletEngine::hideMessageWindow() {
	_messageHidden = true;
	present();

	Common::Event event;
	while (_messageHidden && !shouldQuit()) {
		while (_messageHidden && g_system->getEventManager()->pollEvent(event)) {
			if (handleSkipKey(event))
				continue;
			if (event.type == Common::EVENT_RBUTTONUP ||
					(event.type == Common::EVENT_KEYDOWN && event.kbd.keycode == Common::KEYCODE_ESCAPE))
				_messageHidden = false;
			else if (event.type == Common::EVENT_WHEELUP ||
					(event.type == Common::EVENT_KEYDOWN && event.kbd.keycode == Common::KEYCODE_PAGEUP))
				showBacklog();
		}
		g_system->delayMillis(10);
	}

	_messageHidden = false;
	present();
}

// Per-character delay; a click or key shows the rest of the text at once
bool CompletEngine::waitTyping(uint32 ms) {
	uint32 end = g_system->getMillis() + ms;
	while (!shouldQuit() && !_fastText && !_skipMode && g_system->getMillis() < end) {
		if (pollTextInput())
			_fastText = true;
		else
			g_system->delayMillis(5);
	}
	return !shouldQuit();
}

// %w: clicks do not shorten it, but skipping does
bool CompletEngine::waitTimed(uint32 ms) {
	uint32 end = g_system->getMillis() + ms;
	while (!shouldQuit() && !_skipMode && g_system->getMillis() < end) {
		pollTextInput();
		g_system->delayMillis(5);
	}
	return !shouldQuit();
}

bool CompletEngine::waitClick() {
	present();
	while (!shouldQuit()) {
		bool advance = pollTextInput();
		if (advance || _skipMode) {
			_fastText = false;
			return true;
		}
		g_system->delayMillis(10);
	}
	return false;
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
	GdImage image;
	if (!image.loadFromName(name + ".gd") && !image.loadFromName("null.gd")) {
		warning("Complet: cannot load graphic '%s'", name.c_str());
		return false;
	}

	out.resize(kScreenWidth * kScreenHeight * 3);
	memcpy(out.data(), image.getPixels(), out.size());
	return true;
}

// Draws a sprite layer; pure green is the transparent colour
void CompletEngine::overlaySprite(Common::Array<byte> &dst, const Common::Array<byte> &sprite) {
	for (uint i = 0; i + 2 < dst.size(); i += 3) {
		if (sprite[i] != 0 || sprite[i + 1] != 0xff || sprite[i + 2] != 0)
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
	Common::String names[2] = {args, "null"};
	const char *comma = strchr(args.c_str(), ',');
	if (comma) {
		names[0] = Common::String(args.c_str(), comma);
		names[1] = comma + 1;
	}

	Common::Array<byte> target = _bg;
	for (int i = 0; i < 2; i++) {
		Common::Array<byte> sprite;
		if (!names[i].equalsIgnoreCase("null") && loadImage(names[i], sprite))
			overlaySprite(target, sprite);
	}

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
	if (_fastText || _skipMode)
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
	while (back > 0 && !shouldQuit()) {
		int previous = back;
		while (back > 0 && g_system->getEventManager()->pollEvent(event)) {
			if (handleSkipKey(event))
				continue;
			if (event.type == Common::EVENT_WHEELUP) {
				if (back < pageCount)
					back++;
			} else if (event.type == Common::EVENT_WHEELDOWN) {
				back--;
			} else if (event.type == Common::EVENT_KEYDOWN && event.kbd.keycode == Common::KEYCODE_PAGEUP) {
				back = back < pageCount ? back + 1 : 1;
			} else if (event.type == Common::EVENT_KEYDOWN && event.kbd.keycode == Common::KEYCODE_PAGEDOWN) {
				back--;
			}
		}

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

	const byte *p = text;
	while (*p && !shouldQuit() && script.isRunning()) {
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

	switch (command) {
	case 'e':
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
		quitGame();
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
		if (script.setFlag(atoi(args.c_str())))
			saveCommonData();
		break;
	case 'l':
		warning("Complet: load menu is not implemented yet");
		break;
	default:
		break;
	}

	return q;
}

} // End of namespace Complet
