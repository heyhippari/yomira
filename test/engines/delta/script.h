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
#include "common/memstream.h"
#include "engines/delta/script.h"

class DeltaScriptTestSuite : public CxxTest::TestSuite {
	Common::Array<byte> copyCode(const byte *bytes, uint size) {
		Common::Array<byte> code;
		code.resize(size);
		memcpy(code.begin(), bytes, size);
		return code;
	}

public:
	void testReaderOperands() {
		const byte bytes[] = { 0x34, 0x12, 0x78, 0x56, 0x34, 0x12, 'X', 0 };
		const Common::Array<byte> code = copyCode(bytes, sizeof(bytes));
		Delta::ScriptReader reader(code);
		TS_ASSERT_EQUALS(reader.readWord(), 0x1234);
		TS_ASSERT_EQUALS(reader.readDword(), 0x12345678U);
		TS_ASSERT_EQUALS(reader.readString(), "X");
		TS_ASSERT_EQUALS(reader.position(), sizeof(bytes));
		TS_ASSERT(!reader.failed());
	}

	void testReaderRejectsTruncatedAndInvalidTargets() {
		const byte bytes[] = { 'X' };
		const Common::Array<byte> code = copyCode(bytes, sizeof(bytes));
		Delta::ScriptReader word(code);
		TS_ASSERT_EQUALS(word.readWord(), 0);
		TS_ASSERT(word.failed());
		TS_ASSERT(!word.seek(0));
		Delta::ScriptReader string(code);
		TS_ASSERT_EQUALS(string.readString(), "");
		TS_ASSERT(string.failed());
		Delta::ScriptReader jump(code);
		TS_ASSERT(!jump.seek(0xffffffff));
		TS_ASSERT(jump.failed());
		TS_ASSERT_EQUALS(jump.position(), 0U);
	}

