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

#include <cxxtest/TestSuite.h>
#include "engines/delta/transition.h"

class DeltaTransitionTestSuite : public CxxTest::TestSuite {
	static const Graphics::PixelFormat format() {
		return Graphics::PixelFormat(4, 8, 8, 8, 0, 16, 8, 0, 0);
	}

	static void fill(Graphics::Surface &surface, byte red, byte green, byte blue) {
		surface.create(2, 1, format());
		surface.fillRect(Common::Rect(2, 1), format().RGBToColor(red, green, blue));
	}

	static void rgb(const Graphics::Surface &surface, int x, byte &red, byte &green, byte &blue) {
		format().colorToRGB(surface.getPixel(x, 0), red, green, blue);
	}

public:
	void testDurationUsesTenthsAndEffectSetting() {
		TS_ASSERT_EQUALS(Delta::transitionDuration(5, 1), 500u);
		TS_ASSERT_EQUALS(Delta::transitionDuration(5, 2), 250u);
		TS_ASSERT_EQUALS(Delta::transitionDuration(5, 0), 0u);
		TS_ASSERT_EQUALS(Delta::transitionDuration(-1, 1), 0u);
		TS_ASSERT_EQUALS(Delta::transitionDuration(0, 1), 0u);
	}

	void testStepsFollowSeventeenthSchedule() {
		// 1700 ms: step i is due at 100 * (i + 1) ms and the last at 1600 ms
		TS_ASSERT_EQUALS(Delta::transitionStep(0, 1700), -1);
		TS_ASSERT_EQUALS(Delta::transitionStep(99, 1700), -1);
		TS_ASSERT_EQUALS(Delta::transitionStep(100, 1700), 0);
		TS_ASSERT_EQUALS(Delta::transitionStep(850, 1700), 7);
		TS_ASSERT_EQUALS(Delta::transitionStep(1600, 1700), 15);
		TS_ASSERT_EQUALS(Delta::transitionStep(5000, 1700), 15);
		// Late frames are dropped rather than replayed
		TS_ASSERT_EQUALS(Delta::transitionStep(1000, 500), 15);
	}

	void testCrossFadeWeightsSixteenths() {
		Graphics::Surface from, to, frame;
		fill(from, 200, 0, 17);
		fill(to, 40, 255, 17);
		to.setPixel(1, 0, format().RGBToColor(200, 0, 17));
		Delta::renderTransitionStep(frame, from, to, Delta::kTransitionCrossFade, 3, Common::Rect(2, 1));
		byte red, green, blue;
		rgb(frame, 0, red, green, blue);
		TS_ASSERT_EQUALS(red, (40 * 4 >> 4) + (200 * 12 >> 4));
		TS_ASSERT_EQUALS(green, 255 * 4 >> 4);
		// Each term truncates separately, so equal channels in a changed pixel drift down
		TS_ASSERT_EQUALS(blue, (17 * 4 >> 4) + (17 * 12 >> 4));
		// Unchanged pixels are copied exactly
		rgb(frame, 1, red, green, blue);
		TS_ASSERT_EQUALS(red, 200);
		TS_ASSERT_EQUALS(blue, 17);
		from.free();
		to.free();
		frame.free();
	}

	void testBlackAndWhiteFades() {
		Graphics::Surface from, to, frame;
		fill(from, 160, 160, 160);
		fill(to, 64, 64, 64);
		byte red, green, blue;
		Delta::renderTransitionStep(frame, from, to, Delta::kTransitionFromBlack, 0, Common::Rect(2, 1));
		rgb(frame, 0, red, green, blue);
		TS_ASSERT_EQUALS(red, 64 >> 4);
		Delta::renderTransitionStep(frame, from, to, Delta::kTransitionFromWhite, 0, Common::Rect(2, 1));
		rgb(frame, 0, red, green, blue);
		TS_ASSERT_EQUALS(red, 255);
		Delta::renderTransitionStep(frame, from, to, Delta::kTransitionToBlack, 15, Common::Rect(2, 1));
		rgb(frame, 0, red, green, blue);
		TS_ASSERT_EQUALS(red, 160 >> 4);
		Delta::renderTransitionStep(frame, from, to, Delta::kTransitionToWhite, 7, Common::Rect(2, 1));
		rgb(frame, 0, red, green, blue);
		TS_ASSERT_EQUALS(red, 160 + (95 * 8 >> 4));
		from.free();
		to.free();
		frame.free();
	}

	void testAreaLimitsEffect() {
		Graphics::Surface from, to, frame;
		fill(from, 160, 160, 160);
		fill(to, 0, 0, 0);
		Delta::renderTransitionStep(frame, from, to, Delta::kTransitionToBlack, 0, Common::Rect(1, 1));
		byte red, green, blue;
		rgb(frame, 0, red, green, blue);
		TS_ASSERT_EQUALS(red, 160);
		rgb(frame, 1, red, green, blue);
		TS_ASSERT_EQUALS(red, 0);
		from.free();
		to.free();
		frame.free();
	}
};
