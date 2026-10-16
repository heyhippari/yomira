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
#include "common/archive.h"
#include "common/events.h"
#include "common/file.h"
#include "common/config-manager.h"
#include "common/memstream.h"
#include "common/translation.h"
#include "common/substream.h"
#include "common/system.h"
#include "engines/util.h"
#include "engines/dialogs.h"
#include "gui/chooser.h"
#include "gui/message.h"
#include "graphics/cursorman.h"
#include "graphics/fonts/ttf.h"

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
	delete _messageFont;
	_messageBackground.free();
	for (uint frame = 0; frame < ARRAYSIZE(_messageFrameColor); ++frame) {
		_messageFrameColor[frame].free();
		_messageFrameMask[frame].free();
	}
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
	if (ConfMan.hasKey("save_slot") && ConfMan.getInt("save_slot") >= 0)
		return loadGameState(ConfMan.getInt("save_slot"));
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
	if (!findIafRecord(file, filename, offset, size, &progress)) {
		if (shouldQuit())
			return Common::kUserCanceled;
		Common::File eventBank;
		Graphics::Surface color, mask;
		Common::Point position;
		if (eventBank.open("CG/EV.CGF") &&
			readSpriteBankImage(eventBank, name, color, mask, _screen->format, &progress, &position)) {
			image.copyFrom(_screen->rawSurface());
			const int width = color.w;
			const int height = color.h;
			const bool composited = compositeMasked(image, color, mask, &progress, position);
			color.free();
			mask.free();
			if (!composited)
				return Common::Error(shouldQuit() ? Common::kUserCanceled : Common::kReadingFailed, "Invalid event sprite: " + name);
			debug(1, "Delta event sprite: %s (%dx%d at %d,%d)", name.c_str(), width, height, position.x, position.y);
			return Common::kNoError;
		}
		color.free();
		mask.free();
		if (name.size() != 8 || name[0] != 'E' || name[1] < '0' || name[1] > '9' ||
			name[2] < '0' || name[2] > '9' || name.substr(6) == "00")
			return Common::Error(Common::kNoGameDataFoundError, filename);
		const Common::Error result = loadImage(name.substr(0, 6) + "00", image);
		if (result.getCode() != Common::kNoError)
			return result;
		Common::StringArray layers;
		if (name[6] != '0')
			layers.push_back(name.substr(0, 4) + "00" + name.substr(6, 1) + "0");
		if (name[7] != '0')
			layers.push_back(name.substr(0, 3) + "A000" + name.substr(7, 1));
		const Common::Error compositing = compositeLayers(image, layers, "CG/EV.CGF");
		if (compositing.getCode() != Common::kNoError) {
			image.free();
			return compositing;
		}
		debug(1, "Delta event image: %s (%u layers)", name.c_str(), layers.size());
		return Common::kNoError;
	}
	Common::SeekableSubReadStream member(&file, offset, offset + size);
	if (!readIafImage(member, image, _screen->format, &progress))
		return Common::Error(shouldQuit() ? Common::kUserCanceled : Common::kReadingFailed, filename);
	debug(1, "Delta bank image: %s (%dx%d)", filename.c_str(), image.w, image.h);
	return Common::kNoError;
}

Common::Error DeltaEngine::compositeForeground(Graphics::Surface &image, const Common::String &name) {
	Common::Functor0Mem<bool, DeltaEngine> progress(this, &DeltaEngine::serviceInput);
	Common::File metadata;
	if (!metadata.open("CG/EVINFO.BIN"))
		return Common::Error(Common::kNoGameDataFoundError, "CG/EVINFO.BIN");
	Common::StringArray layers;
	if (!readSpriteInfo(metadata, name, layers, &progress)) {
		if (name.size() == 8 && name[0] == 'E' && name[1] >= '0' && name[1] <= '9' &&
			name[2] >= '0' && name[2] <= '9')
			layers.push_back(name);
		else
			layers.push_back(name + "_D");
		const Common::Error result = compositeLayers(image, layers, "CG/EV.CGF");
		if (result.getCode() == Common::kNoError)
			debug(1, "Delta foreground: %s (direct layer)", name.c_str());
		return result;
	}
	const Common::Error result = compositeLayers(image, layers, "CG/ST.CGF");
	if (result.getCode() == Common::kNoError)
		debug(1, "Delta foreground: %s (%u layers)", name.c_str(), layers.size());
	return result;
}

