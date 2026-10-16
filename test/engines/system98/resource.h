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

#include "common/memstream.h"
#include "engines/system98/resource.h"

class System98ResourceTestSuite : public CxxTest::TestSuite {
public:
	void test_literal_flags_are_lsb_first() {
		const byte packed[] = {0x07, 'A', 'B', 'C', 0, 0};
		const byte expected[] = {'A', 'B', 'C'};
		Common::MemoryReadStream stream(packed, sizeof(packed));
		Common::Array<byte> output;
		TS_ASSERT(System98::Resources::decompress(stream, output, sizeof(expected)));
		TS_ASSERT_EQUALS(output.size(), sizeof(expected));
		TS_ASSERT_SAME_DATA(output.data(), expected, sizeof(expected));
	}

	void test_overlapping_reference_uses_system98_nibble_order() {
		const byte packed[] = {0x01, 'A', 0x12, 0x00, 0, 0};
		const byte expected[] = {'A', 'A', 'A', 'A', 'A', 'A'};
		Common::MemoryReadStream stream(packed, sizeof(packed));
		Common::Array<byte> output;
		TS_ASSERT(System98::Resources::decompress(stream, output, sizeof(expected)));
		TS_ASSERT_EQUALS(output.size(), sizeof(expected));
		TS_ASSERT_SAME_DATA(output.data(), expected, sizeof(expected));
	}

	void test_dictionary_is_initially_zeroed() {
		const byte packed[] = {0x00, 0x10, 0x00, 0, 0};
		const byte expected[] = {0, 0, 0};
		Common::MemoryReadStream stream(packed, sizeof(packed));
		Common::Array<byte> output;
		TS_ASSERT(System98::Resources::decompress(stream, output, sizeof(expected)));
		TS_ASSERT_SAME_DATA(output.data(), expected, sizeof(expected));
	}

	void test_truncated_streams_are_rejected() {
		const byte packed[] = {0x01, 'A', 0x12, 0x00, 0, 0};
		for (uint size = 0; size < sizeof(packed); ++size) {
			Common::MemoryReadStream stream(packed, size);
			Common::Array<byte> output;
			TS_ASSERT(!System98::Resources::decompress(stream, output, 6));
		}
	}

	void test_output_size_must_match_end_marker() {
		const byte packed[] = {0x01, 'A', 0x12, 0x00, 0, 0};
		for (uint size = 0; size <= 7; ++size) {
			Common::MemoryReadStream stream(packed, sizeof(packed));
			Common::Array<byte> output;
			TS_ASSERT_EQUALS(System98::Resources::decompress(stream, output, size), size == 6);
		}
	}

	void test_excessive_output_size_is_rejected_before_allocation() {
		const byte packed[] = {0, 0, 0};
		Common::MemoryReadStream stream(packed, sizeof(packed));
		Common::Array<byte> output;
		TS_ASSERT(!System98::Resources::decompress(stream, output, 16 * 1024 * 1024 + 1));
		TS_ASSERT(output.empty());
		TS_ASSERT_EQUALS(stream.pos(), 0);
	}

	void test_string_file_decodes_two_lines_and_preserves_shift_jis_bytes() {
		const byte input[] = {0x82, 0xa0, 'A', '\r', '\n', 'B', 0x83, 0x40, 0x1a};
		Common::Array<byte> data;
		for (uint index = 0; index < sizeof(input); ++index)
			data.push_back(input[index]);
		byte decoded[2][80];
		memset(decoded, 0xff, sizeof(decoded));
		TS_ASSERT(System98::decodeStringFile(data, decoded));
		TS_ASSERT_EQUALS(decoded[0][0], 0x82);
		TS_ASSERT_EQUALS(decoded[0][1], 0xa0);
		TS_ASSERT_EQUALS(decoded[0][2], 'A');
		TS_ASSERT_EQUALS(decoded[0][3], 0);
		TS_ASSERT_EQUALS(decoded[1][0], 'B');
		TS_ASSERT_EQUALS(decoded[1][1], 0x83);
		TS_ASSERT_EQUALS(decoded[1][2], 0x40);
		TS_ASSERT_EQUALS(decoded[1][3], 0);
		TS_ASSERT_EQUALS(decoded[1][79], 0);
	}

	void test_string_file_accepts_empty_second_line_and_eof_marker() {
		const byte input[] = {'A', '\r', '\n', 0x1a};
		Common::Array<byte> data;
		for (uint index = 0; index < sizeof(input); ++index)
			data.push_back(input[index]);
		byte decoded[2][80];
		memset(decoded, 0xff, sizeof(decoded));
		TS_ASSERT(System98::decodeStringFile(data, decoded));
		TS_ASSERT_EQUALS(decoded[0][0], 'A');
		TS_ASSERT_EQUALS(decoded[0][1], 0);
		TS_ASSERT_EQUALS(decoded[1][0], 0);
	}

	void test_string_file_rejects_missing_line_and_overlong_line() {
		const byte singleLine[] = {'A', 0x1a};
		Common::Array<byte> data;
		for (uint index = 0; index < sizeof(singleLine); ++index)
			data.push_back(singleLine[index]);
		byte decoded[2][80];
		memset(decoded, 0x7f, sizeof(decoded));
		TS_ASSERT(!System98::decodeStringFile(data, decoded));
		TS_ASSERT_EQUALS(decoded[0][0], 0x7f);
		data.clear();
		for (uint index = 0; index < 80; ++index)
			data.push_back('A');
		data.push_back('\r');
		data.push_back('\n');
		data.push_back('B');
		TS_ASSERT(!System98::decodeStringFile(data, decoded));
		TS_ASSERT_EQUALS(decoded[0][0], 0x7f);
	}

	void test_string_length_stops_at_nul_and_respects_capacity() {
		const byte value[] = {'A', 0x82, 0xa0, 0, 'B'};
		TS_ASSERT_EQUALS(System98::getStringLength(value, sizeof(value)), 3);
		TS_ASSERT_EQUALS(System98::getStringLength(value, 2), 2);
		TS_ASSERT_EQUALS(System98::getStringLength(value, 0), 0);
		const byte empty[] = {0};
		TS_ASSERT_EQUALS(System98::getStringLength(empty, sizeof(empty)), 0);
	}

	void test_string_append_handles_normal_and_self_append() {
		byte destination[16] = "name: ";
		const byte source[16] = "title";
		TS_ASSERT_EQUALS(System98::appendString(destination, source, sizeof(destination)), 11);
		TS_ASSERT_EQUALS(strcmp(reinterpret_cast<const char *>(destination), "name: title"), 0);
		TS_ASSERT_EQUALS(System98::appendString(destination, destination, sizeof(destination)), 15);
		TS_ASSERT_EQUALS(strcmp(reinterpret_cast<const char *>(destination), "name: titlename"), 0);
	}

	void test_string_append_truncates_and_terminates_at_capacity() {
		byte destination[8] = "1234567";
		const byte source[4] = "abc";
		TS_ASSERT_EQUALS(System98::appendString(destination, source, sizeof(destination)), 7);
		TS_ASSERT_EQUALS(strcmp(reinterpret_cast<const char *>(destination), "1234567"), 0);
		byte unterminated[] = {'A', 'B', 'C', 'D'};
		TS_ASSERT_EQUALS(System98::appendString(unterminated, source, sizeof(unterminated)), 3);
		TS_ASSERT_EQUALS(unterminated[3], 0);
	}
};
