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

#ifndef ENGINES_COMPLET_SCENARIO_H
#define ENGINES_COMPLET_SCENARIO_H

#include "common/array.h"
#include "common/str.h"
#include "common/types.h"

namespace Complet {

// An "MPX" scenario file ("Mp17" magic):
//   0x00  magic
//   0x04  u16 offset of the script text section
//   0x06  u16 size of the script text section (XOR 0x24 encoded)
//   0x08  u16 number of scenario-local variables
//   0x0a  u16 unused by the loader
//   0x0c  u16 unused by the loader
//   0x10  command stream (up to the script text section)
// Command stream jump targets are file offsets; text references are
// relative to the script text section.
class Scenario {
public:
	enum {
		kCommandsOffset = 0x10
	};

	bool load(const Common::String &name);

	const Common::String &getName() const { return _name; }
	const byte *getData() const { return _data.data(); }
	uint32 getSize() const { return _data.size(); }
	uint32 getTextOffset() const { return _textOffset; }
	uint32 getTextSize() const { return _textSize; }
	uint32 getLocalCount() const { return _localCount; }

private:
	Common::String _name;
	Common::Array<byte> _data;
	uint32 _textOffset = 0;
	uint32 _textSize = 0;
	uint32 _localCount = 0;
};

} // End of namespace Complet

#endif // ENGINES_COMPLET_SCENARIO_H
