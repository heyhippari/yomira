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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <cxxtest/TestSuite.h>

#include "common/archive.h"
#include "common/endian.h"
#include "common/memstream.h"
#include "engines/illusion/archive.h"

class IllusionArchiveTestSuite : public CxxTest::TestSuite {
	class ShortReadStream : public Common::MemoryReadStream {
		const bool *_shortReads;
	public:
		ShortReadStream(const Common::Array<byte> &data, const bool *shortReads) :
			Common::MemoryReadStream(data.data(), data.size()), _shortReads(shortReads) {}
		uint32 read(void *data, uint32 size) override {
			return Common::MemoryReadStream::read(data, *_shortReads && size ? size - 1 : size);
		}
	};

	class MemoryFiles : public Common::Archive {
	public:
		Common::Array<byte> package;
		Common::Array<byte> table;
		bool shortReads = false;

		bool hasFile(const Common::Path &path) const override {
			return path == Common::Path("illusion-review.pp") || path == Common::Path("illusion-review.tbl");
		}
		int listMembers(Common::ArchiveMemberList &members) const override { return 0; }
		const Common::ArchiveMemberPtr getMember(const Common::Path &path) const override {
			if (!hasFile(path))
				return Common::ArchiveMemberPtr();
			return Common::ArchiveMemberPtr(new Common::GenericArchiveMember(path, *this));
		}
		Common::SeekableReadStream *createReadStreamForMember(const Common::Path &path) const override {
			if (path == Common::Path("illusion-review.pp"))
				return new ShortReadStream(package, &shortReads);
			if (path == Common::Path("illusion-review.tbl"))
				return new Common::MemoryReadStream(table.data(), table.size());
			return nullptr;
		}
	};

	MemoryFiles *_files = nullptr;

	Common::Array<byte> directory(const Common::String &name = "asset.bin") {
		Common::Array<byte> data(0x800 + 0x50, 0);
		WRITE_LE_UINT32(data.data() + 'a' * 8 + 4, 1);
		WRITE_LE_UINT32(data.data() + 0x800 + 8, 4);
		memcpy(data.data() + 0x800 + 0x10, name.c_str(), MIN<uint>(name.size(), 64));
		return data;
	}

	void storePayload(const Common::Array<byte> &payload, uint32 declaredSize) {
		_files->table.resize(4 + payload.size());
		WRITE_LE_UINT32(_files->table.data(), declaredSize ^ 0xa67f54cb);
		if (!payload.empty())
			memcpy(_files->table.data() + 4, payload.data(), payload.size());
		for (uint32 offset = 4; offset + 4 <= _files->table.size(); offset += 4)
			WRITE_LE_UINT32(_files->table.data() + offset, READ_LE_UINT32(_files->table.data() + offset) ^ 0x35d8ca2f);
	}

	void pack(const Common::Array<byte> &data, uint32 declaredSize = 0) {
		Common::Array<byte> payload;
		for (uint32 offset = 0; offset < data.size(); offset += 8) {
			payload.push_back(0xff);
			for (uint32 index = offset; index < MIN<uint32>(offset + 8, data.size()); ++index)
				payload.push_back(data[index]);
		}
		storePayload(payload, declaredSize ? declaredSize : data.size());
	}

	void appendToken(Common::Array<byte> &payload, uint &controlOffset, uint &tokenIndex,
			byte first, int second = -1) {
		if (!tokenIndex) {
			controlOffset = payload.size();
			payload.push_back(0);
		}
		if (second < 0)
			payload[controlOffset] |= 1 << tokenIndex;
		payload.push_back(first);
		if (second >= 0)
			payload.push_back(second);
		tokenIndex = (tokenIndex + 1) & 7;
	}

	void embedded() {
		_files->package.resize(48, 0);
		WRITE_LE_UINT32(_files->package.data(), 1);
		const char name[] = "Asset.bin";
		for (uint index = 0; index < sizeof(name); ++index)
			_files->package[8 + index] = (byte)(0 - (byte)name[index]);
		WRITE_LE_UINT32(_files->package.data() + 40, 4);
		const byte payload[] = { 0, 1, 128, 255 };
		for (uint index = 0; index < ARRAYSIZE(payload); ++index)
			_files->package[44 + index] = (byte)(0 - payload[index]);
	}

public:
	void setUp() {
		_files = new MemoryFiles();
		_files->package.resize(4);
		memcpy(_files->package.data(), "DATA", 4);
		SearchMan.add("illusion-review-tests", _files);
	}

	void tearDown() {
		SearchMan.remove("illusion-review-tests");
		_files = nullptr;
	}

