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

#include "delta/script.h"
#include "common/endian.h"
#include "common/util.h"

namespace Delta {

uint16 ScriptReader::readWord() {
	if (_failed || _code.size() - _position < 2) {
		_failed = true;
		return 0;
	}
	const uint16 value = READ_LE_UINT16(&_code[_position]);
	_position += 2;
	return value;
}

uint32 ScriptReader::readDword() {
	if (_failed || _code.size() - _position < 4) {
		_failed = true;
		return 0;
	}
	const uint32 value = READ_LE_UINT32(&_code[_position]);
	_position += 4;
	return value;
}

Common::String ScriptReader::readString() {
	Common::String value;
	while (!_failed && _position < _code.size()) {
		const byte character = _code[_position++];
		if (!character)
			return value;
		value += character;
	}
	_failed = true;
	return Common::String();
}

bool ScriptReader::seek(uint32 position) {
	if (_failed || position >= _code.size()) {
		_failed = true;
		return false;
	}
	_position = position;
	return true;
}

bool ReadHistory::enterBlock(uint16 block) {
	const bool changed = _current >= 0 && !isRead(_current);
	if (changed)
		_read[_current >> 3] |= 1 << (_current & 7);
	_current = block;
	return changed;
}

bool ReadHistory::save(Common::WriteStream &stream) const {
	stream.writeUint32BE(MKTAG('D', 'L', 'R', 'D'));
	stream.writeByte(1);
	stream.write(_read, sizeof(_read));
	return !stream.err();
}

bool ReadHistory::load(Common::SeekableReadStream &stream) {
	byte read[sizeof(_read)];
	if (stream.readUint32BE() != MKTAG('D', 'L', 'R', 'D') || stream.readByte() != 1 ||
		stream.read(read, sizeof(read)) != sizeof(read))
		return false;
	memcpy(_read, read, sizeof(_read));
	return true;
}

bool isHaltOpcode(uint16 opcode) {
	// Table entries sharing native handler 0x40861d
	static const uint16 opcodes[] = {
		0x02, 0x03, 0x04, 0x15, 0x16, 0x1f, 0x4a, 0x4d, 0x4e, 0x4f,
		0x51, 0x5b, 0x6c, 0x7b, 0x82, 0x9b, 0xa9, 0xaa, 0xba, 0xbb
	};
	for (uint index = 0; index < ARRAYSIZE(opcodes); ++index) {
		if (opcodes[index] == opcode)
			return true;
	}
	return false;
}

bool Script::evaluateCondition() {
	Common::Array<uint16> tokens;
	while (!_reader.failed() && tokens.size() < 128) {
		const uint16 token = _reader.readWord();
		if (!(token & 0x8000)) {
			_reader.seek(_reader.position() - 2);
			break;
		}
		tokens.push_back(token & 0x7fff);
	}
	bool result = false;
	bool term = false;
	uint16 operation = 1;
	for (uint index = 0; index < tokens.size(); ++index) {
		const uint16 variable = tokens[index];
		if (variable >= ARRAYSIZE(_variables)) {
			_error = "Invalid condition variable";
			return false;
		}
		uint16 left = _variables[variable] & 0x7fff;
		term = left != 0;
		if (++index < tokens.size()) {
			uint16 code = tokens[index];
			if (code & 0x10) {
				if (++index >= tokens.size()) {
					_error = "Truncated comparison";
					return false;
				}
				uint16 right = tokens[index];
				if (right & 0x4000) {
					right &= 0x3fff;
					if (right >= ARRAYSIZE(_variables)) {
						_error = "Invalid comparison variable";
						return false;
					}
					right = _variables[right] & 0x7fff;
				}
				switch (code & 0xf) {
				case 2: term = left == right; break;
				case 3: term = left > right; break;
				case 4: term = left < right; break;
				case 5: term = left >= right; break;
				case 6: term = left <= right; break;
				case 7: term = left != right; break;
				default: _error = "Unsupported comparison"; return false;
				}
				code = ++index < tokens.size() ? tokens[index] : 2;
			}
			if (code & 8)
				term = !term;
			result = operation == 0 ? result && term : result || term;
			operation = code & 7;
		} else {
			result = operation == 0 ? result && term : result || term;
		}
	}
	if (tokens.empty() || tokens.size() == 128)
		_error = "Invalid condition length";
	return result;
}

bool Script::setVariable(uint16 variable, uint16 value) {
	if (!_error.empty() || variable >= ARRAYSIZE(_variables))
		return false;
	_variables[variable] = value;
	return true;
}

bool Script::saveState(Common::WriteStream &stream) const {
	if (!_error.empty() || _reader.failed())
		return false;
	stream.writeByte(3);
	stream.writeByte(_legacyOpcodes);
	stream.writeUint32LE(_reader.position());
	for (uint index = 0; index < ARRAYSIZE(_variables); ++index)
		stream.writeUint16LE(_variables[index]);
	stream.writeByte(_returnStack.size());
	for (uint index = 0; index < _returnStack.size(); ++index)
		stream.writeUint32LE(_returnStack[index]);
	for (uint index = 0; index < ARRAYSIZE(_checkpoints); ++index)
		stream.writeUint32LE(_checkpoints[index]);
	stream.write(_seenEvents, sizeof(_seenEvents));
	return !stream.err();
}

bool Script::loadState(Common::SeekableReadStream &stream) {
	if (stream.size() - stream.pos() < (int64)(5 + sizeof(_variables)))
		return false;
	const byte tag = stream.readByte();
	if (tag > 3)
		return false;
	const byte legacy = tag >= 2 ? stream.readByte() : tag;
	const uint32 position = stream.readUint32LE();
	uint16 variables[1024];
	for (uint index = 0; index < ARRAYSIZE(variables); ++index)
		variables[index] = stream.readUint16LE();
	Common::Array<uint32> returns;
	uint32 checkpoints[2048] = {};
	byte seenEvents[2048] = {};
	if (tag >= 2) {
		const byte depth = stream.readByte();
		if (depth > 64 || stream.size() - stream.pos() < (int64)(depth * 4 + sizeof(checkpoints)))
			return false;
		for (uint index = 0; index < depth; ++index) {
			const uint32 address = stream.readUint32LE();
			if (address >= _reader.size())
				return false;
			returns.push_back(address);
		}
		for (uint index = 0; index < ARRAYSIZE(checkpoints); ++index)
			checkpoints[index] = stream.readUint32LE();
	}
	if (tag == 3) {
		if (stream.read(seenEvents, sizeof(seenEvents)) != sizeof(seenEvents))
			return false;
	}
	if (stream.err() || legacy != byte(_legacyOpcodes) || position >= _reader.size())
		return false;
	if (!_reader.seek(position))
		return false;
	memcpy(_variables, variables, sizeof(_variables));
	memcpy(_checkpoints, checkpoints, sizeof(_checkpoints));
	memcpy(_seenEvents, seenEvents, sizeof(_seenEvents));
	_returnStack = returns;
	_error.clear();
	return true;
}

bool Script::next(ScriptCommand &command) {
	if (!_error.empty())
		return false;
	command = ScriptCommand();
	command.offset = _reader.position();
	command.opcode = _reader.readWord();
	if (_legacyOpcodes && command.opcode >= 5)
		++command.opcode;
	if (isHaltOpcode(command.opcode))
		return true;
	uint wordCount = 0;
	switch (command.opcode) {
	case 0x21: {
		const uint32 target = _reader.readDword();
		if (_returnStack.size() >= 64) {
			_error = "Script call stack overflow";
			return false;
		}
		_returnStack.push_back(_reader.position());
		command.arguments.push_back(target);
		if (target != 0xffffffff)
			_reader.seek(target);
		break;
	}
	case 0x8a:
		if (_returnStack.empty()) {
			_error = "Script call stack underflow";
			return false;
		}
		_reader.seek(_returnStack.back());
		_returnStack.pop_back();
		break;
	case 0x3a:
	case 0x3b: {
		const uint16 event = _reader.readWord();
		if (!event || event > ARRAYSIZE(_seenEvents)) {
			_error = "Invalid seen-event number";
			return false;
		}
		command.arguments.push_back(event);
		if (command.opcode == 0x3b) {
			_seenEvents[event - 1] = (event - 1) % 2 ? 2 : 1;
		} else {
			const uint16 variable = _reader.readWord();
			if (variable >= ARRAYSIZE(_variables)) {
				_error = "Invalid seen-event destination";
				return false;
			}
			_variables[variable] = _seenEvents[event - 1] != 0;
			command.arguments.push_back(variable);
		}
		break;
	}
	case 0x3f: {
		const uint16 checkpoint = _reader.readWord();
		const uint16 variable = _reader.readWord();
		if (checkpoint >= ARRAYSIZE(_checkpoints) || variable >= ARRAYSIZE(_variables)) {
			_error = "Invalid checkpoint query";
			return false;
		}
		if (!checkpoint)
			memset(_checkpoints, 0, sizeof(_checkpoints));
		_variables[variable] = _checkpoints[checkpoint];
		command.arguments.push_back(checkpoint);
		command.arguments.push_back(variable);
		break;
	}
	case 0x3e: {
		const uint16 checkpoint = _reader.readWord();
		if (checkpoint >= ARRAYSIZE(_checkpoints)) {
			_error = "Invalid checkpoint number";
			return false;
		}
		++_checkpoints[checkpoint];
		command.arguments.push_back(checkpoint);
		break;
	}
	case 0x61:
		command.strings.push_back(_reader.readString());
		break;
	case 0x5e: {
		const uint16 count = _reader.readWord();
		const uint16 variable = _reader.readWord();
		if (!count || count > 127 || variable >= ARRAYSIZE(_variables)) {
			_error = "Invalid random selection";
			return false;
		}
		if (!_random && count > 1) {
			_error = "Random source required for selection";
			return false;
		}
		_variables[variable] = count == 1 ? 1 : _random->getRandomNumber(count - 1) + 1;
		command.arguments.push_back(count);
		command.arguments.push_back(variable);
		break;
	}
	case 0x95:
	case 0x96: {
		const uint16 variable = _reader.readWord();
		const uint16 count = _reader.readWord();
		if (variable >= ARRAYSIZE(_variables) || count > 32 || _variables[variable] > count) {
			_error = "Invalid indexed branch";
			return false;
		}
		uint32 destination = 0;
		for (uint index = 0; index < count; ++index) {
			const uint32 offset = _reader.readDword();
			if (_variables[variable] == index + 1)
				destination = offset;
		}
		command.arguments.push_back(variable);
		command.arguments.push_back(count);
		if (_variables[variable])
			_reader.seek(destination);
		break;
	}
	case 0xb0:
		command.strings.push_back(_reader.readString());
		wordCount = 1;
		break;
	case 0xb1:
		command.strings.push_back(_reader.readString());
		command.strings.push_back(_reader.readString());
		wordCount = 1;
		break;
	case 0x4b: {
		const uint16 count = _reader.readWord();
		if (!count || count > 256) {
			_error = "Invalid message token count";
			return false;
		}
		command.arguments.push_back(count);
		for (uint index = 0; index < count && !_reader.failed(); ++index) {
			const Common::String token = _reader.readString();
			command.strings.push_back(token);
			if (!token.empty() && (token[0] == 'V' || token[0] == 'v')) {
				command.arguments.push_back(_reader.readWord());
				command.arguments.push_back(_reader.readDword());
			}
		}
		break;
	}
	case 0x98:
		command.strings.push_back(_reader.readString());
		break;
	case 0x2c: {
		for (uint index = 0; index < 4; ++index)
			command.strings.push_back(_reader.readString());
		for (uint index = 0; index < 6; ++index)
			command.arguments.push_back(_reader.readWord());
		// The native renderer (0x416740) always returns zero into the result variable.
		const uint16 variable = command.arguments[5];
		if (variable >= ARRAYSIZE(_variables)) {
			_error = Common::String::format("Invalid variable at script offset 0x%x", command.offset);
			return false;
		}
		_variables[variable] = 0;
		break;
	}
	case 0x22:
	case 0x3d: {
		const uint32 destination = _reader.readDword();
		command.arguments.push_back(destination);
		_reader.seek(destination);
		break;
	}
	case 0x23:
		command.strings.push_back(_reader.readString());
		wordCount = 7;
		break;
	case 0x26:
		command.strings.push_back(_reader.readString());
		wordCount = 3;
		break;
	case 0x3c: {
		const bool condition = evaluateCondition();
		command.arguments.push_back(condition);
		if (condition) {
			if (_reader.readWord() != (_legacyOpcodes ? 0x21 : 0x22)) {
				_error = "Expected conditional jump";
				return false;
			}
			_reader.readDword();
		}
		break;
	}
	case 0x44:
	case 0x9c:
		wordCount = 2;
		break;
	case 0x06:
	case 0x0d:
	case 0x48:
	case 0x49:
	case 0x54:
	case 0x5f:
	case 0x89:
	case 0x8d:
	case 0xa0:
	case 0xa5:
		break;
	case 0x9e:
		wordCount = 1;
		break;
	case 0x9f:
		wordCount = 3;
		break;
	case 0xa7:
		command.strings.push_back(_reader.readString());
		wordCount = 4;
		break;
	case 0x07:
	case 0x85:
	case 0x0f:
	case 0x10:
	case 0x14:
	case 0x1e:
	case 0x41:
	case 0x5c:
	case 0x60:
	case 0x67:
	case 0x84:
	case 0xa1:
	case 0xae:
		wordCount = 1;
		break;
	case 0x08:
		wordCount = 3;
		break;
	case 0x12:
	case 0x1c:
	case 0x99:
	case 0xa2:
		wordCount = 3;
		break;
	case 0x18:
	case 0x19:
	case 0xa4:
		wordCount = 7;
		break;
	case 0x91: {
		const uint16 variable = _reader.readWord();
		const uint16 value = _reader.readWord();
		if (variable >= ARRAYSIZE(_variables) || ((value & 0x8000) && (value & 0x7fff) >= ARRAYSIZE(_variables))) {
			_error = Common::String::format("Invalid variable at script offset 0x%x", command.offset);
			return false;
		}
		_variables[variable] = (value & 0x8000) ? _variables[value & 0x7fff] : value;
		command.arguments.push_back(variable);
		command.arguments.push_back(_variables[variable]);
		break;
	}
	case 0x8b:
	case 0x8c:
	case 0x8e:
	case 0x8f:
	case 0x90: {
		const uint16 variable = _reader.readWord();
		uint16 value = _reader.readWord();
		if (variable >= ARRAYSIZE(_variables) || ((value & 0x8000) && (value & 0x7fff) >= ARRAYSIZE(_variables))) {
			_error = Common::String::format("Invalid variable at script offset 0x%x", command.offset);
			return false;
		}
		if (value & 0x8000)
			value = _variables[value & 0x7fff];
		uint16 &target = _variables[variable];
		switch (command.opcode) {
		case 0x8b: target += value; break;
		case 0x8c: target = target < value ? 0 : target - value; break;
		case 0x8f: target *= value; break;
		// The native faults on a zero divisor
		case 0x8e: if (value) target /= value; break;
		default: if (value) target %= value; break;
		}
		command.arguments.push_back(variable);
		command.arguments.push_back(target);
		break;
	}
	case 0x92: {
		const uint16 chance = _reader.readWord();
		const uint16 variable = _reader.readWord();
		if (chance >= ARRAYSIZE(_variables) || variable >= ARRAYSIZE(_variables) || !_random) {
			_error = Common::String::format("Invalid percentage roll at script offset 0x%x", command.offset);
			return false;
		}
		_variables[variable] = _variables[chance] > _random->getRandomNumber(99);
		command.arguments.push_back(chance);
		command.arguments.push_back(variable);
		break;
	}
	case 0x93:
	case 0x94: {
		const uint16 base = _reader.readWord();
		const uint16 count = _reader.readWord();
		const uint16 first = _reader.readWord();
		const uint16 second = _reader.readWord();
		const uint span = MAX<uint>(count, command.opcode == 0x94 ? 2 : 1);
		if (base + span > ARRAYSIZE(_variables) || first >= ARRAYSIZE(_variables) || second >= ARRAYSIZE(_variables)) {
			_error = Common::String::format("Invalid variable range at script offset 0x%x", command.offset);
			return false;
		}
		const uint16 *values = &_variables[base];
		if (command.opcode == 0x93) {
			uint maximum = 0;
			for (uint index = 1; index < count; ++index) {
				if (values[index] > values[maximum])
					maximum = index;
			}
			_variables[first] = maximum + 1;
			_variables[second] = values[maximum];
		} else {
			// 0x406800 seeds from the first two variables, whatever the count
			uint maximum = values[0] >= values[1] ? 0 : 1;
			uint runnerUp = 1 - maximum;
			for (uint index = 2; index < count; ++index) {
				if (values[index] > values[maximum]) {
					runnerUp = maximum;
					maximum = index;
				} else if (values[index] > values[runnerUp]) {
					runnerUp = index;
				}
			}
			_variables[first] = maximum + 1;
			_variables[second] = runnerUp + 1;
		}
		command.arguments.push_back(base);
		command.arguments.push_back(count);
		break;
	}
	case 0x70:
		wordCount = 5;
		break;
	case 0x71:
	case 0x72:
	case 0x73:
	case 0x74:
	case 0x75:
	case 0x76:
	case 0x77:
	case 0x78:
	case 0x79:
	case 0x7f:
	case 0x80:
	case 0x81:
		wordCount = 6;
		break;
	case 0x7a:
	case 0x7e:
		wordCount = 8;
		break;
	case 0x7c:
		command.strings.push_back(_reader.readString());
		command.strings.push_back(_reader.readString());
		wordCount = 7;
		break;
	case 0x7d:
		wordCount = 11;
		break;
	case 0x83:
		for (uint index = 0; index < 3; ++index)
			command.arguments.push_back(_reader.readWord());
		command.arguments.push_back(_reader.readDword());
		break;
	default:
		_error = Common::String::format("Unsupported opcode 0x%04x at script offset 0x%x", command.opcode, command.offset);
		return false;
	}
	for (uint index = 0; index < wordCount; ++index)
		command.arguments.push_back(_reader.readWord());
	if (!_error.empty())
		return false;
	if (_reader.failed()) {
		_error = Common::String::format("Truncated opcode 0x%04x at script offset 0x%x", command.opcode, command.offset);
		return false;
	}
	return true;
}

} // End of namespace Delta
