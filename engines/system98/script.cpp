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
#include "common/endian.h"
#include "common/savefile.h"
#include "common/system.h"
#include "system98/system98.h"
#include "system98/audio.h"
#include "system98/dialogs.h"
#include "system98/ending.h"
#include "system98/image.h"

namespace System98 {

// The Shift-JIS file name ("name") that NAMCHG.EXE writes
static const char *const kNameFile = "\x96\xbc\x91\x4f";

bool System98Engine::loadScene(const Common::String &name, uint bank) {
	Common::Array<byte> data;
	if (bank > 1 || !_resources.load(name, data) || data.size() < 0x102 || data.size() > kSceneBankSize)
		return false;
	for (uint offset = 0x100; offset < data.size(); ++offset)
		data[offset] ^= 1;
	if (bank) {
		_secondaryScene = data;
		_secondaryName = name;
	} else {
		_scene = data;
		_sceneName = name;
		_scriptOffset = 0x100;
		_activeBank = 0;
	}
	debug(1, "System98: scene %s -> bank %u", name.c_str(), bank);
	return true;
}

// Modes 1 and 2 load PMD.COM (26K) or PMDB2.COM (86) with SE.EFC; mode 3 is GS MIDI
bool System98Engine::selectMusicMode(uint mode) {
	if (mode > 3)
		return false;
	_sound->stop();
	_sound->stopEffect();
	if (mode != 1 && mode != 2)
		_fm->shutdown();
	if (mode == 3)
		return _sound->init();
	if (mode == 0)
		return true;
	Common::Array<byte> driver, effects;
	if (!_resources.load(mode == 2 ? "PMDB2.COM" : "PMD.COM", driver))
		warning("System98: %s is missing; SSG drums and effects are unavailable", mode == 2 ? "PMDB2.COM" : "PMD.COM");
	_resources.load("SE.EFC", effects);
	_fm->stop();
	return _fm->init(mode == 2, driver, effects);
}

Common::String System98Engine::saveFileName(const Common::String &name) const {
	Common::String result = _targetName + ".s98-";
	for (uint index = 0; index < name.size(); ++index)
		result += Common::String::format("%02x", byte(name[index]));
	return result;
}

// A string file the game rewrote takes precedence over the shipped one
bool System98Engine::loadStringFile(const Common::String &name, byte strings[2][80]) {
	Common::Array<byte> data;
	Common::InSaveFile *file = _system->getSavefileManager()->openForLoading(saveFileName(name));
	if (file) {
		const int64 size = file->size();
		data.resize(size > 0 && size <= 0x1000 ? size : 0);
		const bool read = !data.empty() && file->read(data.data(), data.size()) == data.size() && !file->err();
		delete file;
		if (read && decodeStringFile(data, strings))
			return true;
	}
	return _resources.load(name, data) && decodeStringFile(data, strings);
}

// Opcode 0x92 runs NAMCHG.EXE, which rewrites the second line of the name file
void System98Engine::changeName() {
	byte strings[2][80];
	if (!loadStringFile(kNameFile, strings))
		memset(strings, 0, sizeof(strings));
	NameDialog dialog(Common::String(reinterpret_cast<const char *>(strings[1])));
	if (runDialog(dialog) == 1) {
		Common::OutSaveFile *file = _system->getSavefileManager()->openForSaving(saveFileName(kNameFile));
		if (file) {
			file->writeString(reinterpret_cast<const char *>(strings[0]));
			file->writeString("\r\n");
			file->writeString(dialog.getName());
			file->writeString("\r\n\x1a");
			file->finalize();
			if (file->err())
				warning("System98: cannot save the protagonist's name");
			delete file;
		} else {
			warning("System98: cannot save the protagonist's name");
		}
	}
	present();
}

uint16 System98Engine::sceneWord(uint offset) const {
	const Common::Array<byte> &data = sceneBank(sceneBankOf(offset));
	offset = sceneBankOffset(offset);
	return offset + 2 <= data.size() ? READ_LE_UINT16(data.data() + offset) : 0;
}

uint16 System98Engine::fetchWord() {
	_activeBank = sceneBankOf(_scriptOffset);
	const Common::Array<byte> &data = sceneBank(_activeBank);
	if (sceneBankOffset(_scriptOffset) + 2U > data.size()) {
		_scriptValid = false;
		return 0;
	}
	const uint16 value = sceneWord(_scriptOffset);
	_scriptOffset += 2;
	return value;
}

Common::String System98Engine::sceneString(uint offset, bool fileName) const {
	const Common::Array<byte> &data = sceneBank(sceneBankOf(offset));
	offset = sceneBankOffset(offset);
	if (fileName)
		++offset;
	Common::String result;
	while (offset < data.size() && data[offset])
		result += char(data[offset++]);
	return result;
}

uint16 &System98Engine::reg(uint index) {
	if (index >= ARRAYSIZE(_registers)) {
		warning("System98: invalid register %04x", index);
		_scriptValid = false;
		index = 0;
	}
	return _registers[index];
}

void System98Engine::updateMenu(const Common::Event &event) {
	if (!_menuEnabled)
		return;
	uint16 selected = 0;
	for (const MenuItem &item : _menu)
		if (item.rect.contains(_mouse)) {
			selected = item.id;
			break;
		}
	const bool engineAction = event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START;
	const bool accept = engineAction && event.customType == kActionAdvance;
	const bool cancel = engineAction && event.customType == kActionCancel;
	const uint16 action = event.type == Common::EVENT_LBUTTONDOWN || accept ? 2 : event.type == Common::EVENT_RBUTTONDOWN || cancel ? 1 : 0;
	// Any input selects, outside every region too; the ID is a sign-extended byte
	if (!action)
		return;
	if (_coordinateMenu) {
		reg(_menuRegisters[0]) = _mouse.x;
		reg(_menuRegisters[1]) = _mouse.y;
		reg(_mouseActionRegister) = action;
	} else {
		reg(_menuRegisters[0]) = int8(selected);
		reg(_menuRegisters[1]) = action;
	}
	_menuReleaseState = action;
	_activeBank = _menuBank;
	_scriptOffset = _menuJump;
	_menuEnabled = false;
	debug(1, "System98: menu selection %u, action %u -> %04x", selected, action, _menuJump);
}

void System98Engine::pollInput() {
	Common::Event event;
	bool menuChanged = false;
	while (_system->getEventManager()->pollEvent(event)) {
		const bool engineAction = event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START;
		const bool engineActionEnd = event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_END;
		const bool textAction = (engineAction || engineActionEnd) &&
			(event.customType == kActionAdvance || event.customType == kActionCancel);
		const bool skipAction = event.customType == kActionSkip &&
			(engineAction || engineActionEnd);
		if (skipAction) {
			_skipMode = engineAction;
			if (_skipMode)
				_skipRequested = true;
		}
		if (textAction) {
			const byte actionMask = event.customType == kActionAdvance ? 2 : 1;
			if (engineAction) {
				_textActionsHeld |= actionMask;
				_inputPressed |= actionMask;
				_keyPressed = true;
			} else {
				_textActionsHeld &= ~actionMask;
			}
		}
		const bool vertical = engineAction && (event.customType == kActionUp || event.customType == kActionDown);
		const bool horizontal = engineAction && (event.customType == kActionLeft || event.customType == kActionRight);
		if (_menuEnabled && (vertical || horizontal)) {
			const int direction = event.customType == kActionUp || event.customType == kActionLeft ? -1 : 1;
			if (_coordinateMenu || horizontal) {
				_mouse.x = CLIP<int>(_mouse.x + (horizontal ? direction * 16 : 0), _mouseBounds.left, _mouseBounds.right - 1);
				_mouse.y = CLIP<int>(_mouse.y + (vertical ? direction * 32 : 0), _mouseBounds.top, _mouseBounds.bottom - 1);
			} else if (!_menu.empty()) {
				uint selected = 0;
				for (uint index = 0; index < _menu.size(); ++index)
					if (_menu[index].rect.contains(_mouse)) selected = index;
				selected = (selected + _menu.size() + direction) % _menu.size();
				_mouse = Common::Point((_menu[selected].rect.left + _menu[selected].rect.right) / 2, (_menu[selected].rect.top + _menu[selected].rect.bottom) / 2);
			}
			_system->warpMouse(_mouse.x, _mouse.y);
		}
		if (event.type == Common::EVENT_MOUSEMOVE || event.type == Common::EVENT_LBUTTONDOWN || event.type == Common::EVENT_RBUTTONDOWN)
			_mouse = Common::Point(CLIP<int>(event.mouse.x, _mouseBounds.left, _mouseBounds.right - 1), CLIP<int>(event.mouse.y, _mouseBounds.top, _mouseBounds.bottom - 1));
		const byte buttonMask = event.type == Common::EVENT_LBUTTONDOWN || event.type == Common::EVENT_LBUTTONUP ? 2 : 1;
		if (event.type == Common::EVENT_LBUTTONDOWN || event.type == Common::EVENT_RBUTTONDOWN) {
			_mouseButtonsHeld |= buttonMask;
			_inputPressed |= buttonMask;
			_keyPressed = true;
		} else if (event.type == Common::EVENT_LBUTTONUP || event.type == Common::EVENT_RBUTTONUP) {
			_mouseButtonsHeld &= ~buttonMask;
		}
		updateMenu(event);
		if (_menuEnabled && !_coordinateMenu && (event.type == Common::EVENT_MOUSEMOVE || engineAction))
			menuChanged = true;
	}
	// One redraw per drained burst; the highlight only depends on the final pointer position
	if (menuChanged && _menuEnabled && !_coordinateMenu)
		present();
	_frameCounter = (_system->getMillis() - _frameEpoch) * 564 / 10000;
}

bool System98Engine::waitFrames(uint frames) {
	const uint32 start = _system->getMillis();
	const uint32 duration = frames * 10000 / 564;
	do {
		pollInput();
		if (shouldQuit())
			return false;
		_system->delayMillis(1);
	} while (_system->getMillis() - start < duration);
	return true;
}

bool System98Engine::execute() {
	// After a menu selection the original waits for the input to change before jumping
	while (_menuReleaseState && heldInput() == _menuReleaseState && waitFrames(1)) {
	}
	_menuReleaseState = 0;
	const uint16 position = _scriptOffset;
	const uint16 opcode = fetchWord();
	debug(2, "System98: %s:%04x opcode %02x", sceneBankName(_activeBank).c_str(), position, opcode);
	uint16 args[10] = {};
	auto arguments = [&](uint count) {
		for (uint index = 0; index < count; ++index)
			args[index] = fetchWord();
	};
	switch (opcode) {
	case 0: return false;
	case 2:
		// Planes 0 and 1 are the VRAM pages; higher planes decode into the software page
		arguments(6);
		_scriptValid = loadImage(sceneString(args[4], true), args[0] * 8, args[1], MIN<uint>(args[3], 2), args[2] != 0, args[5] != 0);
		break;
	case 3:
		arguments(6);
		if (args[0] >= 16) { _scriptValid = false; break; }
		openTextBox(args[0], Common::Rect(args[1] * 16, args[2] * 16, (args[1] + args[3]) * 16, (args[2] + args[4]) * 16), args[5]);
		break;
	case 4: case 5:
		arguments(1);
		if (args[0] >= 16) { _scriptValid = false; break; }
		clearTextBox(args[0], opcode == 4);
		break;
	case 6: memcpy(_palette, _targetPalette, sizeof(_palette)); present(); break;
	case 7: case 8:
		arguments(2);
		fadePalette(args[0] != 0, args[1], opcode == 7);
		break;
	case 9: _paletteLock ^= 1 << (fetchWord() & 15); break;
	case 0x0a: _scriptOffset = fetchWord(); break;
	case 0x0b: case 0x0c: {
		arguments(2);
		const uint16 left = reg(args[0]), right = opcode == 0x0b ? reg(args[1]) : args[1];
		_compareResult = left == right ? 0 : left < right ? 1 : 2;
		break;
	}
	case 0x0d: case 0x0e: case 0x0f: case 0x10: case 0x11: case 0x12: {
		const uint16 target = fetchWord();
		const bool jump = opcode == 0x0d ? _compareResult == 0 : opcode == 0x0e ? _compareResult == 1 : opcode == 0x0f ? _compareResult == 2 : opcode == 0x10 ? _compareResult != 1 : opcode == 0x11 ? _compareResult != 2 : _compareResult != 0;
		if (jump)
			_scriptOffset = target;
		break;
	}
	case 0x13: {
		const uint16 index = reg(fetchWord());
		_scriptOffset = sceneWord(_scriptOffset + index * 2);
		break;
	}
	case 0x14: {
		// PALCSET writes one 4-bit RGB entry to both the image and the displayed palette
		arguments(4);
		const uint index = reg(args[0]) & 15;
		for (uint component = 0; component < 3; ++component)
			_palette[index * 3 + component] = _targetPalette[index * 3 + component] = (reg(args[1 + component]) & 15) * 17;
		present();
		break;
	}
	case 0x15: arguments(2); printText(args[0], args[1]); break;
	case 0x18: arguments(2); reg(args[0]) = args[1]; break;
	case 0x19: arguments(2); reg(args[0]) = reg(args[1]); break;
	case 0x1a: {
		Common::String name = sceneString(fetchWord(), true);
		Common::Array<byte> data;
		if (_musicMode == 1 || _musicMode == 2) {
			if (!_resources.load(name, data) || !_fm->playMusic(data))
				warning("System98: cannot play P.M.D. music %s", name.c_str());
			else debug(1, "System98: FM music %s", name.c_str());
		} else if (_musicMode == 3) {
			if (!_resources.load(name, data) || !_sound->playMusic(data))
				warning("System98: cannot play M.M.D. music %s", name.c_str());
			else debug(1, "System98: MIDI music %s", name.c_str());
		}
		break;
	}
	case 0x1b:
		// P.M.D. takes the low byte of register 0x403 as its fade speed
		if (_musicMode == 3) _sound->fadeOut(MAX<uint>(1, reg(0x403)) * 60);
		else _fm->fadeOut(reg(0x403) & 0xff);
		break;
	case 0x1c:
		_sound->stop();
		_fm->stop();
		break;
	case 0x1d: reg(fetchWord()) = _musicMode; break;
	case 0x1e: {
		// Waits for input, then for one of its bits to be released; stores the pressed bits and the later state
		arguments(1);
		_inputPressed = 0;
		byte pressed = 0;
		while (!pressed && waitFrames(1))
			pressed = _inputPressed | heldInput();
		while (pressed && (heldInput() & pressed) == pressed)
			if (!waitFrames(1))
				break;
		reg(args[0]) = pressed | heldInput() << 8;
		break;
	}
	case 0x1f: {
		const Common::String name = sceneString(fetchWord(), true);
		_scriptValid = loadScene(name, 0);
		break;
	}
	case 0x21: waitFrames(reg(fetchWord())); break;
	case 0x22: _characterDelay = reg(fetchWord()); break;
	case 0x24: arguments(2); reg(args[0]) += args[1]; break;
	case 0x25: case 0x28: {
		// A borrow leaves the magnitude and sets register 0 to 1
		arguments(2);
		const uint16 value = opcode == 0x25 ? args[1] : reg(args[1]);
		uint16 &target = reg(args[0]);
		const bool borrow = target < value;
		target -= value;
		if (borrow) {
			reg(0) = 1;
			target = -target;
		}
		break;
	}
	case 0x27: arguments(2); reg(args[0]) += reg(args[1]); break;
	case 0x26: case 0x3c:
		// Text VRAM fill in 8x16 cells: spaces, or full blocks with attribute 0xe1 (0x26) or 0x01 (0x3c)
		arguments(5);
		fillTextCells(Common::Rect(MIN<uint>(args[0], 80), MIN<uint>(args[1], 25), MIN<uint>(args[0] + args[2], 80), MIN<uint>(args[1] + args[3], 25)),
			args[4] != 0, opcode == 0x26 ? 7 : 0);
		break;
	case 0x2d:
		arguments(2);
		if (args[0] > args[1] || args[1] >= ARRAYSIZE(_registers)) { _scriptValid = false; break; }
		for (uint index = args[0]; index <= args[1]; ++index) reg(index) = 0;
		break;
	case 0x2f: case 0x3a: case 0x63: case 0x64:
		arguments(8);
		if (opcode == 0x3a || opcode == 0x64)
			for (uint index = 0; index < 8; ++index) args[index] = reg(args[index]);
		copyPage(args[0], Common::Rect(args[1] * 8, args[2], (args[1] + args[3]) * 8, args[2] + args[4]), args[5], args[6] * 8, args[7], opcode == 0x63 || opcode == 0x64);
		break;
	case 0x33: _displayPage = reg(fetchWord()) & 1; present(); break;
	case 0x34: reg(fetchWord()) = _displayPage; break;
	case 0x36:
		_windowStyles.free();
		_windowStyles.create(384, 48, Graphics::PixelFormat::createFormatCLUT8());
		for (int row = 0; row < 48; ++row)
			memcpy(_windowStyles.getBasePtr(0, row), _pages[1].getBasePtr(0, row), 384);
		break;
	case 0x38: arguments(2); reg(args[0]) &= reg(args[1]); break;
	case 0x39: arguments(2); reg(args[0]) |= reg(args[1]); break;
	case 0x3d: arguments(2); reg(args[0]) *= reg(args[1]); break;
	case 0x3e: {
		// The remainder goes to the divisor register; dividing by zero clears both
		arguments(2);
		uint16 &dividend = reg(args[0]), &divisor = reg(args[1]);
		const uint16 left = dividend, right = divisor;
		dividend = divisor = right;
		if (right) {
			dividend = left / right;
			divisor = left % right;
		}
		break;
	}
	case 0x3f:
		arguments(2); _mouse = Common::Point(reg(args[0]), reg(args[1])); _system->warpMouse(_mouse.x, _mouse.y); break;
	case 0x40: {
		arguments(4);
		_menu.clear();
		// The list ends at an entry whose ID byte is zero
		for (uint offset = args[2]; sceneBankOffset(offset) + 10U <= sceneBank(_activeBank).size() && (sceneWord(offset) & 0xff); offset += 10) {
			MenuItem item;
			item.id = sceneWord(offset) & 0xff;
			item.rect = Common::Rect(sceneWord(offset + 2), sceneWord(offset + 4), sceneWord(offset + 6), sceneWord(offset + 8));
			_menu.push_back(item);
		}
		_menuRegisters[0] = args[0]; _menuRegisters[1] = args[1]; _menuJump = args[3]; _menuBank = _activeBank; _menuEnabled = true; _coordinateMenu = false; _menuConfigured = true;
		debug(1, "System98: menu %u regions at %s:%04x", _menu.size(), sceneBankName(_activeBank).c_str(), position);
		present();
		break;
	}
	case 0x41:
		arguments(4);
		_menuRegisters[0] = args[0]; _menuRegisters[1] = args[1]; _mouseActionRegister = args[2];
		_menuJump = args[3]; _menuBank = _activeBank; _menuEnabled = true; _coordinateMenu = true; _menuConfigured = true;
		debug(1, "System98: coordinate menu at %s:%04x", _sceneName.c_str(), position);
		break;
	case 0x42:
		// Only a menu that 0x40 or 0x41 set up can be switched; enabling also yields a frame here
		arguments(1);
		if (!_menuConfigured) break;
		_menuEnabled = (args[0] & 0xff) != 0;
		if (_menuEnabled) waitFrames(1);
		break;
	case 0x43: arguments(2); reg(args[0]) = _mouse.x; reg(args[1]) = _mouse.y; break;
	case 0x29: case 0x2a: case 0x4a: case 0x4b: {
		// Buffer byte 0 and 1 select the two 512-register halves; any other value the 8 configuration registers
		arguments(2);
		const byte buffer = args[0] & 0xff;
		const uint begin = buffer > 1 ? 1024 : buffer * 512;
		const uint count = buffer > 1 ? 8 : 512;
		// Mugen dispatches 0x29 and 0x2a to the 0x4a handler
		if (opcode != 0x4b) {
			Common::OutSaveFile *file = _system->getSavefileManager()->openForSaving(saveFileName(sceneString(args[1], true)));
			if (!file) { _scriptValid = false; break; }
			for (uint index = 0; index < count; ++index) file->writeUint16LE(reg(begin + index));
			// Save files carry no timestamp, so the DOS time and date FILETM reports follow the registers
			TimeDate time;
			_system->getTimeAndDate(time);
			file->writeUint16LE(time.tm_hour << 11 | time.tm_min << 5 | time.tm_sec / 2);
			file->writeUint16LE(MAX(time.tm_year - 80, 0) << 9 | (time.tm_mon + 1) << 5 | time.tm_mday);
			file->finalize();
			_scriptValid = !file->err();
			delete file;
		} else {
			// Shorter files fill only the registers they cover; the original reads at most the buffer size
			Common::InSaveFile *file = _system->getSavefileManager()->openForLoading(saveFileName(sceneString(args[1], true)));
			Common::Array<byte> data;
			if (file) {
				data.resize(MIN<int64>(file->size(), count * 2));
				data.resize(file->read(data.data(), data.size()));
				delete file;
			} else {
				_resources.load(sceneString(args[1], true), data);
			}
			for (uint index = 0; index < count && (index + 1) * 2 <= data.size(); ++index)
				reg(begin + index) = READ_LE_UINT16(data.data() + index * 2);
		}
		break;
	}
	case 0x4d: reg(fetchWord()) = _musicMode == 3 ? _sound->measure() : (_musicMode ? _fm->measure() : 0); break;
	case 0x4e: case 0x4f:
		arguments(1);
		if (_musicMode == 3) {
			if (!_sound->playEffect(_soundEffects, args[0] & 255))
				warning("System98: cannot play MIDI effect %u", args[0] & 255);
		} else if (_musicMode && opcode == 0x4e) {
			_fm->playSsgEffect(args[0] & 255);
		} else if (_musicMode) {
			_fm->playFmEffect(args[0] & 255);
		}
		break;
	case 0x50:
		// P.M.D. reports its status byte and loop count, which is 0xff once music stops
		if (_musicMode == 3) reg(fetchWord()) = _sound->playing() ? 1 : 0;
		else reg(fetchWord()) = _musicMode ? _fm->status() : 0;
		break;
	case 0x51: arguments(2); reg(args[0]) &= args[1]; break;
	case 0x52: arguments(2); reg(args[0]) |= args[1]; break;
	case 0x53:
		arguments(2);
		if (args[0] >= ARRAYSIZE(_strings) || args[1] >= ARRAYSIZE(_strings)) { _scriptValid = false; break; }
		appendString(_strings[args[0]], _strings[args[1]], sizeof(_strings[args[0]]));
		break;
	case 0x54:
		arguments(1);
		if (_stackSize >= 8) { _scriptValid = false; break; }
		_returnStack[_stackSize] = _scriptOffset; _returnBanks[_stackSize++] = _activeBank; _scriptOffset = args[0];
		break;
	case 0x55:
		if (!_stackSize) { _scriptValid = false; break; }
		_activeBank = _returnBanks[--_stackSize]; _scriptOffset = _returnStack[_stackSize]; break;
	case 0x59:
		arguments(2);
		if (args[0] >= ARRAYSIZE(_strings)) { _scriptValid = false; break; }
		{
			const Common::String text = sceneString(args[1]);
			uint stringPos = 0;
			while (stringPos < 79 && _strings[args[0]][stringPos]) ++stringPos;
			const uint count = MIN<uint>(79 - stringPos, text.size());
			memcpy(_strings[args[0]] + stringPos, text.c_str(), count);
			_strings[args[0]][stringPos + count] = 0;
		}
		break;
	case 0x5a:
		arguments(1);
		if (args[0] >= ARRAYSIZE(_strings)) { _scriptValid = false; break; }
		memset(_strings[args[0]], 0, 80); break;
	case 0x5e:
		arguments(5);
		if (args[0] >= ARRAYSIZE(_pages)) { _scriptValid = false; break; }
		{
			Common::Rect rect(args[1] * 8, args[2], (args[1] + args[3]) * 8, args[2] + args[4]); rect.clip(Common::Rect(640, 400));
			_pages[args[0]].fillRect(rect, 0); present();
		}
		break;
	case 0x60: {
		arguments(3);
		if (args[1] >= ARRAYSIZE(_strings)) { _scriptValid = false; break; }
		// Found files get their date as NEC half-width "YYYY-MM-DD hh:mm:ss", stored after the registers by 0x4a
		Common::InSaveFile *file = _system->getSavefileManager()->openForLoading(saveFileName(sceneString(args[2], true)));
		reg(args[0]) = file ? 1 : 0;
		_strings[args[1]][0] = 0;
		Common::Array<byte> data;
		if (file) {
			data.resize(file->size() > 0 && file->size() <= 0x1000 ? file->size() : 0);
			data.resize(file->read(data.data(), data.size()));
		}
		if (data.size() == 512 * 2 + 4 || data.size() == 8 * 2 + 4) {
			const uint16 time = READ_LE_UINT16(data.data() + data.size() - 4), date = READ_LE_UINT16(data.data() + data.size() - 2);
			const uint values[] = {(1980U + (date >> 9)) / 100, (1980U + (date >> 9)) % 100, (date >> 5) & 15U, date & 31U, uint(time >> 11), (time >> 5) & 63U, (time & 31U) * 2};
			// Separators before each pair: none within the year, then 854Ch "-", a space, and 8559h ":"
			static const byte separators[] = {0, 0, 0x4c, 0x4c, ' ', 0x59, 0x59};
			byte *text = _strings[args[1]];
			uint length = 0;
			for (uint index = 0; index < ARRAYSIZE(values); ++index) {
				if (separators[index] == ' ') {
					text[length++] = ' ';
				} else if (separators[index]) {
					text[length++] = 0x85;
					text[length++] = separators[index];
				}
				text[length++] = 0x85;
				text[length++] = 0x4f + values[index] / 10 % 10;
				text[length++] = 0x85;
				text[length++] = 0x4f + values[index] % 10;
			}
			text[length] = 0;
		}
		delete file;
		break;
	}
	case 0x62:
		// GDC SCROLL: area 1 starts at the given line and word for 400 - lines rows, area 2 at line 0
		arguments(2);
		if (reg(args[1]) > 400) { _scriptValid = false; break; }
		_scrollWords = reg(args[0]);
		_scrollLines = reg(args[1]);
		present();
		break;
	case 0x6e:
		arguments(2);
		if (args[1] >= ARRAYSIZE(_strings)) { _scriptValid = false; break; }
		reg(args[0]) = getStringLength(_strings[args[1]], sizeof(_strings[args[1]]));
		break;
	case 0x6a:
	{
		// Each edge is capped at (624, 384) and each pair sorted; the pointer is only clamped when it next moves
		arguments(4);
		uint16 edges[4];
		for (uint index = 0; index < 4; ++index)
			edges[index] = MIN<uint16>(args[index], index & 1 ? 0x180 : 0x270);
		_mouseBounds = Common::Rect(MIN(edges[0], edges[2]), MIN(edges[1], edges[3]), MAX(edges[0], edges[2]) + 1, MAX(edges[1], edges[3]) + 1);
		break;
	}
	case 0x72: _frameEpoch = _system->getMillis() - fetchWord() * 10000 / 564; break;
	case 0x73: reg(fetchWord()) = _frameCounter; break;
	case 0x74: arguments(2); if (_frameCounter < args[0]) { _scriptOffset = args[1]; waitFrames(1); } break;
	case 0x76: for (uint index = 0; index < 8; ++index) _idleCharacters[index] = fetchWord(); break;
	case 0x77: for (uint index = 0; index < 8; ++index) _idleDelays[index] = fetchWord(); break;
	case 0x78: arguments(2); _idleOffset[0] = args[0]; _idleOffset[1] = args[1]; break;
	case 0x7a: _frameEpoch = _system->getMillis() - reg(fetchWord()) * 10000 / 564; break;
	case 0x7b: arguments(2); if (_frameCounter < reg(args[0])) { _scriptOffset = args[1]; waitFrames(1); } break;
	case 0x80: {
		arguments(2);
		if (args[0] >= ARRAYSIZE(_strings) - 1) { _scriptValid = false; break; }
		byte strings[2][80];
		const Common::String name = sceneString(args[1], true);
		if (!loadStringFile(name, strings)) {
			warning("System98: cannot load two strings from %s", name.c_str());
			_scriptValid = false;
			break;
		}
		memcpy(_strings[args[0]], strings, sizeof(strings));
		break;
	}
	case 0x8f:
		arguments(2);
		_scriptValid = loadScene(sceneString(args[1], true), args[0]);
		break;
	case 0x90:
		// Discards the newest return address without returning
		if (!_stackSize) { _scriptValid = false; break; }
		--_stackSize;
		break;
	case 0x91:
		arguments(2);
		{
			const uint mode = reg(args[0]);
			if (selectMusicMode(mode)) {
				reg(args[1]) = _musicMode;
				_musicMode = mode;
			} else {
				reg(args[1]) = 0xfffd;
				warning("System98: music driver mode %u is unavailable", mode);
			}
		}
		break;
	case 0x92: changeName(); break;
	case 0x94: break;
	case 0x95:
		arguments(8);
		if (!_scriptValid)
			break;
		if (args[6] >= ARRAYSIZE(_patterns) || _patterns[args[6]].empty())
			break;
		if (args[0] >= 80 || args[1] >= 400 || !args[2] || args[2] > 80 || !args[3] || args[3] > 400 || args[4] >= 80 || args[5] >= 400) {
			_scriptValid = false;
			break;
		}
		debug(1, "System98: patterned blit %u, %u stages, %u frames/stage", args[6], _patterns[args[6]].size() / 8, args[7]);
		_scriptValid = patternedBlit(Common::Rect(args[0] * 8, args[1], (args[0] + args[2]) * 8, args[1] + args[3]), Common::Point(args[4] * 8, args[5]), args[6], args[7]);
		break;
	case 0x96:
		arguments(2);
		if (args[0] >= ARRAYSIZE(_patterns) || !_scriptValid) { _scriptValid = false; break; }
		_scriptValid = captureBlitPattern(_pages[1], args[1], _patterns[args[0]]);
		break;
	case 0x97: {
		// Runs ENDING.EXE, the staff roll; scripts then clear text VRAM and return to the title
		Ending ending(*this);
		ending.play();
		break;
	}
	default:
		warning("System98: unimplemented opcode %02x at %s:%04x", opcode, sceneBankName(_activeBank).c_str(), position);
		_scriptValid = false;
	}
	return _scriptValid;
}

} // namespace System98
