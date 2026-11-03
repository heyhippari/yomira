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

#ifndef SYSTEM98_ENDING_H
#define SYSTEM98_ENDING_H

#include "common/array.h"

namespace System98 {

class System98Engine;

// The staff roll that ENDING.EXE shows when opcode 0x97 runs it
class Ending {
public:
	explicit Ending(System98Engine &engine);
	// Returns false when the data is missing or the engine is quitting
	bool play();

private:
	bool tick();
	bool waitTicks(uint ticks);
	bool waitFade();
	bool waitScroll(uint16 line);
	void setScroll(uint16 line);
	void startFade(const uint16 *palette, uint period);
	void stepFade();
	void applyPalette();
	void resetText();
	void drawText(uint section);
	void beginScroll(uint picture, uint column, uint width, bool fill);
	void scrollLine();
	bool dissolve(uint picture, uint column, uint row, uint sourceRow);
	bool playSection(uint section);
	void resetScreen();

	System98Engine &_engine;
	Common::Array<Common::Array<byte> > _pictures;
	Common::Array<Common::Array<byte> > _glyphs;
	uint32 _epoch;
	uint32 _ticks;
	// The vertical-sync handler state: a divider that applies a posted scroll line every second tick
	uint16 _divider;
	uint16 _scroll;
	bool _scrollApplied;
	uint16 _palette[16];
	uint16 _target[16];
	uint _fadeSteps;
	uint _fadePeriod;
	uint _fadeCounter;
	uint _picture;
	uint _column;
	uint _width;
	bool _fill;
	uint _line;
	uint _row;
};

} // namespace System98

#endif