Common::Error DeltaEngine::compositeLayers(Graphics::Surface &image, const Common::StringArray &layers, const Common::Path &bankName) {
	Common::Functor0Mem<bool, DeltaEngine> progress(this, &DeltaEngine::serviceInput);
	for (uint index = 0; index < layers.size(); ++index) {
		const Common::Path layerBank = layers[index].hasSuffix("_D") ? Common::Path("CG/EV.CGF") : bankName;
		Common::File bank;
		if (!bank.open(layerBank))
			return Common::Error(Common::kNoGameDataFoundError, layerBank.toString());
		Graphics::Surface color, mask;
		Common::Point position;
		const bool valid = readSpriteBankImage(bank, layers[index], color, mask, image.format, &progress, &position) &&
			compositeMasked(image, color, mask, &progress, position);
		color.free();
		mask.free();
		if (!valid)
			return Common::Error(shouldQuit() ? Common::kUserCanceled : Common::kReadingFailed, "Invalid foreground layer: " + layers[index]);
	}
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
	if (_titleState.empty()) {
		Common::MemoryWriteStreamDynamic snapshot(DisposeAfterUse::YES);
		if (saveGameStream(&snapshot).getCode() == Common::kNoError) {
			_titleState.resize(snapshot.size());
			memcpy(_titleState.begin(), snapshot.getData(), snapshot.size());
		}
	}
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

Common::Error DeltaEngine::presentMessage() {
	if (!_messageFont) {
		const char *const names[] = { "NotoSansCJK-Regular.ttc", "NotoSansCJKjp-Regular.otf", "NotoSerifCJK-Regular.ttc" };
		for (uint index = 0; index < ARRAYSIZE(names) && !_messageFont; ++index) {
			Common::SeekableReadStream *stream = SearchMan.createReadStreamForMember(names[index]);
			if (stream)
				_messageFont = Graphics::loadTTFFont(stream, DisposeAfterUse::YES, 24, Graphics::kTTFSizeModeCharacter);
		}
		if (!_messageFont)
			return Common::Error(Common::kNoGameDataFoundError, "A Japanese Noto CJK font is required in the game or extra path");
	}
	Common::U32String text;
	bool boundary = false;
	while (_messageTokenIndex < _messageTokens.size()) {
		const Common::String &token = _messageTokens[_messageTokenIndex++];
		if (token.empty())
			continue;
		const char prefix = token[0];
		if (prefix == 'N' || prefix == 'n') {
			_speaker = atoi(token.c_str() + 1);
		} else if (prefix == 'M' || prefix == 'm' || prefix == '@') {
			if (!text.empty()) {
				boundary = true;
				break;
			}
		} else if (prefix != 'V' && prefix != 'v' && prefix != 'W' && prefix != 'w') {
			Common::U32String line = token.decode(Common::kWindows932);
			bool blank = true;
			for (uint index = 0; index < line.size(); ++index)
				blank = blank && (line[index] == ' ' || line[index] == '\t' || line[index] == 0x3000);
			if (!blank) {
				if (!text.empty())
					text += '\n';
				text += line;
			}
		}
	}
	if (text.empty())
		return Common::kNoError;
	const Common::Error frameResult = loadMessageFrame(_speaker > 0 ? 1 : 0);
	if (frameResult.getCode() != Common::kNoError)
		return frameResult;
	if (!_messageVisible)
		_messageBackground.copyFrom(_screen->rawSurface());
	_messageVisible = true;
	_messageLines.clear();
	_messageLineIndex = 0;
	const int width = _configuration.messageRect.right - _configuration.textPosition.x - 8;
	if (width <= 0)
		return Common::Error(Common::kReadingFailed, "Invalid message text width");
	_messageFont->wordWrapText(text, width, _messageLines);
	_messageBoundary = boundary;
	drawMessagePage();
	debug(1, "Delta message page: %u lines, speaker %d, script offset 0x%x", _messageLines.size(), _speaker, _scriptPosition);
	return Common::kNoError;
}

Common::Error DeltaEngine::loadMessageFrame(uint frame) {
	if (_messageFrameColor[frame].getPixels())
		return Common::kNoError;
	Common::Functor0Mem<bool, DeltaEngine> progress(this, &DeltaEngine::serviceInput);
	Common::File bank;
	if (!bank.open("CG/ST.CGF"))
		return Common::Error(Common::kNoGameDataFoundError, "CG/ST.CGF");
	const Common::String name = Common::String::format("WAKU0%u", frame);
	const Graphics::PixelFormat format(3, 8, 8, 8, 0, 16, 8, 0, 0);
	Graphics::Surface color;
	if (!readSpriteBankImage(bank, name, color, _messageFrameMask[frame], format, &progress, &_messageFramePosition[frame])) {
		color.free();
		_messageFrameMask[frame].free();
		return Common::Error(shouldQuit() ? Common::kUserCanceled : Common::kReadingFailed, "Invalid message frame: " + name);
	}
	Graphics::Surface *converted = color.convertTo(_screen->format);
	color.free();
	if (!converted) {
		_messageFrameMask[frame].free();
		return Common::Error(Common::kReadingFailed, "Cannot convert message frame: " + name);
	}
	_messageFrameColor[frame].copyFrom(*converted);
	converted->free();
	delete converted;
	debug(1, "Delta message frame: %s (%dx%d at %d,%d)", name.c_str(),
		_messageFrameColor[frame].w, _messageFrameColor[frame].h,
		_messageFramePosition[frame].x, _messageFramePosition[frame].y);
	return Common::kNoError;
}

void DeltaEngine::drawMessagePage() {
	_screen->blitFrom(_messageBackground);
	const uint frame = _speaker > 0 ? 1 : 0;
	const Common::Point position(_configuration.messageRect.left + _messageFramePosition[frame].x,
		_configuration.messageRect.top + _messageFramePosition[frame].y);
	compositeMasked(*_screen->surfacePtr(), _messageFrameColor[frame], _messageFrameMask[frame], nullptr, position, 255);
	const int left = _configuration.textPosition.x;
	const int right = _configuration.messageRect.right - 8;
	int top = _configuration.textPosition.y;
	const uint32 foreground = _screen->format.RGBToColor(255, 255, 255);
	const uint32 shadow = _screen->format.RGBToColor(0, 0, 0);
	Common::HashMap<Common::String, Common::StringArray>::const_iterator names = _configuration.stringTables.find("//NameStr");
	if (names != _configuration.stringTables.end() && _speaker > 0 && _speaker < (int)names->_value.size())
		drawShadowedString(*_messageFont, *_screen, names->_value[_speaker].decode(Common::kWindows932),
			_configuration.messageRect.left + 8, _configuration.messageRect.top + 6, _configuration.messageRect.width() - 16,
			foreground, shadow);
	const int lineHeight = MAX(1, _messageFont->getFontHeight());
	while (_messageLineIndex < _messageLines.size() && top + lineHeight <= _configuration.messageRect.bottom) {
		drawShadowedString(*_messageFont, *_screen, _messageLines[_messageLineIndex++], left, top, right - left,
			foreground, shadow);
		top += lineHeight;
	}
	_messageWaiting = _messageBoundary || _messageLineIndex < _messageLines.size();
}

void DeltaEngine::advanceMessage() {
	if (_messageLineIndex < _messageLines.size()) {
		drawMessagePage();
	} else {
		_messageWaiting = false;
	}
}

Common::Error DeltaEngine::advanceScript() {
	if (_loadPending)
		return applyPendingLoad();
	if (_atMenu || _messageWaiting || updateTransition())
		return Common::kNoError;
	if (_messageTokenIndex < _messageTokens.size()) {
		const Common::Error result = presentMessage();
		if (result.getCode() != Common::kNoError || _messageWaiting)
			return result;
	}
	if (_waiting) {
		if ((int32)(_system->getMillis() - _waitUntil) < 0)
			return Common::kNoError;
		_waiting = false;
	}
	for (uint count = 0; count < 64 && !shouldQuit(); ++count) {
		ScriptCommand command;
		if (!_script->next(command)) {
			warning("Delta script stopped: %s", _script->error().c_str());
			if (!_titleState.empty()) {
				GUI::MessageDialog dialog(Common::U32String::format(_("This scenario command is not implemented yet: %s"), _script->error().c_str()));
				_uiBusy = true;
				runDialog(dialog);
				_uiBusy = false;
				_pendingState = _titleState;
				_loadPending = true;
				return Common::kNoError;
			}
			return Common::Error(Common::kReadingFailed, _script->error());
		}
		_scriptPosition = _script->position();
		debug(2, "Delta command 0x%x at 0x%x", command.opcode, command.offset);
		if (command.opcode == 0x0d) {
			if (_messageVisible)
				_screen->blitFrom(_messageBackground);
			_messageVisible = false;
			_messageWaiting = false;
			_messageTokens.clear();
			_messageLines.clear();
			_messageTokenIndex = 0;
		} else if (command.opcode == 0x4b) {
			bool text = false;
			for (uint index = 0; index < command.strings.size(); ++index) {
				if (!command.strings[index].empty() && (byte)command.strings[index][0] >= 0x80)
					text = true;
			}
			if (text) {
				_messageTokens = command.strings;
				_messageTokenIndex = 0;
				const Common::Error result = presentMessage();
				if (result.getCode() != Common::kNoError || _messageWaiting)
					return result;
			}
		} else if (command.opcode == 0x54) {
			_messageWaiting = _messageVisible;
			if (_messageWaiting)
				return Common::kNoError;
		} else if (command.opcode == 0x7c) {
			_menuConfig = command;
		} else if (command.opcode == 0x67) {
			_menuResultVariable = command.arguments[0];
			return enterMenu();
		} else if (command.opcode == 0x99) {
			if (command.arguments[0]) {
				_waitUntil = _system->getMillis() + command.arguments[1] * 100;
			} else {
				_waiting = true;
				return Common::kNoError;
			}
		} else if (command.opcode == 0x23 || command.opcode == 0x26 || command.opcode == 0xb0 || command.opcode == 0xb1) {
			Graphics::Surface image;
			const Common::Error result = loadImage(command.strings[0], image);
			if (result.getCode() != Common::kNoError)
				return result;
			if (image.w != _screen->w || image.h != _screen->h) {
				image.free();
				return Common::Error(Common::kReadingFailed, "Unsupported partial background image");
			}
			if (command.opcode == 0xb1 && !command.strings[1].equalsIgnoreCase("TACLEAR")) {
				const Common::Error compositing = compositeForeground(image, command.strings[1]);
				if (compositing.getCode() != Common::kNoError) {
					image.free();
					return compositing;
				}
			}
			startTransition(image, command.opcode == 0x23 ? 0 : 300);
			_messageVisible = false;
			image.free();
			if (_transitionDuration)
				return Common::kNoError;
		} else if (command.opcode == 0xa5 || command.opcode == 0xae || command.opcode == 0x19 || command.opcode == 0xa4) {
			Graphics::Surface black;
			uint32 duration = 0;
			if (command.opcode == 0x19 || command.opcode == 0xa4) {
				const int left = command.arguments[0];
				const int top = command.arguments[1];
				const int width = command.arguments[2];
				const int height = command.arguments[3];
				if (width <= 0 || height <= 0 || left + width > _screen->w || top + height > _screen->h)
					return Common::Error(Common::kReadingFailed, "Invalid scene-clear rectangle");
				black.copyFrom(_screen->rawSurface());
				black.fillRect(Common::Rect(left, top, left + width, top + height),
					command.opcode == 0xa4 ? _screen->format.RGBToColor(255, 255, 255) : 0);
				duration = command.arguments[4] * 20;
			} else {
				black.create(_screen->w, _screen->h, _screen->format);
				black.fillRect(Common::Rect(black.w, black.h), 0);
				duration = command.opcode == 0xae ? command.arguments[0] * 20 : 0;
			}
			startTransition(black, duration);
			black.free();
			if (_transitionDuration)
				return Common::kNoError;
		}
	}
	return Common::kNoError;
}

int DeltaEngine::menuItemAt(const Common::Point &point) const {
	if (!_atMenu || _menuConfig.arguments.size() != 7)
		return -1;
	const int left = _menuConfig.arguments[2];
	const int top = _menuConfig.arguments[3];
	const int width = _menuConfig.arguments[4];
	const int height = _menuConfig.arguments[5];
	const int step = _menuConfig.arguments[6];
	for (int index = 0; index < _menuConfig.arguments[1]; ++index) {
		if (Common::Rect(left, top + index * step, left + width, top + index * step + height).contains(point))
			return index;
	}
	return -1;
}

void DeltaEngine::showExtras() {
	Common::U32StringArray choices;
	choices.push_back(_("CG gallery"));
	choices.push_back(_("Scene replay"));
	choices.push_back(_("Music room"));
	GUI::ChooserDialog chooser(_("Extras"));
	chooser.setList(choices);
	const int selection = runDialog(chooser);
	if (selection >= 0 && selection < (int)choices.size()) {
		GUI::MessageDialog notice(_("This extra mode is not implemented yet."));
		runDialog(notice);
	}
}

void DeltaEngine::activateMenuItem(int item) {
	if (!_atMenu || _uiBusy || item < 0 || item >= _menuConfig.arguments[1])
		return;
	_menuPressed = -1;
	_uiBusy = true;
	debug(1, "Delta menu selection: %d", item + 1);
	switch (item) {
	case 0:
		if (_script->setVariable(_menuResultVariable, 1))
			_atMenu = false;
		break;
	case 1:
		loadGameDialog();
		break;
	case 2: {
		GUI::ConfigDialog dialog;
		runDialog(dialog);
		syncSoundSettings();
		break;
	}
	case 3: {
		GUI::MessageDialog dialog(_("Quit this game?"), _("Quit"), _("Cancel"));
		if (runDialog(dialog) == GUI::kMessageOK)
			quitGame();
		break;
	}
	case 4:
		if (getGameId() == "onegaitasukete")
			break;
		showExtras();
		break;
	case 5:
		showExtras();
		break;
	default:
		break;
	}
	_uiBusy = false;
	CursorMan.showMouse(_input.mouseActive());
	if (_atMenu)
		drawMenu();
}

void DeltaEngine::handleInput(const Common::Event &event) {
	if (_input.processEvent(event)) {
		CursorMan.showMouse(_input.mouseActive());
		debug(2, "Delta pointer: %s", _input.mouseActive() ? "visible (mouse)" : "hidden (keyboard/controller)");
	}
	if (_uiBusy || _loadPending)
		return;
	if (_messageWaiting && (event.type == Common::EVENT_LBUTTONUP ||
		(event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START && event.customType == kActionSelect))) {
		advanceMessage();
		return;
	}
	if (_atMenu && event.type == Common::EVENT_LBUTTONDOWN) {
		_menuPressed = menuItemAt(event.mouse);
		return;
	}
	if (_atMenu && event.type == Common::EVENT_LBUTTONUP) {
		const int pressed = _menuPressed;
		_menuPressed = -1;
		if (pressed == menuItemAt(event.mouse))
			activateMenuItem(pressed);
		return;
	}
	if (event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START) {
		if (_atMenu && event.customType == kActionSelect) {
			activateMenuItem(_menuHover < 0 ? 0 : _menuHover);
		} else if (_atMenu && event.customType == kActionCancel) {
			_menuPressed = -1;
			_menuHover = -1;
			drawMenu();
		} else if (event.customType == kActionMenu) {
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
		_menuHover = menuItemAt(event.mouse);
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
			if (frameResult.getCode() != Common::kNoError) {
				warning("Delta execution stopped at 0x%x: %s", _scriptPosition, frameResult.getDesc().c_str());
				return shouldQuit() ? Common::kNoError : frameResult;
			}
			_screen->update();
			nextFrame = _system->getMillis() + 16;
		}
		_system->delayMillis(4);
	}
	return Common::kNoError;
}

} // End of namespace Delta
