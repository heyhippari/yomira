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
#include "common/stream.h"
#include "common/system.h"

#include "xex/xex.h"
#include "xex/detection.h"
#include "xex/script.h"

namespace Xex {

Script::Script(XexEngine *vm) : _vm(vm) {
	memset(_counters, 0, sizeof(_counters));
	memset(_globals, 0, sizeof(_globals));
	memset(_flags, 0, sizeof(_flags));
}

bool Script::start(const Common::String &name) {
	if (!loadScenario(name))
		return false;
	_menus.clear();
	newMenu();
	_running = true;
	return true;
}

bool Script::loadScenario(const Common::String &name) {
	debugC(1, kDebugScript, "Loading scenario '%s'", name.c_str());
	Scenario scenario;
	if (!scenario.load(name))
		return false;

	_scenario = scenario;
	_cur = Scenario::kCommandsOffset;
	memset(_counters, 0, sizeof(_counters));
	_fired.clear();
	_deferred.clear();

	// Local variables persist per scenario across visits
	if (!_locals.contains(name)) {
		Common::Array<int16> locals;
		locals.resize(scenario.getLocalCount());
		for (uint i = 0; i < locals.size(); i++)
			locals[i] = 0;
		_locals[name] = locals;
	}
	return true;
}

static void writeString(Common::WriteStream &stream, const Common::String &str) {
	stream.writeUint16LE(str.size());
	stream.writeString(str);
}

static Common::String readString(Common::ReadStream &stream) {
	uint16 size = stream.readUint16LE();
	return stream.readString(0, size);
}

void Script::saveState(Common::WriteStream &stream) const {
	writeString(stream, _scenario.getName());
	stream.writeUint32LE(_cur);
	writeString(stream, _queued);
	stream.write(_counters, sizeof(_counters));
	for (int i = 0; i < kGlobalCount; i++)
		stream.writeSint16LE(_globals[i]);

	stream.writeUint16LE(_fired.size());
	for (uint i = 0; i < _fired.size(); i++)
		stream.writeUint16LE(_fired[i]);
	stream.writeUint16LE(_deferred.size());
	for (uint i = 0; i < _deferred.size(); i++)
		stream.writeUint32LE(_deferred[i]);

	stream.writeByte(_menus.size());
	for (uint i = 0; i < _menus.size(); i++) {
		stream.writeByte(_menus[i].size());
		for (uint j = 0; j < _menus[i].size(); j++) {
			writeString(stream, _menus[i][j].text);
			stream.writeUint32LE(_menus[i][j].body);
		}
	}

	stream.writeUint16LE(_locals.size());
	for (const auto &it : _locals) {
		writeString(stream, it._key);
		stream.writeUint16LE(it._value.size());
		for (uint i = 0; i < it._value.size(); i++)
			stream.writeSint16LE(it._value[i]);
	}
}

bool Script::loadState(Common::ReadStream &stream) {
	Common::String name = readString(stream);
	uint32 cur = stream.readUint32LE();
	Common::String queued = readString(stream);
	if (stream.err() || stream.eos() || !loadScenario(name) || cur < Scenario::kCommandsOffset || cur >= _scenario.getSize())
		return false;

	_cur = cur;
	_queued = queued;
	stream.read(_counters, sizeof(_counters));
	for (int i = 0; i < kGlobalCount; i++)
		_globals[i] = stream.readSint16LE();

	_fired.resize(stream.readUint16LE());
	for (uint i = 0; i < _fired.size(); i++)
		_fired[i] = stream.readUint16LE();
	_deferred.resize(stream.readUint16LE());
	for (uint i = 0; i < _deferred.size(); i++)
		_deferred[i] = stream.readUint32LE();

	_menus.resize(stream.readByte());
	for (uint i = 0; i < _menus.size(); i++) {
		_menus[i].resize(stream.readByte());
		for (uint j = 0; j < _menus[i].size(); j++) {
			_menus[i][j].text = readString(stream);
			_menus[i][j].body = stream.readUint32LE();
		}
	}

	_locals.clear();
	uint16 count = stream.readUint16LE();
	for (uint i = 0; i < count; i++) {
		Common::String scenario = readString(stream);
		Common::Array<int16> &locals = _locals[scenario];
		locals.resize(stream.readUint16LE());
		for (uint j = 0; j < locals.size(); j++)
			locals[j] = stream.readSint16LE();
	}
	if (!_locals.contains(name))
		_locals[name].resize(_scenario.getLocalCount());

	_running = !stream.err() && !stream.eos();
	return _running;
}

void Script::resetVariables() {
	for (auto &it : _locals) {
		for (uint i = 0; i < it._value.size(); i++)
			it._value[i] = 0;
	}
	// The original clears the first 0xc0 global slots
	for (int i = 0; i < 0xc0 && i < kGlobalCount - kLocalVarLimit; i++)
		_globals[kLocalVarLimit + i] = 0;
}

bool Script::setFlag(int number) {
	if (number < 0 || number >= kFlagCount || _flags[number])
		return false;
	_flags[number] = true;
	return true;
}

bool Script::getFlag(int number) const {
	return number >= 0 && number < kFlagCount && _flags[number];
}

void Script::newMenu() {
	if (_menus.size() >= kMaxMenuDepth) {
		warning("Xex: menu nesting too deep");
		_running = false;
		return;
	}
	_menus.push_back(Common::Array<MenuItem>());
}

byte Script::readByte() {
	if (_cur >= _scenario.getSize()) {
		warning("Xex: script read past end of '%s'", _scenario.getName().c_str());
		_running = false;
		return 0;
	}
	return _scenario.getData()[_cur++];
}

uint16 Script::readWord() {
	uint16 lo = readByte();
	return lo | (readByte() << 8);
}

int16 Script::readSWord() {
	return (int16)readWord();
}

int8 &Script::counter(int16 index) {
	if (index < 0 || index >= kCounterCount) {
		_badCounter = 0;
		return _badCounter;
	}
	return _counters[index];
}

int16 Script::getValue(int16 ref) {
	if (ref >= 0)
		return ref;

	int index = -1 - ref;
	if (index < kLocalVarLimit) {
		Common::Array<int16> &locals = _locals[_scenario.getName()];
		return (uint)index < locals.size() ? locals[index] : 0;
	}
	if (index == 0x1ff)
		return g_system->getMillis() % 0x7fff;
	if (index == 0x1fe)
		return _globals[index];
	if (index == 0x1fd)
		return 0;
	if (index >= kFlagBase && index < kFlagBase + kFlagCount)
		return _flags[index - kFlagBase];
	// Key states
	if (index >= 0x1db)
		return 0;
	return index < kGlobalCount ? _globals[index] : 0;
}

void Script::setVariable(int index, int16 value) {
	if (index < 0 || index >= kGlobalCount)
		return;

	if (index < kLocalVarLimit) {
		Common::Array<int16> &locals = _locals[_scenario.getName()];
		if ((uint)index < locals.size())
			locals[index] = value;
	} else {
		_globals[index] = value;
	}
}

bool Script::evalCondition() {
	if (_cur < _scenario.getSize() && _scenario.getData()[_cur] == 0) {
		_cur++;
		return true;
	}

	bool result = false;
	int mode = 0;
	while (_running) {
		byte op = readByte();
		if (!op)
			break;
		if (op == 8) {
			mode = 1;
			continue;
		}
		if (op == 9) {
			mode = 2;
			continue;
		}

		int16 a = getValue(readSWord());
		int16 b = getValue(readSWord());
		bool r = false;
		switch (op) {
		case 1: r = b <= a; break;
		case 2: r = b < a; break;
		case 3: r = a <= b; break;
		case 4: r = a < b; break;
		case 5: r = a == b; break;
		case 6: r = a != b; break;
		default:
			warning("Xex: bad condition operator %d", op);
			_running = false;
			return false;
		}

		if (mode == 0)
			result = r;
		else if (mode == 1)
			result = result && r;
		else
			result = result || r;
		mode = 0;
	}
	return result;
}

void Script::skipAssignments() {
	while (_running && readByte() != 0)
		_cur += 4;
}

void Script::runAssignments() {
	while (_running) {
		byte op = readByte();
		if (!op)
			break;

		int16 target = readSWord();
		int16 value = getValue(readSWord());
		if (target >= 0) {
			warning("Xex: bad assignment target");
			_running = false;
			return;
		}
		int index = -1 - target;

		switch (op) {
		case 1:
			setVariable(index, value);
			break;
		case 2:
			setVariable(index, getValue(target) + value);
			break;
		case 3:
			setVariable(index, getValue(target) - value);
			break;
		default:
			warning("Xex: bad assignment operator %d", op);
			_running = false;
			return;
		}
	}
}

// Postfix expression ending in a store to a variable
void Script::runExpression() {
	int16 stack[32];
	int sp = 32;

	while (_running) {
		int16 token = readSWord();
		if (token < -0x206 || token > -0x201) {
			if (sp == 0) {
				warning("Xex: expression stack overflow");
				_running = false;
				return;
			}
			stack[--sp] = getValue(token);
			continue;
		}

		if (token == -0x201) {
			int target = readSWord();
			if (sp < 32)
				setVariable(target, stack[sp]);
			return;
		}

		if (sp > 30) {
			warning("Xex: expression stack underflow");
			_running = false;
			return;
		}
		int16 b = stack[sp++];
		int16 a = stack[sp];
		switch (token) {
		case -0x206: a = b ? a % b : 0; break;
		case -0x205: a = b ? a / b : 0; break;
		case -0x204: a = a * b; break;
		case -0x203: a = a - b; break;
		case -0x202: a = a + b; break;
		}
		stack[sp] = a;
	}
}

void Script::showText(uint16 offset) {
	uint32 pos = _scenario.getTextOffset() + offset;
	const uint32 textEnd = _scenario.getTextOffset() + _scenario.getTextSize();
	if (pos >= textEnd)
		return;
	_vm->runText(*this, _scenario.getData() + pos, textEnd - pos);
}

void Script::addMenuItem(uint16 textOffset) {
	const uint32 pos = _scenario.getTextOffset() + textOffset;
	const uint32 textEnd = _scenario.getTextOffset() + _scenario.getTextSize();
	if (pos >= textEnd)
		return;

	const byte *text = _scenario.getData() + pos;
	uint32 length = 0;
	while (pos + length < textEnd && text[length])
		length++;

	MenuItem item;
	item.text = Common::String((const char *)text, length);
	item.body = _cur;
	if (!_menus.empty())
		_menus.back().push_back(item);
}

void Script::endPage() {
	for (uint i = 0; i < _deferred.size() && _running; i++) {
		_cur = _deferred[i];
		runAssignments();
	}
	for (uint i = 0; i < _fired.size(); i++)
		counter(_fired[i])++;

	_deferred.clear();
	_fired.clear();
	_menus.clear();

	if (!_queued.empty()) {
		Common::String name = _queued;
		_queued.clear();
		bool leavingTitle = _scenario.getName().equalsIgnoreCase("startup") && !name.equalsIgnoreCase("startup");
		if (!loadScenario(name)) {
			warning("Xex: cannot load scenario '%s'", name.c_str());
			_running = false;
			return;
		}
		if (leavingTitle)
			_vm->leaveTitle();
	} else {
		_cur = Scenario::kCommandsOffset;
	}
	newMenu();
}

void Script::step() {
	if (!_running)
		return;

	// Savegames restart the command that shows the current text or menu
	if (_cur < _scenario.getSize()) {
		byte next = _scenario.getData()[_cur];
		if ((next >= 4 && next <= 7) || next == 10 || next == 0xff)
			_vm->markResumePoint();
	}

	byte op = readByte();
	switch (op) {
	case 0:
		break;
	case 1:
	case 3: {
		// Menu entry: skipped over when the condition fails
		uint16 next = readWord();
		uint16 text = readWord();
		if (evalCondition())
			addMenuItem(text);
		_cur = next;
		break;
	}
	case 4:
	case 5: {
		int16 index = readSWord();
		byte value = readByte();
		uint16 text = readWord();
		if (index < 0)
			index = kCounterCount - 1;
		if ((byte)counter(index) == value) {
			showText(text);
			if (op == 5)
				runAssignments();
			_fired.push_back(index);
		} else if (op == 5) {
			skipAssignments();
		}
		break;
	}
	case 6:
	case 7: {
		uint16 text = readWord();
		bool matched = evalCondition();
		const byte *data = _scenario.getData();
		const uint32 textPos = _scenario.getTextOffset() + text;
		const uint32 textEnd = _scenario.getTextOffset() + _scenario.getTextSize();
		bool hasText = textPos < textEnd && data[textPos];

		if (!matched) {
			skipAssignments();
			// An IF6 block owns the page terminator that follows it
			if (op == 6)
				_cur++;
		} else if (op == 6) {
			_deferred.push_back(_cur);
			if (hasText)
				showText(text);
			skipAssignments();
		} else {
			runAssignments();
			if (hasText)
				showText(text);
		}
		break;
	}
	case 8:
		newMenu();
		break;
	case 9: {
		int16 index = readWord();
		byte limit = readByte();
		if ((int8)limit <= counter(index))
			counter(index) = 0;
		break;
	}
	case 10: {
		int16 index = readWord();
		byte value = readByte();
		uint16 text = readWord();
		if ((byte)counter(index) == value)
			showText(text);
		break;
	}
	case 11:
		runExpression();
		break;
	case 0xfe:
		endPage();
		break;
	case 0xff: {
		if (_menus.empty() || _menus.back().empty()) {
			g_system->delayMillis(16);
			_cur = Scenario::kCommandsOffset;
			break;
		}
		// Cancelling returns to the previous menu level
		int selected = _vm->selectMenu(_menus.back(), _menus.size() > 1);
		if (selected == -2) {
			if (_menus.size() > 1)
				_menus.pop_back();
			_cur--;
		} else if (selected < 0) {
			_running = false;
		} else {
			_cur = _menus.back()[selected].body;
		}
		break;
	}
	default:
		warning("Xex: unknown script opcode 0x%02x at 0x%x in '%s'", op, _cur - 1, _scenario.getName().c_str());
		_running = false;
		break;
	}
}

} // End of namespace Xex
