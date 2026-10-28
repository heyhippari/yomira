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
#include "engines/delta/options.h"

class DeltaOptionsTestSuite : public CxxTest::TestSuite {
	static Common::Point centre(const Delta::OptionsScreen &screen, uint control) {
		const Common::Rect &rect = screen.controls()[control].rect;
		return Common::Point((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2);
	}

	static Delta::OptionsResult click(Delta::OptionsScreen &screen, const Common::Point &point) {
		screen.mouseDown(point);
		return screen.mouseUp(point);
	}

public:
	void testSliderStepsFollowNativeRanges() {
		TS_ASSERT_EQUALS(Delta::sliderStep(0, 255), 16);
		TS_ASSERT_EQUALS(Delta::sliderStep(0, 100), 10);
		TS_ASSERT_EQUALS(Delta::sliderStep(-30, 0), 3);
		TS_ASSERT_EQUALS(Delta::stepSliderValue(128, 0, 255, 16, true), 144);
		TS_ASSERT_EQUALS(Delta::stepSliderValue(5, 0, 100, 10, true), 20);
		TS_ASSERT_EQUALS(Delta::stepSliderValue(5, 0, 100, 10, false), 0);
		TS_ASSERT_EQUALS(Delta::stepSliderValue(250, 0, 255, 16, true), 255);
		TS_ASSERT_EQUALS(Delta::stepSliderValue(-29, -30, 0, 3, false), -30);
	}

	void testDisplayPageReflectsSettings() {
		Delta::OptionsSettings settings;
		settings.fullscreen = 1;
		settings.effect = 0;
		settings.windowTransparency = 255;
		Delta::OptionsScreen screen;
		screen.open(settings, Delta::kOptionsDisplay, false, 0);
		TS_ASSERT_EQUALS(screen.controls().size(), 31u);
		TS_ASSERT(screen.controls()[0].selected);
		TS_ASSERT(!screen.controls()[1].selected);
		TS_ASSERT(screen.controls()[18].selected);
		TS_ASSERT_EQUALS(screen.controls()[23].value, 255);
		TS_ASSERT_EQUALS(screen.sliderThumb(23), 488 + 272 - 16);
	}

	void testRadioClickChangesWorkingCopyAndSelectedRadioIsInert() {
		Delta::OptionsSettings settings;
		Delta::OptionsScreen screen;
		screen.open(settings, Delta::kOptionsDisplay, false, 0);
		TS_ASSERT_EQUALS(click(screen, centre(screen, 8)), Delta::kOptionsRedraw);
		TS_ASSERT_EQUALS(screen.working().textSpeed, 0);
		TS_ASSERT_EQUALS(click(screen, centre(screen, 8)), Delta::kOptionsIdle);
		TS_ASSERT_EQUALS(click(screen, centre(screen, 16)), Delta::kOptionsIdle);
		TS_ASSERT_EQUALS(click(screen, centre(screen, 17)), Delta::kOptionsRedraw);
		TS_ASSERT_EQUALS(screen.working().effect, 2);
		TS_ASSERT_EQUALS(click(screen, centre(screen, 18)), Delta::kOptionsRedraw);
		TS_ASSERT_EQUALS(screen.working().effect, 0);
	}

	void testColourSlidersEditTheSelectedColour() {
		Delta::OptionsSettings settings;
		Delta::OptionsScreen screen;
		screen.open(settings, Delta::kOptionsDisplay, false, 0);
		click(screen, centre(screen, 3));
		TS_ASSERT_EQUALS(screen.colorTarget(), 1);
		TS_ASSERT_EQUALS(screen.controls()[7].value, 0xc0);
		// Clicking left of the thumb steps the blue channel down by 16.
		click(screen, Common::Point(30, 255));
		TS_ASSERT_EQUALS(screen.working().colors[1], 0xffffb0u);
		TS_ASSERT_EQUALS(screen.working().colors[0], 0xffffffu);
	}

	void testThumbDragSetsValue() {
		Delta::OptionsSettings settings;
		settings.windowTransparency = 0;
		Delta::OptionsScreen screen;
		screen.open(settings, Delta::kOptionsDisplay, false, 0);
		screen.mouseDown(Common::Point(490, 360));
		TS_ASSERT_EQUALS(screen.mouseMove(Common::Point(488 + 128, 360)), Delta::kOptionsRedraw);
		TS_ASSERT_EQUALS(screen.mouseUp(Common::Point(488 + 128, 360)), Delta::kOptionsRedraw);
		TS_ASSERT_EQUALS(screen.working().windowTransparency, 255 * 128 / 256);
	}

	void testTabsCommitAndButtonsClose() {
		Delta::OptionsSettings settings;
		Delta::OptionsScreen screen;
		screen.open(settings, Delta::kOptionsDisplay, false, 0);
		TS_ASSERT_EQUALS(click(screen, centre(screen, 27)), Delta::kOptionsCommit);
		TS_ASSERT_EQUALS(screen.page(), Delta::kOptionsSound);
		TS_ASSERT_EQUALS(click(screen, centre(screen, 42)), Delta::kOptionsCommit);
		TS_ASSERT_EQUALS(screen.page(), Delta::kOptionsControls);
		TS_ASSERT_EQUALS(click(screen, centre(screen, 27)), Delta::kOptionsClose);
		TS_ASSERT_EQUALS(click(screen, centre(screen, 28)), Delta::kOptionsCommitClose);
	}

	void testWheelAndRightClickKeepAMenuBinding() {
		Delta::OptionsSettings settings;
		Delta::OptionsScreen screen;
		screen.open(settings, Delta::kOptionsControls, false, 0);
		TS_ASSERT_EQUALS(screen.working().rightClick, 5);
		click(screen, centre(screen, 18));
		TS_ASSERT_EQUALS(screen.working().rightClick, 0);
		TS_ASSERT_EQUALS(screen.working().wheelButton, 5);
		TS_ASSERT(screen.controls()[17].selected);
	}

	void testDefaultsResetOnlyTheCurrentPage() {
		Delta::OptionsSettings settings;
		settings.windowTransparency = 7;
		settings.rightClick = 1;
		settings.wheelButton = 5;
		Delta::OptionsScreen screen;
		screen.open(settings, Delta::kOptionsDisplay, false, 0);
		click(screen, centre(screen, 28));
		TS_ASSERT_EQUALS(screen.working().windowTransparency, 128);
		TS_ASSERT_EQUALS(screen.working().rightClick, 1);
	}

	void testPackageLayoutAndCharacterRows() {
		Delta::OptionsSettings settings;
		Delta::OptionsScreen screen;
		screen.open(settings, Delta::kOptionsDisplay, true, 4);
		TS_ASSERT_EQUALS(screen.controls()[24].rect, Common::Rect(648, 500, 688, 520));
		click(screen, centre(screen, 27));
		TS_ASSERT_EQUALS(screen.controls()[18].type, Delta::kOptionsSlider);
		TS_ASSERT_EQUALS(screen.controls()[19].type, Delta::kOptionsDisabled);
		TS_ASSERT_EQUALS(screen.controls()[29].type, Delta::kOptionsDisabled);
		screen.open(settings, Delta::kOptionsSound, false, 0);
		TS_ASSERT_EQUALS(screen.controlAt(centre(screen, 21)), -1);
	}

	void testKeyboardFocusAdjustsSliders() {
		Delta::OptionsSettings settings;
		Delta::OptionsScreen screen;
		screen.open(settings, Delta::kOptionsDisplay, false, 0);
		TS_ASSERT_EQUALS(screen.moveFocus(0, 1), Delta::kOptionsRedraw);
		TS_ASSERT_EQUALS(screen.hover(), 0);
		TS_ASSERT_EQUALS(screen.moveFocus(0, 1), Delta::kOptionsRedraw);
		TS_ASSERT_EQUALS(screen.hover(), 5);
		TS_ASSERT_EQUALS(screen.adjustFocus(1), Delta::kOptionsIdle);
		TS_ASSERT_EQUALS(screen.adjustFocus(-1), Delta::kOptionsRedraw);
		TS_ASSERT_EQUALS(screen.working().colors[0], 0xf0ffffu);
		TS_ASSERT_EQUALS(screen.activateFocus(), Delta::kOptionsIdle);
		TS_ASSERT_EQUALS(screen.cancel(), Delta::kOptionsClose);
	}
};
