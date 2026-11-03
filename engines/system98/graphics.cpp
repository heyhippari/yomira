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

#include "common/debug.h"
#include "common/system.h"
#include "common/ustr.h"
#include "graphics/font.h"
#include "graphics/paletteman.h"
#include "graphics/sjis.h"
#include "system98/image.h"
#include "system98/system98.h"

namespace System98 {

void System98Engine::present() {
	// Hiding the text shows page 1 without the text plane; text-box graphics belong to page 0
	const uint page = _textHidden ? 1 : _displayPage;
	const byte *background = static_cast<const byte *>(_pages[page].getPixels());
	const byte *text = static_cast<const byte *>(_text.getPixels());
	for (int y = 0; y < 400; ++y) {
		// Graphics VRAM is read linearly from each area's start, wrapping at 0x4000 words per plane
		const uint start = y < 400 - _scrollLines ? (_scrollLines + y) * 640 + _scrollWords * 16 : (y - (400 - _scrollLines)) * 640;
		byte *destination = static_cast<byte *>(_screen.getBasePtr(0, y));
		for (int x = 0; x < 640; ++x) {
			const uint source = (start + x) % (0x4000 * 16);
			const byte hardwareText = text[y * 640 + x];
			if (hardwareText != 255 && !(hardwareText & 0x80) && !_textHidden)
				destination[x] = 16 + hardwareText;
			else if (source >= 640 * 400)
				destination[x] = 0;
			else
				destination[x] = !page && text[source] != 255 && (text[source] & 0x80) ? text[source] & 15 : background[source];
		}
	}
	if (!page && _menuEnabled && !_coordinateMenu && _menuStyle < 8 && _windowStyles.getPixels()) {
		for (const MenuItem &item : _menu) {
			if (!item.rect.contains(_mouse))
				continue;
			const int width = (item.rect.width() / 16 + 2) * 16;
			drawWindowTiles(_windowStyles, _screen, Common::Rect(item.rect.left - 16, item.rect.top - 16, item.rect.left - 16 + width, item.rect.top + 32), _menuStyle, true, _glyphMask.data());
			break;
		}
	}
	static const byte textPalette[] = {0,0,0, 0,0,255, 255,0,0, 255,0,255, 0,255,0, 0,255,255, 255,255,0, 255,255,255};
	_system->getPaletteManager()->setPalette(_palette, 0, 16);
	_system->getPaletteManager()->setPalette(textPalette, 16, 8);
	_system->copyRectToScreen(_screen.getPixels(), _screen.pitch, 0, 0, 640, 400);
	_system->updateScreen();
}

bool System98Engine::loadImage(const Common::String &name, int x, int y, uint page, bool palette, bool transparent) {
	Common::Array<byte> data;
	PiImage image;
	if (page >= ARRAYSIZE(_pages) || !_resources.load(name, data) || !image.load(data)) {
		warning("System98: cannot decode image %s", name.c_str());
		return false;
	}
	const Graphics::Surface &source = image.surface();
	debug(1, "System98: image %s %dx%d -> page %u (%d,%d)", name.c_str(), source.w, source.h, page, x, y);
	for (int row = 0; row < source.h && row + y < 400; ++row) {
		if (row + y < 0)
			continue;
		for (int column = 0; column < source.w && column + x < 640; ++column) {
			if (column + x < 0)
				continue;
			const byte color = *static_cast<const byte *>(source.getBasePtr(column, row));
			if (!transparent || color)
				*static_cast<byte *>(_pages[page].getBasePtr(column + x, row + y)) = color;
		}
	}
	memcpy(_targetPalette, image.palette(), sizeof(_targetPalette));
	if (palette)
		memcpy(_palette, _targetPalette, sizeof(_palette));
	present();
	return true;
}

void System98Engine::copyPage(uint source, const Common::Rect &rect, uint destination, int x, int y, bool transparent) {
	if (source >= ARRAYSIZE(_pages) || destination >= ARRAYSIZE(_pages) || rect.width() < 0 || rect.width() > 640 || rect.height() < 0 || rect.height() > 400) {
		_scriptValid = false;
		return;
	}
	Graphics::Surface copy;
	copy.create(MAX<int>(1, rect.width()), MAX<int>(1, rect.height()), Graphics::PixelFormat::createFormatCLUT8());
	copy.fillRect(Common::Rect(copy.w, copy.h), 0);
	for (int row = 0; row < rect.height(); ++row)
		for (int column = 0; column < rect.width(); ++column)
			if (rect.left + column >= 0 && rect.left + column < 640 && rect.top + row >= 0 && rect.top + row < 400)
				*static_cast<byte *>(copy.getBasePtr(column, row)) = *static_cast<const byte *>(_pages[source].getBasePtr(rect.left + column, rect.top + row));
	for (int row = 0; row < copy.h && row + y < 400; ++row)
		for (int column = 0; column < copy.w && column + x < 640; ++column)
			if (x + column >= 0 && y + row >= 0) {
				const byte color = *static_cast<const byte *>(copy.getBasePtr(column, row));
				if (!transparent || color)
					*static_cast<byte *>(_pages[destination].getBasePtr(column + x, row + y)) = color;
			}
	copy.free();
	present();
}

bool System98Engine::patternedBlit(const Common::Rect &sourceRect, const Common::Point &position, uint pattern, uint delay) {
	if (pattern >= ARRAYSIZE(_patterns) || _patterns[pattern].empty())
		return true;
	for (uint stage = 0; stage < _patterns[pattern].size() / 8 && !shouldQuit(); ++stage) {
		if (!applyBlitPattern(_pages[1], _pages[0], sourceRect, position, _patterns[pattern], stage))
			return false;
		present();
		if (!waitFrames(delay))
			break;
	}
	return true;
}

// 0x07 starts unlocked colors at black or white and steps them to the image palette; 0x08 steps them to black
// or white. Each step moves every 4-bit component by one and waits before showing it, until all have arrived.
void System98Engine::fadePalette(bool white, uint delay, bool toImage) {
	byte level[48], target[48];
	for (uint index = 0; index < 48; ++index) {
		const bool locked = _paletteLock & (1 << (index / 3));
		level[index] = _palette[index] / 17;
		target[index] = locked ? level[index] : toImage ? _targetPalette[index] / 17 : white ? 15 : 0;
		if (toImage && !locked)
			level[index] = white ? 15 : 0;
	}
	bool done;
	do {
		done = true;
		for (uint index = 0; index < 48; ++index) {
			if (level[index] < target[index])
				++level[index];
			else if (level[index] > target[index])
				--level[index];
			done &= level[index] == target[index];
		}
		if (!waitFrames(delay))
			return;
		for (uint index = 0; index < 48; ++index)
			_palette[index] = level[index] * 17;
		present();
	} while (!done);
}

void System98Engine::openTextBox(uint box, const Common::Rect &frame, uint style) {
	TextBox &textBox = _textBoxes[box];
	if (textBox.active)
		return;
	textBox.frame = frame;
	textBox.frame.clip(Common::Rect(640, 400));
	if (textBox.frame.isEmpty() || style > 8) {
		_scriptValid = false;
		return;
	}
	textBox.style = style;
	textBox.active = true;
	textBox.rect = Common::Rect(textBox.frame.left + 16, textBox.frame.top + 16, textBox.frame.right - 16, textBox.frame.bottom);
	textBox.x = textBox.rect.left;
	textBox.y = textBox.rect.top;
	textBox.backing.resize(textBox.frame.width() * textBox.frame.height());
	textBox.backingMask.resize(textBox.backing.size());
	for (int row = 0; row < textBox.frame.height(); ++row) {
		memcpy(textBox.backing.data() + row * textBox.frame.width(), _text.getBasePtr(textBox.frame.left, textBox.frame.top + row), textBox.frame.width());
		memcpy(textBox.backingMask.data() + row * textBox.frame.width(), _glyphMask.data() + (textBox.frame.top + row) * 640 + textBox.frame.left, textBox.frame.width());
		memset(_glyphMask.data() + (textBox.frame.top + row) * 640 + textBox.frame.left, 0, textBox.frame.width());
	}
	drawTextBox(box);
	present();
}

void System98Engine::drawTextBox(uint box) {
	const TextBox &textBox = _textBoxes[box];
	if (!textBox.style || !_windowStyles.getPixels())
		return;
	if (!drawWindowTiles(_windowStyles, _text, textBox.frame, textBox.style - 1, false, nullptr, 0x80))
		_scriptValid = false;
}

// Cells are 8x16 within 80x25; graphics-colored overlay pixels belong to the graphics planes, so clearing keeps them
void System98Engine::fillTextCells(const Common::Rect &cells, bool block, byte color) {
	const Common::Rect rect(cells.left * 8, cells.top * 16, cells.right * 8, cells.bottom * 16);
	for (int y = rect.top; y < rect.bottom; ++y)
		for (int x = rect.left; x < rect.right; ++x) {
			byte &pixel = *static_cast<byte *>(_text.getBasePtr(x, y));
			if (block) {
				pixel = color;
				_glyphMask[y * 640 + x] = 1;
			} else if (pixel != 255 && !(pixel & 0x80)) {
				pixel = 255;
				_glyphMask[y * 640 + x] = 0;
			}
		}
	present();
}

void System98Engine::clearTextBox(uint box, bool close) {
	TextBox &textBox = _textBoxes[box];
	if (!textBox.active)
		return;
	for (int row = 0; row < textBox.frame.height(); ++row) {
		memcpy(_text.getBasePtr(textBox.frame.left, textBox.frame.top + row), textBox.backing.data() + row * textBox.frame.width(), textBox.frame.width());
		memcpy(_glyphMask.data() + (textBox.frame.top + row) * 640 + textBox.frame.left, textBox.backingMask.data() + row * textBox.frame.width(), textBox.frame.width());
	}
	textBox.x = textBox.rect.left;
	textBox.y = textBox.rect.top;
	if (close) {
		textBox.active = false;
		textBox.backing.clear();
		textBox.backingMask.clear();
	} else {
		for (int row = 0; row < textBox.frame.height(); ++row)
			memset(_glyphMask.data() + (textBox.frame.top + row) * 640 + textBox.frame.left, 0, textBox.frame.width());
		drawTextBox(box);
	}
	present();
}

void System98Engine::drawCharacter(uint16 character, int x, int y, byte color, bool graphics) {
	if (x < 0 || x >= 640 || y < 0 || y >= 400)
		return;
	const byte lead = character & 255;
	const byte trail = character >> 8;
	if (lead >= 0xeb && lead <= 0xec && trail >= 0x40 && trail != 0x7f) {
		uint row = (lead - 0xc1) * 2 + 0x21;
		uint column = trail - (trail < 0x7f ? 0x1f : 0x20);
		if (trail >= 0x9f) { ++row; column = trail - 0x7e; }
		if (row >= 0x76 && row <= 0x77 && column >= 0x21 && column <= 0x7e) {
			const uint glyph = (row - 0x76) * 94 + column - 0x21;
			for (int rowPixel = 0; rowPixel < 16 && y + rowPixel < 400; ++rowPixel)
				for (int columnPixel = 0; columnPixel < 16 && x + columnPixel < 640; ++columnPixel)
					if (_font[glyph * 32 + rowPixel * 2 + columnPixel / 8] & (0x80 >> (columnPixel & 7))) {
						*static_cast<byte *>(_text.getBasePtr(x + columnPixel, y + rowPixel)) = graphics ? 0x80 | (color & 15) : color & 7;
						_glyphMask[(y + rowPixel) * 640 + x + columnPixel] = 1;
					}
			return;
		}
	}
	if (lead == 0x85 && trail >= 0x40 && trail <= 0x9e)
		character = trail - 0x1f - (trail >= 0x80 ? 1 : 0);
	Graphics::Surface glyph;
	glyph.create(16, 16, Graphics::PixelFormat::createFormatCLUT8());
	glyph.fillRect(Common::Rect(16, 16), 255);
	if (_sjisFont)
		_sjisFont->drawChar(glyph, character, 0, 0, color & 15, 255);
	else if (_fallbackFont) {
		const char bytes[] = {char(character & 255), char(character >> 8), 0};
		const Common::U32String text(bytes, Common::kWindows932);
		if (!text.empty())
			_fallbackFont->drawChar(&glyph, text[0], 0, 0, color & 15);
	}
	for (int row = 0; row < 16 && y + row < 400; ++row)
		for (int column = 0; column < 16 && x + column < 640; ++column) {
			const byte pixel = *static_cast<const byte *>(glyph.getBasePtr(column, row));
			if (pixel != 255) {
				*static_cast<byte *>(_text.getBasePtr(x + column, y + row)) = graphics ? 0x80 | pixel : pixel & 7;
				_glyphMask[(y + row) * 640 + x + column] = 1;
			}
		}
	glyph.free();
}

// JIS row and column to the Shift-JIS lead and trail bytes, packed as drawCharacter expects
static uint16 jisToShiftJis(uint16 code) {
	const uint row = code >> 8, column = code & 0xff;
	const uint lead = ((row + 1) >> 1) + (row < 0x5f ? 0x70 : 0xb0);
	const uint trail = row & 1 ? column + (column >= 0x60 ? 0x20 : 0x1f) : column + 0x7e;
	return lead | trail << 8;
}

// The vertical-sync handler steps the marker once per frame while a text wait runs
void System98Engine::stepIdleCharacter(uint box) {
	const uint32 frame = uint64(_system->getMillis()) * 564 / 10000;
	if (!_idleVisible) {
		_idlePosition = box < ARRAYSIZE(_textBoxes) && _textBoxes[box].active ?
			Common::Point(_textBoxes[box].frame.right - 32 - _idleOffset[0] * 8, _textBoxes[box].frame.bottom - 32 - _idleOffset[1] * 16) :
			Common::Point(576, 352);
		for (int row = 0; row < 16; ++row)
			for (int column = 0; column < 16; ++column) {
				const int x = CLIP<int>(_idlePosition.x + column, 0, 639), y = CLIP<int>(_idlePosition.y + row, 0, 399);
				_idleBacking[0][row * 16 + column] = *static_cast<const byte *>(_text.getBasePtr(x, y));
				_idleBacking[1][row * 16 + column] = _glyphMask[y * 640 + x];
			}
		_idleFrame = frame;
	}
	const byte shown = _idleIndex;
	for (; _idleFrame < frame; ++_idleFrame)
		if (!--_idleCounter) {
			_idleIndex = (_idleIndex + 1) & 7;
			if (!_idleCharacters[_idleIndex])
				_idleIndex = 0;
			_idleCounter = _idleDelays[_idleIndex];
		}
	if (_idleVisible && shown == _idleIndex)
		return;
	removeIdleCharacter();
	drawCharacter(jisToShiftJis(_idleCharacters[_idleIndex]), _idlePosition.x, _idlePosition.y, 7);
	_idleVisible = true;
	present();
}

void System98Engine::removeIdleCharacter() {
	if (!_idleVisible)
		return;
	for (int row = 0; row < 16; ++row)
		for (int column = 0; column < 16; ++column) {
			const int x = _idlePosition.x + column, y = _idlePosition.y + row;
			if (x >= 0 && x < 640 && y >= 0 && y < 400) {
				*static_cast<byte *>(_text.getBasePtr(x, y)) = _idleBacking[0][row * 16 + column];
				_glyphMask[y * 640 + x] = _idleBacking[1][row * 16 + column];
			}
		}
	_idleVisible = false;
}

// Turns the text plane off and shows page 1 from one press until the next
bool System98Engine::showHiddenText() {
	_textHidden = true;
	present();
	while ((heldInput() & 3) && waitFrames(1)) {
	}
	_inputPressed = 0;
	while (!((heldInput() | _inputPressed) & 3) && waitFrames(1)) {
	}
	_textHidden = false;
	present();
	while ((heldInput() & 3) && waitFrames(1)) {
	}
	_inputPressed = 0;
	return !shouldQuit();
}

// Accept continues; cancel shows the hidden-text view when 0Fh 02h allowed it, and otherwise does nothing.
// ScummVM's skip action also continues unless 0Fh 01h made delays mandatory.
bool System98Engine::waitForTextAdvance(uint box) {
	present();
	_inputPressed = 0;
	while (true) {
		pollInput();
		if (shouldQuit())
			return false;
		if ((_skipMode || _skipRequested) && !_textDelayForced) {
			_skipRequested = false;
			break;
		}
		const byte input = heldInput() | _inputPressed;
		_inputPressed = 0;
		if (input & 1) {
			if (_textHideEnabled && !showHiddenText())
				return false;
		} else if (input & 2) {
			break;
		}
		stepIdleCharacter(box);
		_system->delayMillis(5);
	}
	while ((heldInput() & 3) && waitFrames(1)) {
	}
	removeIdleCharacter();
	present();
	return !shouldQuit();
}

// The skip key drops character delays unless 0Fh 01h made them mandatory; holding cancel cuts each one short
bool System98Engine::waitTextDelay(uint frames) {
	if (!frames || (_skipMode && !_textDelayForced))
		return !shouldQuit();
	// The glyph just drawn must be visible while the delay runs
	present();
	for (uint frame = 0; frame < frames; ++frame) {
		if (!waitFrames(1))
			return false;
		if (heldInput() & 1)
			break;
	}
	return true;
}

void System98Engine::printText(uint box, uint offset) {
	if (box >= ARRAYSIZE(_textBoxes)) {
		_scriptValid = false;
		return;
	}
	TextBox &textBox = _textBoxes[box];
	const Common::Array<byte> &data = sceneBank(sceneBankOf(offset));
	uint position = sceneBankOffset(offset);
	Common::String text;
	while (position < data.size()) {
		const byte value = data[position++];
		if (!value)
			break;
		if (value == 0x0f) {
			if (position >= data.size()) { _scriptValid = false; return; }
			const byte control = data[position++];
			if (!control) {
				if (position >= data.size()) { _scriptValid = false; return; }
				_menuStyle = data[position++];
			} else if (control <= 3) {
				text += char(value);
				text += char(control);
			}
		} else if (value == 4) {
			if (position >= data.size() || data[position] >= ARRAYSIZE(_strings)) { _scriptValid = false; return; }
			text += reinterpret_cast<const char *>(_strings[data[position++]]);
		} else if (value == 5) {
			if (position + 2 > data.size()) { _scriptValid = false; return; }
			text += Common::String::format("%u", reg(READ_LE_UINT16(data.data() + position)));
			position += 2;
		} else if (value == 3) {
			if (position >= data.size()) { _scriptValid = false; return; }
			text += char(value);
			text += char(data[position++] | 0x80);
		} else if ((value >= 6 && value <= 8) || (value >= 0x0a && value <= 0x0c) || value == 0x0e) {
			// The native printer consumes one operand byte for these and draws nothing
			if (position >= data.size()) { _scriptValid = false; return; }
			++position;
		} else {
			text += char(value);
		}
	}
	for (uint index = 0; index < text.size() && !shouldQuit(); ++index) {
		const byte value = text[index];
		if (value == 0x0f) {
			const byte control = text[++index];
			if (control == 1)
				_textDelayForced = !_textDelayForced;
			else
				_textHideEnabled = control == 2;
		} else if (value == 1 || value == 2) {
			if (!waitForTextAdvance(box))
				return;
			if (value == 1)
				clearTextBox(box, false);
		} else if (value == 3) {
			if (++index < text.size())
				_textColor = text[index] & 15;
		} else if (value == 9) {
			textBox.x += 64;
		} else if (value == 13) {
			textBox.x = textBox.rect.left;
			textBox.y += reg(0x402) ? reg(0x402) : 16;
		} else if (value >= 0x10) {
			// Scripts break lines themselves; the native printer never wraps or clips to the box
			uint16 character = value;
			int width = 8;
			if ((value >= 0x81 && value <= 0x9f) || value >= 0xe0) {
				if (++index >= text.size())
					break;
				const byte trail = text[index];
				character |= trail << 8;
				// JIS rows 0x29 to 0x2b (Shift-JIS 8540h to 869Eh) are NEC half-width characters
				if (value != 0x85 && (value != 0x86 || trail > 0x9e))
					width = 16;
			}
			drawCharacter(character, textBox.x, textBox.y, _textColor, textBox.style != 0);
			textBox.x += width;
			// Full-width spaces print without a delay
			if (character != 0x4081 && !waitTextDelay(_characterDelay))
				return;
		}
	}
	present();
}

} // namespace System98