	void testTableLookupAndMissingMemberClearsOutput() {
		pack(directory());
		Illusion::IllusionArchive archive;
		TS_ASSERT(archive.open("illusion-review.pp", "illusion-review.tbl"));
		Common::Array<byte> data;
		TS_ASSERT(archive.readMember("ASSET.BIN", data));
		TS_ASSERT_EQUALS(data.size(), 4U);
		TS_ASSERT_SAME_DATA(data.data(), "DATA", 4);
		TS_ASSERT(!archive.readMember("absent.bin", data));
		TS_ASSERT(data.empty());
	}

	void testTruncatedHeaderIsRejected() {
		const uint32 lengths[] = { 0, 1, 2047 };
		for (uint index = 0; index < ARRAYSIZE(lengths); ++index) {
			Common::Array<byte> truncated(lengths[index], 0);
			if (truncated.size() >= 'a' * 8 + 8)
				WRITE_LE_UINT32(truncated.data() + 'a' * 8 + 4, 1);
			pack(truncated, 0x800 + 0x50);
			Illusion::IllusionArchive archive;
			TS_ASSERT(!archive.open("illusion-review.pp", "illusion-review.tbl"));
			Common::Array<byte> output(1, 0x5a);
			TS_ASSERT(!archive.readMember("asset.bin", output));
			TS_ASSERT(output.empty());
		}
	}

	void testReferencedRecordMustBeFullyDecoded() {
		Common::Array<byte> data = directory();
		data.resize(data.size() - 1);
		pack(data, 0x800 + 0x50);
		Illusion::IllusionArchive archive;
		TS_ASSERT(!archive.open("illusion-review.pp", "illusion-review.tbl"));
	}

	void testShortAdvertisedSizeAndUnusedPaddingAreAccepted() {
		Common::Array<byte> data = directory();
		data.resize(data.size() + 0x50, 0xff);
		pack(data, data.size() + 1);
		Illusion::IllusionArchive archive;
		TS_ASSERT(archive.open("illusion-review.pp", "illusion-review.tbl"));
		Common::Array<byte> output;
		TS_ASSERT(archive.readMember("asset.bin", output));
		TS_ASSERT_EQUALS(output.size(), 4U);
		TS_ASSERT_SAME_DATA(output.data(), "DATA", 4);
	}

	void testPartialXorWordPreservesFinalNameBytes() {
		Common::String name;
		for (uint index = 0; index < 64; ++index)
			name += 'a';
		pack(directory(name));
		TS_ASSERT_DIFFERS((_files->table.size() - 4) % 4, 0U);
		Illusion::IllusionArchive archive;
		TS_ASSERT(archive.open("illusion-review.pp", "illusion-review.tbl"));
		Common::Array<byte> output;
		TS_ASSERT(archive.readMember(name, output));
		TS_ASSERT_EQUALS(output.size(), 4U);
		TS_ASSERT_SAME_DATA(output.data(), "DATA", 4);
	}

	void testFailedOpenClearsEarlierDirectoryGroups() {
		Common::Array<byte> data = directory();
		WRITE_LE_UINT32(data.data() + 'z' * 8 + 4, 2);
		pack(data);
		Illusion::IllusionArchive archive;
		TS_ASSERT(!archive.open("illusion-review.pp", "illusion-review.tbl"));
		Common::Array<byte> output(1, 0x5a);
		TS_ASSERT(!archive.readMember("asset.bin", output));
		TS_ASSERT(output.empty());
	}

	void testOutOfBoundsMemberRejectsOpenAndAllowsReopen() {
		Common::Array<byte> data = directory();
		WRITE_LE_UINT32(data.data() + 0x800 + 4, 4);
		pack(data);
		Illusion::IllusionArchive archive;
		TS_ASSERT(!archive.open("illusion-review.pp", "illusion-review.tbl"));
		Common::Array<byte> output(1, 0x5a);
		TS_ASSERT(!archive.readMember("asset.bin", output));
		TS_ASSERT(output.empty());
		pack(directory());
		TS_ASSERT(archive.open("illusion-review.pp", "illusion-review.tbl"));
		TS_ASSERT(archive.readMember("asset.bin", output));
		TS_ASSERT_EQUALS(output.size(), 4U);
		TS_ASSERT_SAME_DATA(output.data(), "DATA", 4);
	}

	void testShortMemberReadClearsOutput() {
		pack(directory());
		Illusion::IllusionArchive archive;
		TS_ASSERT(archive.open("illusion-review.pp", "illusion-review.tbl"));
		_files->shortReads = true;
		Common::Array<byte> output(1, 0x5a);
		TS_ASSERT(!archive.readMember("asset.bin", output));
		TS_ASSERT(output.empty());
	}

	void testEmbeddedNamesAndPayloadUseByteNegation() {
		embedded();
		Illusion::IllusionArchive archive;
		TS_ASSERT(archive.openEmbedded("illusion-review.pp"));
		Common::Array<byte> output;
		const byte expected[] = { 0, 1, 128, 255 };
		TS_ASSERT(archive.readMember("ASSET.BIN", output));
		TS_ASSERT_EQUALS(output.size(), ARRAYSIZE(expected));
		TS_ASSERT_SAME_DATA(output.data(), expected, sizeof(expected));
	}

