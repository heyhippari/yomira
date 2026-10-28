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

#include "delta/transition.h"

namespace Delta {

uint32 transitionDuration(int units, int effectSetting) {
	if (units <= 0 || effectSetting <= 0)
		return 0;
	return units * 100 / effectSetting;
}

int transitionStep(uint32 elapsed, uint32 duration) {
	int step = -1;
	while (step + 1 < kTransitionSteps && duration * (step + 2) / (kTransitionSteps + 1) <= elapsed)
		++step;
	return step;
}

// The native loops weight 8-bit channels in sixteenths with MMX (pmullw, psrlw 4)
static byte blendChannel(byte from, byte to, TransitionEffect effect, int step) {
	switch (effect) {
	case kTransitionCrossFade:
		return (to * (step + 1) >> 4) + (from * (15 - step) >> 4);
	case kTransitionFromBlack:
		return to * (step + 1) >> 4;
	case kTransitionFromWhite:
		return to + ((255 - to) * (16 - step) >> 4);
	case kTransitionToBlack:
		return from * (16 - step) >> 4;
	case kTransitionToWhite:
	default:
		return from + ((255 - from) * (step + 1) >> 4);
	}
}

void renderTransitionStep(Graphics::Surface &frame, const Graphics::Surface &from, const Graphics::Surface &to,
	TransitionEffect effect, int step, const Common::Rect &area) {
	frame.copyFrom(to);
	Common::Rect clip(area);
	clip.clip(Common::Rect(frame.w, frame.h));
	for (int y = clip.top; y < clip.bottom; ++y) {
		for (int x = clip.left; x < clip.right; ++x) {
			const uint32 oldColor = from.getPixel(x, y);
			const uint32 newColor = to.getPixel(x, y);
			// 0x40edc0 copies unchanged data instead of blending it
			if (effect == kTransitionCrossFade && oldColor == newColor)
				continue;
			byte oldRed, oldGreen, oldBlue, newRed, newGreen, newBlue;
			from.format.colorToRGB(oldColor, oldRed, oldGreen, oldBlue);
			to.format.colorToRGB(newColor, newRed, newGreen, newBlue);
			frame.setPixel(x, y, frame.format.RGBToColor(blendChannel(oldRed, newRed, effect, step),
				blendChannel(oldGreen, newGreen, effect, step), blendChannel(oldBlue, newBlue, effect, step)));
		}
	}
}

} // End of namespace Delta
