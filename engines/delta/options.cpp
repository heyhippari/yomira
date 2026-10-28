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

#include "common/util.h"

#include "delta/options.h"

namespace Delta {

namespace {

struct ControlDefinition {
	int16 x, y, w, h;
	byte type;
	byte group;
};

// One Lover's tables at 0x465848/0x465ab8, 0x465e68/0x466200 and 0x4663e0/0x466628.
const ControlDefinition kDisplayControls[] = {
	{ 96, 116, 128, 20, 2, 0 }, { 240, 116, 128, 20, 2, 0 },
	{ 68, 180, 60, 24, 2, 1 }, { 164, 180, 60, 24, 2, 1 }, { 260, 180, 108, 24, 2, 1 },
	{ 24, 211, 256, 16, 4, 2 }, { 24, 231, 256, 16, 4, 3 }, { 24, 251, 256, 16, 4, 4 },
	{ 200, 308, 40, 20, 2, 5 }, { 264, 308, 40, 20, 2, 5 }, { 328, 308, 40, 20, 2, 5 },
	{ 200, 404, 40, 20, 2, 6 }, { 264, 404, 40, 20, 2, 6 }, { 328, 404, 40, 20, 2, 6 },
	{ 264, 500, 40, 20, 2, 7 }, { 328, 500, 40, 20, 2, 7 },
	{ 584, 116, 40, 20, 2, 8 }, { 648, 116, 40, 20, 2, 8 }, { 712, 116, 40, 20, 2, 8 },
	{ 712, 180, 40, 20, 2, 9 }, { 648, 180, 40, 20, 2, 9 },
	{ 712, 228, 40, 20, 2, 10 }, { 648, 228, 40, 20, 2, 10 },
	{ 488, 352, 272, 16, 4, 11 },
	{ 584, 500, 80, 20, 2, 12 }, { 688, 500, 64, 20, 2, 12 },
	{ 520, 40, 120, 24, 2, 13 }, { 648, 40, 120, 24, 2, 13 },
	{ 416, 524, 84, 32, 1, 14 }, { 536, 524, 112, 32, 1, 15 }, { 656, 524, 112, 32, 1, 16 }
};

const ControlDefinition kSoundControls[] = {
	{ 112, 120, 272, 16, 4, 0 }, { 112, 184, 272, 16, 4, 1 }, { 112, 248, 272, 16, 4, 2 },
	{ 112, 312, 272, 16, 4, 3 }, { 112, 376, 272, 16, 4, 4 },
	{ 264, 156, 40, 20, 2, 5 }, { 304, 156, 40, 20, 2, 5 },
	{ 264, 220, 40, 20, 2, 6 }, { 304, 220, 40, 20, 2, 6 },
	{ 264, 284, 40, 20, 2, 7 }, { 304, 284, 40, 20, 2, 7 },
	{ 328, 436, 40, 20, 2, 8 }, { 264, 436, 40, 20, 2, 8 },
	{ 264, 500, 40, 20, 2, 9 }, { 328, 500, 40, 20, 2, 9 },
	{ 496, 120, 272, 16, 4, 10 }, { 496, 184, 272, 16, 4, 11 }, { 496, 248, 272, 16, 4, 12 },
	{ 496, 312, 272, 16, 4, 13 }, { 496, 376, 272, 16, 4, 14 }, { 496, 440, 272, 16, 4, 15 },
	{ 648, 92, 40, 20, 2, 16 }, { 688, 92, 40, 20, 2, 16 },
	{ 648, 156, 40, 20, 2, 17 }, { 688, 156, 40, 20, 2, 17 },
	{ 648, 220, 40, 20, 2, 18 }, { 688, 220, 40, 20, 2, 18 },
	{ 648, 284, 40, 20, 2, 19 }, { 688, 284, 40, 20, 2, 19 },
	{ 648, 348, 40, 20, 2, 20 }, { 688, 348, 40, 20, 2, 20 },
	{ 648, 412, 40, 20, 2, 21 }, { 688, 412, 40, 20, 2, 21 },
	{ 344, 156, 40, 20, 1, 22 }, { 344, 284, 40, 20, 1, 23 },
	{ 728, 92, 40, 20, 1, 24 }, { 728, 156, 40, 20, 1, 25 }, { 728, 220, 40, 20, 1, 26 },
	{ 728, 284, 40, 20, 1, 27 }, { 728, 348, 40, 20, 1, 28 }, { 728, 412, 40, 20, 1, 29 },
	{ 520, 40, 120, 24, 2, 30 }, { 648, 40, 120, 24, 2, 30 },
	{ 416, 524, 84, 32, 1, 31 }, { 536, 524, 112, 32, 1, 32 }, { 656, 524, 112, 32, 1, 33 }
};

const ControlDefinition kControlsControls[] = {
	{ 328, 116, 40, 20, 2, 0 }, { 264, 116, 40, 20, 2, 0 },
	{ 328, 180, 40, 20, 2, 1 }, { 264, 180, 40, 20, 2, 1 },
	{ 328, 244, 40, 20, 2, 2 }, { 264, 244, 40, 20, 2, 2 },
	{ 328, 308, 40, 20, 2, 3 }, { 264, 308, 40, 20, 2, 3 },
	{ 264, 372, 56, 20, 2, 4 }, { 328, 372, 56, 20, 2, 4 },
	{ 712, 116, 40, 20, 2, 5 }, { 648, 116, 40, 20, 2, 5 },
	{ 588, 180, 80, 20, 2, 6 }, { 680, 180, 80, 20, 2, 6 }, { 588, 204, 80, 20, 2, 6 },
	{ 680, 204, 80, 20, 2, 6 }, { 588, 228, 80, 20, 2, 6 }, { 680, 228, 80, 20, 2, 6 },
	{ 588, 308, 80, 20, 2, 7 }, { 680, 308, 80, 20, 2, 7 }, { 588, 332, 80, 20, 2, 7 },
	{ 680, 332, 80, 20, 2, 7 }, { 588, 356, 80, 20, 2, 7 }, { 680, 356, 80, 20, 2, 7 },
	{ 520, 40, 120, 24, 2, 8 }, { 648, 40, 120, 24, 2, 8 },
	{ 416, 524, 84, 32, 1, 9 }, { 536, 524, 112, 32, 1, 10 }, { 656, 524, 112, 32, 1, 11 }
};

// Display group 8 lists Normal, Fast and Off; EFFECT stores them as 1, 2 and 0.
const int kEffectValues[] = { 1, 2, 0 };

const int kThumbWidth = 16;

} // End of anonymous namespace

void OptionsSettings::resetPage(OptionsPage page) {
	const OptionsSettings defaults;
	switch (page) {
	case kOptionsDisplay:
		fullscreen = defaults.fullscreen;
		for (uint index = 0; index < ARRAYSIZE(colors); ++index)
			colors[index] = defaults.colors[index];
		textSpeed = defaults.textSpeed;
		autoSpeed = defaults.autoSpeed;
		noMovie = defaults.noMovie;
		effect = defaults.effect;
		fontSmoothing = defaults.fontSmoothing;
		fontShadow = defaults.fontShadow;
		windowTransparency = defaults.windowTransparency;
		wheelMode = defaults.wheelMode;
		break;
	case kOptionsSound:
		// The original reset leaves the DirectSound choice unchanged.
		volumeAll = defaults.volumeAll;
		volumeMusic = defaults.volumeMusic;
		volumeEffects = defaults.volumeEffects;
		volumeVoice = defaults.volumeVoice;
		volumeDuck = defaults.volumeDuck;
		musicOff = defaults.musicOff;
		effectsOff = defaults.effectsOff;
		voiceOff = defaults.voiceOff;
		voiceCut = defaults.voiceCut;
		for (uint index = 0; index < kOptionsCharacterVoices; ++index) {
			characterVoiceOff[index] = 0;
			characterVolume[index] = 0;
		}
		break;
	case kOptionsControls:
		unreadSkip = defaults.unreadSkip;
		readSkip = defaults.readSkip;
		skipStop = defaults.skipStop;
		autoStop = defaults.autoStop;
		skipKey = defaults.skipKey;
		autoCursor = defaults.autoCursor;
		wheelButton = defaults.wheelButton;
		rightClick = defaults.rightClick;
		break;
	default:
		break;
	}
}

int sliderStep(int minimum, int maximum) {
	int range = maximum - minimum;
	if (range == 255)
		range = 256;
	if (range % 16 == 0)
		return MAX(1, range / 16);
	if (range % 8 == 0)
		return range / 8;
	return MAX(1, range / 10);
}

int stepSliderValue(int value, int minimum, int maximum, int step, bool increase) {
	if (increase)
		value = value % step == 0 ? value + step : ((step / 2 + value) / step + 1) * step;
	else
		value = value % step == 0 ? value - step : ((value - step / 2) / step) * step;
	return CLIP(value, minimum, maximum);
}

void OptionsScreen::open(const OptionsSettings &settings, OptionsPage page, bool packageLayout, uint characterVoices) {
	_working = settings;
	_packageLayout = packageLayout;
	_characterVoices = MIN(characterVoices, kOptionsCharacterVoices);
	_page = page;
	_colorTarget = 0;
	build();
	syncControls();
}

void OptionsScreen::build() {
	const ControlDefinition *definitions = kDisplayControls;
	uint count = ARRAYSIZE(kDisplayControls);
	if (_page == kOptionsSound) {
		definitions = kSoundControls;
		count = ARRAYSIZE(kSoundControls);
	} else if (_page == kOptionsControls) {
		definitions = kControlsControls;
		count = ARRAYSIZE(kControlsControls);
	}
	_controls.clear();
	_groupFirst.clear();
	_hover = -1;
	_drag = -1;
	for (uint index = 0; index < count; ++index) {
		const ControlDefinition &definition = definitions[index];
		OptionsControl control;
		control.rect = Common::Rect(definition.x, definition.y, definition.x + definition.w, definition.y + definition.h);
		control.type = (OptionsControlType)definition.type;
		control.group = definition.group;
		if (_groupFirst.size() <= control.group)
			_groupFirst.push_back(index);
		_controls.push_back(control);
	}
	if (_page == kOptionsDisplay && _packageLayout) {
		// Onegai Tasukete's DirectShow on/off buttons are 40-pixel buttons, matched against its artwork.
		_controls[24].rect = Common::Rect(648, 500, 688, 520);
		_controls[25].rect = Common::Rect(712, 500, 752, 520);
	}
	if (_page == kOptionsDisplay) {
		for (uint control = 5; control <= 7; ++control)
			setSlider(control, 0, 255, 0);
		setSlider(23, 0, 255, 0);
	} else if (_page == kOptionsSound) {
		setSlider(0, 0, 100, 0);
		setSlider(1, -30, 0, 0);
		setSlider(2, -30, 0, 0);
		setSlider(3, 0, 100, 0);
		setSlider(4, 0, 100, 0);
		for (uint row = 0; row < kOptionsCharacterVoices; ++row) {
			if (row < _characterVoices) {
				setSlider(15 + row, -30, 0, 0);
			} else {
				_controls[15 + row].type = kOptionsDisabled;
				_controls[21 + row * 2].type = kOptionsDisabled;
				_controls[22 + row * 2].type = kOptionsDisabled;
				_controls[35 + row].type = kOptionsDisabled;
			}
		}
	}
}

void OptionsScreen::setSlider(uint control, int minimum, int maximum, int value) {
	OptionsControl &slider = _controls[control];
	if (slider.minimum != minimum || slider.maximum != maximum || !slider.step)
		slider.step = sliderStep(minimum, maximum);
	slider.minimum = minimum;
	slider.maximum = maximum;
	slider.value = CLIP(value, minimum, maximum);
}

void OptionsScreen::selectRadio(uint control) {
	const uint group = _controls[control].group;
	for (uint index = _groupFirst[group]; index < _controls.size() && _controls[index].group == group; ++index)
		_controls[index].selected = index == control;
}

void OptionsScreen::setSelection(uint group, int index) {
	const uint first = _groupFirst[group];
	for (uint control = first; control < _controls.size() && _controls[control].group == group; ++control)
		_controls[control].selected = (int)(control - first) == index;
}

int OptionsScreen::selection(uint group) const {
	for (uint control = _groupFirst[group]; control < _controls.size() && _controls[control].group == group; ++control) {
		if (_controls[control].selected)
			return control - _groupFirst[group];
	}
	return -1;
}

void OptionsScreen::loadColorSliders() {
	const uint32 color = _working.colors[_colorTarget];
	setSlider(5, 0, 255, (color >> 16) & 0xff);
	setSlider(6, 0, 255, (color >> 8) & 0xff);
	setSlider(7, 0, 255, color & 0xff);
}

void OptionsScreen::syncControls() {
	const OptionsSettings &s = _working;
	switch (_page) {
	case kOptionsDisplay: {
		setSelection(0, s.fullscreen ? 0 : 1);
		setSelection(1, _colorTarget);
		loadColorSliders();
		setSelection(5, s.textSpeed);
		setSelection(6, s.autoSpeed);
		setSelection(7, s.noMovie);
		int effect = -1;
		for (uint index = 0; index < ARRAYSIZE(kEffectValues); ++index) {
			if (kEffectValues[index] == s.effect)
				effect = index;
		}
		setSelection(8, effect);
		setSelection(9, s.fontSmoothing);
		setSelection(10, s.fontShadow);
		setSlider(23, 0, 255, s.windowTransparency);
		setSelection(12, s.wheelMode);
		break;
	}
	case kOptionsSound:
		setSlider(0, 0, 100, s.volumeAll);
		setSlider(1, -30, 0, s.volumeMusic);
		setSlider(2, -30, 0, s.volumeEffects);
		setSlider(3, 0, 100, s.volumeVoice);
		setSlider(4, 0, 100, s.volumeDuck);
		setSelection(5, s.musicOff);
		setSelection(6, s.effectsOff);
		setSelection(7, s.voiceOff);
		setSelection(8, s.voiceCut);
		setSelection(9, s.noDirectSound);
		for (uint row = 0; row < _characterVoices; ++row) {
			setSlider(15 + row, -30, 0, s.characterVolume[row]);
			setSelection(16 + row, s.characterVoiceOff[row]);
		}
		break;
	case kOptionsControls:
		setSelection(0, s.unreadSkip);
		setSelection(1, s.readSkip);
		setSelection(2, s.skipStop);
		setSelection(3, s.autoStop);
		setSelection(4, s.skipKey == 17 ? 0 : 1);
		setSelection(5, s.autoCursor);
		setSelection(6, s.wheelButton);
		setSelection(7, s.rightClick);
		break;
	default:
		break;
	}
}

int OptionsScreen::controlAt(const Common::Point &point) const {
	for (uint index = 0; index < _controls.size(); ++index) {
		if (_controls[index].type != kOptionsDisabled && _controls[index].rect.contains(point))
			return index;
	}
	return -1;
}

int OptionsScreen::sliderThumb(uint control) const {
	const OptionsControl &slider = _controls[control];
	if (slider.maximum <= slider.minimum)
		return slider.rect.left;
	return slider.rect.left + (slider.value - slider.minimum) * (slider.rect.width() - kThumbWidth) / (slider.maximum - slider.minimum);
}

int OptionsScreen::stepSlider(uint control, int x) {
	OptionsControl &slider = _controls[control];
	const int thumb = sliderThumb(control);
	if (x >= slider.rect.left && x < thumb)
		slider.value = stepSliderValue(slider.value, slider.minimum, slider.maximum, slider.step, false);
	else if (x >= thumb + kThumbWidth && x < slider.rect.right)
		slider.value = stepSliderValue(slider.value, slider.minimum, slider.maximum, slider.step, true);
	return slider.value;
}

OptionsResult OptionsScreen::mouseMove(const Common::Point &point) {
	if (_drag >= 0) {
		OptionsControl &slider = _controls[_drag];
		const int previous = slider.value;
		if (point.x <= slider.rect.left)
			slider.value = slider.minimum;
		else if (point.x >= slider.rect.right - kThumbWidth)
			slider.value = slider.maximum;
		else
			slider.value = MIN(slider.maximum, slider.minimum + (slider.maximum - slider.minimum) *
				(point.x - slider.rect.left) / (slider.rect.width() - kThumbWidth));
		return slider.value == previous ? kOptionsIdle : kOptionsRedraw;
	}
	int hover = controlAt(point);
	// Only push buttons and unselected radio buttons have a hover image.
	if (hover >= 0 && !(_controls[hover].type == kOptionsButton ||
		(_controls[hover].type == kOptionsRadio && !_controls[hover].selected)))
		hover = -1;
	if (hover == _hover)
		return kOptionsIdle;
	_hover = hover;
	return kOptionsRedraw;
}

OptionsResult OptionsScreen::mouseDown(const Common::Point &point) {
	const int control = controlAt(point);
	if (control < 0 || _controls[control].type != kOptionsSlider)
		return kOptionsIdle;
	const int thumb = sliderThumb(control);
	if (point.x >= thumb && point.x < thumb + kThumbWidth)
		_drag = control;
	return kOptionsIdle;
}

OptionsResult OptionsScreen::mouseUp(const Common::Point &point) {
	if (_drag >= 0) {
		const uint control = _drag;
		_drag = -1;
		return dispatch(_controls[control].group, control - _groupFirst[_controls[control].group]);
	}
	const int control = controlAt(point);
	if (control < 0)
		return kOptionsIdle;
	return activate(control, point.x);
}

OptionsResult OptionsScreen::activate(uint control, int x) {
	OptionsControl &target = _controls[control];
	const uint group = target.group;
	switch (target.type) {
	case kOptionsButton:
		break;
	case kOptionsRadio:
		if (target.selected)
			return kOptionsIdle;
		selectRadio(control);
		break;
	case kOptionsSlider:
		stepSlider(control, x);
		break;
	default:
		return kOptionsIdle;
	}
	return dispatch(group, control - _groupFirst[group]);
}

OptionsResult OptionsScreen::switchPage(OptionsPage page) {
	_page = page;
	_colorTarget = 0;
	build();
	syncControls();
	return kOptionsCommit;
}

OptionsResult OptionsScreen::dispatch(uint group, int index) {
	OptionsSettings &s = _working;
	if (_page == kOptionsDisplay) {
		switch (group) {
		case 0: s.fullscreen = index == 0; break;
		case 1:
			_colorTarget = index;
			loadColorSliders();
			break;
		case 2:
		case 3:
		case 4: {
			const uint shift = (4 - group) * 8;
			s.colors[_colorTarget] = (s.colors[_colorTarget] & ~(0xffu << shift)) | ((uint32)_controls[5 + group - 2].value << shift);
			break;
		}
		case 5: s.textSpeed = index; break;
		case 6: s.autoSpeed = index; break;
		case 7: s.noMovie = index; break;
		case 8: s.effect = kEffectValues[index]; break;
		case 9: s.fontSmoothing = index; break;
		case 10: s.fontShadow = index; break;
		case 11: s.windowTransparency = _controls[23].value; break;
		case 12: s.wheelMode = index; break;
		case 13: return switchPage(index == 0 ? kOptionsControls : kOptionsSound);
		case 14:
			s.resetPage(kOptionsDisplay);
			_colorTarget = 0;
			syncControls();
			break;
		case 15: return kOptionsClose;
		case 16: return kOptionsCommitClose;
		default: break;
		}
	} else if (_page == kOptionsSound) {
		switch (group) {
		case 0: s.volumeAll = _controls[0].value; break;
		case 1: s.volumeMusic = _controls[1].value; break;
		case 2: s.volumeEffects = _controls[2].value; break;
		case 3: s.volumeVoice = _controls[3].value; break;
		case 4: s.volumeDuck = _controls[4].value; break;
		case 5: s.musicOff = index; break;
		case 6: s.effectsOff = index; break;
		case 7: s.voiceOff = index; break;
		case 8: s.voiceCut = index; break;
		case 9: s.noDirectSound = index; break;
		case 30: return switchPage(index == 0 ? kOptionsDisplay : kOptionsControls);
		case 31:
			s.resetPage(kOptionsSound);
			syncControls();
			break;
		case 32: return kOptionsClose;
		case 33: return kOptionsCommitClose;
		default:
			if (group >= 10 && group < 16)
				s.characterVolume[group - 10] = _controls[15 + group - 10].value;
			else if (group >= 16 && group < 22)
				s.characterVoiceOff[group - 16] = index;
			// Groups 22-29 are the sample-playback buttons.
			break;
		}
	} else {
		switch (group) {
		case 0: s.unreadSkip = index; break;
		case 1: s.readSkip = index; break;
		case 2: s.skipStop = index; break;
		case 3: s.autoStop = index; break;
		case 4: s.skipKey = index == 0 ? 17 : 16; break;
		case 5: s.autoCursor = index; break;
		case 6:
			// At least one of the wheel and right-click bindings must open the menu (index 5).
			s.wheelButton = index;
			if (index != 5 && s.rightClick != 5) {
				s.rightClick = 5;
				setSelection(7, 5);
			}
			break;
		case 7:
			s.rightClick = index;
			if (index != 5 && s.wheelButton != 5) {
				s.wheelButton = 5;
				setSelection(6, 5);
			}
			break;
		case 8: return switchPage(index == 0 ? kOptionsDisplay : kOptionsSound);
		case 9:
			s.resetPage(kOptionsControls);
			syncControls();
			break;
		case 10: return kOptionsClose;
		case 11: return kOptionsCommitClose;
		default: break;
		}
	}
	return kOptionsRedraw;
}

OptionsResult OptionsScreen::moveFocus(int dx, int dy) {
	if (_drag >= 0)
		return kOptionsIdle;
	if (_hover < 0) {
		for (uint index = 0; index < _controls.size() && _hover < 0; ++index) {
			if (_controls[index].type != kOptionsDisabled)
				_hover = index;
		}
		return kOptionsRedraw;
	}
	const Common::Point origin((_controls[_hover].rect.left + _controls[_hover].rect.right) / 2,
		(_controls[_hover].rect.top + _controls[_hover].rect.bottom) / 2);
	int best = -1, bestScore = 0;
	for (uint index = 0; index < _controls.size(); ++index) {
		if ((int)index == _hover || _controls[index].type == kOptionsDisabled)
			continue;
		const int x = (_controls[index].rect.left + _controls[index].rect.right) / 2 - origin.x;
		const int y = (_controls[index].rect.top + _controls[index].rect.bottom) / 2 - origin.y;
		const int primary = dx ? x * dx : y * dy;
		if (primary <= 0)
			continue;
		const int score = primary + 2 * ABS(dx ? y : x);
		if (best < 0 || score < bestScore) {
			best = index;
			bestScore = score;
		}
	}
	if (best < 0)
		return kOptionsIdle;
	_hover = best;
	return kOptionsRedraw;
}

OptionsResult OptionsScreen::adjustFocus(int direction) {
	if (_hover < 0 || _controls[_hover].type != kOptionsSlider)
		return moveFocus(direction, 0);
	OptionsControl &slider = _controls[_hover];
	const int previous = slider.value;
	slider.value = stepSliderValue(slider.value, slider.minimum, slider.maximum, slider.step, direction > 0);
	if (slider.value == previous)
		return kOptionsIdle;
	return dispatch(slider.group, _hover - _groupFirst[slider.group]);
}

OptionsResult OptionsScreen::activateFocus() {
	if (_hover < 0 || _controls[_hover].type == kOptionsSlider)
		return kOptionsIdle;
	return activate(_hover, 0);
}

OptionsResult OptionsScreen::cancel() {
	return kOptionsClose;
}

} // End of namespace Delta
