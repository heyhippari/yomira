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

#include "delta/delta.h"
#include "delta/detection.h"
#include "delta/console.h"
#include "common/debug.h"
#include "common/events.h"
#include "common/file.h"
#include "common/substream.h"
#include "common/system.h"
#include "engines/util.h"
#include "graphics/cursorman.h"

namespace Delta {

DeltaEngine::DeltaEngine(OSystem *syst, const ADGameDescription *gameDesc) : Engine(syst),
	_gameDescription(gameDesc), _randomSource("Delta") {
}

DeltaEngine::~DeltaEngine() {
	if (_cursorInstalled) {
		CursorMan.popCursor();
		CursorMan.popCursorPalette();
	}
	delete _script;
	_transitionFrom.free();
	_transitionTo.free();
	_transitionFrame.free();
	_menuBackground.free();
	_menuButtons.free();
	delete _screen;
}

uint32 DeltaEngine::getFeatures() const {
	return _gameDescription->flags;
}

Common::String DeltaEngine::getGameId() const {
	return _gameDescription->gameId;
}

Common::Error DeltaEngine::initialize() {
	const Graphics::PixelFormat format(2, 5, 6, 5, 0, 11, 5, 0, 0);
	initGraphics(800, 600, &format);
	if (_system->getScreenFormat().bytesPerPixel == 1)
		return Common::kUnsupportedColorMode;
	_screen = new Graphics::Screen();
	_screen->clear();
	_screen->update();
	CursorMan.setDefaultArrowCursor(true);
	CursorMan.showMouse(true);
	_cursorInstalled = true;
	_input.setMousePosition(_system->getEventManager()->getMousePos());
	syncSoundSettings();

	Common::Functor0Mem<bool, DeltaEngine> progress(this, &DeltaEngine::serviceInput);
	Common::File file;
	if (!file.open("ms.mhu"))
		return Common::Error(Common::kNoGameDataFoundError, "Missing ms.mhu");
	if (!readStartupConfig(file, _configuration, &progress))
		return Common::Error(shouldQuit() ? Common::kUserCanceled : Common::kReadingFailed, "Invalid Delta startup configuration");
	file.close();

	if (!file.open(Common::Path(_configuration.scenarioFile, '\\')))
		return Common::Error(Common::kNoGameDataFoundError, _configuration.scenarioFile);
	if (!readScenarioData(file, _scenario, &progress))
		return Common::Error(shouldQuit() ? Common::kUserCanceled : Common::kReadingFailed, _configuration.scenarioFile);
	_scriptPosition = 0;
	_script = new Script(_scenario, getGameId() == "onegaitasukete", &_randomSource);

	debug(1, "Delta display: %dx%d, %u bytes per pixel", _screen->w, _screen->h, _screen->format.bytesPerPixel);
	debug(1, "Delta initialized: scenario number %u, prefix %s, scenario %s (%u bytes), %u string tables, script offset %u",
		_configuration.scenarioNumber, _configuration.resourcePrefix.c_str(),
		_configuration.scenarioFile.c_str(), _scenario.size(),
		_configuration.stringTables.size(), _scriptPosition);
	return Common::kNoError;
}

Common::Error DeltaEngine::loadImage(const Common::String &name, Graphics::Surface &image) {
	Common::Functor0Mem<bool, DeltaEngine> progress(this, &DeltaEngine::serviceInput);
	if (!serviceInput())
		return Common::kUserCanceled;
	Common::String filename = name + ".IAF";
	Common::File file;
	if (file.open(Common::Path(filename)) || file.open(Common::Path("CG/" + filename))) {
		if (readIafImage(file, image, _screen->format, &progress)) {
			debug(1, "Delta image: %s (%dx%d)", filename.c_str(), image.w, image.h);
			return Common::kNoError;
		}
		return Common::Error(shouldQuit() ? Common::kUserCanceled : Common::kReadingFailed, filename);
	}
	if (!file.open("CG/EVB.CGF"))
		return Common::Error(Common::kNoGameDataFoundError, filename);
	uint32 offset = 0;
	uint32 size = 0;
	if (!findIafRecord(file, filename, offset, size, &progress))
		return Common::Error(shouldQuit() ? Common::kUserCanceled : Common::kNoGameDataFoundError, filename);
	Common::SeekableSubReadStream member(&file, offset, offset + size);
	if (!readIafImage(member, image, _screen->format, &progress))
		return Common::Error(shouldQuit() ? Common::kUserCanceled : Common::kReadingFailed, filename);
	debug(1, "Delta bank image: %s (%dx%d)", filename.c_str(), image.w, image.h);
	return Common::kNoError;
}

void DeltaEngine::startTransition(const Graphics::Surface &image, uint32 duration) {
	_transitionFrom.copyFrom(_screen->rawSurface());
	_transitionTo.copyFrom(image);
	_transitionFrame.copyFrom(image);
	_transitionStart = _system->getMillis();
	_transitionDuration = duration;
	if (!duration)
		_screen->blitFrom(image);
}

bool DeltaEngine::updateTransition() {
	if (!_transitionDuration)
		return false;
	const uint32 elapsed = _system->getMillis() - _transitionStart;
	if (elapsed >= _transitionDuration) {
		_screen->blitFrom(_transitionTo);
		_transitionDuration = 0;
		return false;
	}
	const uint32 weight = elapsed * 256 / _transitionDuration;
	const bool rgb565 = _screen->format == Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0);
	for (int row = 0; row < _transitionFrame.h; ++row) {
		if ((row & 15) == 0 && !pollInput())
			return false;
		if (rgb565) {
			const uint16 *source = static_cast<const uint16 *>(_transitionFrom.getBasePtr(0, row));
			const uint16 *target = static_cast<const uint16 *>(_transitionTo.getBasePtr(0, row));
			uint16 *destination = static_cast<uint16 *>(_transitionFrame.getBasePtr(0, row));
			for (int column = 0; column < _transitionFrame.w; ++column) {
				const uint32 previous = source[column];
				const uint32 next = target[column];
				const uint32 red = ((previous >> 11) * (256 - weight) + (next >> 11) * weight) >> 8;
				const uint32 green = (((previous >> 5) & 63) * (256 - weight) + ((next >> 5) & 63) * weight) >> 8;
				const uint32 blue = ((previous & 31) * (256 - weight) + (next & 31) * weight) >> 8;
				destination[column] = (red << 11) | (green << 5) | blue;
			}
			continue;
		}
		for (int column = 0; column < _transitionFrame.w; ++column) {
			byte oldRed, oldGreen, oldBlue, newRed, newGreen, newBlue;
			_screen->format.colorToRGB(_transitionFrom.getPixel(column, row), oldRed, oldGreen, oldBlue);
			_screen->format.colorToRGB(_transitionTo.getPixel(column, row), newRed, newGreen, newBlue);
			_transitionFrame.setPixel(column, row, _screen->format.RGBToColor(
				(oldRed * (256 - weight) + newRed * weight) / 256,
				(oldGreen * (256 - weight) + newGreen * weight) / 256,
				(oldBlue * (256 - weight) + newBlue * weight) / 256));
		}
	}
	_screen->blitFrom(_transitionFrame);
	return true;
}

