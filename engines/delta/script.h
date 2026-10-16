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

#ifndef DELTA_SCRIPT_H
#define DELTA_SCRIPT_H

#include "common/array.h"
#include "common/str.h"
#include "common/random.h"

namespace Delta {

class ScriptReader {
public:
	explicit ScriptReader(const Common::Array<byte> &code) : _code(code) {}
	uint16 readWord();
	uint32 readDword();
	Common::String readString();
	bool seek(uint32 position);
	uint32 position() const { return _position; }
	bool failed() const { return _failed; }

private:
	const Common::Array<byte> &_code;
	uint32 _position = 0;
	bool _failed = false;
};

struct ScriptCommand {
	uint16 opcode = 0;
	uint32 offset = 0;
	Common::Array<int32> arguments;
	Common::Array<Common::String> strings;
};

class Script {
public:
	explicit Script(const Common::Array<byte> &code, bool legacyOpcodes = false, Common::RandomSource *random = nullptr) : _reader(code), _legacyOpcodes(legacyOpcodes), _random(random) {}
	bool next(ScriptCommand &command);
	const Common::String &error() const { return _error; }
	uint32 position() const { return _reader.position(); }

private:
	ScriptReader _reader;
	bool _legacyOpcodes;
	Common::RandomSource *_random;
	uint16 _variables[1024] = {};
	Common::String _error;
	bool evaluateCondition();
};

} // End of namespace Delta

#endif
