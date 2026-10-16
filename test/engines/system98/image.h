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
#include "engines/system98/image.h"

class System98ImageTestSuite : public CxxTest::TestSuite {
	static Common::Array<byte> sample() {
		Common::Array<byte> data(57);
		memset(data.data(), 0, data.size());
		WRITE_BE_UINT16(data.data(), 10);
		WRITE_BE_UINT16(data.data() + 2, 1);
		for (uint index = 0; index < 48; ++index)
			data[4 + index] = (index & 15) << 4;
		const byte payload[] = {0x5d, 0x29, 0x80, 0x26, 0x75};
		memcpy(data.data() + 52, payload, sizeof(payload));
		return data;
	}

	static Common::Array<byte> repeatedPair(uint width) {
		Common::Array<byte> data(55);
		memset(data.data(), 0, data.size());
		WRITE_BE_UINT16(data.data(), width);
		WRITE_BE_UINT16(data.data() + 2, 1);
		const char *bits = "0111110111110011000";
		for (uint index = 0; bits[index]; ++index)
			if (bits[index] == '1')
				data[52 + index / 8] |= 0x80 >> (index & 7);
		return data;
	}

public:
	void test_published_pi_vector_and_palette() {
		System98::PiImage image;
		TS_ASSERT(image.load(sample()));
		TS_ASSERT_EQUALS(image.surface().w, 10);
		TS_ASSERT_EQUALS(image.surface().h, 1);
		const byte expected[] = {9, 9, 9, 1, 9, 9, 1, 8, 8, 8};
		TS_ASSERT_SAME_DATA(image.surface().getPixels(), expected, sizeof(expected));
		for (uint index = 0; index < 48; ++index)
			TS_ASSERT_EQUALS(image.palette()[index], (index & 15) * 17);
	}

	void test_overlapping_pixel_pair_repeat() {
		System98::PiImage image;
		TS_ASSERT(image.load(repeatedPair(8)));
		const byte expected[] = {1, 2, 1, 2, 1, 2, 1, 2};
		TS_ASSERT_SAME_DATA(image.surface().getPixels(), expected, sizeof(expected));
	}

	void test_truncated_streams_and_invalid_dimensions() {
		const Common::Array<byte> original = sample();
		System98::PiImage image;
		for (uint size = 0; size < original.size(); ++size) {
			Common::Array<byte> data = original;
			data.resize(size);
			TS_ASSERT(!image.load(data));
		}
		const uint widths[] = {0, 9, 4098};
		for (uint width : widths) {
			Common::Array<byte> data = original;
			WRITE_BE_UINT16(data.data(), width);
			TS_ASSERT(!image.load(data));
		}
		Common::Array<byte> data = original;
		WRITE_BE_UINT16(data.data() + 2, 0);
		TS_ASSERT(!image.load(data));
	}

	void test_repeat_cannot_overrun_image() {
		System98::PiImage image;
		TS_ASSERT(!image.load(repeatedPair(4)));
		TS_ASSERT_EQUALS(image.surface().w, 0);
	}

	void test_window_tiles_cover_corners_edges_and_fill() {
		Graphics::Surface atlas, destination;
		atlas.create(48, 48, Graphics::PixelFormat::createFormatCLUT8());
		destination.create(80, 80, Graphics::PixelFormat::createFormatCLUT8());
		for (int row = 0; row < 3; ++row)
			for (int column = 0; column < 3; ++column)
				atlas.fillRect(Common::Rect(column * 16, row * 16, column * 16 + 16, row * 16 + 16), row * 3 + column);
		destination.fillRect(Common::Rect(80, 80), 14);
		TS_ASSERT(System98::drawWindowTiles(atlas, destination, Common::Rect(8, 8, 72, 72), 0, false, nullptr, 0x80));
		const int coordinates[] = {8, 32, 71};
		for (int row = 0; row < 3; ++row)
			for (int column = 0; column < 3; ++column)
				TS_ASSERT_EQUALS(*static_cast<const byte *>(destination.getBasePtr(coordinates[column], coordinates[row])), 0x80 | (row * 3 + column));
		TS_ASSERT_EQUALS(*static_cast<const byte *>(destination.getBasePtr(40, 40)), 0x84);
		TS_ASSERT_EQUALS(*static_cast<const byte *>(destination.getBasePtr(7, 8)), 14);
		TS_ASSERT_EQUALS(*static_cast<const byte *>(destination.getBasePtr(72, 71)), 14);
		atlas.free();
		destination.free();
	}

