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
#include "engines/delta/data.h"
#include "graphics/font.h"
#include "graphics/managed_surface.h"
#include "graphics/surface.h"

class DeltaDataTestSuite : public CxxTest::TestSuite {
	class BlockFont : public Graphics::Font {
	public:
		int getFontHeight() const override { return 3; }
		int getMaxCharWidth() const override { return 3; }
		int getCharWidth(uint32 chr) const override { return 3; }
		void drawChar(Graphics::Surface *destination, uint32 chr, int x, int y, uint32 color) const override {
			Common::Rect bounds(x, y, x + 3, y + 3);
			bounds.clip(Common::Rect(destination->w, destination->h));
			if (!bounds.isEmpty())
				destination->fillRect(bounds, color);
		}
	};

	struct ProgressCounter {
		uint32 calls = 0;
		uint32 limit = 0xffffffff;
		bool tick() { return ++calls <= limit; }
	};

public:
	void testMessageShadowOffsetAndDrawOrder() {
		const Graphics::PixelFormat format(2, 5, 6, 5, 0, 11, 5, 0, 0);
		Graphics::ManagedSurface destination(12, 10, format);
		const BlockFont font;
		destination.clear(0x1234);
		Delta::drawShadowedString(font, destination, Common::U32String("A"), 2, 2, 8, 0xffff, 0);
		for (int row = 0; row < destination.h; ++row) {
			for (int column = 0; column < destination.w; ++column) {
				uint32 expected = 0x1234;
				if (column >= 4 && column < 7 && row >= 4 && row < 7)
					expected = 0;
				if (column >= 2 && column < 5 && row >= 2 && row < 5)
					expected = 0xffff;
				TS_ASSERT_EQUALS(destination.getPixel(column, row), expected);
			}
		}
	}

	void testMessageShadowClippingAndEmptyWidth() {
		const Graphics::PixelFormat format(2, 5, 6, 5, 0, 11, 5, 0, 0);
		Graphics::ManagedSurface destination(8, 6, format);
		const BlockFont font;
		destination.clear(0x1234);
		Delta::drawShadowedString(font, destination, Common::U32String("A"), 6, 4, 8, 0xffff, 0);
		for (int row = 0; row < destination.h; ++row) {
			for (int column = 0; column < destination.w; ++column)
				TS_ASSERT_EQUALS(destination.getPixel(column, row), column >= 6 && row >= 4 ? 0xffffU : 0x1234U);
		}
		destination.clear(0x1234);
		Delta::drawShadowedString(font, destination, Common::U32String("A"), 0, 0, 0, 0xffff, 0);
		Delta::drawShadowedString(font, destination, Common::U32String("A"), 0, 0, -1, 0xffff, 0);
		for (int row = 0; row < destination.h; ++row) {
			for (int column = 0; column < destination.w; ++column)
				TS_ASSERT_EQUALS(destination.getPixel(column, row), 0x1234U);
		}
	}

	void testRawLoadProgressAndCancellation() {
		Common::Array<byte> packed;
		packed.resize(4 + 49152);
		WRITE_LE_UINT32(packed.begin(), 0x40000000 | 49152);
		memset(&packed[4], 0x5a, 49152);
		Common::MemoryReadStream stream(packed.begin(), packed.size());
		Common::Array<byte> data;
		ProgressCounter counter;
		Common::Functor0Mem<bool, ProgressCounter> progress(&counter, &ProgressCounter::tick);
		TS_ASSERT(Delta::readPackedData(stream, data, &progress));
		TS_ASSERT_EQUALS(counter.calls, 3U);
		TS_ASSERT_EQUALS(data.size(), 49152U);
		TS_ASSERT_EQUALS(data[49151], 0x5a);
		stream.seek(0);
		counter.calls = 0;
		counter.limit = 1;
		TS_ASSERT(!Delta::readPackedData(stream, data, &progress));
		TS_ASSERT_EQUALS(counter.calls, 2U);
		TS_ASSERT_EQUALS(stream.pos(), 4 + 16384);
	}

