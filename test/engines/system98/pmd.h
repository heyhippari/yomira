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
#include "engines/system98/pmd.h"

class System98PmdTestSuite : public CxxTest::TestSuite {
	struct Write {
		byte port, reg, value;
	};

	struct Recorder : public System98::PmdChip {
		Common::Array<Write> writes;
		void writeRegister(uint8 port, uint8 reg, uint8 value) override {
			Write write = {port, reg, value};
			writes.push_back(write);
		}
		int last(byte port, byte reg) const {
			for (uint index = writes.size(); index--; )
				if (writes[index].port == port && writes[index].reg == reg)
					return writes[index].value;
			return -1;
		}
		uint count(byte port, byte reg, int value = -1) const {
			uint result = 0;
			for (uint index = 0; index < writes.size(); ++index)
				if (writes[index].port == port && writes[index].reg == reg && (value < 0 || writes[index].value == value))
					++result;
			return result;
		}
	};

	// Builds a .M song: each part's data is appended after the 13-word header, then one voice
	static Common::Array<byte> song(const Common::Array<byte> *parts, uint count, const byte *rhythmTable = nullptr, uint rhythmSize = 0) {
		Common::Array<byte> data(27, 0);
		Common::Array<byte> body;
		uint16 offsets[13];
		const byte end = 0x80;
		for (uint index = 0; index < 11; ++index) {
			offsets[index] = 26 + body.size();
			if (index < count && !parts[index].empty())
				append(body, parts[index].data(), parts[index].size());
			else
				body.push_back(end);
		}
		offsets[11] = 26 + body.size();
		if (rhythmTable)
			append(body, rhythmTable, rhythmSize);
		offsets[12] = 26 + body.size();
		const byte voice[26] = {1, 0x01, 0x01, 0x01, 0x01, 0x20, 0x20, 0x20, 0x10, 0x1f, 0x1f, 0x1f, 0x1f,
		                        0, 0, 0, 0, 0, 0, 0, 0, 0x0f, 0x0f, 0x0f, 0x0f, 0x07};
		append(body, voice, sizeof(voice));
		for (uint index = 0; index < 13; ++index)
			WRITE_LE_UINT16(data.data() + 1 + index * 2, offsets[index]);
		append(data, body.data(), body.size());
		return data;
	}

	static void append(Common::Array<byte> &data, const byte *bytes, uint size) {
		for (uint index = 0; index < size; ++index)
			data.push_back(bytes[index]);
	}

	static Common::Array<byte> bytes(const byte *data, uint size) {
		return Common::Array<byte>(data, size);
	}

public:
	void test_fm_note_frequency_keyon_and_end() {
		const byte fm[] = {0xff, 1, 0xfd, 0x7f, 0x45, 4, 0x80};
		Common::Array<byte> parts[1] = {bytes(fm, sizeof(fm))};
		Recorder chip;
		System98::PmdPlayer player(chip, false);
		TS_ASSERT(player.playMusic(song(parts, 1)));
		player.timerB();
		TS_ASSERT_EQUALS(chip.last(0, 0xb0), 0x07);
		TS_ASSERT_EQUALS(chip.last(0, 0xa4), 0x23);
		TS_ASSERT_EQUALS(chip.last(0, 0xa0), 0x39);
		TS_ASSERT_EQUALS(chip.last(0, 0x28), 0xf0);
		// Algorithm 7 makes every slot a carrier: TL = voice TL + 127 - volume scaled by /DF20
		TS_ASSERT_EQUALS(chip.last(0, 0x40), 0x20 + 127 - ((127 * 236) >> 8));
		for (uint tick = 0; tick < 3; ++tick)
			player.timerB();
		TS_ASSERT_EQUALS(chip.last(0, 0x28), 0xf0);
		player.timerB();
		TS_ASSERT_EQUALS(chip.last(0, 0x28), 0x00);
		TS_ASSERT_EQUALS(player.status() & 0xff, 0xff);
	}

