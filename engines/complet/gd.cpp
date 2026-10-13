#include "common/file.h"

#include "complet/gd.h"

namespace Complet {

namespace {

const uint32 kPixelCount = kScreenWidth * kScreenHeight;
const uint32 kBufferSize = kPixelCount * 3;
const uint32 kGd3DataOffset = 0x3756;

// MSB-first bit reader
class BitReader {
public:
	BitReader(const byte *data, uint32 size, uint32 pos) : _data(data), _size(size), _pos(pos) {}

	bool overrun() const { return _pos >= _size; }

	uint32 readBit() {
		if (_pos >= _size)
			return 0;
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

bool GdImage::load(Common::SeekableReadStream &stream) {
	uint32 size = stream.size();
	Common::Array<byte> data;
	data.resize(size);
	if (stream.read(data.data(), size) != size)
		return false;

	// Only the "GD3" flavour has a fixed header; the type byte precedes the payload
	uint32 pos = (size > 3 && data[0] == 'G' && data[1] == 'D' && data[2] == '3') ? kGd3DataOffset : 2;
	if (pos > size)
		return false;

	switch (data[pos - 2]) {
	case 'b':
		if (size < kGd3DataOffset + kBufferSize)
			return false;
		memcpy(_pixels.data(), data.data() + kGd3DataOffset, kBufferSize);
		return true;
	case 'l':
		return decodeLz(data.data(), size, pos);
	case 'p':
		return decodePredictive(data.data(), size, pos);
	default:
		return false;
	}
}

// LZSS with a 64KB ring buffer whose write position starts at 1
bool GdImage::decodeLz(const byte *data, uint32 size, uint32 pos) {
	BitReader bits(data, size, pos);
	byte *out = _pixels.data();
	byte *window = new byte[0x10000]();
	uint32 wpos = 1, opos = 0;

	while (!bits.overrun()) {
		if (bits.readBit()) {
			byte b = bits.readBits(8);
			if (opos >= kBufferSize)
				break;
			out[opos++] = b;
			window[wpos] = b;
			wpos = (wpos + 1) & 0xffff;
		} else {
			uint32 offset = bits.readBits(16);
			if (offset == 0)
				break;
			uint32 length = bits.readBits(4) + 3;
			for (uint32 i = 0; i < length && opos < kBufferSize; i++) {
				byte b = window[(offset + i) & 0xffff];
				out[opos++] = b;
				window[wpos] = b;
				wpos = (wpos + 1) & 0xffff;
			}
		}
	}

	delete[] window;
	return opos == kBufferSize;
}

// Sparse pixel stream: each record places a colour at a skip distance and
// optionally repeats it at neighbouring pixels. Unwritten (white) pixels are
// filled from the previous pixel afterwards.
bool GdImage::decodePredictive(const byte *data, uint32 size, uint32 pos) {
	BitReader bits(data, size, pos);
	byte *out = _pixels.data();
	memset(out, 0xff, kBufferSize);

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
		if (p >= kPixelCount)
			return false;
		memcpy(out + p * 3, color, 3);

		if (bits.readBit()) {
			uint32 q = p;
			for (;;) {
				switch (bits.readBits(2)) {
				case 0:
					if (!bits.readBit())
						goto recordDone;
					q += bits.readBit() ? 802 : 798;
					break;
				case 1:
					q += 799;
					break;
				case 2:
					q += 800;
					break;
				default:
					q += 801;
					break;
				}
				if (q >= kPixelCount || bits.overrun())
					return false;
				memcpy(out + q * 3, color, 3);
			}
		}
recordDone:
		cur = p + 1;
	}

	byte last[3] = {0, 0, 0};
	for (uint32 i = 0; i < kPixelCount; i++) {
		byte *px = out + i * 3;
		if (px[0] == 0xff && px[1] == 0xff && px[2] == 0xff)
			memcpy(px, last, 3);
		else
			memcpy(last, px, 3);
	}
	return true;
}

} // End of namespace Complet
