#include "common/file.h"

#include "complet/scenario.h"

namespace Complet {

bool Scenario::load(const Common::String &name) {
	Common::File file;
	if (!file.open(Common::Path(name + ".mpx")))
		return false;

	uint32 size = file.size();
	if (size < kCommandsOffset)
		return false;

	Common::Array<byte> data;
	data.resize(size);
	if (file.read(data.data(), size) != size)
		return false;

	if (READ_BE_UINT32(data.data()) != MKTAG('M', 'p', '1', '7'))
		return false;

	uint32 textOffset = READ_LE_UINT16(data.data() + 4);
	uint32 textSize = READ_LE_UINT16(data.data() + 6);
	uint32 localCount = READ_LE_UINT16(data.data() + 8);
	if (textOffset < kCommandsOffset || textOffset + textSize > size)
		return false;

	for (uint32 i = 0; i < textSize; i++)
		data[textOffset + i] ^= 0x24;

	_data = data;
	_textOffset = textOffset;
	_localCount = localCount;
	_name = name;
	return true;
}

} // End of namespace Complet