Common::Error DeltaEngine::enterMenu() {
	if (_menuConfig.strings.size() != 2 || _menuConfig.arguments.size() != 7)
		return Common::Error(Common::kReadingFailed, "Missing title-menu configuration");
	Common::Error result = loadImage(_menuConfig.strings[0], _menuBackground);
	if (result.getCode() != Common::kNoError)
		return result;
	result = loadImage(_menuConfig.strings[1], _menuButtons);
	if (result.getCode() != Common::kNoError)
		return result;
	const int count = _menuConfig.arguments[1];
	const int width = _menuConfig.arguments[4];
	const int height = _menuConfig.arguments[5];
	if (count < 1 || count > 16 || width <= 0 || height <= 0 ||
		width > _menuButtons.w || count * height > _menuButtons.h)
		return Common::Error(Common::kReadingFailed, "Invalid title-menu button geometry");
	_atMenu = true;
	CursorMan.showMouse(_input.mouseActive());
	drawMenu();
	_screen->update();
	debug(1, "Delta title menu reached: %s at script offset 0x%x, %d buttons", getGameId().c_str(), _scriptPosition, count);
	return Common::kNoError;
}

void DeltaEngine::drawMenu() {
	_screen->blitFrom(_menuBackground);
	const int count = _menuConfig.arguments[1];
	const int left = _menuConfig.arguments[2];
	const int top = _menuConfig.arguments[3];
	const int width = _menuConfig.arguments[4];
	const int height = _menuConfig.arguments[5];
	const int step = _menuConfig.arguments[6];
	for (int index = 0; index < count; ++index) {
		const int sourceTop = (index == _menuHover && count * height * 2 <= _menuButtons.h ? count + index : index) * height;
		_screen->blitFrom(_menuButtons, Common::Rect(0, sourceTop,
			width, sourceTop + height), Common::Point(left, top + index * step));
	}
}

