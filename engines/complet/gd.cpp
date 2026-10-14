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

#include "complet/gd.h"

namespace Complet {

namespace {

const uint32 kPixelCount = kScreenWidth * kScreenHeight;
const uint32 kBufferSize = kPixelCount * 3;
const uint32 kGd2DataOffset = 0x2346;
const uint32 kGd3DataOffset = 0x3756;

// MSB-first bit reader
class BitReader {
public:
	BitReader(const byte *data, uint32 size, uint32 pos) : _data(data), _size(size), _pos(pos) {}

	bool overrun() const { return _overrun; }

	uint32 readBit() {
		if (_pos >= _size) {
			_overrun = true;
			return 0;
		}
		uint32 bit = (_data[_pos] & _mask) ? 1 : 0;
		_mask >>= 1;
		if (!_mask) {
			_mask = 0x80;
			_pos++;
		}
		return bit;
	}

	uint32 readBits(int count) {
		uint32 value = 0;
		while (count--)
			value = (value << 1) | readBit();
		return value;
	}

private:
	const byte *_data;
	uint32 _size;
	uint32 _pos;
	uint32 _mask = 0x80;
	bool _overrun = false;
};

} // End of anonymous namespace

GdImage::GdImage() {
	_pixels.resize(kBufferSize);
}

bool GdImage::loadFromName(const Common::String &name) {
	Common::File file;
	if (!file.open(Common::Path(name)))
		return false;
	return load(file);
}

// The header holds 59 preview rows; the original zero-fills the missing top row
bool GdImage::loadThumbnail(const Common::String &name, Common::Array<byte> &out) {
	Common::File file;
	if (!file.open(Common::Path(name + ".gd")))
		return false;

	byte magic[3];
	if (file.read(magic, sizeof(magic)) != sizeof(magic))
		return false;
	if (memcmp(magic, "GD3", sizeof(magic))) {
		if (!file.seek(0))
			return false;
		GdImage image;
		if (!image.load(file))
			return false;

		const int thumbWidth = image.getWidth() / 10;
		const int thumbHeight = image.getHeight() / 10;
		out.resize(thumbWidth * thumbHeight * 3);
		const byte *pixels = image.getPixels();
		for (int y = 0; y < thumbHeight; y++) {
			const byte *src = pixels + (y * 10 * image.getWidth()) * 3;
			byte *dst = out.data() + y * thumbWidth * 3;
			for (int x = 0; x < thumbWidth; x++)
				memcpy(dst + x * 3, src + x * 10 * 3, 3);
		}
		return true;
	}

	const uint32 kRowsBytes = 0x3750;
	out.resize(kThumbWidth * kThumbHeight * 3);
	memset(out.data(), 0, out.size());
	file.seek(4);
	return file.read(out.data(), kRowsBytes) == kRowsBytes;
}

bool GdImage::load(Common::SeekableReadStream &stream) {
	uint32 size = stream.size();
	Common::Array<byte> data;
	data.resize(size);
	if (stream.read(data.data(), size) != size)
		return false;

	uint32 pos = 2;
	bool gd3 = size >= 3 && !memcmp(data.data(), "GD3", 3);
	bool gd2 = size >= 3 && !memcmp(data.data(), "GD2", 3);
	const uint32 width = gd2 ? (uint32)kGd2Width : (uint32)kScreenWidth;
	const uint32 height = gd2 ? (uint32)kGd2Height : (uint32)kScreenHeight;
	if (gd3)
		pos = kGd3DataOffset;
	else if (gd2)
		pos = kGd2DataOffset;
	if (pos > size)
		return false;

	Common::Array<byte> pixels(width * height * 3);
	bool decoded = false;
	switch (data[pos - 2]) {
	case 'b':
		if (!gd3)
			return false;
		if (size < kGd3DataOffset + pixels.size())
			return false;
		memcpy(pixels.data(), data.data() + kGd3DataOffset, pixels.size());
		decoded = true;
		break;
	case 'l':
		decoded = decodeLz(data.data(), size, pos, pixels);
		break;
	case 'p':
		decoded = decodePredictive(data.data(), size, pos, width, height, pixels);
		break;
	default:
		return false;
	}
	if (!decoded)
		return false;
	_isGd2 = gd2;
	_pixels = pixels;
	return true;
}

// LZSS with a 64KB ring buffer whose write position starts at 1
bool GdImage::decodeLz(const byte *data, uint32 size, uint32 pos, Common::Array<byte> &pixels) {
	BitReader bits(data, size, pos);
	byte *out = pixels.data();
	byte *window = new byte[0x10000]();
	uint32 wpos = 1, opos = 0;
	const uint32 bufferSize = pixels.size();

	while (!bits.overrun() && opos < bufferSize) {
		if (bits.readBit()) {
			byte b = bits.readBits(8);
			if (opos >= bufferSize)
				break;
			out[opos++] = b;
			window[wpos] = b;
			wpos = (wpos + 1) & 0xffff;
		} else {
			uint32 offset = bits.readBits(16);
			if (offset == 0)
				break;
			uint32 length = bits.readBits(4) + 3;
			for (uint32 i = 0; i < length && opos < bufferSize; i++) {
				byte b = window[(offset + i) & 0xffff];
				out[opos++] = b;
				window[wpos] = b;
				wpos = (wpos + 1) & 0xffff;
			}
		}
	}

	delete[] window;
	return opos == bufferSize;
}

// Sparse pixel stream: each record places a colour at a skip distance and
// optionally repeats it at neighbouring pixels. Unwritten (white) pixels are
// filled from the previous pixel afterwards.
bool GdImage::decodePredictive(const byte *data, uint32 size, uint32 pos, uint32 width, uint32 height,
		Common::Array<byte> &pixels) {
	BitReader bits(data, size, pos);
	byte *out = pixels.data();
	const uint32 pixelCount = width * height;
	memset(out, 0xff, pixels.size());

	uint32 cur = 0;
	while (!bits.overrun()) {
		uint32 skip = bits.readBits(2);
		if (skip == 2) {
			skip = bits.readBits(2) + 2;
		} else if (skip == 3) {
			int extra = 3;
			while (bits.readBit() && !bits.overrun())
				extra++;
			if (extra > 0x17)
				break;
			skip = (1u << extra) - 2 + bits.readBits(extra);
		}

		uint32 p = cur + skip;
		byte color[3];
		for (int i = 0; i < 3; i++)
			color[i] = bits.readBits(8);
		if (p >= pixelCount)
			return false;
		memcpy(out + p * 3, color, 3);

		if (bits.readBit()) {
			uint32 q = p;
			for (;;) {
				switch (bits.readBits(2)) {
				case 0:
					if (!bits.readBit())
						goto recordDone;
					q += bits.readBit() ? width + 2 : width - 2;
					break;
				case 1:
					q += width - 1;
					break;
				case 2:
					q += width;
					break;
				default:
					q += width + 1;
					break;
				}
				if (q >= pixelCount || bits.overrun())
					return false;
				memcpy(out + q * 3, color, 3);
			}
		}
recordDone:
		cur = p + 1;
	}

	byte last[3] = {0, 0, 0};
	for (uint32 i = 0; i < pixelCount; i++) {
		byte *px = out + i * 3;
		if (px[0] == 0xff && px[1] == 0xff && px[2] == 0xff)
			memcpy(px, last, 3);
		else
			memcpy(last, px, 3);
	}
	return true;
}

} // End of namespace Complet
