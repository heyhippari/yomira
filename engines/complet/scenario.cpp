#include "common/file.h"
#include "common/debug.h"

#include "complet/scenario.h"

namespace Complet {

static const uint32 kHeaderSize = 0x10;

bool Scenario::load(const Common::String &name) {
	Common::File file;
	if (!file.open(Common::Path(name + ".mpx")))
		return false;

	if (file.readUint32BE() != MKTAG('M', 'p', '1', '7'))
		return false;

	uint32 textOffset = file.readUint16LE();
	uint32 textSize = file.readUint16LE();
	file.skip(6);

	uint32 textEnd = textOffset + textSize;
	if (textOffset < kHeaderSize || textEnd > (uint32)file.size())
		return false;

	file.seek(kHeaderSize);
	_commands.resize(textOffset - kHeaderSize);
	if (file.read(_commands.data(), _commands.size()) != _commands.size())
		return false;

	_scriptText.resize(textSize);
	if (file.read(_scriptText.data(), textSize) != textSize)
		return false;
	for (uint32 i = 0; i < textSize; i++)
		_scriptText[i] ^= 0x24;

	// The name table is a run of NUL-terminated strings
	_names.clear();
	uint32 remaining = file.size() - textEnd;
	Common::String current;
	for (uint32 i = 0; i < remaining; i++) {
		char c = file.readByte();
		if (c) {
			current += c;
		} else {
			_names.push_back(current);
			current.clear();
		}
	}

	_name = name;
	debug(1, "Scenario '%s': %u command bytes, %u script text bytes, %u names",
		name.c_str(), _commands.size(), textSize, _names.size());
	return true;
}

} // End of namespace Complet
