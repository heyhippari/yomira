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

#ifndef ENGINES_XEX_SCRIPT_H
#define ENGINES_XEX_SCRIPT_H

#include "common/array.h"
#include "common/hash-str.h"
#include "common/hashmap.h"
#include "common/str.h"

#include "xex/scenario.h"

namespace Common {
class ReadStream;
class WriteStream;
}

namespace Xex {

class XexEngine;

struct MenuItem {
	Common::String text;
	// Command stream offset of the item's body
	uint32 body;
};

// Interpreter for the MPX command stream
class Script {
public:
	explicit Script(XexEngine *vm);

	bool start(const Common::String &name);
	bool isRunning() const { return _running; }
	void stop() { _running = false; }
	void step();

	// %j: scenario to switch to at the end of the current page
	void queueScenario(const Common::String &name) { _queued = name; }
	// %i
	void resetVariables();

	// Persistent flags set by %s and readable as variables 0x1dc and up
	bool setFlag(int number);
	bool getFlag(int number) const;

	// Variables by index: scenario locals below 0x100, globals above
	int16 getVariable(int index) { return getValue(-1 - index); }
	void setVariable(int index, int16 value);

	const Common::String &getScenarioName() const { return _scenario.getName(); }
	const byte *getData() const { return _scenario.getData(); }

	// Interpreter state, without the persistent flags
	void saveState(Common::WriteStream &stream) const;
	bool loadState(Common::ReadStream &stream);

private:
	enum {
		kCounterCount = 0x400,
		kGlobalCount = 0x200,
		kLocalVarLimit = 0x100,
		kMaxMenuDepth = 8,
		kFlagBase = 0x1dc,
		kFlagCount = 32
	};

	byte readByte();
	uint16 readWord();
	int16 readSWord();

	int16 getValue(int16 ref);
	bool evalCondition();
	void skipAssignments();
	void runAssignments();
	void runExpression();
	void showText(uint16 offset);
	void endPage();
	bool loadScenario(const Common::String &name);
	void newMenu();
	void addMenuItem(uint16 textOffset);
	int8 &counter(int16 index);

	XexEngine *_vm;
	Scenario _scenario;
	bool _running = false;
	uint32 _cur = 0;
	Common::String _queued;

	int8 _counters[kCounterCount];
	// Target of out-of-range counter references
	int8 _badCounter = 0;
	// Counters bumped when the page ends
	Common::Array<uint16> _fired;
	// Assignment lists to run when the page ends
	Common::Array<uint32> _deferred;
	Common::Array<Common::Array<MenuItem> > _menus;

	int16 _globals[kGlobalCount];
	bool _flags[kFlagCount];
	Common::HashMap<Common::String, Common::Array<int16>, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> _locals;
	Common::Array<int16> *_currentLocals = nullptr;
};

} // End of namespace Xex

#endif // ENGINES_XEX_SCRIPT_H