	void testCompressedLoadProgressAndCancellation() {
		for (uint mode = 0; mode < 2; ++mode) {
			Common::Array<byte> packed;
			packed.resize(4);
			WRITE_LE_UINT32(packed.begin(), 65025 | (mode == 0 ? 0x80000000 : 0));
			if (mode == 0) {
				for (uint run = 0; run < 255; ++run) {
					packed.push_back(255);
					packed.push_back(0x5a);
				}
			} else {
				uint produced = 0;
				while (produced < 65025) {
					packed.push_back(0);
					for (uint reference = 0; reference < 8 && produced < 65025; ++reference) {
						packed.push_back(0);
						packed.push_back(15);
						produced += 18;
					}
				}
			}
			Common::MemoryReadStream stream(packed.begin(), packed.size());
			Common::Array<byte> data;
			ProgressCounter counter;
			Common::Functor0Mem<bool, ProgressCounter> progress(&counter, &ProgressCounter::tick);
			TS_ASSERT(Delta::readPackedData(stream, data, &progress));
			TS_ASSERT(counter.calls >= 3);
			TS_ASSERT_EQUALS(data.size(), 65025U);
			TS_ASSERT_EQUALS(data[65024], mode == 0 ? 0x5a : 0);
			stream.seek(0);
			counter.calls = 0;
			counter.limit = 1;
			TS_ASSERT(!Delta::readPackedData(stream, data, &progress));
			TS_ASSERT_EQUALS(counter.calls, 2U);
		}
	}

	void testLzssLiterals() {
		const byte packed[] = { 3, 0, 0, 0, 7, 'A', 'B', 'C' };
		Common::MemoryReadStream stream(packed, sizeof(packed));
		Common::Array<byte> data;
		TS_ASSERT(Delta::readPackedData(stream, data));
		TS_ASSERT_EQUALS(data.size(), 3U);
		TS_ASSERT_SAME_DATA(data.begin(), "ABC", 3);
	}

	void testLzssOverlappingReference() {
		const byte packed[] = { 7, 0, 0, 0, 1, 'A', 0xee, 0xf3 };
		Common::MemoryReadStream stream(packed, sizeof(packed));
		Common::Array<byte> data;
		TS_ASSERT(Delta::readPackedData(stream, data));
		TS_ASSERT_EQUALS(data.size(), 7U);
		TS_ASSERT_SAME_DATA(data.begin(), "AAAAAAA", 7);
	}

	void testLzssZeroHistoryAndClippedFinalReference() {
		const byte packed[] = { 2, 0, 0, 0, 0, 0, 0 };
		const byte expected[] = { 0, 0 };
		Common::MemoryReadStream stream(packed, sizeof(packed));
		Common::Array<byte> data;
		TS_ASSERT(Delta::readPackedData(stream, data));
		TS_ASSERT_EQUALS(data.size(), 2U);
		TS_ASSERT_SAME_DATA(data.begin(), expected, 2);
	}

	void testRawData() {
		const byte packed[] = { 3, 0, 0, 0x40, 'r', 'a', 'w', 0xff };
		Common::MemoryReadStream stream(packed, sizeof(packed));
		Common::Array<byte> data;
		TS_ASSERT(Delta::readPackedData(stream, data));
		TS_ASSERT_EQUALS(data.size(), 3U);
		TS_ASSERT_SAME_DATA(data.begin(), "raw", 3);
		TS_ASSERT_EQUALS(stream.pos(), 7);
	}

	void testRleRunsAndLiterals() {
		const byte packed[] = { 5, 0, 0, 0x80, 3, 'X', 0, 2, 'Y', 'Z' };
		Common::MemoryReadStream stream(packed, sizeof(packed));
		Common::Array<byte> data;
		TS_ASSERT(Delta::readPackedData(stream, data));
		TS_ASSERT_EQUALS(data.size(), 5U);
		TS_ASSERT_SAME_DATA(data.begin(), "XXXYZ", 5);
	}

