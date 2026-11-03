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

#include "common/system.h"
#include "common/textconsole.h"
#include "system98/ending.h"
#include "system98/system98.h"

namespace System98 {

// Layout tables from ENDING.EXE: picture byte columns and widths, then text blocks as top, column, bottom, end column
static const uint16 kPictureColumns[] = {10, 30, 2, 40, 10, 38, 10, 30, 2, 30, 10};
static const uint16 kPictureWidths[] = {36, 42, 38, 32, 26, 34, 34, 50, 44, 50, 60};
static const uint16 kTextBlocks[][4] = {
	{112, 48, 176, 70}, {112, 10, 192, 28}, {112, 46, 208, 72}, {104, 13, 216, 27}, {104, 45, 216, 63},
	{104, 8, 216, 30}, {104, 42, 184, 70}, {104, 8, 216, 36}, {112, 46, 208, 62}, {112, 8, 176, 36}
};
// Palette words are green << 8 | red << 4 | blue
static const uint16 kShowPalette[16] = {
	0x000, 0xfff, 0xdbf, 0x000, 0xf00, 0xbf9, 0x9e7, 0x666, 0x000, 0x000, 0x0f6, 0xdec, 0x362, 0x694, 0x9c6, 0xfff
};
static const uint16 kBlackPalette[16] = {};
// Each dissolve stage masks the copied pixels of every row with the entry for row & 7
static const byte kDissolveMasks[14][8] = {
	{0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80}, {0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81},
	{0x91, 0x62, 0x64, 0x98, 0x18, 0x24, 0x42, 0x81}, {0x91, 0x62, 0x64, 0x98, 0x19, 0x26, 0x46, 0x89},
	{0x91, 0x62, 0x64, 0x98, 0x99, 0x66, 0x66, 0x99}, {0x99, 0x66, 0x66, 0x99, 0x99, 0x66, 0x66, 0x99},
	{0xb9, 0x76, 0x6e, 0x9d, 0x9b, 0x67, 0x66, 0x99}, {0xb9, 0x76, 0xee, 0xdd, 0xbb, 0x77, 0x6e, 0x9d},
	{0xb9, 0x76, 0xef, 0xdf, 0xbf, 0x7f, 0x7e, 0xbd}, {0xbd, 0x7e, 0xff, 0xff, 0xff, 0xff, 0x7e, 0xbd},
	{0xbd, 0xfe, 0xff, 0xff, 0xff, 0xff, 0x7e, 0xbf}, {0xbd, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7e, 0xff},
	{0xfd, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff}, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}
};
static const uint kPictureRows = 304;
static const uint kSections = 10;
static const uint kFinalPicture = 10;
static const uint kTitlePicture = 11;
static const uint kFinPicture = 12;

// Pictures store their B, R, G and I planes one after another, each width bytes by height rows
static void drawPlanar(Graphics::Surface &page, const Common::Array<byte> &planes, uint width, uint height, uint sourceRow, uint rows, int x, int y) {
	for (uint row = 0; row < rows && y + int(row) < page.h; ++row)
		for (uint column = 0; column < width && x + int(column * 8) < page.w; ++column) {
			byte bits[4];
			for (uint plane = 0; plane < 4; ++plane)
				bits[plane] = planes[(plane * height + sourceRow + row) * width + column];
			byte *pixel = static_cast<byte *>(page.getBasePtr(x + column * 8, y + row));
			for (uint bit = 0; bit < 8; ++bit)
				pixel[bit] = ((bits[0] >> (7 - bit)) & 1) | ((bits[1] >> (7 - bit)) & 1) << 1 | ((bits[2] >> (7 - bit)) & 1) << 2 | ((bits[3] >> (7 - bit)) & 1) << 3;
		}
}

Ending::Ending(System98Engine &engine) : _engine(engine), _epoch(0), _ticks(0), _divider(0x80ff), _scroll(0), _scrollApplied(false),
	_fadeSteps(0), _fadePeriod(0), _fadeCounter(0), _picture(0), _column(0), _width(0), _fill(false), _line(0), _row(0) {
	memset(_palette, 0, sizeof(_palette));
	memset(_target, 0, sizeof(_target));
}

