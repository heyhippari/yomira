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

#ifndef ENGINES_COMPLET_DETECTION_TABLES_H
#define ENGINES_COMPLET_DETECTION_TABLES_H

#include "engines/advancedDetector.h"

#include "complet/detection.h"

namespace Complet {

// Voices are digital, and the text is always shown
#define COMPLET_GUIO GUIO_NOSFX GUIO_NOSUBTITLES GUIO_NOASPECT GUIO_NOLANG GAMEOPTION_TEXT_ANIMATION

static const ADGameDescription gameDescriptions[] = {
	{
		"mamatuma",
		nullptr,
		AD_ENTRY1s("mpx/startup.mpx", "9de255cdc728753d982abb61ad6cc7e2", 2390),
		Common::JA_JPN,
		Common::kPlatformWindows,
		ADGF_NO_FLAGS,
		GUIO3(COMPLET_GUIO, GUIO_MIDIGM, GAMEOPTION_MIDI_MUSIC)
	},
	{
		"matty",
		nullptr,
		AD_ENTRY1s("mpx/startup.mpx", "c4b0060e5952f95c75c058835297fd4c", 3562),
		Common::JA_JPN,
		Common::kPlatformWindows,
		ADGF_NO_FLAGS,
		GUIO3(COMPLET_GUIO, GUIO_MIDIGM, GAMEOPTION_MIDI_MUSIC)
	},
	{
		"yagai",
		nullptr,
		AD_ENTRY1s("mpx/startup.mpx", "eae53720ffed59edbd447a6d16452cd4", 2691),
		Common::JA_JPN,
		Common::kPlatformWindows,
		ADGF_NO_FLAGS,
		GUIO3(COMPLET_GUIO, GUIO_MIDIGM, GAMEOPTION_MIDI_MUSIC)
	},
	{
		"koisuruscience",
		nullptr,
		AD_ENTRY1s("mpx/startup.mpx", "d5aa4f549eb99b106efc3ce304252251", 1957),
		Common::JA_JPN,
		Common::kPlatformWindows,
		ADGF_NO_FLAGS,
		GUIO2(COMPLET_GUIO, GUIO_NOMIDI)
	},
	AD_TABLE_END_MARKER
};

} // End of namespace Complet

#endif // ENGINES_COMPLET_DETECTION_TABLES_H