	void testTruncatedData() {
		const byte examples[][8] = {
			{ 0, 0, 0 },
			{ 3, 0, 0, 0x40, 'A', 'B' },
			{ 3, 0, 0, 0, 7, 'A' },
			{ 3, 0, 0, 0, 0, 0 },
			{ 3, 0, 0, 0x80, 0, 3, 'A', 'B' }
		};
		const uint sizes[] = { 3, 6, 6, 6, 8 };
		for (uint index = 0; index < ARRAYSIZE(sizes); ++index) {
			Common::MemoryReadStream stream(examples[index], sizes[index]);
			Common::Array<byte> data;
			TS_ASSERT(!Delta::readPackedData(stream, data));
		}
	}

	void testInvalidSizesAndRuns() {
		const byte examples[][6] = {
			{ 0, 0, 0, 0 },
			{ 1, 0, 0, 1 },
			{ 1, 0, 0, 0x80, 2, 'A' },
			{ 1, 0, 0, 0x80, 0, 0 }
		};
		for (uint index = 0; index < ARRAYSIZE(examples); ++index) {
			Common::MemoryReadStream stream(examples[index], sizeof(examples[index]));
			Common::Array<byte> data;
			TS_ASSERT(!Delta::readPackedData(stream, data));
		}
	}

	void testStartupConfig() {
		const char text[] = "//SinNo,1012 ;version,Title,OLV,OLV_DLN.SD,DLTOLV_DL.INI,"
			"Save Data,9,99772,OLV,vSD,Name,,4,468,792,132,112,488,//END,"
			"//NameStr,First,,Third,//END,";
		Common::Array<byte> packed;
		packed.resize(4 + sizeof(text));
		WRITE_LE_UINT32(packed.begin(), 0x40000000 | sizeof(text));
		memcpy(&packed[4], text, sizeof(text));
		Common::MemoryReadStream stream(packed.begin(), packed.size());
		Delta::StartupConfig config;
		TS_ASSERT(Delta::readStartupConfig(stream, config));
		TS_ASSERT_EQUALS(config.scenarioNumber, 1012U);
		TS_ASSERT_EQUALS(config.scenarioFile, "OLV_DLN.SD");
		TS_ASSERT_EQUALS(config.optionsFile, "DLTOLV_DL.INI");
		TS_ASSERT_EQUALS(config.resourcePrefix, "OLV");
		TS_ASSERT_EQUALS(config.defaultNames[0], "Name");
		TS_ASSERT_EQUALS(config.defaultNames[1], "");
		TS_ASSERT_EQUALS(config.messageRect, Common::Rect(4, 468, 796, 600));
		TS_ASSERT_EQUALS(config.textPosition, Common::Point(112, 488));
		TS_ASSERT_EQUALS(config.stringTables["//NameStr"].size(), 3U);
		TS_ASSERT_EQUALS(config.stringTables["//NameStr"][1], "");
		TS_ASSERT_EQUALS(config.stringTables["//NameStr"][2], "Third");
	}

	void testStartupConfigRejectsMissingHeader() {
		const byte packed[] = { 4, 0, 0, 0x40, 'n', 'o', 'p', 'e' };
		Common::MemoryReadStream stream(packed, sizeof(packed));
		Delta::StartupConfig config;
		TS_ASSERT(!Delta::readStartupConfig(stream, config));
	}

