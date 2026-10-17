/* ScummVM - Graphic Adventure Engine
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

#ifndef ILLUSION_ARCHIVE_H
#define ILLUSION_ARCHIVE_H

#include "common/array.h"
#include "common/file.h"
#include "common/noncopyable.h"
#include "common/path.h"
#include "common/str.h"

namespace Illusion {

class IllusionArchive : public Common::NonCopyable {
private:
	struct Entry {
		Common::String name;
		uint32 offset;
		uint32 size;
	};

	Common::File _package;
	Common::Array<Entry> _entries;
	bool _negated;
	uint32 _groupStart[256];
	uint32 _groupCount[256];

	void close();
	bool loadTable(const Common::Path &packagePath, const Common::Path &tablePath);
	bool loadEmbedded(const Common::Path &packagePath);
	static bool decompressTable(const Common::Array<byte> &packed, Common::Array<byte> &table, uint32 &decodedSize);

public:
	IllusionArchive();
	~IllusionArchive();

	bool open(const Common::Path &packagePath, const Common::Path &tablePath);
	bool openEmbedded(const Common::Path &packagePath);
	bool readMember(const Common::String &name, Common::Array<byte> &data);
};

} // End of namespace Illusion

#endif // ILLUSION_ARCHIVE_H
