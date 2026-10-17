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
	for (int y = 0; y < 400; ++y) {
		const byte *background = static_cast<const byte *>(_pages[_displayPage].getBasePtr(0, y));
		const byte *text = static_cast<const byte *>(_text.getBasePtr(0, y));
		byte *destination = static_cast<byte *>(_screen.getBasePtr(0, y));
		for (int x = 0; x < 640; ++x)
			destination[x] = text[x] == 255 ? background[x] : text[x] & 0x80 ? text[x] & 15 : 16 + text[x];
	}
	if (_menuEnabled && !_coordinateMenu && _menuStyle < 8 && _windowStyles.getPixels()) {
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
	_drawPage = 0;
	return true;
}

void System98Engine::fadePalette(bool out, uint delay, bool monochrome) {
	byte start[48], end[48];
	memcpy(start, _palette, sizeof(start));
	memcpy(end, _targetPalette, sizeof(end));
	if (monochrome) {
		memset(start, out ? 255 : 0, sizeof(start));
	} else {
		memset(end, out ? 255 : 0, sizeof(end));
	}
	for (int step = 0; step <= 15 && !shouldQuit(); ++step) {
		for (uint color = 0; color < 16; ++color)
			if (!(_paletteLock & (1 << color)))
				for (uint component = 0; component < 3; ++component) {
					const uint index = color * 3 + component;
					_palette[index] = (start[index] * (15 - step) + end[index] * step) / 15;
				}
		present();
		waitFrames(delay);
	}
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

bool System98Engine::waitForTextAdvance() {
	_keyPressed = false;
	bool pressed = false;
	present();
	while (!shouldQuit()) {
		pollInput();
		if (_skipRequested || _skipMode) {
			_keyPressed = false;
			_skipRequested = false;
			return true;
		}
		if (_keyPressed) {
			pressed = true;
			_keyPressed = false;
		}
		if (pressed && !_textActionsHeld && !_textMouseHeld)
			return true;
		_system->delayMillis(5);
	}
	return false;
}

bool System98Engine::waitTextDelay(uint frames, bool &fastText) {
	if (!frames || fastText)
		return !shouldQuit();
	// The glyph just drawn must be visible while the delay runs
	present();
	const uint32 start = _system->getMillis();
	const uint32 duration = frames * 10000 / 564;
	while (!shouldQuit() && _system->getMillis() - start < duration) {
		pollInput();
		if (_keyPressed || _skipRequested || _skipMode) {
			_keyPressed = false;
			fastText = true;
			return true;
		}
		_system->delayMillis(1);
	}
	return !shouldQuit();
}

void System98Engine::printText(uint box, uint offset) {
	if (box >= ARRAYSIZE(_textBoxes)) {
		_scriptValid = false;
		return;
	}
	_keyPressed = false;
	bool fastText = _skipMode || _skipRequested;
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
			if (!data[position++]) {
				if (position >= data.size()) { _scriptValid = false; return; }
				_menuStyle = data[position++];
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
		} else {
			text += char(value);
		}
	}
	for (uint index = 0; index < text.size() && !shouldQuit(); ++index) {
		const byte value = text[index];
		if (value == 1 || value == 2) {
			if (!waitForTextAdvance())
				return;
			fastText = _skipMode;
			if (value == 1)
				clearTextBox(box, false);
		} else if (value == 3) {
			if (++index < text.size())
				textBox.color = text[index] & 15;
		} else if (value == 13) {
			textBox.x = textBox.rect.left;
			textBox.y += reg(0x402) ? reg(0x402) : 16;
		} else if (value >= 0x20) {
			uint16 character = value;
			int width = 8;
			if ((value >= 0x81 && value <= 0x9f) || value >= 0xe0) {
				if (++index >= text.size())
					break;
				character |= byte(text[index]) << 8;
				width = 16;
			}
			if (textBox.x + width > textBox.rect.right) {
				textBox.x = textBox.rect.left;
				textBox.y += reg(0x402) ? reg(0x402) : 16;
			}
			if (textBox.y + 16 <= textBox.rect.bottom)
				drawCharacter(character, textBox.x, textBox.y, textBox.color, textBox.style != 0);
			textBox.x += width;
			if (!waitTextDelay(_characterDelay, fastText))
				return;
		}
	}
	present();
}

} // namespace System98
