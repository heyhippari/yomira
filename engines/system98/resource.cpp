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
#include "common/file.h"
#include "common/memstream.h"
#include "common/str-enc.h"
#include "common/textconsole.h"
#include "system98/resource.h"

namespace System98 {

bool Resources::decompress(Common::ReadStream &stream, Common::Array<byte> &data, uint32 size) {
	if (size > 16 * 1024 * 1024)
		return false;
	data.resize(size);
	byte dictionary[4096] = {};
	uint writePos = 1;
	uint32 output = 0;
	while (!stream.eos() && !stream.err()) {
		const byte flags = stream.readByte();
		for (uint mask = 1; mask <= 0x80; mask <<= 1) {
			if (flags & mask) {
				const byte value = stream.readByte();
				if (stream.eos() || stream.err() || output >= size)
					return false;
				data[output++] = value;
				dictionary[writePos] = value;
				writePos = (writePos + 1) & 0xfff;
			} else {
				const uint16 reference = stream.readUint16LE();
				if (stream.eos() || stream.err())
					return false;
				if (!reference)
					return output == size;
				const uint length = (reference & 0xf) + 3;
				if (length > size - output)
					return false;
				uint readPos = reference >> 4;
				for (uint count = 0; count < length; ++count) {
					const byte value = dictionary[readPos];
					readPos = (readPos + 1) & 0xfff;
					data[output++] = value;
					dictionary[writePos] = value;
					writePos = (writePos + 1) & 0xfff;
				}
			}
		}
	}
	return false;
}

bool decodeStringFile(const Common::Array<byte> &data, byte strings[2][80]) {
	byte decoded[2][80];
	memset(decoded, 0, sizeof(decoded));
	uint line = 0;
	uint column = 0;
	for (uint position = 0; position < data.size(); ++position) {
		const byte value = data[position];
		if (value == 0x1a) {
			if (line != 1)
				return false;
			memcpy(strings, decoded, sizeof(decoded));
			return true;
		}
		if (value == '\r' && position + 1 < data.size() && data[position + 1] == '\n') {
			++position;
			if (line == 1) {
				memcpy(strings, decoded, sizeof(decoded));
				return true;
			}
			line = 1;
			column = 0;
			continue;
		}
		if (column >= 79)
			return false;
		decoded[line][column++] = value;
	}
	if (line != 1)
		return false;
	memcpy(strings, decoded, sizeof(decoded));
	return true;
}

uint getStringLength(const byte *string, uint capacity) {
	uint length = 0;
	while (length < capacity && string[length])
		++length;
	return length;
}

uint appendString(byte *destination, const byte *source, uint capacity) {
	if (!capacity)
		return 0;
	uint destinationLength = getStringLength(destination, capacity);
	if (destinationLength == capacity) {
		destinationLength = capacity - 1;
		destination[destinationLength] = 0;
	}
	const uint sourceLength = getStringLength(source, capacity);
	const uint appendLength = MIN<uint>(sourceLength, capacity - 1 - destinationLength);
	memmove(destination + destinationLength, source, appendLength);
	destination[destinationLength + appendLength] = 0;
	return destinationLength + appendLength;
}

bool Resources::loadCatalog(char disk) {
	const Common::String catalogName = Common::String::format("DISK_%c.CAT", disk);
	const Common::String libraryName = Common::String::format("DISK_%c.LIB", disk);
	Common::File catalog, library;
	if (!catalog.open(Common::Path(catalogName)) || !library.open(Common::Path(libraryName)))
		return false;
	const uint32 signature = catalog.readUint32BE();
	const uint16 count = catalog.readUint16LE();
	if ((signature != MKTAG('C', 'a', 't', '0') && signature != MKTAG('C', 'a', 't', '1')) ||
	    library.readUint32BE() != MKTAG('L', 'i', 'b', '0') || library.readUint16LE() != count)
		return false;
	Common::Array<byte> table;
	const uint32 tableSize = count * 22;
	if (signature == MKTAG('C', 'a', 't', '1')) {
		if (!decompress(catalog, table, tableSize))
			return false;
	} else {
		table.resize(tableSize);
		if (catalog.read(table.data(), tableSize) != tableSize)
			return false;
	}
	uint unavailable = 0;
	for (uint index = 0; index < count; ++index) {
		const byte *record = table.data() + index * 22;
		Entry entry;
		entry.name = Common::String(reinterpret_cast<const char *>(record), 12);
		const uint terminator = entry.name.findFirstOf('\0');
		if (terminator != Common::String::npos)
			entry.name = entry.name.substr(0, terminator);
		entry.name.trim();
		entry.flags = READ_LE_UINT16(record + 12);
		entry.size = READ_LE_UINT32(record + 14);
		entry.offset = READ_LE_UINT32(record + 18);
		entry.library = libraryName;
		if (entry.name.empty() || (entry.flags & ~1))
			return false;
		if (entry.offset > uint64(library.size() - 6) || entry.size > uint64(library.size() - 6) - entry.offset)
			++unavailable;
		_entries.push_back(entry);
	}
	if (unavailable)
		warning("System98: %s is truncated; %u resources extend beyond the file", libraryName.c_str(), unavailable);
	debug(1, "System98: indexed %u resources from %s", count, catalogName.c_str());
	return !catalog.err() && !library.err();
}

bool Resources::init() {
	_entries.clear();
	for (char disk = 'A'; disk <= 'D'; ++disk) {
		if (!loadCatalog(disk)) {
			warning("System98: cannot load DISK_%c.CAT/LIB", disk);
			_entries.clear();
			return false;
		}
	}
	return true;
}

bool Resources::load(const Common::String &name, Common::Array<byte> &data) const {
	data.clear();
	Common::File file;
	const Common::String utf8Name = name.decode(Common::kWindows1252).encode(Common::kUtf8);
	if (file.open(Common::Path(name)) || (utf8Name != name && file.open(Common::Path(utf8Name)))) {
		if (file.size() < 0 || file.size() > 16 * 1024 * 1024)
			return false;
		data.resize(file.size());
		return file.read(data.data(), data.size()) == data.size() && !file.err();
	}
	for (const Entry &entry : _entries) {
		if (!entry.name.equalsIgnoreCase(name))
			continue;
		if (entry.size > 16 * 1024 * 1024 || !file.open(Common::Path(entry.library)))
			return false;
		if (file.size() < 6 || entry.offset > uint64(file.size() - 6) ||
		    entry.size > uint64(file.size() - 6) - entry.offset || !file.seek(6 + entry.offset))
			return false;
		Common::Array<byte> packed;
		packed.resize(entry.size);
		if (file.read(packed.data(), packed.size()) != packed.size() || file.err())
			return false;
		if (!(entry.flags & 1)) {
			data = packed;
			return true;
		}
		if (packed.size() < 4)
			return false;
		Common::MemoryReadStream stream(packed.data() + 4, packed.size() - 4);
		return decompress(stream, data, READ_LE_UINT32(packed.data()));
	}
	return false;
}

} // namespace System98
