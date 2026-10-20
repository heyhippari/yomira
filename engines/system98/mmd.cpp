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

#include "common/algorithm.h"
#include "common/endian.h"
#include "common/memstream.h"
#include "system98/mmd.h"

namespace System98 {

namespace {

struct MidiEvent {
	uint32 tick;
	uint32 order;
	Common::Array<byte> data;
};

void writeVariable(Common::WriteStream &stream, uint32 value) {
	byte bytes[4];
	uint count = 0;
	bytes[count++] = value & 127;
	while ((value >>= 7) && count < 4)
		bytes[count++] = (value & 127) | 128;
	while (count)
		stream.writeByte(bytes[--count]);
}

void addEvent(Common::Array<MidiEvent> &events, uint32 tick, byte status, byte first, byte second, uint length = 3) {
	MidiEvent event;
	event.tick = tick;
	event.order = events.size();
	event.data.push_back(status);
	if (length >= 2) event.data.push_back(first);
	if (length >= 3) event.data.push_back(second);
	events.push_back(event);
}

bool addSysEx(const Common::Array<byte> &source, uint &offset, uint32 tick, byte first, byte second, byte channel, Common::Array<MidiEvent> &events) {
	Common::Array<byte> payload;
	uint checksum = 0;
	while (offset < source.size() && payload.size() < 4096) {
		byte value = source[offset++];
		if (value == 0xf7) {
			payload.push_back(value);
			MidiEvent event;
			event.tick = tick;
			event.order = events.size();
			Common::MemoryWriteStreamDynamic stream(DisposeAfterUse::YES);
			stream.writeByte(0xf0);
			writeVariable(stream, payload.size());
			stream.write(payload.data(), payload.size());
			event.data.resize(stream.size());
			memcpy(event.data.data(), stream.getData(), stream.size());
			events.push_back(event);
			return true;
		}
		if (value == 0x83) { checksum = 0; continue; }
		if (value == 0x80) value = first;
		else if (value == 0x81) value = second;
		else if (value == 0x82) value = channel;
		else if (value == 0x84) value = (0U - checksum) & 127;
		if (value >= 128)
			return false;
		payload.push_back(value);
		checksum += value;
	}
	return false;
}

bool convertTrack(const Common::Array<byte> &source, uint track, Common::MemoryWriteStreamDynamic &output) {
	uint position = READ_LE_UINT16(source.data() + 2 + track * 4);
	if (position >= source.size())
		return false;
	byte channel = source[5 + track * 4];
	bool muted = channel == 255;
	channel &= 15;
	const byte transpose = source[4 + track * 4];
	const int shift = transpose & 128 ? 0 : (transpose & 64 ? int(transpose) - 128 : transpose) + int8(source[1]);
	Common::Array<MidiEvent> events;
	int pendingNotes[128];
	for (uint index = 0; index < 128; ++index) pendingNotes[index] = -1;
	struct Loop {
		uint position;
		uint count;
		byte command[4];
	};
	Loop loops[16];
	uint depth = 0;
	byte command[4] = {};
	uint32 tick = 0;
	bool ended = false;
	for (uint budget = 0; budget < 262144 && !ended; ++budget) {
		if (position >= source.size() || tick > 0xffffff || events.size() > 262144)
			return false;
		const byte marker = source[position];
		if (marker == 0xfe || marker == 0xff) {
			ended = true;
			break;
		}
		if (marker >= 0x80 && marker <= 0x8f) {
			++position;
			for (uint index = 0; index < 4; ++index)
				if (marker & (8 >> index)) {
					if (position >= source.size()) return false;
					command[index] = source[position++];
				}
		} else {
			if (position + 4 > source.size()) return false;
			memcpy(command, source.data() + position, 4);
			position += 4;
		}
		const byte type = command[0], first = command[2], second = command[3];
		if (type < 128) {
			if (!muted && first && second) {
				const byte note = (type + shift) & 127;
				const int pending = pendingNotes[note];
				if (pending >= 0 && events[pending].tick > tick) {
					events[pending].tick = tick + first;
				} else {
					addEvent(events, tick, 0x90 | channel, note, second & 127);
					pendingNotes[note] = events.size();
					addEvent(events, tick + first, 0x80 | channel, note, 0);
				}
			}
		} else if (type >= 0x90 && type <= 0x97) {
			const uint table = READ_LE_UINT16(source.data() + 0x4a);
			if (table + (type & 7) * 2 + 2 > source.size()) return false;
			uint offset = READ_LE_UINT16(source.data() + table + (type & 7) * 2);
			if (!muted && !addSysEx(source, offset, tick, first, second, channel, events)) return false;
		} else switch (type) {
		case 0x98:
			if (!muted) {
				if (!addSysEx(source, position, tick, first, second, channel, events)) return false;
			} else {
				while (position < source.size() && source[position++] != 0xf7) {}
			}
			break;
		case 0xe2:
			if (!muted) {
				addEvent(events, tick, 0xb0 | channel, 0, second & 127);
				addEvent(events, tick, 0xb0 | channel, 32, 0);
				addEvent(events, tick, 0xc0 | channel, first & 127, 0, 2);
			}
			break;
		case 0xe6:
			muted = first == 0;
			channel = (first - 1) & 15;
			break;
		case 0xe7: {
			if (!first) return false;
			const uint32 tempo = 60000000U * 64 / (uint32(source[0]) * first);
			MidiEvent event;
			event.tick = tick; event.order = events.size();
			const byte bytes[] = {0xff, 0x51, 3, byte(tempo >> 16), byte(tempo >> 8), byte(tempo)};
			event.data.resize(sizeof(bytes)); memcpy(event.data.data(), bytes, sizeof(bytes)); events.push_back(event);
			break;
		}
		case 0xea: if (!muted) addEvent(events, tick, 0xd0 | channel, first & 127, 0, 2); break;
		case 0xeb: if (!muted) addEvent(events, tick, 0xb0 | channel, first & 127, second & 127); break;
		case 0xec: if (!muted) addEvent(events, tick, 0xc0 | channel, first & 127, 0, 2); break;
		case 0xed: if (!muted) addEvent(events, tick, 0xa0 | channel, first & 127, second & 127); break;
		case 0xee: if (!muted) addEvent(events, tick, 0xe0 | channel, first & 127, second & 127); break;
		case 0xf9:
			if (depth >= ARRAYSIZE(loops)) return false;
			loops[depth].position = position; loops[depth].count = 0;
			memcpy(loops[depth++].command, command, 4);
			break;
		case 0xf8:
			if (!depth) return false;
			{
				Loop &loop = loops[depth - 1];
				const uint count = !command[1] || command[1] >= 127 ? 2 : command[1];
				if (++loop.count < count) {
					position = loop.position;
					memcpy(command, loop.command, 4);
				} else --depth;
			}
			break;
		case 0xfe: case 0xff: ended = true; break;
		default: return false;
		}
		if (type < 0xf0) tick += command[1];
	}
	if (!ended) return false;
	Common::sort(events.begin(), events.end(), [](const MidiEvent &left, const MidiEvent &right) {
		return left.tick < right.tick || (left.tick == right.tick && left.order < right.order);
	});
	uint32 last = 0;
	for (const MidiEvent &event : events) {
		writeVariable(output, event.tick - last);
		output.write(event.data.data(), event.data.size());
		last = event.tick;
	}
	writeVariable(output, tick > last ? tick - last : 0);
	output.writeByte(0xff); output.writeByte(0x2f); output.writeByte(0);
	return true;
}

} // namespace

bool convertMmdToMidi(const Common::Array<byte> &source, Common::Array<byte> &midi) {
	midi.clear();
	if (source.size() < 0x4c || source.size() > 0x4000 || !source[0])
		return false;
	Common::MemoryWriteStreamDynamic output(DisposeAfterUse::YES);
	output.writeUint32BE(MKTAG('M', 'T', 'h', 'd')); output.writeUint32BE(6);
	output.writeUint16BE(1); output.writeUint16BE(19); output.writeUint16BE(48);
	output.writeUint32BE(MKTAG('M', 'T', 'r', 'k')); output.writeUint32BE(11);
	const uint32 tempo = 60000000U / source[0];
	const byte header[] = {0, 0xff, 0x51, 3, byte(tempo >> 16), byte(tempo >> 8), byte(tempo), 0, 0xff, 0x2f, 0};
	output.write(header, sizeof(header));
	for (uint track = 0; track < 18; ++track) {
		Common::MemoryWriteStreamDynamic data(DisposeAfterUse::YES);
		if (!convertTrack(source, track, data)) return false;
		output.writeUint32BE(MKTAG('M', 'T', 'r', 'k'));
		output.writeUint32BE(data.size()); output.write(data.getData(), data.size());
	}
	midi.resize(output.size());
	memcpy(midi.data(), output.getData(), output.size());
	return true;
}

bool extractMmdEffect(const Common::Array<byte> &bank, uint effect, Common::Array<byte> &data) {
	data.clear();
	if (bank.size() < 2 || effect >= READ_LE_UINT16(bank.data())) return false;
	uint position = 2;
	for (uint index = 0; index <= effect; ++index) {
		if (position + 2 > bank.size()) return false;
		const uint size = READ_LE_UINT16(bank.data() + position);
		position += 2;
		if (size > bank.size() - position) return false;
		if (index == effect) {
			data.resize(size); memcpy(data.data(), bank.data() + position, size); return true;
		}
		position += size;
	}
	return false;
}

} // namespace System98
