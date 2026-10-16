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

#include "common/file.h"

#include "xex/scenario.h"

namespace Xex {

bool Scenario::load(const Common::String &name) {
	Common::File file;
	if (!file.open(Common::Path(name + ".mpx")))
		return false;

	uint32 size = file.size();
	if (size < kCommandsOffset)
		return false;

	Common::Array<byte> data;
	data.resize(size);
	if (file.read(data.data(), size) != size)
		return false;

	if (READ_BE_UINT32(data.data()) != MKTAG('M', 'p', '1', '7'))
		return false;

	uint32 textOffset = READ_LE_UINT16(data.data() + 4);
	uint32 textSize = READ_LE_UINT16(data.data() + 6);
	uint32 localCount = READ_LE_UINT16(data.data() + 8);
	if (textOffset < kCommandsOffset || textOffset + textSize > size)
		return false;

	for (uint32 i = 0; i < textSize; i++)
		data[textOffset + i] ^= 0x24;

	_data = data;
	_textOffset = textOffset;
	_textSize = textSize;
	_localCount = localCount;
	_name = name;
	return true;
}

} // End of namespace Xex
