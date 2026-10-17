/* ScummVM - Graphic Adventure Engine
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

#include "illusion/archive.h"

#include "common/endian.h"

#include <string.h>

namespace Illusion {

static const uint32 kTableHeaderSize = 0x800;
static const uint32 kTableEntrySize = 0x50;
static const uint32 kTableSizeMask = 0xa67f54cb;
static const uint32 kPayloadXor = 0x35d8ca2f;
static const uint32 kMaximumTableSize = 16 * 1024 * 1024;

IllusionArchive::IllusionArchive() : _negated(false) {
	for (uint i = 0; i < ARRAYSIZE(_groupStart); ++i) {
		_groupStart[i] = 0;
		_groupCount[i] = 0;
	}
}

IllusionArchive::~IllusionArchive() {
	_package.close();
}

void IllusionArchive::close() {
	_package.close();
	_entries.clear();
	_negated = false;
	for (uint group = 0; group < ARRAYSIZE(_groupStart); ++group) {
		_groupStart[group] = 0;
		_groupCount[group] = 0;
	}
}

bool IllusionArchive::decompressTable(const Common::Array<byte> &packed, Common::Array<byte> &table,
		uint32 &decodedSize) {
	if (packed.size() < 4)
		return false;

	const uint32 unpackedSize = READ_LE_UINT32(packed.data()) ^ kTableSizeMask;
	if (unpackedSize < kTableHeaderSize || unpackedSize > kMaximumTableSize)
		return false;

	const uint32 packedSize = packed.size() - 4;
	Common::Array<byte> encoded(packedSize, 0);
	if (packedSize)
		memcpy(encoded.data(), packed.data() + 4, packedSize);
	for (uint32 offset = 0; offset + 4 <= packedSize; offset += 4)
		WRITE_LE_UINT32(encoded.data() + offset, READ_LE_UINT32(encoded.data() + offset) ^ kPayloadXor);

	table.resize(unpackedSize, 0);
	byte ring[0x1000] = { 0 };
	uint32 ringPosition = 0xfee;
	uint32 inputPosition = 0;
	uint32 bitBuffer = 0;
	decodedSize = 0;

	while (decodedSize < unpackedSize) {
		bitBuffer >>= 1;
		if ((bitBuffer & 0x100) == 0) {
			if (inputPosition >= packedSize)
				break;
			bitBuffer = 0xff00 | encoded[inputPosition++];
		}

		if (bitBuffer & 1) {
			if (inputPosition >= packedSize)
				break;
			const byte value = encoded[inputPosition++];
			table[decodedSize++] = value;
			ring[ringPosition] = value;
			ringPosition = (ringPosition + 1) & 0xfff;
		} else {
			if (inputPosition + 1 >= packedSize)
				break;
			const uint32 backReference = encoded[inputPosition] |
				((encoded[inputPosition + 1] & 0xf0) << 4);
			const uint32 length = (encoded[inputPosition + 1] & 0x0f) + 3;
			inputPosition += 2;
			for (uint32 i = 0; i < length && decodedSize < unpackedSize; ++i) {
				const byte value = ring[(backReference + i) & 0xfff];
				table[decodedSize++] = value;
				ring[ringPosition] = value;
				ringPosition = (ringPosition + 1) & 0xfff;
			}
		}
	}

	return decodedSize >= kTableHeaderSize;
}

bool IllusionArchive::open(const Common::Path &packagePath, const Common::Path &tablePath) {
	close();
	if (!loadTable(packagePath, tablePath)) {
		close();
		return false;
	}
	return true;
}

bool IllusionArchive::loadTable(const Common::Path &packagePath, const Common::Path &tablePath) {
	if (!_package.open(packagePath))
		return false;
	const int64 packageSize = _package.size();
	if (packageSize <= 0 || packageSize > 0xffffffffLL)
		return false;

	Common::File tableFile;
	if (!tableFile.open(tablePath))
		return false;
	const int64 tableFileSize = tableFile.size();
	if (tableFileSize < 4 || tableFileSize > kMaximumTableSize)
		return false;

	Common::Array<byte> packed((uint32)tableFileSize, 0);
	if (tableFile.read(packed.data(), (uint32)tableFileSize) != (uint32)tableFileSize)
		return false;

	Common::Array<byte> table;
	uint32 decodedSize = 0;
	if (!decompressTable(packed, table, decodedSize) || decodedSize < kTableHeaderSize)
		return false;
	const uint32 entryCount = (decodedSize - kTableHeaderSize) / kTableEntrySize;

	uint32 directoryEntryCount = 0;
	for (uint32 group = 0; group < ARRAYSIZE(_groupStart); ++group) {
		const uint32 byteOffset = READ_LE_UINT32(table.data() + group * 8);
		const uint32 count = READ_LE_UINT32(table.data() + group * 8 + 4);
		if (byteOffset % kTableEntrySize != 0)
			return false;
		const uint32 start = byteOffset / kTableEntrySize;
		if (start > entryCount || count > entryCount - start)
			return false;
		_groupStart[group] = start;
		_groupCount[group] = count;
		if (count)
			directoryEntryCount = MAX(directoryEntryCount, start + count);
	}

	_entries.resize(directoryEntryCount);
	for (uint32 i = 0; i < directoryEntryCount; ++i) {
		const byte *record = table.data() + kTableHeaderSize + i * kTableEntrySize;
		Entry &entry = _entries[i];
		entry.offset = READ_LE_UINT32(record + 4);
		entry.size = READ_LE_UINT32(record + 8);
		char name[65];
		memcpy(name, record + 0x10, 64);
		name[64] = 0;
		entry.name = name;
		if (entry.name.empty())
			continue;
		if (entry.offset > (uint64)packageSize || entry.size > (uint64)packageSize - entry.offset)
			return false;
	}

	return true;
}

bool IllusionArchive::openEmbedded(const Common::Path &packagePath) {
	close();
	if (!loadEmbedded(packagePath)) {
		close();
		return false;
	}
	return true;
}

bool IllusionArchive::loadEmbedded(const Common::Path &packagePath) {
	_negated = true;
	if (!_package.open(packagePath) || _package.size() < 8)
		return false;
	const uint64 packageSize = _package.size();
	const uint32 count = _package.readUint32LE();
	if (count > (packageSize - 8) / 36 || count > 65536 || !_package.seek(8))
		return false;

	Common::Array<Entry> entries;
	entries.resize(count);
	for (uint32 index = 0; index < count; ++index) {
		char name[33];
		if (_package.read(name, 32) != 32)
			return false;
		for (uint32 position = 0; position < 32; ++position)
			name[position] = (char)(byte)(0 - (byte)name[position]);
		name[32] = 0;
		entries[index].name = name;
	}

	uint64 offset = 8 + (uint64)count * 36;
	for (uint32 index = 0; index < count; ++index) {
		Entry &entry = entries[index];
		entry.size = _package.readUint32LE();
		if (_package.err() || offset > 0xffffffffULL || entry.size > packageSize - offset)
			return false;
		entry.offset = (uint32)offset;
		offset += entry.size;
	}
	if (offset != packageSize)
		return false;
	_entries = entries;
	return true;
}

bool IllusionArchive::readMember(const Common::String &name, Common::Array<byte> &data) {
	data.clear();
	if (!_package.isOpen() || name.empty())
		return false;
	const byte first = (byte)name[0];
	const uint32 group = (first >= 'A' && first <= 'Z') ? first - 'A' + 'a' : first;
	if (group >= ARRAYSIZE(_groupStart))
		return false;

	const uint32 start = _negated ? 0 : _groupStart[group];
	const uint32 end = _negated ? _entries.size() : start + _groupCount[group];
	if (start > _entries.size() || end < start || end > _entries.size())
		return false;
	for (uint32 i = start; i < end; ++i) {
		const Entry &entry = _entries[i];
		if (!entry.name.equalsIgnoreCase(name))
			continue;
		if (entry.size == 0)
			return true;
		_package.clearErr();
		if (!_package.seek(entry.offset))
			return false;
		data.resize(entry.size, 0);
		if (_package.read(data.data(), entry.size) != entry.size || _package.err()) {
			data.clear();
			return false;
		}
		if (_negated) {
			for (uint32 position = 0; position < data.size(); ++position)
				data[position] = (byte)(0 - data[position]);
		}
		return true;
	}
	return false;
}

} // End of namespace Illusion
