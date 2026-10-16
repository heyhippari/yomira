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

#include "delta/data.h"
#include "common/memstream.h"
#include "graphics/surface.h"
#include "image/bmp.h"

namespace Delta {

static bool readWithProgress(Common::ReadStream &stream, byte *data, uint32 size, LoadProgress *progress) {
	for (uint32 position = 0; position < size;) {
		if (progress && !(*progress)())
			return false;
		const uint32 length = MIN<uint32>(16384, size - position);
		if (stream.read(data + position, length) != length || stream.err())
			return false;
		position += length;
	}
	return true;
}

class ProgressReadStream : public Common::SeekableReadStream {
public:
	ProgressReadStream(Common::SeekableReadStream &stream, LoadProgress *progress) : _stream(stream), _progress(progress) {}
	uint32 read(void *data, uint32 size) override {
		const int64 start = _stream.pos();
		if (!_cancelled && !readWithProgress(_stream, static_cast<byte *>(data), size, _progress))
			_cancelled = true;
		return _stream.pos() - start;
	}
	bool eos() const override { return _cancelled || _stream.eos(); }
	bool err() const override { return _cancelled || _stream.err(); }
	void clearErr() override { _stream.clearErr(); }
	int64 pos() const override { return _stream.pos(); }
	int64 size() const override { return _stream.size(); }
	bool seek(int64 offset, int whence = SEEK_SET) override { return !_cancelled && _stream.seek(offset, whence); }

private:
	Common::SeekableReadStream &_stream;
	LoadProgress *_progress;
	bool _cancelled = false;
};

bool readPackedData(Common::SeekableReadStream &stream, Common::Array<byte> &data, LoadProgress *progress) {
	data.clear();
	if (stream.size() - stream.pos() < 4)
		return false;

	const uint32 header = stream.readUint32LE();
	const uint32 size = header & ((header & 0x80000000) ? 0x7fffffff : 0x3fffffff);
	if (!size || size > 16 * 1024 * 1024)
		return false;
	data.resize(size);
	uint32 nextProgress = 0;

	if (header & 0x80000000) {
		uint32 position = 0;
		while (position < size) {
			if (progress && position >= nextProgress) {
				if (!(*progress)())
					return false;
				nextProgress = position + 16384;
			}
			if (stream.size() - stream.pos() < 2)
				return false;
			const byte count = stream.readByte();
			const byte value = stream.readByte();
			const uint32 length = count ? count : value;
			if (!length || length > size - position)
				return false;
			if (count) {
				memset(&data[position], value, length);
			} else if (stream.read(&data[position], length) != length) {
				return false;
			}
			position += length;
		}
		return !stream.err();
	}

	if (header & 0x40000000)
		return readWithProgress(stream, data.begin(), size, progress);

	byte window[4096] = {};
	uint32 windowPosition = 0xfee;
	uint32 position = 0;
	uint16 flags = 0;
	while (position < size) {
		if (progress && position >= nextProgress) {
			if (!(*progress)())
				return false;
			nextProgress = position + 16384;
		}
		flags >>= 1;
		if (!(flags & 0x100))
			flags = stream.readByte() | 0xff00;
		if (flags & 1) {
			const byte value = stream.readByte();
			if (stream.eos() || stream.err())
				return false;
			data[position++] = value;
			window[windowPosition] = value;
			windowPosition = (windowPosition + 1) & 0xfff;
		} else {
			uint32 offset = stream.readByte();
			const byte code = stream.readByte();
			if (stream.eos() || stream.err())
				return false;
			offset |= (code & 0xf0) << 4;
			const uint32 length = (code & 0xf) + 3;
			for (uint32 index = 0; index < length && position < size; ++index) {
				const byte value = window[(offset + index) & 0xfff];
				data[position++] = value;
				window[windowPosition] = value;
				windowPosition = (windowPosition + 1) & 0xfff;
			}
		}
	}
	return true;
}

bool readIafData(Common::SeekableReadStream &stream, Common::Array<byte> &data, LoadProgress *progress) {
	data.clear();
	if (stream.size() - stream.pos() < 25)
		return false;
	stream.readByte();
	const uint32 packedSize = stream.readUint32LE();
	if (!packedSize || packedSize > 16 * 1024 * 1024 || stream.size() - stream.pos() < packedSize + 20)
		return false;
	Common::Array<byte> packed;
	packed.resize(packedSize + 4);
	if (!readWithProgress(stream, &packed[4], packedSize, progress))
		return false;
	stream.skip(16);
	const uint32 header = stream.readUint32LE();
	if (stream.err())
		return false;
	WRITE_LE_UINT32(packed.begin(), header);
	Common::MemoryReadStream compressed(packed.begin(), packed.size());
	return readPackedData(compressed, data, progress);
}

bool readIafImage(Common::SeekableReadStream &stream, Graphics::Surface &surface, const Graphics::PixelFormat &format, LoadProgress *progress) {
	Common::Array<byte> data;
	if (!readIafData(stream, data, progress) || data.size() < 54 || READ_BE_UINT16(data.begin()) != MKTAG16('B', 'M'))
		return false;
	const uint32 width = READ_LE_UINT32(&data[18]);
	const uint32 height = READ_LE_UINT32(&data[22]);
	if (!width || width > 4096 || !height || height > 4096 || width * height > 4 * 1024 * 1024)
		return false;
	Common::MemoryReadStream bitmap(data.begin(), data.size());
	ProgressReadStream responsiveBitmap(bitmap, progress);
	Image::BitmapDecoder decoder;
	if (!decoder.loadStream(responsiveBitmap) || !decoder.getSurface() || responsiveBitmap.err())
		return false;
	Graphics::Surface converted;
	converted.create(width, height, format);
	for (uint32 row = 0; row < height; row += 16) {
		if (progress && !(*progress)()) {
			converted.free();
			return false;
		}
		const uint32 rows = MIN<uint32>(16, height - row);
		Graphics::Surface part = decoder.getSurface()->getSubArea(Common::Rect(0, row, width, row + rows));
		Graphics::Surface *strip = part.convertTo(format, decoder.getPalette().data());
		if (!strip) {
			converted.free();
			return false;
		}
		converted.copyRectToSurface(strip->getPixels(), strip->pitch, 0, row, width, rows);
		strip->free();
		delete strip;
	}
	surface.copyFrom(converted);
	converted.free();
	return true;
}

bool findIafRecord(Common::SeekableReadStream &stream, const Common::String &name, uint32 &offset, uint32 &size, LoadProgress *progress) {
	offset = size = 0;
	if (!stream.seek(0) || stream.size() < 4 || stream.size() > 0xffffffffLL)
		return false;
	const uint32 count = stream.readUint32LE();
	if (!count || count > 65536 || stream.size() < 4 + count * 20)
		return false;
	Common::Array<uint32> offsets;
	for (uint32 index = 0; index < count; ++index) {
		if (progress && (index & 63) == 0 && !(*progress)())
			return false;
		char entry[16];
		if (stream.read(entry, sizeof(entry)) != sizeof(entry))
			return false;
		const uint32 position = stream.readUint32LE();
		if (position < 4 + count * 20 || position >= stream.size())
			return false;
		offsets.push_back(position);
		uint length = 0;
		while (length < sizeof(entry) && entry[length])
			++length;
		if (Common::String(entry, length).equalsIgnoreCase(name))
			offset = position;
	}
	if (!offset || stream.err())
		return false;
	uint32 end = stream.size();
	for (uint index = 0; index < offsets.size(); ++index) {
		if (offsets[index] > offset && offsets[index] < end)
			end = offsets[index];
	}
	size = end - offset;
	return true;
}

static bool parseNumber(const Common::String &text, uint32 &value) {
	value = 0;
	if (text.empty())
		return false;
	for (uint index = 0; index < text.size(); ++index) {
		if (text[index] < '0' || text[index] > '9' || value > (0xffffffffU - 9) / 10)
			return false;
		value = value * 10 + text[index] - '0';
	}
	return true;
}

bool readStartupConfig(Common::SeekableReadStream &stream, StartupConfig &config, LoadProgress *progress) {
	config = StartupConfig();
	Common::Array<byte> data;
	if (!readPackedData(stream, data, progress))
		return false;

	Common::StringArray fields;
	uint32 start = 0;
	while (start < data.size() && data[start]) {
		if (progress && !(*progress)())
			return false;
		uint32 end = start;
		while (end < data.size() && data[end] && data[end] != ',')
			++end;
		Common::String field(reinterpret_cast<const char *>(&data[start]), end - start);
		field.trim();
		fields.push_back(field);
		if (end == data.size() || !data[end])
			break;
		start = end + 1;
	}
	if (fields.size() < 20 || fields[0] != "//SinNo" || fields[19] != "//END")
		return false;
	for (uint index = 1; index < 19; ++index) {
		const uint32 comment = fields[index].find(';');
		if (comment != Common::String::npos)
			fields[index] = fields[index].substr(0, comment);
		fields[index].trim();
	}

	uint32 geometry[6];
	if (!parseNumber(fields[1], config.scenarioNumber) || !config.scenarioNumber ||
		!parseNumber(fields[7], config.saveSignatureSize) ||
		!parseNumber(fields[8], config.saveSize))
		return false;
	for (uint index = 0; index < ARRAYSIZE(geometry); ++index) {
		if (!parseNumber(fields[13 + index], geometry[index]) || geometry[index] > 800)
			return false;
	}
	if (geometry[2] == 0 || geometry[3] == 0 || geometry[0] + geometry[2] > 800 ||
		geometry[1] + geometry[3] > 600 || geometry[4] >= 800 || geometry[5] >= 600)
		return false;
	config.title = fields[2];
	config.resourcePrefix = fields[3];
	config.scenarioFile = fields[4];
	config.optionsFile = fields[5];
	config.saveSignature = fields[6];
	config.soundPrefix = fields[9];
	config.systemVoicePrefix = fields[10];
	config.defaultNames[0] = fields[11];
	config.defaultNames[1] = fields[12];
	config.messageRect = Common::Rect(geometry[0], geometry[1],
		geometry[0] + geometry[2], geometry[1] + geometry[3]);
	config.textPosition = Common::Point(geometry[4], geometry[5]);
	if (config.title.empty() || config.resourcePrefix.empty() || config.scenarioFile.empty())
		return false;

	Common::String section;
	for (uint index = 20; index < fields.size(); ++index) {
		if (fields[index] == "//END") {
			if (section.empty())
				return false;
			section.clear();
		} else if (fields[index].hasPrefix("//")) {
			if (!section.empty() || config.stringTables.contains(fields[index]))
				return false;
			section = fields[index];
			config.stringTables[section] = Common::StringArray();
		} else {
			if (section.empty())
				return false;
			config.stringTables[section].push_back(fields[index]);
		}
	}
	return section.empty();
}

bool readScenarioData(Common::SeekableReadStream &stream, Common::Array<byte> &data, LoadProgress *progress) {
	data.clear();
	const int64 size = stream.size() - stream.pos();
	if (size <= 0 || size > 16 * 1024 * 1024)
		return false;
	data.resize(size);
	if (!readWithProgress(stream, data.begin(), data.size(), progress))
		return false;
	for (uint32 index = 0; index < data.size(); ++index) {
		if (progress && (index & 16383) == 0 && !(*progress)())
			return false;
		data[index] ^= 0xff;
	}
	return true;
}

} // End of namespace Delta
