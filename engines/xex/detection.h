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

#ifndef ENGINES_XEX_DETECTION_H
#define ENGINES_XEX_DETECTION_H

#include "engines/advancedDetector.h"

namespace Xex {

struct XexMenuStyle {
	uint16 centerX;
	uint16 leftPerCharacter;
	uint16 widthPerCharacter;
	uint16 widthPadding;
	uint16 topBase;
	uint16 topPerRow;
	uint16 rowHeight;
	uint16 heightPadding;
	uint16 charAdvance;
	uint16 cornerRadius;
	int16 offsetX;
	int16 offsetY;
	uint32 fillColor;
	uint32 borderColor;
	uint32 idleTextColor;
	uint32 hoverTextColor;
	uint32 textShadowColor;
	uint32 hoverOutlineColor;
	bool showQuickSaveLoad;
	bool outlinedMenu;
	bool plainMenu;
	uint16 menuFontSize;
	uint16 menuFontWidth;
	bool menuFontBold;
	bool outlineSelectedText;
};

enum XexMessageLayout {
	kMessageDialogue,
	kMessageFullScreen,
	kMessageFramedDialogue
};

enum XexEngineFamily {
	kXexEngineXex,
	kXexEngineC4
};

enum XexMapLayout {
	kXexMapNone,
	kXexMapCase266,
	kXexMapMht
};

struct XexGallery {
	const char *const *names;
	uint16 count;
	const uint16 *groupEnds;
	uint16 groupCount;
};

struct XexGameDescription {
	ADGameDescription gameDescription;
	uint16 screenWidth;
	uint16 screenHeight;
	XexEngineFamily engineFamily;
	bool showMessageButtons;
	XexMenuStyle menuStyle;
	XexMessageLayout messageLayout;
	uint16 waitUnitMillis;
	bool midiOnly;
	XexGallery gallery;
	XexMapLayout mapLayout;

	AD_GAME_DESCRIPTION_HELPERS(gameDescription)
};

enum XexDebugChannels {
	kDebugScript = 1,
	kDebugGraphics,
	kDebugAudio
};

#define GAMEOPTION_TEXT_ANIMATION GUIO_GAMEOPTIONS2
#define GAMEOPTION_MIDI_MUSIC     GUIO_GAMEOPTIONS3
#define GAMEOPTION_NAME_FRAME     GUIO_GAMEOPTIONS4

} // End of namespace Xex

#endif // ENGINES_XEX_DETECTION_H