	void testStartupHeaderAlignment() {
		const byte bytes[] = {
			0x84, 0, 0xf2, 3, 0x3e, 0, 0, 0,
			0x83, 0, 0, 0, 0, 0, 0, 0, 0x78, 0x56, 0x34, 0x12
		};
		const Common::Array<byte> code = copyCode(bytes, sizeof(bytes));
		Delta::Script script(code);
		Delta::ScriptCommand command;
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.opcode, 0x84);
		TS_ASSERT_EQUALS(command.arguments[0], 1010);
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.offset, 4U);
		TS_ASSERT_EQUALS(command.opcode, 0x3e);
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.arguments.size(), 4U);
		TS_ASSERT_EQUALS(command.arguments[3], 0x12345678);
		TS_ASSERT_EQUALS(script.position(), 20U);
	}

	void testStartupConditionBothPaths() {
		for (uint value = 0; value < 2; ++value) {
			byte bytes[] = {
				0x91, 0, 1, 0, 0, 0,
				0x3c, 0, 1, 0x80, 0x12, 0x80, 1, 0x80, 2, 0x80,
				0x22, 0, 26, 0, 0, 0,
				0x84, 0, 0x11, 0x11,
				0x84, 0, 0x22, 0x22
			};
			bytes[4] = value;
			const Common::Array<byte> code = copyCode(bytes, sizeof(bytes));
			Delta::Script script(code);
			Delta::ScriptCommand command;
			TS_ASSERT(script.next(command));
			TS_ASSERT(script.next(command));
			TS_ASSERT_EQUALS(command.arguments[0], value);
			if (!value) {
				TS_ASSERT(script.next(command));
				TS_ASSERT_EQUALS(command.opcode, 0x22);
			}
			TS_ASSERT(script.next(command));
			TS_ASSERT_EQUALS(command.opcode, 0x84);
			TS_ASSERT_EQUALS(command.arguments[0], value ? 0x1111 : 0x2222);
		}
	}

	void testLegacyOpcodeMapping() {
		const byte bytes[] = { 0x83, 0, 0xf2, 3, 0x3d, 0, 0, 0, 0x66, 0, 1, 0 };
		const Common::Array<byte> code = copyCode(bytes, sizeof(bytes));
		Delta::Script script(code, true);
		Delta::ScriptCommand command;
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.opcode, 0x84);
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.opcode, 0x3e);
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.opcode, 0x67);
		TS_ASSERT_EQUALS(command.arguments[0], 1);
		TS_ASSERT_EQUALS(script.position(), sizeof(bytes));
	}

	void testMenuResultSelectsOriginalBranch() {
		const byte bytes[] = {
			0x67, 0, 6, 0,
			0x95, 0, 6, 0, 2, 0,
			18, 0, 0, 0, 22, 0, 0, 0,
			0x84, 0, 0x11, 0x11,
			0x84, 0, 0x22, 0x22
		};
		const Common::Array<byte> code = copyCode(bytes, sizeof(bytes));
		for (uint selection = 1; selection <= 2; ++selection) {
			Delta::Script script(code);
			Delta::ScriptCommand command;
			TS_ASSERT(script.next(command));
			TS_ASSERT_EQUALS(command.opcode, 0x67);
			TS_ASSERT(script.setVariable(command.arguments[0], selection));
			TS_ASSERT(!script.setVariable(1024, 1));
			TS_ASSERT(script.next(command));
			TS_ASSERT(script.next(command));
			TS_ASSERT_EQUALS(command.arguments[0], selection == 1 ? 0x1111 : 0x2222);
		}
	}

	void testMessageTokenAlignment() {
		const byte bytes[] = {
			0x4b, 0, 4, 0,
			'N', '2', 0,
			'H', 'e', 'l', 'l', 'o', 0,
			'v', 'T', 'E', 'S', 'T', 0, 1, 0, 0x78, 0x56, 0x34, 0x12,
			'M', '0', '1', 0,
			0x84, 0, 42, 0
		};
		const Common::Array<byte> code = copyCode(bytes, sizeof(bytes));
		Delta::Script script(code);
		Delta::ScriptCommand command;
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.strings.size(), 4U);
		TS_ASSERT_EQUALS(command.strings[1], "Hello");
		TS_ASSERT_EQUALS(command.arguments.size(), 3U);
		TS_ASSERT_EQUALS(command.arguments[2], 0x12345678);
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.opcode, 0x84);
		TS_ASSERT_EQUALS(command.arguments[0], 42);
	}

	void testSceneClearOperandsAndAlignment() {
		const byte bytes[] = { 0x19, 0, 0, 0, 0, 0, 0x20, 3, 0x58, 2, 10, 0, 0, 0, 1, 0, 0x84, 0, 42, 0 };
		const Common::Array<byte> code = copyCode(bytes, sizeof(bytes));
		Delta::Script script(code);
		Delta::ScriptCommand command;
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.opcode, 0x19);
		TS_ASSERT_EQUALS(command.arguments.size(), 7U);
		TS_ASSERT_EQUALS(command.arguments[2], 800);
		TS_ASSERT_EQUALS(command.arguments[3], 600);
		TS_ASSERT_EQUALS(command.arguments[4], 10);
		TS_ASSERT_EQUALS(script.position(), 16U);
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.arguments[0], 42);
	}

	void testSaveStateRoundTrip() {
		const byte bytes[] = { 0x91, 0, 3, 0, 42, 0, 0x91, 0, 1, 0, 3, 0x80, 0x67, 0, 6, 0 };
		const Common::Array<byte> code = copyCode(bytes, sizeof(bytes));
		Delta::Script original(code);
		Delta::ScriptCommand command;
		TS_ASSERT(original.next(command));
		Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);
		TS_ASSERT(original.saveState(saved));
		Common::MemoryReadStream input(saved.getData(), saved.size());
		Delta::Script restored(code);
		TS_ASSERT(restored.loadState(input));
		TS_ASSERT_EQUALS(restored.position(), 6U);
		TS_ASSERT(restored.next(command));
		TS_ASSERT_EQUALS(command.arguments[1], 42);
		Common::MemoryReadStream truncated(saved.getData(), saved.size() - 1);
		TS_ASSERT(!restored.loadState(truncated));
		TS_ASSERT_EQUALS(restored.position(), 12U);
		Common::MemoryReadStream otherDialect(saved.getData(), saved.size());
		Delta::Script legacy(code, true);
		TS_ASSERT(!legacy.loadState(otherDialect));
		TS_ASSERT_EQUALS(legacy.position(), 0U);
	}

	void testWhiteTransitionAndSoundRecordAlignment() {
		const byte bytes[] = {
			0xa4, 0, 0, 0, 0, 0, 0x20, 3, 0x58, 2, 5, 0, 0, 0, 1, 0,
			0xa2, 0, 1, 0, 2, 0, 3, 0,
			0x84, 0, 42, 0
		};
		const Common::Array<byte> code = copyCode(bytes, sizeof(bytes));
		Delta::Script script(code);
		Delta::ScriptCommand command;
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.arguments.size(), 7U);
		TS_ASSERT_EQUALS(command.arguments[4], 5);
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.offset, 16U);
		TS_ASSERT_EQUALS(command.arguments.size(), 3U);
		TS_ASSERT_EQUALS(command.arguments[2], 3);
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.arguments[0], 42);
	}

	void testOpaqueThreeWordCommandAlignment() {
		const byte bytes[] = { 0x08, 0, 0x4e, 0x30, 0, 0x81, 0x63, 0x81, 0x84, 0, 42, 0 };
		const Common::Array<byte> code = copyCode(bytes, sizeof(bytes));
		Delta::Script script(code);
		Delta::ScriptCommand command;
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.opcode, 0x08);
		TS_ASSERT_EQUALS(command.arguments.size(), 3U);
		TS_ASSERT_EQUALS(command.arguments[0], 0x304e);
		TS_ASSERT_EQUALS(command.arguments[2], 0x8163);
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.opcode, 0x84);
		TS_ASSERT_EQUALS(command.arguments[0], 42);
	}

	void testCheckpointZeroClearsCounters() {
		const byte bytes[] = {
			0x3e, 0, 7, 0, 0x3e, 0, 7, 0,
			0x3f, 0, 7, 0, 3, 0, 0x91, 0, 4, 0, 3, 0x80,
			0x3f, 0, 0, 0, 3, 0, 0x3f, 0, 7, 0, 3, 0,
			0x91, 0, 4, 0, 3, 0x80
		};
		const Common::Array<byte> code = copyCode(bytes, sizeof(bytes));
		Delta::Script script(code);
		Delta::ScriptCommand command;
		for (uint index = 0; index < 4; ++index)
			TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.arguments[1], 2);
		for (uint index = 0; index < 3; ++index)
			TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.arguments[1], 0);
	}

	void testNestedCallSavePreservesReturnsAndCheckpoint() {
		const byte bytes[] = {
			0x21, 0, 18, 0, 0, 0,
			0x3f, 0, 7, 0, 3, 0,
			0x91, 0, 4, 0, 3, 0x80,
			0x21, 0, 26, 0, 0, 0,
			0x8a, 0,
			0x3e, 0, 7, 0,
			0x8a, 0
		};
		const Common::Array<byte> code = copyCode(bytes, sizeof(bytes));
		Delta::Script original(code);
		Delta::ScriptCommand command;
		TS_ASSERT(original.next(command));
		TS_ASSERT_EQUALS(original.position(), 18U);
		TS_ASSERT(original.next(command));
		TS_ASSERT_EQUALS(original.position(), 26U);
		TS_ASSERT(original.next(command));
		Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);
		TS_ASSERT(original.saveState(saved));
		Common::MemoryReadStream input(saved.getData(), saved.size());
		Delta::Script restored(code);
		TS_ASSERT(restored.loadState(input));
		TS_ASSERT_EQUALS(restored.position(), 30U);
		TS_ASSERT(restored.next(command));
		TS_ASSERT_EQUALS(restored.position(), 24U);
		TS_ASSERT(restored.next(command));
		TS_ASSERT_EQUALS(restored.position(), 6U);
		TS_ASSERT(restored.next(command));
		TS_ASSERT(restored.next(command));
		TS_ASSERT_EQUALS(command.arguments[1], 1);
	}

	void testCallStackBounds() {
		const byte recurse[] = { 0x21, 0, 0, 0, 0, 0 };
		const Common::Array<byte> code = copyCode(recurse, sizeof(recurse));
		Delta::Script script(code);
		Delta::ScriptCommand command;
		for (uint depth = 0; depth < 64; ++depth)
			TS_ASSERT(script.next(command));
		TS_ASSERT(!script.next(command));
		TS_ASSERT_EQUALS(script.error(), "Script call stack overflow");
		const byte returnCode[] = { 0x8a, 0 };
		const Common::Array<byte> emptyCode = copyCode(returnCode, sizeof(returnCode));
		Delta::Script empty(emptyCode);
		TS_ASSERT(!empty.next(command));
		TS_ASSERT_EQUALS(empty.error(), "Script call stack underflow");
	}

	void testSeenEventSurvivesSave() {
		const byte bytes[] = { 0x3b, 0, 8, 0, 0x3a, 0, 8, 0, 3, 0, 0x91, 0, 4, 0, 3, 0x80 };
		const Common::Array<byte> code = copyCode(bytes, sizeof(bytes));
		Delta::Script script(code);
		Delta::ScriptCommand command;
		TS_ASSERT(script.next(command));
		Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);
		TS_ASSERT(script.saveState(saved));
		Common::MemoryReadStream input(saved.getData(), saved.size());
		Delta::Script restored(code);
		TS_ASSERT(restored.loadState(input));
		TS_ASSERT(restored.next(command));
		TS_ASSERT(restored.next(command));
		TS_ASSERT_EQUALS(command.arguments[1], 1);
	}

	void testLegacySeenEventTakesOneOperand() {
		// PHM script offset 0x137f: mark event 1, clear a rectangle, load BGH_07_01
		const byte bytes[] = { 0x3a, 0, 1, 0, 0x11, 0, 0, 0, 0, 0, 0, 0, 0xaf, 0, 'B', 0, 5, 0 };
		const Common::Array<byte> code = copyCode(bytes, sizeof(bytes));
		Delta::Script script(code, true);
		Delta::ScriptCommand command;
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.opcode, 0x3b);
		TS_ASSERT_EQUALS(command.arguments.size(), 1U);
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.opcode, 0x12);
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.opcode, 0xb0);
		TS_ASSERT_EQUALS(command.strings[0], "B");
	}

	void testLegacyCharacterSceneClearsResultVariable() {
		// PHM script offset 0x2228, with variable 1 preset first
		const byte bytes[] = { 0x90, 0, 1, 0, 7, 0,
			0x2b, 0, 'A', 0, 'B', 0, 'N', 'U', 'L', 'L', 0, 'B', 'G', 0, 2, 0, 1, 0, 3, 0, 1, 0, 5, 0, 1, 0,
			0x3b, 0, 1, 0x80, 0x0d, 0 };
		const Common::Array<byte> code = copyCode(bytes, sizeof(bytes));
		Delta::Script script(code, true);
		Delta::ScriptCommand command;
		TS_ASSERT(script.next(command));
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.opcode, 0x2c);
		TS_ASSERT_EQUALS(command.strings.size(), 4U);
		TS_ASSERT_EQUALS(command.strings[2], "NULL");
		TS_ASSERT_EQUALS(command.strings[3], "BG");
		TS_ASSERT_EQUALS(command.arguments.size(), 6U);
		TS_ASSERT_EQUALS(command.arguments[0], 2);
		TS_ASSERT_EQUALS(command.arguments[4], 5);
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.opcode, 0x3c);
		TS_ASSERT_EQUALS(command.arguments[0], 0);
	}

	void testOldStateLoadsVariablesAndRejectsUnknownVersion() {
		const byte bytes[] = { 0x91, 0, 4, 0, 3, 0x80 };
		const Common::Array<byte> code = copyCode(bytes, sizeof(bytes));
		Common::MemoryWriteStreamDynamic saved(DisposeAfterUse::YES);
		saved.writeByte(0);
		saved.writeUint32LE(0);
		for (uint variable = 0; variable < 1024; ++variable)
			saved.writeUint16LE(variable == 3 ? 9 : 0);
		Common::MemoryReadStream input(saved.getData(), saved.size());
		Delta::Script script(code);
		TS_ASSERT(script.loadState(input));
		Delta::ScriptCommand command;
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.arguments[1], 9);
		Common::Array<byte> invalid = copyCode(saved.getData(), saved.size());
		invalid[0] = 4;
		Common::MemoryReadStream badVersion(invalid.begin(), invalid.size());
		Delta::Script other(code);
		TS_ASSERT(!other.loadState(badVersion));
		TS_ASSERT_EQUALS(other.position(), 0U);
	}

	void testMessageCountAndVoiceTruncation() {
		const byte examples[][10] = {
			{ 0x4b, 0, 0, 0 },
			{ 0x4b, 0, 1, 1 },
			{ 0x4b, 0, 1, 0, 'v', 0, 1, 0, 0, 0 }
		};
		const uint sizes[] = { 4, 4, 10 };
		for (uint index = 0; index < ARRAYSIZE(examples); ++index) {
			const Common::Array<byte> code = copyCode(examples[index], sizes[index]);
			Delta::Script script(code);
			Delta::ScriptCommand command;
			TS_ASSERT(!script.next(command));
			TS_ASSERT(!script.error().empty());
		}
	}

	void testLegacyConditionalJump() {
		const byte bytes[] = {
			0x3b, 0, 1, 0x80, 0x12, 0x80, 0, 0x80, 2, 0x80,
			0x21, 0, 20, 0, 0, 0,
			0x83, 0, 0x34, 0x12,
			0x83, 0, 0x78, 0x56
		};
		const Common::Array<byte> code = copyCode(bytes, sizeof(bytes));
		Delta::Script script(code, true);
		Delta::ScriptCommand command;
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.opcode, 0x3c);
		TS_ASSERT_EQUALS(command.arguments[0], 1);
		TS_ASSERT_EQUALS(script.position(), 16U);
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.arguments[0], 0x1234);
	}

	void testVariableReferenceAssignment() {
		const byte bytes[] = { 0x91, 0, 3, 0, 42, 0, 0x91, 0, 1, 0, 3, 0x80 };
		const Common::Array<byte> code = copyCode(bytes, sizeof(bytes));
		Delta::Script script(code);
		Delta::ScriptCommand command;
		TS_ASSERT(script.next(command));
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.arguments[0], 1);
		TS_ASSERT_EQUALS(command.arguments[1], 42);
	}

	void testOneBasedIndexedBranch() {
		const byte bytes[] = {
			0x5e, 0, 1, 0, 6, 0,
			0x95, 0, 6, 0, 1, 0, 16, 0, 0, 0,
			0x84, 0, 0x34, 0x12
		};
		const Common::Array<byte> code = copyCode(bytes, sizeof(bytes));
		Delta::Script script(code);
		Delta::ScriptCommand command;
		TS_ASSERT(script.next(command));
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(script.position(), 16U);
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.arguments[0], 0x1234);
	}

	void testMalformedCommandsStop() {
		const byte examples[][8] = {
			{ 0x91, 0, 0, 4, 0, 0 },
			{ 0x22, 0, 0xff, 0xff, 0xff, 0xff },
			{ 0x3c, 0, 0, 0x84, 0x22, 0 },
			{ 0x95, 0, 0, 0, 33, 0 },
			{ 0xad, 0xde }
		};
		for (uint index = 0; index < ARRAYSIZE(examples); ++index) {
			const Common::Array<byte> code = copyCode(examples[index], sizeof(examples[index]));
			Delta::Script script(code);
			Delta::ScriptCommand command;
			TS_ASSERT(!script.next(command));
			TS_ASSERT(!script.error().empty());
			TS_ASSERT(!script.next(command));
		}
	}

	void testAddVariableWrapsAndAcceptsReferences() {
		const byte bytes[] = {
			0x91, 0, 3, 0, 42, 0, 0x91, 0, 1, 0, 0xff, 0x7f, 0x8b, 0, 3, 0, 5, 0,
			0x8b, 0, 3, 0, 1, 0x80, 0x8b, 0, 3, 0, 1, 0x80, 0x8b, 0, 3, 0, 0, 0x84
		};
		const Common::Array<byte> code = copyCode(bytes, sizeof(bytes));
		Delta::Script script(code);
		Delta::ScriptCommand command;
		TS_ASSERT(script.next(command));
		TS_ASSERT(script.next(command));
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.arguments[0], 3);
		TS_ASSERT_EQUALS(command.arguments[1], 47);
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.arguments[1], 32814);
		TS_ASSERT(script.next(command));
		TS_ASSERT_EQUALS(command.arguments[1], 45);
		TS_ASSERT(!script.next(command));
	}

	void testReadHistoryMarksLeftBlocksAndPersists() {
		Delta::ReadHistory history;
		TS_ASSERT(!history.currentRead());
		TS_ASSERT(!history.enterBlock(7));
		TS_ASSERT(!history.currentRead());
		TS_ASSERT(history.enterBlock(65535));
		TS_ASSERT(history.isRead(7));
		TS_ASSERT(!history.isRead(65535));
		TS_ASSERT(history.enterBlock(7));
		TS_ASSERT(history.currentRead());
		TS_ASSERT(history.isRead(65535));
		history.leaveBlock();
		TS_ASSERT(!history.currentRead());
		TS_ASSERT(!history.enterBlock(8));

		Common::MemoryWriteStreamDynamic out(DisposeAfterUse::YES);
		TS_ASSERT(history.save(out));
		Common::MemoryReadStream in(out.getData(), out.size());
		Delta::ReadHistory restored;
		TS_ASSERT(restored.load(in));
		TS_ASSERT(restored.isRead(7));
		TS_ASSERT(restored.isRead(65535));
		TS_ASSERT(!restored.isRead(8));
		Common::MemoryReadStream truncated(out.getData(), out.size() - 1);
		TS_ASSERT(!Delta::ReadHistory().load(truncated));
	}

	void testVariableArithmetic() {
		const byte bytes[] = {
			0x91, 0, 1, 0, 10, 0, 0x91, 0, 2, 0, 3, 0,
			0x8c, 0, 1, 0, 0x02, 0x80, // 10 - var[2] = 7
			0x8f, 0, 1, 0, 0xff, 0x3f, // 7 * 0x3fff wraps
			0x91, 0, 1, 0, 17, 0,
			0x8e, 0, 1, 0, 0x02, 0x80, // 17 / 3 = 5
			0x90, 0, 1, 0, 3, 0, // 5 % 3 = 2
			0x8c, 0, 1, 0, 9, 0, // saturates at 0
			0x90, 0, 2, 0, 0, 0 // zero divisor leaves 3
		};
		const Common::Array<byte> code = copyCode(bytes, sizeof(bytes));
		Delta::Script script(code);
		Delta::ScriptCommand command;
		const int expected[] = { 10, 3, 7, (7 * 0x3fff) & 0xffff, 17, 5, 2, 0, 3 };
		for (uint index = 0; index < ARRAYSIZE(expected); ++index) {
			TS_ASSERT(script.next(command));
			TS_ASSERT_EQUALS(command.arguments[1], expected[index]);
		}
		TS_ASSERT_EQUALS(script.position(), sizeof(bytes));
	}

	void testVariableRangeMaximum() {
		const byte bytes[] = {
			0x91, 0, 10, 0, 4, 0, 0x91, 0, 11, 0, 9, 0, 0x91, 0, 12, 0, 9, 0, 0x91, 0, 13, 0, 6, 0,
			0x93, 0, 10, 0, 4, 0, 1, 0, 2, 0,
			0x94, 0, 10, 0, 4, 0, 3, 0, 4, 0,
			0x93, 0, 10, 0, 0, 0, 5, 0, 6, 0
		};
		const Common::Array<byte> code = copyCode(bytes, sizeof(bytes));
		Delta::Script script(code);
		Delta::ScriptCommand command;
		for (uint index = 0; index < 7; ++index)
			TS_ASSERT(script.next(command));
		// Ties keep the earlier variable
		TS_ASSERT_EQUALS(script.variable(1), 2);
		TS_ASSERT_EQUALS(script.variable(2), 9);
		TS_ASSERT_EQUALS(script.variable(3), 2);
		TS_ASSERT_EQUALS(script.variable(4), 3);
		TS_ASSERT_EQUALS(script.variable(5), 1);
		TS_ASSERT_EQUALS(script.variable(6), 4);
	}

	void testWaitAndNoOperandCommands() {
		const byte bytes[] = {
			0x96, 0, 5, 0, 1, 0, 0, 0, 0, 0,
			0x9e, 0, 5, 0, 0x9f, 0, 1, 0, 2, 0, 3, 0, 0xa0, 0,
			0xa7, 0, 'X', 0, 1, 0, 2, 0, 3, 0, 4, 0, 0x4a, 0, 0x48, 0, 0x85, 0, 1, 0
		};
		const Common::Array<byte> code = copyCode(bytes, sizeof(bytes));
		Delta::Script script(code);
		Delta::ScriptCommand command;
		const uint16 opcodes[] = { 0x96, 0x9e, 0x9f, 0xa0, 0xa7, 0x4a, 0x48, 0x85 };
		for (uint index = 0; index < ARRAYSIZE(opcodes); ++index) {
			TS_ASSERT(script.next(command));
			TS_ASSERT_EQUALS(command.opcode, opcodes[index]);
		}
		TS_ASSERT(Delta::isHaltOpcode(0x4a));
		TS_ASSERT(!Delta::isHaltOpcode(0x48));
		TS_ASSERT_EQUALS(script.position(), sizeof(bytes));
		// The percentage roll needs a random source
		const byte roll[] = { 0x92, 0, 1, 0, 2, 0 };
		const Common::Array<byte> rollCode = copyCode(roll, sizeof(roll));
		Delta::Script unseeded(rollCode);
		TS_ASSERT(!unseeded.next(command));
	}
};