	void testStartupConfigRejectsInvalidGeometryAndSections() {
		const char *invalid[] = {
			"//SinNo,1012,Title,OLV,Game.SD,Game.INI,Save,4,100,OLV,vSD,,,4,468,797,132,112,488,//END,",
			"//SinNo,1012,Title,OLV,Game.SD,Game.INI,Save,4,100,OLV,vSD,,,4,468,792,133,112,488,//END,",
			"//SinNo,1012,Title,OLV,Game.SD,Game.INI,Save,4,100,OLV,vSD,,,4,468,792,132,112,488,//END,//NameStr,Name,",
			"//SinNo,1012,Title,OLV,Game.SD,Game.INI,Save,4,100,OLV,vSD,,,4,468,792,132,112,488,//END,Value,"
		};
		for (uint index = 0; index < ARRAYSIZE(invalid); ++index) {
			const uint32 size = strlen(invalid[index]) + 1;
			Common::Array<byte> packed;
			packed.resize(4 + size);
			WRITE_LE_UINT32(packed.begin(), 0x40000000 | size);
			memcpy(&packed[4], invalid[index], size);
			Common::MemoryReadStream stream(packed.begin(), packed.size());
			Delta::StartupConfig config;
			TS_ASSERT(!Delta::readStartupConfig(stream, config));
		}
	}

	void testScenarioInversion() {
		const byte input[] = { 0x7b, 0xff, 0x0d, 0xfc, 0xc1, 0xff, 0xff, 0xff };
		const byte expected[] = { 0x84, 0, 0xf2, 3, 0x3e, 0, 0, 0 };
		Common::MemoryReadStream stream(input, sizeof(input));
		Common::Array<byte> data;
		TS_ASSERT(Delta::readScenarioData(stream, data));
		TS_ASSERT_EQUALS(data.size(), sizeof(expected));
		TS_ASSERT_SAME_DATA(data.begin(), expected, sizeof(expected));
		TS_ASSERT_EQUALS(input[0], 0x7b);
	}

	void testScenarioRejectsEmptyInput() {
		Common::MemoryReadStream stream(nullptr, 0);
		Common::Array<byte> data;
		data.push_back(1);
		TS_ASSERT(!Delta::readScenarioData(stream, data));
		TS_ASSERT_EQUALS(data.size(), 0U);
	}

	void testIafRleWrapper() {
		const byte packed[] = {
			0x33, 7, 0, 0, 0, 0, 5, 'B', 'M', 'P', '2', '4',
			0, 0, 0, 0, 0, 0, 0, 0, 0x20, 3, 0, 0, 0x58, 2, 0, 0,
			5, 0, 0, 0x80
		};
		Common::MemoryReadStream stream(packed, sizeof(packed));
		Common::Array<byte> data;
		TS_ASSERT(Delta::readIafData(stream, data));
		TS_ASSERT_EQUALS(data.size(), 5U);
		TS_ASSERT_SAME_DATA(data.begin(), "BMP24", 5);
	}

	void testIafRejectsTruncatedWrapper() {
		const byte packed[] = { 0x33, 20, 0, 0, 0, 1, 2, 3 };
		Common::MemoryReadStream stream(packed, sizeof(packed));
		Common::Array<byte> data;
		TS_ASSERT(!Delta::readIafData(stream, data));
	}

	void testIafBitmapPixels() {
		byte packed[5 + 70 + 20] = {};
		packed[0] = 0x33;
		WRITE_LE_UINT32(packed + 1, 70);
		byte *bitmap = packed + 5;
		bitmap[0] = 'B';
		bitmap[1] = 'M';
		WRITE_LE_UINT32(bitmap + 2, 70);
		WRITE_LE_UINT32(bitmap + 10, 54);
		WRITE_LE_UINT32(bitmap + 14, 40);
		WRITE_LE_UINT32(bitmap + 18, 2);
		WRITE_LE_UINT32(bitmap + 22, 2);
		WRITE_LE_UINT16(bitmap + 26, 1);
		WRITE_LE_UINT16(bitmap + 28, 24);
		bitmap[54] = 255;
		bitmap[57] = 255;
		bitmap[58] = 255;
		bitmap[59] = 255;
		bitmap[64] = 255;
		bitmap[66] = 255;
		WRITE_LE_UINT32(packed + sizeof(packed) - 4, 0x40000000 | 70);
		Common::MemoryReadStream stream(packed, sizeof(packed));
		Graphics::Surface surface;
		const Graphics::PixelFormat format(2, 5, 6, 5, 0, 11, 5, 0, 0);
		TS_ASSERT(Delta::readIafImage(stream, surface, format));
		TS_ASSERT_EQUALS(surface.w, 2);
		TS_ASSERT_EQUALS(surface.h, 2);
		TS_ASSERT_EQUALS(surface.getPixel(0, 0), format.RGBToColor(255, 0, 0));
		TS_ASSERT_EQUALS(surface.getPixel(1, 0), format.RGBToColor(0, 255, 0));
		TS_ASSERT_EQUALS(surface.getPixel(0, 1), format.RGBToColor(0, 0, 255));
		TS_ASSERT_EQUALS(surface.getPixel(1, 1), format.RGBToColor(255, 255, 255));
		surface.free();
	}

