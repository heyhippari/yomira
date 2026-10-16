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
#include "engines/delta/input.h"

class DeltaInputTestSuite : public CxxTest::TestSuite {
public:
	void testPointerStartsInMouseMode() {
		Delta::InputState input;
		TS_ASSERT(input.mouseActive());
		Common::Event event;
		event.type = Common::EVENT_SCREEN_CHANGED;
		TS_ASSERT(!input.processEvent(event));
		TS_ASSERT(input.mouseActive());
	}

	void testKeyboardHidesUntilMouseMoves() {
		Delta::InputState input;
		input.setMousePosition(Common::Point(20, 20));
		Common::Event event;
		event.type = Common::EVENT_KEYDOWN;
		TS_ASSERT(input.processEvent(event));
		TS_ASSERT(!input.mouseActive());
		event.type = Common::EVENT_KEYUP;
		TS_ASSERT(!input.processEvent(event));
		event.type = Common::EVENT_MOUSEMOVE;
		event.mouse = Common::Point(20, 20);
		TS_ASSERT(!input.processEvent(event));
		TS_ASSERT(!input.mouseActive());
		event.mouse.x = 21;
		TS_ASSERT(input.processEvent(event));
		TS_ASSERT(input.mouseActive());
	}

	void testRelativeMovementRestoresPointer() {
		Delta::InputState input;
		Common::Event event;
		event.type = Common::EVENT_CUSTOM_ENGINE_ACTION_START;
		event.customType = Delta::kActionDown;
		TS_ASSERT(input.processEvent(event));
		event.type = Common::EVENT_MOUSEMOVE;
		event.relMouse = Common::Point(1, 0);
		TS_ASSERT(input.processEvent(event));
		TS_ASSERT(input.mouseActive());
	}

	void testMappedAndUnmappedControllerButtonsHide() {
		const Common::EventType events[] = {
			Common::EVENT_CUSTOM_ENGINE_ACTION_START,
			Common::EVENT_CUSTOM_ENGINE_ACTION_END,
			Common::EVENT_JOYBUTTON_DOWN,
			Common::EVENT_JOYBUTTON_UP
		};
		for (uint index = 0; index < ARRAYSIZE(events); ++index) {
			Delta::InputState input;
			Common::Event event;
			event.type = events[index];
			event.customType = Delta::kActionSelect;
			TS_ASSERT(input.processEvent(event));
			TS_ASSERT(!input.mouseActive());
			event.type = Common::EVENT_SCREEN_CHANGED;
			TS_ASSERT(!input.processEvent(event));
			TS_ASSERT(!input.mouseActive());
		}
	}

	void testControllerDeadzoneDoesNotHideMouse() {
		const int16 positions[] = { -8001, -8000, 0, 8000, 8001 };
		for (uint index = 0; index < ARRAYSIZE(positions); ++index) {
			Delta::InputState input;
			Common::Event event;
			event.type = Common::EVENT_JOYAXIS_MOTION;
			event.joystick.position = positions[index];
			const bool active = positions[index] < -8000 || positions[index] > 8000;
			TS_ASSERT_EQUALS(input.processEvent(event), active);
			TS_ASSERT_EQUALS(input.mouseActive(), !active);
		}
	}

	void testMouseButtonsAndWheelRestorePointer() {
		const Common::EventType events[] = {
			Common::EVENT_LBUTTONDOWN, Common::EVENT_LBUTTONUP,
			Common::EVENT_RBUTTONDOWN, Common::EVENT_RBUTTONUP,
			Common::EVENT_MBUTTONDOWN, Common::EVENT_MBUTTONUP,
			Common::EVENT_WHEELUP, Common::EVENT_WHEELDOWN
		};
		for (uint index = 0; index < ARRAYSIZE(events); ++index) {
			Delta::InputState input;
			Common::Event event;
			event.type = Common::EVENT_KEYDOWN;
			input.processEvent(event);
			event.type = events[index];
			TS_ASSERT(input.processEvent(event));
			TS_ASSERT(input.mouseActive());
		}
	}
};
