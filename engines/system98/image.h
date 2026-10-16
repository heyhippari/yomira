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

#ifndef SYSTEM98_IMAGE_H
#define SYSTEM98_IMAGE_H

#include "common/array.h"
#include "graphics/surface.h"

namespace System98 {

bool drawWindowTiles(const Graphics::Surface &atlas, Graphics::Surface &destination, const Common::Rect &frame, uint style, bool transparent, const byte *preserveMask = nullptr, byte colorTag = 0);
bool captureBlitPattern(const Graphics::Surface &source, uint columns, Common::Array<byte> &pattern);
bool applyBlitPattern(const Graphics::Surface &source, Graphics::Surface &destination, const Common::Rect &sourceRect, const Common::Point &position, const Common::Array<byte> &pattern, uint stage);

class PiImage {
public:
	~PiImage() { _surface.free(); }
	bool load(const Common::Array<byte> &data);
	const Graphics::Surface &surface() const { return _surface; }
	const byte *palette() const { return _palette; }

private:
	Graphics::Surface _surface;
	byte _palette[48];
};

} // namespace System98

#endif