	void testMaskedCompositing() {
		const Graphics::PixelFormat format(2, 5, 6, 5, 0, 11, 5, 0, 0);
		Graphics::Surface target, color, mask;
		target.create(3, 1, format);
		color.create(3, 1, format);
		mask.create(3, 1, format);
		for (int column = 0; column < 3; ++column)
			target.setPixel(column, 0, 0x1234);
		color.setPixel(0, 0, 0xffff);
		mask.setPixel(0, 0, 0xffff);
		color.setPixel(1, 0, 0xf800);
		mask.setPixel(1, 0, 0);
		color.setPixel(2, 0, 0x001f);
		mask.setPixel(2, 0, 0x8410);
		TS_ASSERT(Delta::compositeMasked(target, color, mask));
		TS_ASSERT_EQUALS(target.getPixel(0, 0), 0x1234U);
		TS_ASSERT_EQUALS(target.getPixel(1, 0), 0xf800U);
		TS_ASSERT_EQUALS(target.getPixel(2, 0), 0x0919U);
		target.free();
		color.free();
		mask.free();
	}

	void testFrameTransparencyPreservesMaskPrecision() {
		const Graphics::PixelFormat format(2, 5, 6, 5, 0, 11, 5, 0, 0);
		const Graphics::PixelFormat maskFormat(3, 8, 8, 8, 0, 16, 8, 0, 0);
		Graphics::Surface target, color, mask;
		target.create(4, 1, format);
		color.create(4, 1, format);
		mask.create(4, 1, maskFormat);
		color.fillRect(Common::Rect(4, 1), 0xf800);
		mask.setPixel(0, 0, maskFormat.RGBToColor(0, 0, 0));
		mask.setPixel(1, 0, maskFormat.RGBToColor(1, 1, 1));
		mask.setPixel(2, 0, maskFormat.RGBToColor(128, 128, 128));
		mask.setPixel(3, 0, maskFormat.RGBToColor(255, 255, 255));
		const byte transparency[] = { 0, 128, 255 };
		const uint32 expected[] = { 0xf800, 0x780f, 0x001f };
		for (uint index = 0; index < ARRAYSIZE(transparency); ++index) {
			target.fillRect(Common::Rect(4, 1), 0x001f);
			TS_ASSERT(Delta::compositeMasked(target, color, mask, nullptr, Common::Point(), transparency[index]));
			TS_ASSERT_EQUALS(target.getPixel(0, 0), 0xf800U);
			TS_ASSERT_EQUALS(target.getPixel(1, 0), expected[index]);
			TS_ASSERT_EQUALS(target.getPixel(2, 0), 0x780fU);
			TS_ASSERT_EQUALS(target.getPixel(3, 0), 0x001fU);
			TS_ASSERT_EQUALS(mask.getPixel(1, 0), maskFormat.RGBToColor(1, 1, 1));
		}
		target.free();
		color.free();
		mask.free();
	}

