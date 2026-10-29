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

#ifndef DELTA_INPUT_H
#define DELTA_INPUT_H

#include "common/events.h"

namespace Delta {

enum DeltaAction {
	kActionSelect = 1,
	kActionCancel,
	kActionUp,
	kActionDown,
	kActionLeft,
	kActionRight,
	kActionMenu,
	kActionSkip
};

class InputState {
public:
	bool mouseActive() const { return _mouseActive; }
	void setMousePosition(const Common::Point &position) { _mousePosition = position; }

	bool processEvent(const Common::Event &event) {
		const bool previous = _mouseActive;
		switch (event.type) {
		case Common::EVENT_MOUSEMOVE:
			if (event.mouse != _mousePosition || event.relMouse != Common::Point())
				_mouseActive = true;
			_mousePosition = event.mouse;
			break;
		case Common::EVENT_LBUTTONDOWN:
		case Common::EVENT_LBUTTONUP:
		case Common::EVENT_RBUTTONDOWN:
		case Common::EVENT_RBUTTONUP:
		case Common::EVENT_MBUTTONDOWN:
		case Common::EVENT_MBUTTONUP:
		case Common::EVENT_WHEELUP:
		case Common::EVENT_WHEELDOWN:
			_mouseActive = true;
			_mousePosition = event.mouse;
			break;
		case Common::EVENT_KEYDOWN:
		case Common::EVENT_KEYUP:
		case Common::EVENT_CUSTOM_ENGINE_ACTION_START:
		case Common::EVENT_CUSTOM_ENGINE_ACTION_END:
		case Common::EVENT_JOYBUTTON_DOWN:
		case Common::EVENT_JOYBUTTON_UP:
			_mouseActive = false;
			break;
		case Common::EVENT_JOYAXIS_MOTION:
			if (event.joystick.position < -8000 || event.joystick.position > 8000)
				_mouseActive = false;
			break;
		default:
			break;
		}
		return previous != _mouseActive;
	}

private:
	bool _mouseActive = true;
	Common::Point _mousePosition;
};

} // End of namespace Delta

#endif