	void test_finite_loop_and_tempo() {
		const byte fm[] = {0xfc, 0xc0, 0xff, 1, 0xf9, 0, 0, 0x40, 2, 0xf8, 3, 0, 0, 0, 0x80};
		Common::Array<byte> parts[1] = {bytes(fm, sizeof(fm))};
		Common::Array<byte> data = song(parts, 1);
		// Point the loop operands at the loop end's count byte and at the loop start operand
		const uint start = 1 + READ_LE_UINT16(data.data() + 1);
		WRITE_LE_UINT16(data.data() + start + 5, start + 10 - 1);
		WRITE_LE_UINT16(data.data() + start + 12, start + 5 - 1);
		Recorder chip;
		System98::PmdPlayer player(chip, false);
		TS_ASSERT(player.playMusic(data));
		for (uint tick = 0; tick < 12; ++tick)
			player.timerB();
		TS_ASSERT_EQUALS(chip.count(0, 0x28, 0xf0), 3U);
		TS_ASSERT_EQUALS(chip.last(0, 0x26), 0xc0);
		TS_ASSERT_EQUALS(player.status() & 0xff, 0xff);
	}

	void test_ssg_envelope_release() {
		// E2,-1,1,1 then o4 A for four ticks and a rest
		const byte ssg[] = {0xf0, 2, 0xff, 1, 1, 0xfd, 12, 0x49, 4, 0x0f, 8, 0x80};
		Common::Array<byte> parts[1] = {bytes(ssg, sizeof(ssg))};
		Common::Array<byte> slots[7];
		slots[6] = parts[0];
		Recorder chip;
		System98::PmdPlayer player(chip, false);
		TS_ASSERT(player.playMusic(song(slots, 7)));
		player.timerB();
		// o4 A: period 0x8dd shifted by three octaves with rounding
		TS_ASSERT_EQUALS(chip.last(0, 0), 0x8e);
		TS_ASSERT_EQUALS(chip.last(0, 1), 0);
		TS_ASSERT_EQUALS(chip.last(0, 7) & 0x01, 0);
		TS_ASSERT_EQUALS(chip.last(0, 8), 12);
		player.timerB();
		player.timerB();
		TS_ASSERT_EQUALS(chip.last(0, 8), 11);
		player.timerB();
		TS_ASSERT_EQUALS(chip.last(0, 8), 10);
		for (uint tick = 0; tick < 4; ++tick)
			player.timerB();
		// The note was keyed off and the release lowers the envelope every tick
		TS_ASSERT_LESS_THAN(chip.last(0, 8), 10);
	}

	void test_rhythm_pattern_plays_opna_rhythm_and_ssg_drum() {
		const byte rhythm[] = {0x00, 0x80};
		Common::Array<byte> slots[11];
		slots[10] = bytes(rhythm, sizeof(rhythm));
		byte table[2 + 7];
		Common::Array<byte> data = song(slots, 11, table, sizeof(table));
		const uint tablePosition = 1 + READ_LE_UINT16(data.data() + 1 + 22);
		WRITE_LE_UINT16(data.data() + tablePosition, tablePosition + 2 - 1);
		const byte pattern[] = {0x80, 0x03, 4, 0x0f, 4, 0xff};
		memcpy(data.data() + tablePosition + 2, pattern, sizeof(pattern));

		Common::Array<byte> driver(0x200, 0);
		const byte lookup[] = {0x03, 0xdb, 0x03, 0xd8, 0x81, 0xc3, 0x00, 0x02};
		memcpy(driver.data() + 0x10, lookup, sizeof(lookup));
		const byte effects[] = {1, 6, 0, 1, 6, 0, 2, 0x34, 0x12, 0x05, 0x36, 0x0f, 0, 0, 0, 0, 0, 0xff};
		memcpy(driver.data() + 0x100, effects, sizeof(effects));

		Recorder chip;
		System98::PmdPlayer player(chip, true);
		TS_ASSERT(player.loadSsgEffects(driver));
		TS_ASSERT(player.playMusic(data));
		player.timerB();
		TS_ASSERT_EQUALS(chip.count(0, 0x10, 0x01), 1U);
		TS_ASSERT_EQUALS(chip.count(0, 0x10, 0x02), 1U);
		TS_ASSERT_EQUALS(chip.last(0, 4), 0x34);
		TS_ASSERT_EQUALS(chip.last(0, 5), 0x12);
		TS_ASSERT_EQUALS(chip.last(0, 10), 0x0f);
		TS_ASSERT(player.ssgEffectPlaying());
		player.timerA();
		player.timerA();
		TS_ASSERT(!player.ssgEffectPlaying());
		TS_ASSERT_EQUALS(chip.last(0, 10), 0);
	}

