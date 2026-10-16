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

#include "common/debug.h"
#include "common/file.h"
#include "common/endian.h"
#include "common/system.h"
#include "engines/util.h"
#include "graphics/paletteman.h"
#include "graphics/sjis.h"
#include "graphics/font.h"
#include "graphics/fonts/ttf.h"
#include "graphics/cursorman.h"
#include "system98/system98.h"
#include "system98/audio.h"

namespace System98 {

System98Engine::System98Engine(OSystem *system) : Engine(system),
	_sound(nullptr),
	_sjisFont(nullptr), _fallbackFont(nullptr), _mouseBounds(0, 0, 626, 386), _mouse(320, 200),
	_menuJump(0), _menuBank(0), _menuEnabled(false), _coordinateMenu(false), _menuStyle(0), _mouseActionRegister(0), _activeBank(0), _drawPage(0),
	_scriptValid(true), _keyPressed(false), _skipMode(false), _skipRequested(false), _textActionsHeld(0), _textMouseHeld(false), _frameEpoch(0),
	_scriptOffset(0), _stackSize(0), _paletteLock(0), _frameCounter(0), _characterDelay(0),
	_musicMode(0), _displayPage(0), _compareResult(0) {
	memset(_palette, 0, sizeof(_palette));
	memset(_registers, 0, sizeof(_registers));
	memset(_longRegisters, 0, sizeof(_longRegisters));
	memset(_strings, 0, sizeof(_strings));
	memset(_returnStack, 0, sizeof(_returnStack));
	memset(_returnBanks, 0, sizeof(_returnBanks));
	memset(_targetPalette, 0, sizeof(_targetPalette));
	memset(_idleCharacters, 0, sizeof(_idleCharacters));
	memset(_idleDelays, 0, sizeof(_idleDelays));
	memset(_menuRegisters, 0, sizeof(_menuRegisters));
	_registers[0x400] = 3;
}

System98Engine::~System98Engine() {
	delete _sound;
	delete _sjisFont;
	delete _fallbackFont;
	_text.free();
	_screen.free();
	_windowStyles.free();
	for (Graphics::Surface &page : _pages)
		page.free();
}

bool System98Engine::hasFeature(EngineFeature feature) const {
	return feature == kSupportsReturnToLauncher;
}

void System98Engine::syncSoundSettings() {
	Engine::syncSoundSettings();
	if (_sound) _sound->syncSettings();
}

void System98Engine::pauseEngineIntern(bool paused) {
	Engine::pauseEngineIntern(paused);
	if (_sound) _sound->setPaused(paused);
}

Common::Error System98Engine::initialize() {
	if (!_resources.init())
		return Common::Error(Common::kReadingFailed, "Cannot read the four DISK_A through DISK_D CAT/LIB archives.");
	if (!_resources.load("MUGEN.FNT", _font) || _font.size() != 6016)
		return Common::Error(Common::kReadingFailed, "MUGEN.FNT is missing or is not a 6016-byte custom font.");
	if (!_resources.load("SE.MSE", _soundEffects) || _soundEffects.empty() || _soundEffects.size() > 0x800)
		return Common::Error(Common::kReadingFailed, "SE.MSE is missing or exceeds the startup sound-effect buffer.");
	_sound = new Sound();
	if (!_sound->init())
		warning("System98: MIDI audio device could not be initialized");
	if (!loadScene("run.s", 0))
		return Common::Error(Common::kReadingFailed, "run.s is missing or has an invalid scene size.");
	if (READ_LE_UINT16(_scene.data() + _scriptOffset) > 0x97)
		return Common::Error(Common::kReadingFailed, "run.s does not begin with a System-98 instruction.");
	initGraphics(640, 400);
	for (Graphics::Surface &page : _pages) {
		page.create(640, 400, Graphics::PixelFormat::createFormatCLUT8());
		page.fillRect(Common::Rect(640, 400), 0);
	}
	_text.create(640, 400, Graphics::PixelFormat::createFormatCLUT8());
	_text.fillRect(Common::Rect(640, 400), 255);
	_glyphMask.resize(640 * 400);
	memset(_glyphMask.data(), 0, _glyphMask.size());
	_screen.create(640, 400, Graphics::PixelFormat::createFormatCLUT8());
	_sjisFont = Graphics::FontSJIS::createFont(Common::kPlatformPC98);
	if (!_sjisFont) {
		static const char *const fontNames[] = {"NotoSansCJK-Regular.ttc", "NotoSansCJKjp-Regular.otf", "NotoSansJP-Regular.otf"};
		for (const char *name : fontNames) {
			Common::SeekableReadStream *stream = SearchMan.createReadStreamForMember(name);
			if (stream)
				_fallbackFont = Graphics::loadTTFFont(stream, DisposeAfterUse::YES, 16, Graphics::kTTFSizeModeCell);
			if (_fallbackFont)
				break;
		}
	}
	if (!_sjisFont && !_fallbackFont)
		return Common::Error(Common::kNoGameDataFoundError, "Japanese text needs FONT.ROM, FONT.BMP, SJIS.FNT, or a Noto Japanese font in the extra path.");
	CursorMan.showMouse(false);
	_system->getPaletteManager()->setPalette(_palette, 0, 16);
	_system->warpMouse(320, 200);
	_system->copyRectToScreen(_pages[_displayPage].getPixels(), _pages[_displayPage].pitch, 0, 0, 640, 400);
	_system->updateScreen();
	debug(1, "System98: initialized 640x400, 16 colors, font %u bytes, SE.MSE %u bytes, run.s %u bytes, PC=%04x, opcode=%04x, music mode=%u",
	      _font.size(), _soundEffects.size(), _scene.size(), _scriptOffset,
	      READ_LE_UINT16(_scene.data() + _scriptOffset), _musicMode);
	return Common::kNoError;
}

Common::Error System98Engine::run() {
	const Common::Error result = initialize();
	if (result.getCode() != Common::kNoError)
		return result;
	_frameEpoch = _system->getMillis();
	uint instructions = 0;
	while (!shouldQuit() && _scriptValid) {
		pollInput();
		if (!execute())
			break;
		if (++instructions % 128 == 0)
			_system->delayMillis(1);
	}
	if (!_scriptValid)
		return Common::Error(Common::kUnsupportedGameidError, "An unimplemented or invalid System-98 instruction was encountered. See the debug log.");
	return Common::kNoError;
}

} // namespace System98
