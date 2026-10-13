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
//   0x08  u16 number of scenario-local variables
//   0x0a  u16 unused by the loader
//   0x0c  u16 unused by the loader
//   0x10  command stream (up to the script text section)
// Command stream jump targets are file offsets; text references are
// relative to the script text section.
class Scenario {
public:
	enum {
		kCommandsOffset = 0x10
	};

	bool load(const Common::String &name);

	const Common::String &getName() const { return _name; }
	const byte *getData() const { return _data.data(); }
	uint32 getSize() const { return _data.size(); }
	uint32 getTextOffset() const { return _textOffset; }
	uint32 getLocalCount() const { return _localCount; }

private:
	Common::String _name;
	Common::Array<byte> _data;
	uint32 _textOffset = 0;
	uint32 _localCount = 0;
};

} // End of namespace Complet

#endif // ENGINES_COMPLET_SCENARIO_H