	void test_selection_preserves_transparent_pixels_and_glyphs() {
		Graphics::Surface atlas, destination;
		atlas.create(48, 48, Graphics::PixelFormat::createFormatCLUT8());
		destination.create(80, 80, Graphics::PixelFormat::createFormatCLUT8());
		atlas.fillRect(Common::Rect(48, 48), 4);
		atlas.fillRect(Common::Rect(0, 0, 16, 16), 0);
		destination.fillRect(Common::Rect(80, 80), 14);
		Common::Array<byte> mask(80 * 80);
		memset(mask.data(), 0, mask.size());
		mask[40 * 80 + 40] = 1;
		TS_ASSERT(System98::drawWindowTiles(atlas, destination, Common::Rect(8, 8, 72, 72), 0, true, mask.data()));
		TS_ASSERT_EQUALS(*static_cast<const byte *>(destination.getBasePtr(8, 8)), 14);
		TS_ASSERT_EQUALS(*static_cast<const byte *>(destination.getBasePtr(40, 40)), 14);
		TS_ASSERT_EQUALS(*static_cast<const byte *>(destination.getBasePtr(41, 40)), 4);
		TS_ASSERT_EQUALS(*static_cast<const byte *>(destination.getBasePtr(72, 40)), 14);
		atlas.free();
		destination.free();
	}

	void test_window_tiles_clip_and_reject_missing_styles() {
		Graphics::Surface atlas, destination;
		atlas.create(48, 48, Graphics::PixelFormat::createFormatCLUT8());
		destination.create(80, 80, Graphics::PixelFormat::createFormatCLUT8());
		atlas.fillRect(Common::Rect(48, 48), 5);
		destination.fillRect(Common::Rect(80, 80), 14);
		TS_ASSERT(!System98::drawWindowTiles(atlas, destination, Common::Rect(8, 8, 72, 72), 1, false));
		TS_ASSERT(!System98::drawWindowTiles(atlas, destination, Common::Rect(8, 8, 24, 24), 0, false));
		TS_ASSERT_EQUALS(*static_cast<const byte *>(destination.getBasePtr(8, 8)), 14);
		TS_ASSERT(System98::drawWindowTiles(atlas, destination, Common::Rect(-8, -8, 56, 56), 0, false));
		TS_ASSERT_EQUALS(*static_cast<const byte *>(destination.getBasePtr(0, 0)), 5);
		TS_ASSERT_EQUALS(*static_cast<const byte *>(destination.getBasePtr(55, 55)), 5);
		TS_ASSERT_EQUALS(*static_cast<const byte *>(destination.getBasePtr(56, 55)), 14);
		atlas.free();
		destination.free();
	}

	void test_pattern_capture_uses_fourth_plane_column_order() {
		Graphics::Surface source;
		source.create(16, 8, Graphics::PixelFormat::createFormatCLUT8());
		source.fillRect(Common::Rect(16, 8), 0);
		for (int row = 0; row < 8; ++row) {
			*static_cast<byte *>(source.getBasePtr(row, row)) = 8;
			*static_cast<byte *>(source.getBasePtr(7 - row, row)) = 7;
			*static_cast<byte *>(source.getBasePtr(15 - row, row)) = 15;
		}
		Common::Array<byte> pattern;
		TS_ASSERT(System98::captureBlitPattern(source, 2, pattern));
		const byte expected[] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
		TS_ASSERT_EQUALS(pattern.size(), sizeof(expected));
		TS_ASSERT_SAME_DATA(pattern.data(), expected, sizeof(expected));
		source.free();
	}

	void test_pattern_stages_tile_and_copy_zero_colors() {
		Graphics::Surface source, destination;
		source.create(16, 16, Graphics::PixelFormat::createFormatCLUT8());
		destination.create(16, 16, Graphics::PixelFormat::createFormatCLUT8());
		for (int y = 0; y < 16; ++y)
			for (int x = 0; x < 16; ++x)
				*static_cast<byte *>(source.getBasePtr(x, y)) = (x + y) & 15;
		destination.fillRect(Common::Rect(16, 16), 12);
		Common::Array<byte> pattern(16);
		for (uint row = 0; row < 8; ++row) {
			pattern[row] = row & 1 ? 0x55 : 0xaa;
			pattern[row + 8] = ~pattern[row];
		}
		TS_ASSERT(System98::applyBlitPattern(source, destination, Common::Rect(16, 16), Common::Point(0, 0), pattern, 0));
		for (int y = 0; y < 16; ++y)
			for (int x = 0; x < 16; ++x)
				TS_ASSERT_EQUALS(*static_cast<const byte *>(destination.getBasePtr(x, y)), (x + y) & 1 ? 12 : (x + y) & 15);
		TS_ASSERT_EQUALS(*static_cast<const byte *>(destination.getBasePtr(0, 0)), 0);
		TS_ASSERT(System98::applyBlitPattern(source, destination, Common::Rect(16, 16), Common::Point(0, 0), pattern, 1));
		for (int y = 0; y < 16; ++y)
			for (int x = 0; x < 16; ++x)
				TS_ASSERT_EQUALS(*static_cast<const byte *>(destination.getBasePtr(x, y)), (x + y) & 15);
		source.free();
		destination.free();
	}

