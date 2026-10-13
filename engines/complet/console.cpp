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

#include "complet/complet.h"
#include "complet/console.h"

namespace Complet {

Console::Console(CompletEngine *vm) : GUI::Debugger(), _vm(vm) {
	registerCmd("scenario", WRAP_METHOD(Console, cmdScenario));
	registerCmd("var", WRAP_METHOD(Console, cmdVar));
	registerCmd("flag", WRAP_METHOD(Console, cmdFlag));
	registerCmd("unlockcg", WRAP_METHOD(Console, cmdUnlockCg));
}

bool Console::cmdScenario(int argc, const char **argv) {
	debugPrintf("Current scenario: %s\n", _vm->getScript().getScenarioName().c_str());
	return true;
}

bool Console::cmdVar(int argc, const char **argv) {
	if (argc < 2 || argc > 3) {
		debugPrintf("Usage: %s <index> [value]\n", argv[0]);
		debugPrintf("Indexes below 256 are scenario locals, 256 to 511 globals\n");
		return true;
	}

	int index = strtol(argv[1], nullptr, 0);
	if (index < 0 || index >= 0x200) {
		debugPrintf("Invalid variable index\n");
		return true;
	}

	if (argc == 3)
		_vm->getScript().setVariable(index, (int16)strtol(argv[2], nullptr, 0));
	debugPrintf("var[%d] = %d\n", index, _vm->getScript().getVariable(index));
	return true;
}

bool Console::cmdFlag(int argc, const char **argv) {
	if (argc != 2) {
		debugPrintf("Usage: %s <number>\n", argv[0]);
		debugPrintf("Sets one of the 32 persistent flags; current flags:");
		for (int i = 0; i < 32; i++) {
			if (_vm->getScript().getFlag(i))
				debugPrintf(" %d", i);
		}
		debugPrintf("\n");
		return true;
	}

	int number = atoi(argv[1]);
	if (number < 0 || number >= 32)
		debugPrintf("Invalid flag number\n");
	else
		_vm->setPersistentFlag(number);
	return true;
}

bool Console::cmdUnlockCg(int argc, const char **argv) {
	_vm->unlockAllCgs();
	debugPrintf("All CGs unlocked in the gallery\n");
	return true;
}

} // End of namespace Complet
