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
};