	void test_pattern_blit_clips_without_shifting_mask_phase() {
		Graphics::Surface source, destination;
		source.create(20, 12, Graphics::PixelFormat::createFormatCLUT8());
		destination.create(12, 12, Graphics::PixelFormat::createFormatCLUT8());
		for (int y = 0; y < 12; ++y)
			for (int x = 0; x < 20; ++x)
				*static_cast<byte *>(source.getBasePtr(x, y)) = (x + y) & 15;
		destination.fillRect(Common::Rect(12, 12), 9);
		Common::Array<byte> pattern(8);
		memset(pattern.data(), 0x80, pattern.size());
		TS_ASSERT(System98::applyBlitPattern(source, destination, Common::Rect(-2, 1, 14, 11), Common::Point(-3, 2), pattern, 0));
		for (int y = 0; y < 12; ++y)
			for (int x = 0; x < 12; ++x)
				TS_ASSERT_EQUALS(*static_cast<const byte *>(destination.getBasePtr(x, y)), x == 5 && y >= 2 ? (y + 5) & 15 : 9);
		source.free();
		destination.free();
	}

	void test_short_pattern_stages_restart_mask_row_phase() {
		Graphics::Surface source, destination;
		source.create(8, 3, Graphics::PixelFormat::createFormatCLUT8());
		destination.create(8, 3, Graphics::PixelFormat::createFormatCLUT8());
		source.fillRect(Common::Rect(8, 3), 0);
		destination.fillRect(Common::Rect(8, 3), 9);
		Common::Array<byte> pattern(16);
		memset(pattern.data(), 0, pattern.size());
		pattern[8] = 0x80;
		pattern[9] = 0x40;
		pattern[10] = 0x20;
		TS_ASSERT(System98::applyBlitPattern(source, destination, Common::Rect(8, 3), Common::Point(0, 0), pattern, 0));
		for (int y = 0; y < 3; ++y)
			for (int x = 0; x < 8; ++x)
				TS_ASSERT_EQUALS(*static_cast<const byte *>(destination.getBasePtr(x, y)), 9);
		TS_ASSERT(System98::applyBlitPattern(source, destination, Common::Rect(8, 3), Common::Point(0, 0), pattern, 1));
		for (int y = 0; y < 3; ++y)
			for (int x = 0; x < 8; ++x)
				TS_ASSERT_EQUALS(*static_cast<const byte *>(destination.getBasePtr(x, y)), x == y ? 0 : 9);
		source.free();
		destination.free();
	}

	void test_invalid_pattern_capture_and_stages_are_rejected() {
		Graphics::Surface source, destination;
		source.create(16, 8, Graphics::PixelFormat::createFormatCLUT8());
		destination.create(16, 8, Graphics::PixelFormat::createFormatCLUT8());
		source.fillRect(Common::Rect(16, 8), 8);
		destination.fillRect(Common::Rect(16, 8), 9);
		Common::Array<byte> pattern;
		TS_ASSERT(!System98::captureBlitPattern(source, 0, pattern));
		TS_ASSERT(!System98::captureBlitPattern(source, 3, pattern));
		TS_ASSERT(!System98::applyBlitPattern(source, destination, Common::Rect(16, 8), Common::Point(0, 0), pattern, 0));
		TS_ASSERT(System98::captureBlitPattern(source, 1, pattern));
		TS_ASSERT(!System98::applyBlitPattern(source, destination, Common::Rect(16, 8), Common::Point(0, 0), pattern, 1));
		pattern.resize(7);
		TS_ASSERT(!System98::applyBlitPattern(source, destination, Common::Rect(16, 8), Common::Point(0, 0), pattern, 0));
		byte expected[128];
		memset(expected, 9, sizeof(expected));
		TS_ASSERT_SAME_DATA(destination.getPixels(), expected, sizeof(expected));
		source.free();
		destination.free();
	}
};
