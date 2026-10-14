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

static const CompletMenuStyle kPlainC4MenuStyle = {
	0x170, 7, 14, 0x30,
	0x11c, 16, 32, 6,
	32, 16, 0, 0,
	0x000030, 0x000030, 0x4040ff, 0xffffff, 0x3f3f7f, 0xffffff,
	true, false, true, 30, 0, true, false
};

static const CompletMenuStyle kMattyMenuStyle = {
	0x124, 7, 14, 42,
	240, 14, 28, 0,
	28, 16, 0, 0,
	0x000030, 0x000030, 0x4040ff, 0xffffff, 0x50507f, 0xffffff,
	true, false, true, 26, 32, false, true
};

static const CompletMenuStyle kKoiMenuStyle = {
	0x124, 7, 14, 42,
	240, 14, 28, 0,
	28, 16, 0, 0,
	0x000030, 0xffff00, 0xffffff, 0xf39b23, 0x606060, 0xffffff,
	false, true, false, 0, 0, false, false
};

static const CompletGameDescription gameDescriptions[] = {
	{
		{
			"mamatuma",
			nullptr,
			AD_ENTRY1s("mpx/startup.mpx", "9de255cdc728753d982abb61ad6cc7e2", 2390),
			Common::JA_JPN,
			Common::kPlatformWindows,
			ADGF_NO_FLAGS,
			GUIO3(COMPLET_GUIO, GUIO_MIDIGM, GAMEOPTION_MIDI_MUSIC)
		},
		800, 600, false, kPlainC4MenuStyle
	},
	{
		{
			"mamatuma",
			"Updated",
			AD_ENTRY1s("mpx/startup.mpx", "05b01d4bec391bfc0ee93465a4954cb5", 2453),
			Common::JA_JPN,
			Common::kPlatformWindows,
			ADGF_NO_FLAGS,
			GUIO3(COMPLET_GUIO, GUIO_MIDIGM, GAMEOPTION_MIDI_MUSIC)
		},
		800, 600, false, kPlainC4MenuStyle
	},
	{
		{
			"matty",
			nullptr,
			AD_ENTRY2s("mpx/startup.mpx", "c4b0060e5952f95c75c058835297fd4c", 3562,
					   "mpx/e-home.mpx", "7f61e282ce09ae8b6bfa65fde129db38", 7469),
			Common::JA_JPN,
			Common::kPlatformWindows,
			ADGF_NO_FLAGS,
			GUIO3(COMPLET_GUIO, GUIO_MIDIGM, GAMEOPTION_MIDI_MUSIC)
		},
		640, 480, true, kMattyMenuStyle
	},
	{
		{
			"matty",
			"Updated",
			AD_ENTRY2s("mpx/startup.mpx", "c4b0060e5952f95c75c058835297fd4c", 3562,
					   "mpx/e-home.mpx", "d0773d14d5c3b460b93a904856f3ff5d", 8377),
			Common::JA_JPN,
			Common::kPlatformWindows,
			ADGF_NO_FLAGS,
			GUIO3(COMPLET_GUIO, GUIO_MIDIGM, GAMEOPTION_MIDI_MUSIC)
		},
		640, 480, true, kMattyMenuStyle
	},
	{
		{
			"yagai",
			nullptr,
			AD_ENTRY1s("mpx/startup.mpx", "eae53720ffed59edbd447a6d16452cd4", 2691),
			Common::JA_JPN,
			Common::kPlatformWindows,
			ADGF_NO_FLAGS,
			GUIO3(COMPLET_GUIO, GUIO_MIDIGM, GAMEOPTION_MIDI_MUSIC)
		},
		800, 600, false, kPlainC4MenuStyle
	},
	{
		{
			"koisuruscience",
			nullptr,
			AD_ENTRY1s("mpx/startup.mpx", "d5aa4f549eb99b106efc3ce304252251", 1957),
			Common::JA_JPN,
			Common::kPlatformWindows,
			ADGF_NO_FLAGS,
			GUIO2(COMPLET_GUIO, GUIO_NOMIDI)
		},
		640, 480, true, kKoiMenuStyle
	},
	{
		AD_TABLE_END_MARKER,
		0, 0, false, {}
	}
};

} // End of namespace Complet

#endif // ENGINES_COMPLET_DETECTION_TABLES_H