Common::Error DeltaEngine::advanceScript() {
	if (_atMenu || updateTransition())
		return Common::kNoError;
	if (_waiting) {
		if ((int32)(_system->getMillis() - _waitUntil) < 0)
			return Common::kNoError;
		_waiting = false;
	}
	for (uint count = 0; count < 64 && !shouldQuit(); ++count) {
		ScriptCommand command;
		if (!_script->next(command)) {
			warning("Delta script stopped: %s", _script->error().c_str());
			return Common::Error(Common::kReadingFailed, _script->error());
		}
		_scriptPosition = _script->position();
		debug(2, "Delta command 0x%x at 0x%x", command.opcode, command.offset);
		if (command.opcode == 0x7c) {
			_menuConfig = command;
		} else if (command.opcode == 0x67) {
			return enterMenu();
		} else if (command.opcode == 0x99) {
			if (command.arguments[0]) {
				_waitUntil = _system->getMillis() + command.arguments[1] * 100;
			} else {
				_waiting = true;
				return Common::kNoError;
			}
		} else if (command.opcode == 0x23 || command.opcode == 0x26 || command.opcode == 0xb0) {
			Graphics::Surface image;
			const Common::Error result = loadImage(command.strings[0], image);
			if (result.getCode() != Common::kNoError)
				return result;
			if (image.w != _screen->w || image.h != _screen->h) {
				image.free();
				return Common::Error(Common::kReadingFailed, "Unsupported partial background image");
			}
			startTransition(image, command.opcode == 0x23 ? 0 : 300);
			image.free();
			if (_transitionDuration)
				return Common::kNoError;
		} else if (command.opcode == 0xa5 || command.opcode == 0xae) {
			Graphics::Surface black;
			black.create(_screen->w, _screen->h, _screen->format);
			black.fillRect(Common::Rect(black.w, black.h), 0);
			startTransition(black, command.opcode == 0xae ? command.arguments[0] * 20 : 0);
			black.free();
			if (_transitionDuration)
				return Common::kNoError;
		}
	}
	return Common::kNoError;
}

void DeltaEngine::handleInput(const Common::Event &event) {
	if (_input.processEvent(event)) {
		CursorMan.showMouse(_input.mouseActive());
		debug(2, "Delta pointer: %s", _input.mouseActive() ? "visible (mouse)" : "hidden (keyboard/controller)");
	}
	if (event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START) {
		if (event.customType == kActionMenu) {
			openMainMenuDialog();
			CursorMan.showMouse(_input.mouseActive());
		} else if (_atMenu && (event.customType == kActionUp || event.customType == kActionDown ||
			event.customType == kActionLeft || event.customType == kActionRight)) {
			const int count = _menuConfig.arguments[1];
			const int direction = event.customType == kActionUp || event.customType == kActionLeft ? -1 : 1;
			_menuHover = _menuHover < 0 ? (direction < 0 ? count - 1 : 0) : (_menuHover + direction + count) % count;
			drawMenu();
			debug(2, "Delta menu focus: %d", _menuHover);
		}
	}
	if (_atMenu && event.type == Common::EVENT_MOUSEMOVE && _input.mouseActive()) {
		const int left = _menuConfig.arguments[2];
		const int top = _menuConfig.arguments[3];
		const int width = _menuConfig.arguments[4];
		const int height = _menuConfig.arguments[5];
		const int step = _menuConfig.arguments[6];
		_menuHover = -1;
		for (int index = 0; index < _menuConfig.arguments[1]; ++index) {
			if (Common::Rect(left, top + index * step, left + width, top + index * step + height).contains(event.mouse))
				_menuHover = index;
		}
		drawMenu();
	}
}

bool DeltaEngine::pollInput() {
	Common::Event event;
	for (uint count = 0; count < 64 && _system->getEventManager()->pollEvent(event); ++count) {
		handleInput(event);
	}
	return !shouldQuit();
}

bool DeltaEngine::serviceInput() {
	const uint32 now = _system->getMillis();
	if ((int32)(now - _nextInputUpdate) < 0)
		return !shouldQuit();
	_nextInputUpdate = now + 4;
	if (!pollInput())
		return false;
	if ((int32)(now - _nextPresent) >= 0) {
		_system->updateScreen();
		const uint32 end = _system->getMillis();
		// A moved cursor makes the present block (~one vsync); budget loader work after it returns.
		_nextPresent = end + 16;
	}
	return !shouldQuit();
}

Common::Error DeltaEngine::run() {
	setDebugger(new Console());
	const Common::Error result = initialize();
	if (result.getCode() != Common::kNoError)
		return result;

	uint32 nextFrame = _system->getMillis();
	while (!shouldQuit()) {
		if (!serviceInput())
			break;
		if ((int32)(_system->getMillis() - nextFrame) >= 0) {
			const Common::Error frameResult = advanceScript();
			if (frameResult.getCode() != Common::kNoError)
				return shouldQuit() ? Common::kNoError : frameResult;
			_screen->update();
			nextFrame = _system->getMillis() + 16;
		}
		_system->delayMillis(4);
	}
	return Common::kNoError;
}

} // End of namespace Delta