	void testEmbeddedTruncationAndFailedReopen() {
		embedded();
		Illusion::IllusionArchive archive;
		TS_ASSERT(archive.openEmbedded("illusion-review.pp"));
		_files->package.resize(7);
		TS_ASSERT(!archive.openEmbedded("illusion-review.pp"));
		Common::Array<byte> output(1, 0x5a);
		TS_ASSERT(!archive.readMember("asset.bin", output));
		TS_ASSERT(output.empty());
		embedded();
		WRITE_LE_UINT32(_files->package.data() + 40, 5);
		TS_ASSERT(!archive.openEmbedded("illusion-review.pp"));
		TS_ASSERT(!archive.readMember("asset.bin", output));
		TS_ASSERT(output.empty());
	}

	void testLzssZeroHistoryAndClippedMatch() {
		Common::Array<byte> payload;
		uint controlOffset = 0, tokenIndex = 0;
		for (uint produced = 0; produced < 0x800; produced += 18)
			appendToken(payload, controlOffset, tokenIndex, 0, 15);
		storePayload(payload, 0x800);
		Illusion::IllusionArchive archive;
		TS_ASSERT(archive.open("illusion-review.pp", "illusion-review.tbl"));
		Common::Array<byte> output(1, 0x5a);
		TS_ASSERT(!archive.readMember("asset.bin", output));
		TS_ASSERT(output.empty());
	}

	void testLzssOverlappingMatchAcrossRingWrap() {
		Common::String name;
		for (uint index = 0; index < 64; ++index)
			name += 'a';
		Common::Array<byte> data(0x800 + 26 * 0x50, 0);
		WRITE_LE_UINT32(data.data() + 'a' * 8, 25 * 0x50);
		WRITE_LE_UINT32(data.data() + 'a' * 8 + 4, 1);
		const uint nameOffset = 0x800 + 25 * 0x50 + 0x10;
		WRITE_LE_UINT32(data.data() + nameOffset - 8, 4);
		memcpy(data.data() + nameOffset, name.c_str(), 64);
		Common::Array<byte> payload;
		uint controlOffset = 0, tokenIndex = 0;
		uint produced = 0;
		for (; produced <= nameOffset; ++produced)
			appendToken(payload, controlOffset, tokenIndex, data[produced]);
		while (produced < data.size()) {
			const uint length = MIN<uint>(18, data.size() - produced);
			const uint reference = (0xfee + produced - 1) & 0xfff;
			appendToken(payload, controlOffset, tokenIndex, reference & 0xff,
				((reference >> 4) & 0xf0) | (length - 3));
			produced += length;
		}
		storePayload(payload, data.size());
		Illusion::IllusionArchive archive;
		TS_ASSERT(archive.open("illusion-review.pp", "illusion-review.tbl"));
		Common::Array<byte> output;
		TS_ASSERT(archive.readMember(name, output));
		TS_ASSERT_EQUALS(output.size(), 4U);
		TS_ASSERT_SAME_DATA(output.data(), "DATA", 4);
	}

	void testDeclaredSizeLimitsAreRejected() {
		const uint32 sizes[] = { 0, 0x7ff, 16 * 1024 * 1024 + 1 };
		for (uint index = 0; index < ARRAYSIZE(sizes); ++index) {
			storePayload(Common::Array<byte>(), sizes[index]);
			Illusion::IllusionArchive archive;
			TS_ASSERT(!archive.open("illusion-review.pp", "illusion-review.tbl"));
		}
	}

	void testDirectoryOffsetsMustBeAlignedAndInRange() {
		const uint offsets[] = { 1, 2 * 0x50 };
		for (uint index = 0; index < ARRAYSIZE(offsets); ++index) {
			Common::Array<byte> data = directory();
			WRITE_LE_UINT32(data.data() + 'a' * 8, offsets[index]);
			pack(data);
			Illusion::IllusionArchive archive;
			TS_ASSERT(!archive.open("illusion-review.pp", "illusion-review.tbl"));
		}
	}

	void testEmbeddedTrailingBytesAndEmptyMember() {
		embedded();
		_files->package.push_back(0);
		Illusion::IllusionArchive archive;
		TS_ASSERT(!archive.openEmbedded("illusion-review.pp"));
		embedded();
		_files->package.resize(44);
		WRITE_LE_UINT32(_files->package.data() + 40, 0);
		TS_ASSERT(archive.openEmbedded("illusion-review.pp"));
		Common::Array<byte> output(1, 0x5a);
		_files->shortReads = true;
		TS_ASSERT(archive.readMember("asset.bin", output));
		TS_ASSERT(output.empty());
	}
};