// One 56.4 Hz vertical sync and the handler ENDING.EXE installs on it; the skip action runs ticks without waiting
bool Ending::tick() {
	++_ticks;
	do {
		_engine.pollInput();
		if (_engine.shouldQuit())
			return false;
		if (_engine._skipMode) {
			_epoch = _engine._system->getMillis() - _ticks * 10000 / 564;
			break;
		}
		_engine._system->delayMillis(1);
	} while (_engine._system->getMillis() - _epoch < _ticks * 10000 / 564);
	_divider += 0x100;
	if (int16(_divider) > 0) {
		_divider -= 0x200;
		if (!_scrollApplied) {
			setScroll(_scroll);
			_scrollApplied = true;
		}
	}
	if (_fadeCounter && !--_fadeCounter) {
		stepFade();
		if (--_fadeSteps)
			_fadeCounter = _fadePeriod;
	}
	_engine.present();
	return true;
}

bool Ending::waitTicks(uint ticks) {
	while (ticks--)
		if (!tick())
			return false;
	return true;
}

bool Ending::waitFade() {
	while (_fadeSteps)
		if (!tick())
			return false;
	return true;
}

// The main loop posts a line once the handler has applied the previous one
bool Ending::waitScroll(uint16 line) {
	while (!_scrollApplied)
		if (!tick())
			return false;
	_scroll = line;
	_scrollApplied = false;
	return true;
}

void Ending::setScroll(uint16 line) {
	_engine._scrollLines = line % 400;
	_engine._scrollWords = 0;
}

void Ending::startFade(const uint16 *palette, uint period) {
	_fadePeriod = _fadeCounter = period;
	if (!period) {
		_fadeSteps = 0;
		memcpy(_palette, palette, sizeof(_palette));
		applyPalette();
	} else {
		memcpy(_target, palette, sizeof(_target));
		_fadeSteps = 15;
	}
}

// Components fall by one per step; a rising one waits until it is as far from its target as the steps left
void Ending::stepFade() {
	for (uint index = 0; index < 16; ++index) {
		uint16 color = 0;
		for (uint shift = 0; shift < 12; shift += 4) {
			const uint current = (_palette[index] >> shift) & 15, target = (_target[index] >> shift) & 15;
			uint value = current;
			if (current > target)
				--value;
			else if (current < target && target - current == _fadeSteps)
				++value;
			color |= value << shift;
		}
		_palette[index] = color;
	}
	applyPalette();
}

void Ending::applyPalette() {
	for (uint index = 0; index < 16; ++index) {
		_engine._palette[index * 3] = ((_palette[index] >> 4) & 15) * 17;
		_engine._palette[index * 3 + 1] = ((_palette[index] >> 8) & 15) * 17;
		_engine._palette[index * 3 + 2] = (_palette[index] & 15) * 17;
	}
}

// Blank text VRAM, then a row of glyphs whose top four lines are black hides the line the scroll wraps in
void Ending::resetText() {
	_engine._text.fillRect(Common::Rect(640, 400), 255);
	_engine._text.fillRect(Common::Rect(640, 4), 0);
	memset(_engine._glyphMask.data(), 0, _engine._glyphMask.size());
}

// Text blocks are user-defined 16x16 glyphs in black text attributes, laid out left to right and top to bottom
void Ending::drawText(uint section) {
	const uint16 *block = kTextBlocks[section];
	const uint columns = (block[3] - block[1]) / 2, rows = (block[2] - block[0]) / 16;
	const Common::Array<byte> &glyphs = _glyphs[section];
	if (columns * rows >= 0xbd || glyphs.size() < columns * rows * 32)
		return;
	for (uint glyph = 0; glyph < columns * rows; ++glyph) {
		const int x = block[1] * 8 + (glyph % columns) * 16, y = (block[0] / 16 + glyph / columns) * 16;
		for (uint line = 0; line < 16; ++line) {
			const uint bits = glyphs[glyph * 32 + line * 2] << 8 | glyphs[glyph * 32 + line * 2 + 1];
			for (uint bit = 0; bit < 16; ++bit)
				if (bits & (0x8000 >> bit))
					*static_cast<byte *>(_engine._text.getBasePtr(x + bit, y + line)) = 0;
		}
	}
}

void Ending::beginScroll(uint picture, uint column, uint width, bool fill) {
	_picture = picture;
	_column = column;
	_width = width;
	_fill = fill;
	_line = 0;
	_row = 0;
}

// Draws the picture line that the next scroll line brings in at the bottom, then clears lines once they leave the top
void Ending::scrollLine() {
	Graphics::Surface &page = _engine._pages[0];
	if (_line < kPictureRows) {
		if (_fill)
			page.fillRect(Common::Rect(0, _row, 640, _row + 1), 11);
		drawPlanar(page, _pictures[_picture], _width, kPictureRows, _line, 1, _column * 8, _row);
		++_row;
	} else {
		if (_line >= 400)
			page.fillRect(Common::Rect(0, _row, 640, _row + 1), 0);
		_row = (_row + 1) % 400;
	}
	++_line;
}