	void testPartialMaskPlacementAndClipping() {
		const Graphics::PixelFormat format(2, 5, 6, 5, 0, 11, 5, 0, 0);
		Graphics::Surface target, color, mask;
		target.create(3, 2, format);
		color.create(2, 1, format);
		mask.create(2, 1, format);
		target.fillRect(Common::Rect(3, 2), 0x1234);
		color.setPixel(0, 0, 0xf800);
		color.setPixel(1, 0, 0x001f);
		mask.fillRect(Common::Rect(2, 1), 0);
		TS_ASSERT(Delta::compositeMasked(target, color, mask, nullptr, Common::Point(2, 1)));
		TS_ASSERT_EQUALS(target.getPixel(2, 1), 0xf800U);
		TS_ASSERT_EQUALS(target.getPixel(1, 1), 0x1234U);
		TS_ASSERT_EQUALS(target.getPixel(2, 0), 0x1234U);
		TS_ASSERT(Delta::compositeMasked(target, color, mask, nullptr, Common::Point(-1, 0)));
		TS_ASSERT_EQUALS(target.getPixel(0, 0), 0x001fU);
		TS_ASSERT_EQUALS(target.getPixel(1, 0), 0x1234U);
		target.free();
		color.free();
		mask.free();
	}

	void testTallMenuSheet() {
		const uint32 height = 702;
		const uint32 bitmapSize = 54 + height * 4;
		Common::Array<byte> packed;
		packed.resize(5 + bitmapSize + 20);
		memset(packed.begin(), 0, packed.size());
		WRITE_LE_UINT32(&packed[1], bitmapSize);
		byte *bitmap = &packed[5];
		bitmap[0] = 'B';
		bitmap[1] = 'M';
		WRITE_LE_UINT32(bitmap + 2, bitmapSize);
		WRITE_LE_UINT32(bitmap + 10, 54);
		WRITE_LE_UINT32(bitmap + 14, 40);
		WRITE_LE_UINT32(bitmap + 18, 1);
		WRITE_LE_UINT32(bitmap + 22, height);
		WRITE_LE_UINT16(bitmap + 26, 1);
		WRITE_LE_UINT16(bitmap + 28, 24);
		for (uint32 row = 0; row < height; ++row)
			bitmap[54 + row * 4 + 1] = 255;
		WRITE_LE_UINT32(&packed[packed.size() - 4], 0x40000000 | bitmapSize);
		Common::MemoryReadStream stream(packed.begin(), packed.size());
		Graphics::Surface surface;
		const Graphics::PixelFormat format(2, 5, 6, 5, 0, 11, 5, 0, 0);
		ProgressCounter counter;
		Common::Functor0Mem<bool, ProgressCounter> progress(&counter, &ProgressCounter::tick);
		TS_ASSERT(Delta::readIafImage(stream, surface, format, &progress));
		TS_ASSERT(counter.calls >= height / 16);
		TS_ASSERT_EQUALS(surface.h, 702);
		TS_ASSERT_EQUALS(surface.getPixel(0, 701), format.RGBToColor(0, 255, 0));
		surface.free();
	}

	void testIafBankRecordBounds() {
		byte bank[64] = {};
		WRITE_LE_UINT32(bank, 2);
		memcpy(bank + 4, "FIRST.IAF", 10);
		WRITE_LE_UINT32(bank + 20, 44);
		memcpy(bank + 24, "TITLE.IAF", 10);
		WRITE_LE_UINT32(bank + 40, 54);
		Common::MemoryReadStream stream(bank, sizeof(bank));
		uint32 offset = 0;
		uint32 size = 0;
		TS_ASSERT(Delta::findIafRecord(stream, "title.iaf", offset, size));
		TS_ASSERT_EQUALS(offset, 54U);
		TS_ASSERT_EQUALS(size, 10U);
		TS_ASSERT(Delta::findIafRecord(stream, "FIRST.IAF", offset, size));
		TS_ASSERT_EQUALS(offset, 44U);
		TS_ASSERT_EQUALS(size, 10U);
		TS_ASSERT(!Delta::findIafRecord(stream, "MISSING.IAF", offset, size));
		WRITE_LE_UINT32(bank + 20, 8);
		TS_ASSERT(!Delta::findIafRecord(stream, "TITLE.IAF", offset, size));
	}
};