	void test_fm_effect_masks_music_channel_three() {
		const byte fm3[] = {0xff, 1, 0x40, 16, 0x80};
		Common::Array<byte> slots[3];
		slots[2] = bytes(fm3, sizeof(fm3));
		Common::Array<byte> efc(0x120, 0x80);
		WRITE_LE_UINT16(efc.data() + 2, 0x100);
		WRITE_LE_UINT16(efc.data() + 0xfe, 0x106);
		const byte effect[] = {0xff, 1, 0x50, 2, 0x80};
		memcpy(efc.data() + 0x100, effect, sizeof(effect));
		const byte voice[26] = {1, 0x02, 0x02, 0x02, 0x02, 0, 0, 0, 0, 0x1f, 0x1f, 0x1f, 0x1f,
		                        0, 0, 0, 0, 0, 0, 0, 0, 0x0f, 0x0f, 0x0f, 0x0f, 0x07};
		efc.resize(0x106);
		append(efc, voice, sizeof(voice));

		Recorder chip;
		System98::PmdPlayer player(chip, false);
		TS_ASSERT(player.loadFmEffects(efc));
		TS_ASSERT(player.playMusic(song(slots, 3)));
		player.timerB();
		TS_ASSERT_EQUALS(chip.last(0, 0x28), 0xf2);
		player.playFmEffect(1);
		player.timerA();
		TS_ASSERT_EQUALS(chip.last(0, 0x32), 0x02);
		TS_ASSERT_EQUALS(chip.last(0, 0xa6), 0x2a);
		TS_ASSERT(player.fmEffectPlaying());
		player.timerA();
		player.timerA();
		TS_ASSERT(!player.fmEffectPlaying());
		// The music voice is restored once the effect releases the channel
		TS_ASSERT_EQUALS(chip.last(0, 0x32), 0x01);
	}

	void test_malformed_data_is_bounded() {
		const byte loop[] = {0xf6, 0xfa, 0, 0};
		Common::Array<byte> parts[1] = {bytes(loop, sizeof(loop))};
		Common::Array<byte> data = song(parts, 1);
		Recorder chip;
		System98::PmdPlayer player(chip, false);
		TS_ASSERT(player.playMusic(data));
		player.timerB();
		player.timerB();
		TS_ASSERT_EQUALS(player.status() & 0xff, 0xff);

		const byte invalid[] = {0x40, 1, 0xa0, 0x80};
		parts[0] = bytes(invalid, sizeof(invalid));
		TS_ASSERT(player.playMusic(song(parts, 1)));
		for (uint tick = 0; tick < 4; ++tick)
			player.timerB();
		TS_ASSERT_EQUALS(player.status() & 0xff, 0xff);

		Common::Array<byte> truncated(data.data(), 20);
		TS_ASSERT(!player.playMusic(truncated));
		Common::Array<byte> driver(64, 0);
		TS_ASSERT(!player.loadSsgEffects(driver));
		TS_ASSERT(!player.loadFmEffects(driver));
	}
};
