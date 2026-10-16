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

#include <cxxtest/TestSuite.h>
#include "common/endian.h"
#include "engines/system98/mmd.h"

class System98MmdTestSuite : public CxxTest::TestSuite {
	static Common::Array<byte> song(const byte *commands, uint size) {
		Common::Array<byte> data(0x52 + size);
		memset(data.data(), 0, data.size());
		data[0] = 120;
		for (uint track = 0; track < 18; ++track) {
			WRITE_LE_UINT16(data.data() + 2 + track * 4, track ? 0x51 + size : 0x51);
			data[5 + track * 4] = track ? 255 : 0;
		}
		memcpy(data.data() + 0x51, commands, size);
		data[0x51 + size] = 0xfe;
		return data;
	}

public:
	void test_note_timing_and_smf_header() {
		const byte commands[] = {60, 12, 6, 100, 0xfe};
		Common::Array<byte> midi;
		TS_ASSERT(System98::convertMmdToMidi(song(commands, sizeof(commands)), midi));
		TS_ASSERT_EQUALS(READ_BE_UINT32(midi.data()), MKTAG('M','T','h','d'));
		TS_ASSERT_EQUALS(READ_BE_UINT16(midi.data() + 10), 19);
		TS_ASSERT_EQUALS(READ_BE_UINT16(midi.data() + 12), 48);
		const byte expected[] = {0,0x90,60,100, 6,0x80,60,0, 6,0xff,0x2f,0};
		TS_ASSERT_EQUALS(READ_BE_UINT32(midi.data() + 37), sizeof(expected));
		TS_ASSERT_SAME_DATA(midi.data() + 41, expected, sizeof(expected));
	}

	void test_compressed_commands_and_finite_loop() {
		const byte commands[] = {0xf9,0,0,0, 60,12,6,100, 0x8b,62,3,80, 0xf8,2,0,0, 0xfe};
		Common::Array<byte> midi;
		TS_ASSERT(System98::convertMmdToMidi(song(commands, sizeof(commands)), midi));
		const byte expected[] = {0,0x90,60,100, 6,0x80,60,0, 6,0x90,62,80, 3,0x80,62,0,
		                         9,0x90,60,100, 6,0x80,60,0, 6,0x90,62,80, 3,0x80,62,0, 9,0xff,0x2f,0};
		TS_ASSERT_EQUALS(READ_BE_UINT32(midi.data() + 37), sizeof(expected));
		TS_ASSERT_SAME_DATA(midi.data() + 41, expected, sizeof(expected));
	}

	void test_invalid_headers_offsets_and_commands_are_rejected() {
		const byte commands[] = {60,12,6,100,0xfe};
		Common::Array<byte> data = song(commands, sizeof(commands)), midi;
		for (uint size = 0; size < 0x4c; ++size) {
			Common::Array<byte> shortData = data; shortData.resize(size);
			TS_ASSERT(!System98::convertMmdToMidi(shortData, midi));
		}
		data[0] = 0; TS_ASSERT(!System98::convertMmdToMidi(data, midi));
		data[0] = 120; WRITE_LE_UINT16(data.data() + 2, data.size());
		TS_ASSERT(!System98::convertMmdToMidi(data, midi));
		const byte badLoop[] = {0xf8,2,0,0,0xfe};
		TS_ASSERT(!System98::convertMmdToMidi(song(badLoop, sizeof(badLoop)), midi));
		const byte truncated[] = {60,12};
		data = song(truncated, sizeof(truncated)); data.resize(0x53);
		TS_ASSERT(!System98::convertMmdToMidi(data, midi));
	}

	void test_effect_bank_lengths_and_index_bounds() {
		const byte bytes[] = {2,0, 3,0, 1,2,3, 2,0, 4,5};
		Common::Array<byte> bank(sizeof(bytes)), data;
		memcpy(bank.data(), bytes, sizeof(bytes));
		TS_ASSERT(System98::extractMmdEffect(bank, 0, data));
		TS_ASSERT_EQUALS(data.size(), 3);
		TS_ASSERT_SAME_DATA(data.data(), bytes + 4, 3);
		TS_ASSERT(System98::extractMmdEffect(bank, 1, data));
		TS_ASSERT_EQUALS(data.size(), 2);
		TS_ASSERT_SAME_DATA(data.data(), bytes + 9, 2);
		TS_ASSERT(!System98::extractMmdEffect(bank, 2, data));
		TS_ASSERT(data.empty());
		bank.resize(bank.size() - 1);
		TS_ASSERT(!System98::extractMmdEffect(bank, 1, data));
	}
};
