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

#ifndef GRAPHICS_FONTS_FALLBACKFONT_H
#define GRAPHICS_FONTS_FALLBACKFONT_H

#include "graphics/font.h"

namespace Graphics {

/**
 * A font that draws each character with a primary font, and with a fallback
 * font for the characters the primary font does not have.
 *
 * Line metrics come from the primary font. Fallback glyphs are shifted so
 * that both fonts share the same baseline. Neither font is owned.
 */
class FallbackFont : public Font {
public:
	FallbackFont(const Font *primary, const Font *fallback);

	int getFontHeight() const override;
	Common::String getFontName() const override;
	int getFontAscent() const override;
	int getFontDescent() const override;
	int getFontLeading() const override;
	int getMaxCharWidth() const override;
	int getCharWidth(uint32 chr) const override;
	int getKerningOffset(uint32 left, uint32 right) const override;
	Common::Rect getBoundingBox(uint32 chr) const override;
	bool hasChar(uint32 chr) const override;

	using Font::drawChar;
	using Font::drawAlphaChar;
	void drawChar(Surface *dst, uint32 chr, int x, int y, uint32 color) const override;
	void drawAlphaChar(Surface *dst, uint32 chr, int x, int y, uint32 color) const override;

private:
	const Font *fontFor(uint32 chr) const;
	int yOffsetFor(const Font *font) const;

	const Font *_primary;
	const Font *_fallback;
};

} // End of namespace Graphics

#endif
