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
#include "engines/complet/gd.h"

class CompletGdTestSuite : public CxxTest::TestSuite {
	static void appendBits(Common::Array<byte> &data, uint32 &bitPosition, uint32 value, int count) {
		for (int bit = count - 1; bit >= 0; bit--) {
			if (!(bitPosition & 7))
				data.push_back(0);
			data[bitPosition / 8] |= ((value >> bit) & 1) << (7 - (bitPosition & 7));
			bitPosition++;
		}
	}

	static Common::Array<byte> makeImage(int version, bool lz) {
		const uint32 offset = version == 3 ? 0x3756 : (version == 2 ? 0x2346 : 2);
		Common::Array<byte> data(offset);
		memset(data.data(), 0, data.size());
		if (version > 1) {
			data[0] = 'G';
			data[1] = 'D';
			data[2] = '0' + version;
		}
		data[offset - 2] = lz ? 'l' : 'p';
		uint32 bitPosition = offset * 8;
		const byte color[] = {0x12, 0x34, 0x56};
		if (lz) {
			for (int channel = 0; channel < 3; channel++) {
				appendBits(data, bitPosition, 1, 1);
				appendBits(data, bitPosition, color[channel], 8);
			}
			uint32 remaining = (version == 3 ? 800 * 600 : 640 * 480) * 3 - 3;
			uint32 written = 3;
			while (remaining >= 3) {
				const uint32 matchOffset = (written - 2) & 0xffff;
				if (!matchOffset) {
					for (int channel = 0; channel < 3; channel++) {
						appendBits(data, bitPosition, 1, 1);
						appendBits(data, bitPosition, color[channel], 8);
					}
					written += 3;
					remaining -= 3;
					continue;
				}
				const uint32 length = MIN(remaining, 18U);
				appendBits(data, bitPosition, 0, 1);
				appendBits(data, bitPosition, matchOffset, 16);
				appendBits(data, bitPosition, length - 3, 4);
				remaining -= length;
				written += length;
			}
		} else {
			appendBits(data, bitPosition, 0, 2);
			for (int channel = 0; channel < 3; channel++)
				appendBits(data, bitPosition, color[channel], 8);
			appendBits(data, bitPosition, 0, 1);
			appendBits(data, bitPosition, 3, 2);
			appendBits(data, bitPosition, 0x1fffff, 21);
			appendBits(data, bitPosition, 0, 1);
		}
		return data;
	}

	void checkImage(Complet::GdImage &image, int version, bool lz) {
		const Common::Array<byte> data = makeImage(version, lz);
		Common::MemoryReadStream stream(data.data(), data.size());
		TS_ASSERT(image.load(stream));
		const uint32 width = version == 3 ? 800 : 640;
		const uint32 height = version == 3 ? 600 : 480;
		TS_ASSERT_EQUALS(image.getWidth(), (int)width);
		TS_ASSERT_EQUALS(image.getHeight(), (int)height);
		TS_ASSERT_EQUALS(image.isGd2(), version == 2);
		Common::Array<byte> expected(width * height * 3);
		for (uint32 pixel = 0; pixel < width * height; pixel++) {
			expected[pixel * 3] = 0x12;
			expected[pixel * 3 + 1] = 0x34;
			expected[pixel * 3 + 2] = 0x56;
		}
		TS_ASSERT_EQUALS(memcmp(image.getPixels(), expected.data(), expected.size()), 0);
	}

public:
	void test_predictive_dimensions_and_pixels_across_revisions() {
		Complet::GdImage image;
		checkImage(image, 3, false);
		checkImage(image, 1, false);
		checkImage(image, 2, false);
	}

	void test_lz_dimensions_and_pixels_across_revisions() {
		Complet::GdImage image;
		checkImage(image, 2, true);
		checkImage(image, 3, true);
		checkImage(image, 1, true);
	}

	void test_short_and_unsupported_headers_are_rejected() {
		const byte data[] = {'x', 0, 0};
		Complet::GdImage image;
		for (uint32 size = 0; size <= sizeof(data); size++) {
			Common::MemoryReadStream stream(data, size);
			TS_ASSERT(!image.load(stream));
		}
	}
};
