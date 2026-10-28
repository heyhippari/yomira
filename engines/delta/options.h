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

#ifndef DELTA_OPTIONS_H
#define DELTA_OPTIONS_H

#include "common/array.h"
#include "common/rect.h"

namespace Delta {

enum OptionsPage {
	kOptionsDisplay,
	kOptionsSound,
	kOptionsControls
};

enum OptionsControlType {
	kOptionsDisabled = 0,
	kOptionsButton = 1,
	kOptionsRadio = 2,
	kOptionsSlider = 4
};

enum OptionsResult {
	kOptionsIdle,
	kOptionsRedraw,
	kOptionsCommit,
	kOptionsClose,
	kOptionsCommitClose
};

static const uint kOptionsCharacterVoices = 6;

// Values use the original INI units; radio-backed fields hold the INI value, not the button index.
struct OptionsSettings {
	int fullscreen = 0;
	uint32 colors[3] = { 0xffffff, 0xffffc0, 0x00ffff };
	int textSpeed = 1;
	int autoSpeed = 1;
	int noMovie = 0;
	int effect = 1;
	int fontSmoothing = 1;
	int fontShadow = 1;
	int windowTransparency = 128;
	int wheelMode = 0;
	int volumeAll = 100;
	int volumeMusic = -9;
	int volumeEffects = -9;
	int volumeVoice = 81;
	int volumeDuck = 71;
	int musicOff = 0;
	int effectsOff = 0;
	int voiceOff = 0;
	int voiceCut = 1;
	int noDirectSound = 0;
	int characterVoiceOff[kOptionsCharacterVoices] = {};
	int characterVolume[kOptionsCharacterVoices] = {};
	int unreadSkip = 0;
	int readSkip = 0;
	int skipStop = 1;
	int autoStop = 0;
	int skipKey = 17;
	int autoCursor = 0;
	int wheelButton = 0;
	int rightClick = 5;

	void resetPage(OptionsPage page);
};

struct OptionsControl {
	Common::Rect rect;
	OptionsControlType type = kOptionsDisabled;
	uint group = 0;
	bool selected = false;
	int value = 0, minimum = 0, maximum = -1, step = 0;
};

class OptionsScreen {
public:
	void open(const OptionsSettings &settings, OptionsPage page, bool packageLayout, uint characterVoices);
	OptionsPage page() const { return _page; }
	const OptionsSettings &working() const { return _working; }
	const Common::Array<OptionsControl> &controls() const { return _controls; }
	int hover() const { return _hover; }
	int colorTarget() const { return _colorTarget; }

	int controlAt(const Common::Point &point) const;
	int sliderThumb(uint control) const;
	OptionsResult mouseMove(const Common::Point &point);
	OptionsResult mouseDown(const Common::Point &point);
	OptionsResult mouseUp(const Common::Point &point);
	OptionsResult moveFocus(int dx, int dy);
	OptionsResult adjustFocus(int direction);
	OptionsResult activateFocus();
	OptionsResult cancel();

private:
	OptionsSettings _working;
	OptionsPage _page = kOptionsDisplay;
	bool _packageLayout = false;
	uint _characterVoices = 0;
	Common::Array<OptionsControl> _controls;
	Common::Array<uint> _groupFirst;
	int _hover = -1;
	int _drag = -1;
	int _colorTarget = 0;

	void build();
	void selectRadio(uint control);
	void setSelection(uint group, int index);
	int selection(uint group) const;
	void setSlider(uint control, int minimum, int maximum, int value);
	void syncControls();
	void loadColorSliders();
	int stepSlider(uint control, int x);
	OptionsResult activate(uint control, int x);
	OptionsResult dispatch(uint group, int index);
	OptionsResult switchPage(OptionsPage page);
};

int stepSliderValue(int value, int minimum, int maximum, int step, bool increase);
int sliderStep(int minimum, int maximum);

} // End of namespace Delta

#endif
