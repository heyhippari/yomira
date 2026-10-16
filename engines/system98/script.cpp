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
#include "system98/image.h"

namespace System98 {

bool System98Engine::loadScene(const Common::String &name, uint bank) {
	Common::Array<byte> data;
	if (bank > 1 || !_resources.load(name, data) || data.size() < 0x102 || data.size() > 0x4000)
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

uint16 System98Engine::sceneWord(uint offset) const {
	const Common::Array<byte> &data = offset >= 0x4000 ? _secondaryScene : _scene;
	offset &= 0x3fff;
	return offset + 2 <= data.size() ? READ_LE_UINT16(data.data() + offset) : 0;
}

uint16 System98Engine::fetchWord() {
	_activeBank = _scriptOffset >= 0x4000;
	const Common::Array<byte> &data = _activeBank ? _secondaryScene : _scene;
	if ((_scriptOffset & 0x3fff) + 2U > data.size()) {
		_scriptValid = false;
		return 0;
	}
	const uint16 value = sceneWord(_scriptOffset);
	_scriptOffset += 2;
	return value;
}

Common::String System98Engine::sceneString(uint offset, bool fileName) const {
	const Common::Array<byte> &data = offset >= 0x4000 ? _secondaryScene : _scene;
	offset &= 0x3fff;
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
	if ((!selected && !_coordinateMenu) || !action)
		return;
	if (_coordinateMenu) {
		reg(_menuRegisters[0]) = _mouse.x;
		reg(_menuRegisters[1]) = _mouse.y;
		reg(_mouseActionRegister) = action;
	} else {
		reg(_menuRegisters[0]) = selected;
		reg(_menuRegisters[1]) = action;
	}
	_activeBank = _menuBank;
	_scriptOffset = _menuJump;
	_menuEnabled = false;
	debug(1, "System98: menu selection %u, action %u -> %04x", selected, action, _menuJump);
}

void System98Engine::pollInput() {
	Common::Event event;
	while (_system->getEventManager()->pollEvent(event)) {
		const bool engineAction = event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START;
		const bool engineActionEnd = event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_END;
		const bool textAction = (engineAction || engineActionEnd) &&
			(event.customType == kActionAdvance || event.customType == kActionCancel);
		const bool skipAction = event.customType == kActionSkip &&
			(engineAction || engineActionEnd);
		if (skipAction) {
			_skipMode = event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START;
			if (_skipMode)
				_skipRequested = true;
		}
		if (textAction) {
			const byte actionMask = event.customType == kActionAdvance ? 1 : 2;
			if (engineAction) {
				_textActionsHeld |= actionMask;
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
		if (event.type == Common::EVENT_LBUTTONDOWN || event.type == Common::EVENT_RBUTTONDOWN) {
			_textMouseHeld = true;
			_keyPressed = true;
		} else if (event.type == Common::EVENT_LBUTTONUP || event.type == Common::EVENT_RBUTTONUP) {
			_textMouseHeld = false;
		}
		updateMenu(event);
		if (_menuEnabled && !_coordinateMenu && (event.type == Common::EVENT_MOUSEMOVE || engineAction))
			present();
	}
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
	const uint16 position = _scriptOffset;
	const uint16 opcode = fetchWord();
	debug(2, "System98: %s:%04x opcode %02x", (_activeBank ? _secondaryName : _sceneName).c_str(), position, opcode);
	uint16 args[10] = {};
	auto arguments = [&](uint count) {
		for (uint index = 0; index < count; ++index)
			args[index] = fetchWord();
	};
	auto saveName = [&](uint offset) {
		Common::String name = _targetName + ".s98-";
		const Common::String original = sceneString(offset, true);
		for (uint index = 0; index < original.size(); ++index)
			name += Common::String::format("%02x", byte(original[index]));
		return name;
	};
	switch (opcode) {
	case 0: return false;
	case 2:
		arguments(6);
		if (args[3] < 2)
			_drawPage = args[3];
		_scriptValid = loadImage(sceneString(args[4], true), args[0] * 8, args[1], _drawPage, args[2] != 0, args[5] != 0);
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
	case 0x15: arguments(2); printText(args[0], args[1]); break;
	case 0x18: arguments(2); reg(args[0]) = args[1]; break;
	case 0x19: arguments(2); reg(args[0]) = reg(args[1]); break;
	case 0x1a: {
		Common::String name = sceneString(fetchWord(), true);
		if (_musicMode == 3) {
			if (!name.empty() && (name[0] == 'b' || name[0] == 'B' || name[0] == 'n' || name[0] == 'N')) name.setChar('g', 0);
			Common::Array<byte> data;
			if (!_resources.load(name, data) || !_sound->playMusic(data))
				warning("System98: cannot play M.M.D. music %s", name.c_str());
			else debug(1, "System98: MIDI music %s", name.c_str());
		}
		break;
	}
	case 0x1b:
		if (_sound) _sound->fadeOut(MAX<uint>(1, reg(0x403)) * 60);
		break;
	case 0x1c:
		if (_sound) _sound->stop();
		break;
	case 0x1d: reg(fetchWord()) = _musicMode; break;
	case 0x1e:
		arguments(1);
		_keyPressed = false;
		while (!_keyPressed && !shouldQuit())
			waitFrames(1);
		reg(args[0]) = 1;
		break;
	case 0x1f: {
		const Common::String name = sceneString(fetchWord(), true);
		_scriptValid = loadScene(name, 0);
		break;
	}
	case 0x21: waitFrames(reg(fetchWord())); break;
	case 0x22: _characterDelay = reg(fetchWord()); break;
	case 0x24: arguments(2); reg(args[0]) += args[1]; break;
	case 0x25: arguments(2); reg(args[0]) -= args[1]; break;
	case 0x27: arguments(2); reg(args[0]) += reg(args[1]); break;
	case 0x28: arguments(2); reg(args[0]) -= reg(args[1]); break;
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
	case 0x3e:
		arguments(2);
		if (!reg(args[1])) { _scriptValid = false; break; }
		reg(args[0]) /= reg(args[1]);
		break;
	case 0x3f:
		arguments(2); _mouse = Common::Point(reg(args[0]), reg(args[1])); _system->warpMouse(_mouse.x, _mouse.y); break;
	case 0x40: {
		arguments(4);
		_menu.clear();
		for (uint offset = args[2]; (offset & 0x3fff) + 10U <= (_activeBank ? _secondaryScene : _scene).size() && sceneWord(offset); offset += 10) {
			MenuItem item;
			item.id = sceneWord(offset);
			item.rect = Common::Rect(sceneWord(offset + 2), sceneWord(offset + 4), sceneWord(offset + 6), sceneWord(offset + 8));
			_menu.push_back(item);
		}
		_menuRegisters[0] = args[0]; _menuRegisters[1] = args[1]; _menuJump = args[3]; _menuBank = _activeBank; _menuEnabled = true; _coordinateMenu = false;
		debug(1, "System98: menu %u regions at %s:%04x", _menu.size(), _sceneName.c_str(), position);
		present();
		break;
	}
	case 0x41:
		arguments(4);
		_menuRegisters[0] = args[0]; _menuRegisters[1] = args[1]; _mouseActionRegister = args[2];
		_menuJump = args[3]; _menuBank = _activeBank; _menuEnabled = true; _coordinateMenu = true;
		debug(1, "System98: coordinate menu at %s:%04x", _sceneName.c_str(), position);
		break;
	case 0x42:
		_menuEnabled = fetchWord() != 0;
		if (_menuEnabled) waitFrames(1);
		break;
	case 0x43: arguments(2); reg(args[0]) = _mouse.x; reg(args[1]) = _mouse.y; break;
	case 0x4a: case 0x4b: {
		arguments(2);
		if (args[0] > 2) { _scriptValid = false; break; }
		const uint begin = args[0] == 2 ? 1024 : args[0] * 512;
		const uint count = args[0] == 2 ? 8 : 512;
		if (opcode == 0x4a) {
			Common::OutSaveFile *file = _system->getSavefileManager()->openForSaving(saveName(args[1]));
			if (!file) { _scriptValid = false; break; }
			for (uint index = 0; index < count; ++index) file->writeUint16LE(reg(begin + index));
			file->finalize();
			_scriptValid = !file->err();
			delete file;
		} else {
			Common::InSaveFile *file = _system->getSavefileManager()->openForLoading(saveName(args[1]));
			Common::Array<byte> data;
			if (file) {
				if (file->size() == count * 2)
					for (uint index = 0; index < count; ++index) reg(begin + index) = file->readUint16LE();
				delete file;
			} else if (_resources.load(sceneString(args[1], true), data) && data.size() == count * 2) {
				for (uint index = 0; index < count; ++index) reg(begin + index) = READ_LE_UINT16(data.data() + index * 2);
			}
		}
		break;
	}
	case 0x4d: reg(fetchWord()) = _sound && _musicMode ? _sound->measure() : 0; break;
	case 0x4e: case 0x4f:
		arguments(1);
		if (_sound && _musicMode == 3 && !_sound->playEffect(_soundEffects, args[0] & 255))
			warning("System98: cannot play MIDI effect %u", args[0] & 255);
		break;
	case 0x50: reg(fetchWord()) = _sound && _musicMode && _sound->playing() ? 1 : 0; break;
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
		Common::InSaveFile *file = _system->getSavefileManager()->openForLoading(saveName(args[2]));
		reg(args[0]) = file ? 1 : 0;
		_strings[args[1]][0] = 0;
		delete file;
		break;
	}
	case 0x6e:
		arguments(2);
		if (args[1] >= ARRAYSIZE(_strings)) { _scriptValid = false; break; }
		reg(args[0]) = getStringLength(_strings[args[1]], sizeof(_strings[args[1]]));
		break;
	case 0x6a:
		arguments(4);
		_mouseBounds = Common::Rect(args[0], args[1], args[2] + 1, args[3] + 1);
		_mouse.x = CLIP<int>(_mouse.x, _mouseBounds.left, _mouseBounds.right - 1);
		_mouse.y = CLIP<int>(_mouse.y, _mouseBounds.top, _mouseBounds.bottom - 1);
		_system->warpMouse(_mouse.x, _mouse.y);
		break;
	case 0x72: _frameEpoch = _system->getMillis() - fetchWord() * 10000 / 564; break;
	case 0x73: reg(fetchWord()) = _frameCounter; break;
	case 0x74: arguments(2); if (_frameCounter < args[0]) { _scriptOffset = args[1]; waitFrames(1); } break;
	case 0x76: for (uint index = 0; index < 8; ++index) _idleCharacters[index] = fetchWord(); break;
	case 0x77: for (uint index = 0; index < 8; ++index) _idleDelays[index] = fetchWord(); break;
	case 0x78: arguments(2); break;
	case 0x7a: _frameEpoch = _system->getMillis() - reg(fetchWord()) * 10000 / 564; break;
	case 0x7b: arguments(2); if (_frameCounter < reg(args[0])) { _scriptOffset = args[1]; waitFrames(1); } break;
	case 0x80: {
		arguments(2);
		if (args[0] >= ARRAYSIZE(_strings) - 1) { _scriptValid = false; break; }
		Common::Array<byte> data;
		byte strings[2][80];
		const Common::String name = sceneString(args[1], true);
		if (!_resources.load(name, data) || !decodeStringFile(data, strings)) {
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
	case 0x91:
		arguments(2);
		{
			const uint mode = reg(args[0]);
			if (mode == 0 || (mode == 3 && _sound && _sound->init())) {
				reg(args[1]) = _musicMode;
				_musicMode = mode;
				_sound->stop();
				_sound->stopEffect();
			} else {
				reg(args[1]) = 0xfffd;
				warning("System98: music driver mode %u is unavailable; select MIDI", mode);
			}
		}
		break;
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
		_drawPage = 0;
		break;
	default:
		warning("System98: unimplemented opcode %02x at %s:%04x", opcode, (_activeBank ? _secondaryName : _sceneName).c_str(), position);
		_scriptValid = false;
	}
	return _scriptValid;
}

} // namespace System98
