#ifndef ENGINES_COMPLET_SCENARIO_H
#define ENGINES_COMPLET_SCENARIO_H

#include "common/array.h"
#include "common/str.h"
#include "common/types.h"

namespace Complet {

// An "MPX" scenario file ("Mp17" magic):
//   0x00  magic
//   0x04  u16 offset of the script text section
//   0x06  u16 size of the script text section (XOR 0x24 encoded)
//   0x08  u16 value kept by the original loader (unused by init)
//   0x0a  u16 value kept by the original loader (unused by init)
//   0x0c  u16 value kept by the original loader (unused by init)
//   0x10  command table (up to the script text section)
class Scenario {
public:
	bool load(const Common::String &name);

	const Common::String &getName() const { return _name; }
	const Common::Array<byte> &getCommands() const { return _commands; }
	const Common::Array<byte> &getScriptText() const { return _scriptText; }
	const Common::Array<Common::String> &getNames() const { return _names; }

private:
	Common::String _name;
	Common::Array<byte> _commands;
	Common::Array<byte> _scriptText;
	Common::Array<Common::String> _names;
};

} // End of namespace Complet

#endif // ENGINES_COMPLET_SCENARIO_H
