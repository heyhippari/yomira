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

#include "common/bitstream.h"
#include "common/endian.h"
#include "system98/image.h"

namespace System98 {

bool captureBlitPattern(const Graphics::Surface &source, uint columns, Common::Array<byte> &pattern) {
	if (!source.getPixels() || source.format.bytesPerPixel != 1 || source.h < 8 || !columns || columns > uint(source.w / 8))
		return false;
	pattern.resize(columns * 8);
	for (uint column = 0; column < columns; ++column)
		for (uint row = 0; row < 8; ++row) {
			byte mask = 0;
			for (uint bit = 0; bit < 8; ++bit)
				if (*static_cast<const byte *>(source.getBasePtr(column * 8 + bit, row)) & 8)
					mask |= 0x80 >> bit;
			pattern[column * 8 + row] = mask;
		}
	return true;
}

bool applyBlitPattern(const Graphics::Surface &source, Graphics::Surface &destination, const Common::Rect &sourceRect, const Common::Point &position, const Common::Array<byte> &pattern, uint stage) {
	if (!source.getPixels() || !destination.getPixels() || source.format.bytesPerPixel != 1 || destination.format.bytesPerPixel != 1 ||
	    pattern.empty() || (pattern.size() & 7) || stage >= pattern.size() / 8 || sourceRect.isEmpty())
		return false;
	Common::Rect sourceArea = sourceRect;
	sourceArea.clip(Common::Rect(source.w, source.h));
	if (sourceArea.isEmpty())
		return true;
	Common::Rect destinationArea(position.x + sourceArea.left - sourceRect.left, position.y + sourceArea.top - sourceRect.top,
	                             position.x + sourceArea.right - sourceRect.left, position.y + sourceArea.bottom - sourceRect.top);
	destinationArea.clip(Common::Rect(destination.w, destination.h));
	for (int y = destinationArea.top; y < destinationArea.bottom; ++y) {
		const int row = y - position.y;
		const byte mask = pattern[stage * 8 + (row & 7)];
		for (int x = destinationArea.left; x < destinationArea.right; ++x) {
			const int column = x - position.x;
			if (mask & (0x80 >> (column & 7)))
				*static_cast<byte *>(destination.getBasePtr(x, y)) = *static_cast<const byte *>(source.getBasePtr(sourceRect.left + column, sourceRect.top + row));
		}
	}
	return true;
}

bool drawWindowTiles(const Graphics::Surface &atlas, Graphics::Surface &destination, const Common::Rect &frame, uint style, bool transparent, const byte *preserveMask, byte colorTag) {
	if (!atlas.getPixels() || !destination.getPixels() || atlas.format.bytesPerPixel != 1 || destination.format.bytesPerPixel != 1 ||
	    atlas.h < 48 || style >= uint(atlas.w / 48) || frame.width() < 32 || frame.height() < 32)
		return false;
	Common::Rect clipped = frame;
	clipped.clip(Common::Rect(destination.w, destination.h));
	for (int y = clipped.top; y < clipped.bottom; ++y) {
		const int row = y - frame.top;
		const int tileY = row < 16 ? 0 : row >= frame.height() - 16 ? 2 : 1;
		for (int x = clipped.left; x < clipped.right; ++x) {
			if (preserveMask && preserveMask[y * destination.w + x])
				continue;
			const int column = x - frame.left;
			const int tileX = column < 16 ? 0 : column >= frame.width() - 16 ? 2 : 1;
			const byte color = *static_cast<const byte *>(atlas.getBasePtr(style * 48 + tileX * 16 + (column & 15), tileY * 16 + (row & 15)));
			if (!transparent || color)
				*static_cast<byte *>(destination.getBasePtr(x, y)) = colorTag | color;
		}
	}
	return true;
}

bool PiImage::load(const Common::Array<byte> &data) {
	_surface.free();
	if (data.size() <= 52)
		return false;
	const uint width = READ_BE_UINT16(data.data());
	const uint height = READ_BE_UINT16(data.data() + 2);
	if (!width || (width & 1) || !height || width > 4096 || height > 4096)
		return false;
	const uint32 size = width * height;
	Common::Array<byte> pixels(size);
	Common::BitStreamMemoryStream stream(data.data() + 52, data.size() - 52);
	Common::BitStreamMemory8MSB bits(stream);
	bool valid = true;
	auto readBits = [&](uint count) -> uint32 {
		if (count > bits.size() - bits.pos()) {
			valid = false;
			return 0;
		}
		return bits.getBits(count);
	};
	byte table[16][16];
	for (uint color = 0; color < 16; ++color)
		for (uint rank = 0; rank < 16; ++rank)
			table[color][rank] = (color - rank) & 15;
	uint32 output = 0;
	byte previous = 0;
	auto delta = [&]() {
		uint rank;
		if (readBits(1))
			rank = readBits(1);
		else if (!readBits(1))
			rank = 2 + readBits(1);
		else if (!readBits(1))
			rank = 4 + readBits(2);
		else
			rank = 8 + readBits(3);
		const byte color = table[previous][rank];
		memmove(table[previous] + 1, table[previous], rank);
		table[previous][0] = color;
		pixels[output++] = color;
		previous = color;
	};
	auto location = [&]() -> uint {
		const uint value = readBits(2);
		return value == 3 ? 3 + readBits(1) : value;
	};
	bool first = true;
	while (output < size && valid) {
		delta();
		delta();
		if (output == size)
			break;
		if (!first && readBits(1))
			continue;
		uint code = location();
		while (valid) {
			uint lengthBits = 0;
			while (readBits(1) && valid) {
				if (++lengthBits > 23)
					return false;
			}
			uint32 pairs = (1U << lengthBits) + readBits(lengthBits);
			if (first) {
				--pairs;
				first = false;
			}
			if (pairs > (size - output) / 2)
				return false;
			int source = output;
			switch (code) {
			case 0: source -= output >= 4 && pixels[output - 1] != pixels[output - 2] ? 4 : 2; break;
			case 1: source -= width; break;
			case 2: source -= width * 2; break;
			case 3: source -= width - 1; break;
			case 4: source -= width + 1; break;
			}
			for (uint32 count = 0; count < pairs * 2; ++count, ++source) {
				if (source >= int(output))
					return false;
				pixels[output++] = pixels[source < 0 ? (source & 1) : source];
			}
			previous = pixels[output - 1];
			if (output == size)
				break;
			const uint next = location();
			if (next == code)
				break;
			code = next;
		}
	}
	if (!valid || output != size)
		return false;
	for (uint component = 0; component < 48; ++component)
		_palette[component] = (data[4 + component] >> 4) * 17;
	_surface.create(width, height, Graphics::PixelFormat::createFormatCLUT8());
	for (uint row = 0; row < height; ++row)
		memcpy(_surface.getBasePtr(0, row), pixels.data() + row * width, width);
	return true;
}

} // namespace System98
