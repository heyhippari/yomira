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

#ifndef SYSTEM98_RESOURCE_H
#define SYSTEM98_RESOURCE_H

#include "common/array.h"
#include "common/str.h"

namespace Common {
class ReadStream;
}

namespace System98 {

class Resources {
public:
	bool init();
	bool load(const Common::String &name, Common::Array<byte> &data) const;
	static bool decompress(Common::ReadStream &stream, Common::Array<byte> &data, uint32 size);

private:
	struct Entry {
		Common::String name;
		Common::String library;
		uint32 offset;
		uint32 size;
		uint16 flags;
	};

	Common::Array<Entry> _entries;
	bool loadCatalog(char disk);
};

bool decodeStringFile(const Common::Array<byte> &data, byte strings[2][80]);
uint getStringLength(const byte *string, uint capacity);
uint appendString(byte *destination, const byte *source, uint capacity);

} // namespace System98

#endif