// Stages page 1 at a source row, then merges it into page 0 in 14 masked stages of 8 ticks each
bool Ending::dissolve(uint picture, uint column, uint row, uint sourceRow) {
	const uint width = picture == kTitlePicture ? 38 : 8, height = picture == kTitlePicture ? 96 : 40;
	if (_pictures[picture].size() != width * height * 4)
		return true;
	drawPlanar(_engine._pages[1], _pictures[picture], width, height, 0, height, column * 8, sourceRow);
	if (!waitTicks(picture == kTitlePicture ? 0x3a : 0x57))
		return false;
	for (uint stage = 0; stage < ARRAYSIZE(kDissolveMasks); ++stage) {
		for (uint line = 0; line < height; ++line) {
			const byte mask = kDissolveMasks[stage][line & 7];
			const byte *source = static_cast<const byte *>(_engine._pages[1].getBasePtr(column * 8, sourceRow + line));
			byte *destination = static_cast<byte *>(_engine._pages[0].getBasePtr(column * 8, row + line));
			for (uint x = 0; x < width * 8; ++x)
				if (mask & (0x80 >> (x & 7)))
					destination[x] = source[x];
		}
		if (!waitTicks(8))
			return false;
	}
	return true;
}

bool Ending::playSection(uint section) {
	resetText();
	if (_pictures[section].size() != kPictureWidths[section] * kPictureRows * 4)
		return true;
	beginScroll(section, kPictureColumns[section], kPictureWidths[section], true);
	drawText(section);
	if (!waitFade())
		return false;
	startFade(kShowPalette, 10);
	while (_line < 0x230) {
		scrollLine();
		if (!waitScroll(_line - 1))
			return false;
	}
	if (!waitFade())
		return false;
	startFade(kBlackPalette, 11);
	while (_line < 0x2c0) {
		scrollLine();
		if (!waitScroll(_line - 1))
			return false;
	}
	return true;
}

// ENDING.EXE starts and ends by blanking text VRAM, clearing graphics page 0 and resetting the scroll
void Ending::resetScreen() {
	_engine._text.fillRect(Common::Rect(640, 400), 255);
	memset(_engine._glyphMask.data(), 0, _engine._glyphMask.size());
	_engine._pages[0].fillRect(Common::Rect(640, 400), 0);
	setScroll(0);
	_engine._displayPage = 0;
}

bool Ending::play() {
	Common::Array<byte> data;
	if (!_engine._resources.load("ENDING.GLB", data) || !decodeLzssArchive(data, _pictures) || _pictures.size() <= kFinPicture ||
	    !_engine._resources.load("ENDING.FNT", data) || !decodeLzssArchive(data, _glyphs) || _glyphs.size() < kSections) {
		warning("System98: ENDING.GLB or ENDING.FNT is missing or invalid; skipping the staff roll");
		return false;
	}
	resetScreen();
	startFade(kBlackPalette, 0);
	_epoch = _engine._system->getMillis();
	for (uint section = 0; section < kSections; ++section)
		if (!playSection(section))
			return false;

	// The last picture scrolls in over black and stops 368 lines in
	resetText();
	if (_pictures[kFinalPicture].size() == kPictureWidths[kFinalPicture] * kPictureRows * 4) {
		beginScroll(kFinalPicture, kPictureColumns[kFinalPicture], kPictureWidths[kFinalPicture], false);
		if (!waitFade())
			return false;
		startFade(kShowPalette, 9);
		while (_line < 0x170) {
			scrollLine();
			if (!waitScroll(_line - 1))
				return false;
		}
	}
	// Then it is copied to page 1 at row 32 and from there back to page 0, which is shown unscrolled
	_engine._pages[1].fillRect(Common::Rect(640, 400), 0);
	_engine._pages[1].copyRectToSurface(_engine._pages[0], 80, 32, Common::Rect(80, 0, 560, kPictureRows));
	if (!tick())
		return false;
	_scrollApplied = true;
	setScroll(0);
	_engine._pages[0].copyRectToSurface(_engine._pages[1], 0, 0, Common::Rect(640, 400));
	if (!dissolve(kTitlePicture, 22, 136, 304) || !dissolve(kFinPicture, 57, 344, 344) || !waitTicks(0x244))
		return false;

	// Return ends the staff roll; the accept action and the left button stand in for it
	_engine._inputPressed = 0;
	while (!((_engine._inputPressed | _engine.heldInput()) & 2))
		if (!tick())
			return false;
	startFade(kBlackPalette, 9);
	if (!waitFade())
		return false;
	resetScreen();
	_engine.present();
	return true;
}

} // namespace System98
