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

#ifndef DELTA_TRANSITION_H
#define DELTA_TRANSITION_H

#include "common/rect.h"
#include "graphics/surface.h"

namespace Delta {

enum TransitionEffect {
	kTransitionCrossFade, // 0x40edc0, 0x412f50
	kTransitionFromBlack, // 0x411880
	kTransitionFromWhite, // 0x411fa0
	kTransitionToBlack,   // 0x411c00
	kTransitionToWhite    // 0x412420
};

static const int kTransitionSteps = 16;

/**
 * Converts a script duration in 100 ms units into milliseconds, divided by
 * the EFFECT setting (1 normal, 2 fast). Zero means an instant cut, which is
 * also the result for EFFECT 0 (off) and for the -1 (0xff) duration.
 */
uint32 transitionDuration(int units, int effectSetting);

/**
 * Returns the last of the 16 steps due after elapsed milliseconds, or -1
 * before the first. Step i is shown at duration * (i + 1) / 17.
 */
int transitionStep(uint32 elapsed, uint32 duration);

/**
 * Renders one step into frame. Outside area, frame receives to. All three
 * surfaces must share a size and pixel format.
 */
void renderTransitionStep(Graphics::Surface &frame, const Graphics::Surface &from, const Graphics::Surface &to,
	TransitionEffect effect, int step, const Common::Rect &area);

} // End of namespace Delta

#endif
