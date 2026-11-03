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

#include "graphics/fonts/fallbackfont.h"

namespace Graphics {

FallbackFont::FallbackFont(const Font *primary, const Font *fallback)
	: _primary(primary), _fallback(fallback) {
}

int FallbackFont::getFontHeight() const {
	return _primary->getFontHeight();
}

Common::String FallbackFont::getFontName() const {
	return _primary->getFontName();
}

int FallbackFont::getFontAscent() const {
	return _primary->getFontAscent();
}

int FallbackFont::getFontDescent() const {
	return _primary->getFontDescent();
}

int FallbackFont::getFontLeading() const {
	return _primary->getFontLeading();
}

int FallbackFont::getMaxCharWidth() const {
	return MAX(_primary->getMaxCharWidth(), _fallback->getMaxCharWidth());
}

int FallbackFont::getCharWidth(uint32 chr) const {
	return fontFor(chr)->getCharWidth(chr);
}

int FallbackFont::getKerningOffset(uint32 left, uint32 right) const {
	const Font *font = fontFor(left);
	if (font != fontFor(right))
		return 0;
	return font->getKerningOffset(left, right);
}

Common::Rect FallbackFont::getBoundingBox(uint32 chr) const {
	const Font *font = fontFor(chr);
	Common::Rect box = font->getBoundingBox(chr);
	box.translate(0, yOffsetFor(font));
	return box;
}

bool FallbackFont::hasChar(uint32 chr) const {
	return _primary->hasChar(chr) || _fallback->hasChar(chr);
}

void FallbackFont::drawChar(Surface *dst, uint32 chr, int x, int y, uint32 color) const {
	const Font *font = fontFor(chr);
	font->drawChar(dst, chr, x, y + yOffsetFor(font), color);
}

void FallbackFont::drawAlphaChar(Surface *dst, uint32 chr, int x, int y, uint32 color) const {
	const Font *font = fontFor(chr);
	font->drawAlphaChar(dst, chr, x, y + yOffsetFor(font), color);
}

const Font *FallbackFont::fontFor(uint32 chr) const {
	if (_primary->hasChar(chr) || !_fallback->hasChar(chr))
		return _primary;
	return _fallback;
}

int FallbackFont::yOffsetFor(const Font *font) const {
	if (font == _primary)
		return 0;

	int primaryAscent = _primary->getFontAscent();
	int fallbackAscent = _fallback->getFontAscent();
	if (primaryAscent < 0 || fallbackAscent < 0)
		return 0;
	return primaryAscent - fallbackAscent;
}

} // End of namespace Graphics
