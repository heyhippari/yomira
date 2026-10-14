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

#ifndef ENGINES_COMPLET_GD_H
#define ENGINES_COMPLET_GD_H

#include "common/array.h"
#include "common/stream.h"
#include "common/str.h"
#include "common/types.h"

namespace Complet {

enum {
	kScreenWidth = 800,
	kScreenHeight = 600,
	kThumbWidth = 80,
	kThumbHeight = 60
};

// Decoder for the C4 engine's GD2/GD3 images (GDS\*.GD)
class GdImage {
public:
	GdImage();

	bool load(Common::SeekableReadStream &stream);
	bool loadFromName(const Common::String &name);
	bool isGd2() const { return _isGd2; }

	// 24bpp, B,G,R byte order, bottom-up rows, kScreenWidth x kScreenHeight
	const byte *getPixels() const { return _pixels.data(); }

	// An 80x60 preview, same pixel layout
	static bool loadThumbnail(const Common::String &name, Common::Array<byte> &out);

private:
	bool decodeLz(const byte *data, uint32 size, uint32 pos, Common::Array<byte> &pixels);
	bool decodePredictive(const byte *data, uint32 size, uint32 pos, uint32 width, uint32 height,
			Common::Array<byte> &pixels);

	Common::Array<byte> _pixels;
	bool _isGd2 = false;
};

} // End of namespace Complet

#endif // ENGINES_COMPLET_GD_H
