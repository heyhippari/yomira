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

static const CompletMenuStyle kKexMenuStyle = {
	0x124, 7, 14, 42,
	240, 14, 28, 0,
	28, 16, 0, 0,
	0x000030, 0xffff00, 0xffffff, 0xf39b23, 0x606060, 0xffffff,
	false, true, false, 0, 0, false, false
};

static const char *const kC4CgNames[] = {
	"CG01", "CG02", "CG02R", "CG03", "CG03R", "CG04", "CG05", "CG05R", "CG06", "CG07",
	"CG08", "CG09", "CG09R", "CG10", "CG11", "CG12", "CG13", "CG14", "CG15", "CG16",
	"CG17", "CG18", "CG19", "CG20", "CG21", "CG22", "CG23", "CG24", "CG25", "CG26",
	"CG27", "CG28", "CG29", "CG30", "CG31", "CG32", "CG33", "CG34", "CG35", "CG36",
	"CG37", "CG37R", "CG38", "CG39", "CG40", "CG41", "CG41R", "CG42", "CG42R", "CG43",
	"CG43R", "CG44", "CG44R", "CG45", "CG46", "CG47", "CG47R", "CG48", "CG49", "CG50",
	"CG51", "CG51n", "CG51R", "CG52", "CG53", "CG54", "CG54R", "CG55", "CG56", "CG57",
	"CG58", "CG59", "CG60", "CG60R", "CG61", "CG61R", "CG62", "CG63", "CG64", "CG65",
	"CG66", "CG67", "CG68", "CG69", "CG70", "CG71", "CG72", "CG72R", "CG73", "CG74",
	"CG75", "CG76", "CG76R", "CG77", "CG78", "CG79", "CG80", "CG81", "CG82", "CG83",
	"CG83R", "CG84", "CG85", "CG86", "CG87", "CG88", "CG89", "CG90", "CG91", "CG92",
	"CG93", "CG94", "CG94R", "CG95", "CG96", "CG97", "CG98", "CG99", "CG100", "CGSP1",
	"CGSP2", "CGSP3", "CGSP4", "CGSP5"
};

static const char *const kCollectionCgNames[] = {
	"YO01", "YO02", "YO03", "YO04", "YO05", "YO06", "YO07", "YO08", "YO09", "YO10",
	"YO10R", "YO11", "YO12", "YO13", "YO14", "YO14A", "YO14B", "YO15", "YO16", "YO17",
	"YO18", "YO19", "YO20", "YO21", "YO22", "YA01", "YA02", "YA03", "YA04", "YA05",
	"YA06", "YA07", "YA08", "YA09", "YA10", "YA11", "YA11R", "YA12", "YA13", "YA14",
	"YA15", "YA16", "YA17", "YA18", "YA19", "YA20", "YA21", "YA22", "NA01", "NA02",
	"NA03", "NA04", "NA05", "NA06", "NA07", "NA08", "NA09", "NA10", "NA11", "NA11R",
	"NA12", "NA13", "NA13R", "NA14", "NA15", "NA16", "NA17", "NA18", "NA19", "NA20",
	"YU01", "YU02", "YU03", "YU03R", "YU04", "YU05", "YU06", "YU07", "YU08", "YU08R",
	"YU09", "YU10", "YU11", "YU12", "YU12A", "YU13", "YU13A", "YU14", "YU14A", "YU15",
	"YU15R", "YU16", "YU17", "YU17R", "YU18", "YU19", "YU20", "YU21", "YU22", "YK01",
	"YK02", "YK03", "YK04", "YK05", "YK06", "YK07", "YK08", "YK09", "YK10", "YK11",
	"YK12", "YK13", "YK14", "YK15", "YK16", "YK17", "YK18", "YK19", "YK20", "YK21",
	"YK22", "YK23", "YK24", "YK25", "YK26", "YK27", "KR01", "ED01", "KR02", "KR03",
	"KR03R", "KR04", "KR04R", "KR05", "KR05A", "KR05B", "KR06", "KR07", "KR08", "KR09",
	"KR11", "KR11R", "OP04", "KR10", "ED02", "ED03", "ED04", "ED05", "ED06", "ED07",
	"ED08", "ED09", "OP01", "OP02", "OP03"
};

static const uint16 kCollectionCgGroupEnds[] = {24, 47, 69, 98, 125, 143, 154};
static const char *const kCase266CgNames[] = {
	"CG01", "CG02", "CG03", "CG04", "CG05", "CG06", "CG07", "CG07N", "CG08", "CG09",
	"CG10", "CG11", "CG12", "CG13", "CG14", "CG15", "CG16", "CG17", "CG18", "CG19",
	"CG20", "CG20R", "CG21", "CG22", "CG23", "CG24", "CG25", "CG26", "CG27", "CG28",
	"CG29", "CG30", "CG31", "CG32", "CG33", "CG34", "CG35", "CG36", "CG37", "CG38",
	"CG39", "CG40", "CG41", "CG42", "CG43", "CG44", "CG45", "CG45R", "CG46", "CG47",
	"CG48", "CG49", "CG50", "CG51", "CG52", "CG53", "CG54", "CG55", "CG56", "CG57",
	"CG58", "CG59", "CG60", "CG61", "CG62", "CG63", "CG64", "CG65", "CG66", "CG67",
	"CG68", "CG100", "CG101", "CG102", "CG103", "CG104", "CG105"
};

static const uint16 kCase266CgGroupEnds[] = {76};
static const CompletGallery kCase266Gallery = {
	kCase266CgNames, ARRAYSIZE(kCase266CgNames), kCase266CgGroupEnds, ARRAYSIZE(kCase266CgGroupEnds)
};
static const CompletGallery kC4Gallery = {kC4CgNames, ARRAYSIZE(kC4CgNames), nullptr, 0};
static const CompletGallery kCollectionGallery = {
	kCollectionCgNames, ARRAYSIZE(kCollectionCgNames), kCollectionCgGroupEnds, ARRAYSIZE(kCollectionCgGroupEnds)
};

static const CompletGameDescription gameDescriptions[] = {
	{
		{
			"collection",
			nullptr,
			AD_ENTRY1s("mpx/startup.mpx", "2dd5b65ed0e30b913a725d5a4fcd2356", 2225),
			Common::JA_JPN,
			Common::kPlatformWindows,
			ADGF_NO_FLAGS,
			GUIO3(COMPLET_GUIO, GUIO_MIDIGM, GUIO_NOSPEECH)
		},
		640, 480, kCompletEngineXex, false, kKexMenuStyle, kMessageFullScreen, 1, true, kCollectionGallery, kCompletMapNone
	},
	{
		{
			"case266",
			nullptr,
			AD_ENTRY1s("mpx/startup.mpx", "a3395e80f6ac700372f223d32744b5e6", 1666),
			Common::JA_JPN,
			Common::kPlatformWindows,
			ADGF_NO_FLAGS,
			GUIO3(COMPLET_GUIO, GUIO_MIDIGM, GUIO_NOSPEECH)
		},
		640, 480, kCompletEngineXex, false, kKexMenuStyle, kMessageFramedDialogue, 1, true, kCase266Gallery, kCompletMapCase266
	},
	{
		{
			"bokunohimitsutaiken",
			nullptr,
			AD_ENTRY1s("mpx/startup.mpx", "f63e47a50859b39bee96367e8605c207", 2155),
			Common::JA_JPN,
			Common::kPlatformWindows,
			ADGF_NO_FLAGS,
				GUIO3(COMPLET_GUIO, GUIO_MIDIGM, GAMEOPTION_NAME_FRAME)
		},
		640, 480, kCompletEngineXex, false, kKexMenuStyle, kMessageDialogue, 100, true, kC4Gallery, kCompletMapNone
	},
	{
		{
			"koisuruscience",
			nullptr,
			AD_ENTRY1s("mpx/startup.mpx", "d5aa4f549eb99b106efc3ce304252251", 1957),
			Common::JA_JPN,
			Common::kPlatformWindows,
			ADGF_NO_FLAGS,
				GUIO3(COMPLET_GUIO, GUIO_NOMIDI, GAMEOPTION_NAME_FRAME)
		},
		640, 480, kCompletEngineXex, false, kKexMenuStyle, kMessageDialogue, 100, false, kC4Gallery, kCompletMapNone
	},
	{
		{
			"mottohimitsutaiken",
			nullptr,
			AD_ENTRY1s("mpx/startup.mpx", "e9bf0e797cd78f7af6b12e4bf792353e", 2280),
			Common::JA_JPN,
			Common::kPlatformWindows,
			ADGF_NO_FLAGS,
				GUIO4(COMPLET_GUIO, GUIO_MIDIGM, GAMEOPTION_MIDI_MUSIC, GAMEOPTION_NAME_FRAME)
		},
		640, 480, kCompletEngineXex, false, kKexMenuStyle, kMessageDialogue, 100, false, kC4Gallery, kCompletMapMht
	},
	{
		{
			"boseimamaclub",
			nullptr,
			AD_ENTRY1s("mpx/startup.mpx", "a3f8b989737e6856be21c720b29263d1", 2133),
			Common::JA_JPN,
			Common::kPlatformWindows,
			ADGF_NO_FLAGS,
				GUIO4(COMPLET_GUIO, GUIO_MIDIGM, GAMEOPTION_MIDI_MUSIC, GAMEOPTION_NAME_FRAME)
		},
		640, 480, kCompletEngineXex, false, kKexMenuStyle, kMessageDialogue, 100, false, kC4Gallery, kCompletMapNone
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
				GUIO4(COMPLET_GUIO, GUIO_MIDIGM, GAMEOPTION_MIDI_MUSIC, GAMEOPTION_NAME_FRAME)
		},
		640, 480, kCompletEngineXex, true, kMattyMenuStyle, kMessageDialogue, 100, false, kC4Gallery, kCompletMapNone
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
				GUIO4(COMPLET_GUIO, GUIO_MIDIGM, GAMEOPTION_MIDI_MUSIC, GAMEOPTION_NAME_FRAME)
		},
		640, 480, kCompletEngineXex, true, kMattyMenuStyle, kMessageDialogue, 100, false, kC4Gallery, kCompletMapNone
	},
	{
		{
			"yagai",
			nullptr,
			AD_ENTRY1s("mpx/startup.mpx", "eae53720ffed59edbd447a6d16452cd4", 2691),
			Common::JA_JPN,
			Common::kPlatformWindows,
			ADGF_NO_FLAGS,
				GUIO4(COMPLET_GUIO, GUIO_MIDIGM, GAMEOPTION_MIDI_MUSIC, GAMEOPTION_NAME_FRAME)
		},
		800, 600, kCompletEngineC4, true, kPlainC4MenuStyle, kMessageDialogue, 100, false, kC4Gallery, kCompletMapNone
	},
	{
		{
			"mamatuma",
			nullptr,
			AD_ENTRY1s("mpx/startup.mpx", "9de255cdc728753d982abb61ad6cc7e2", 2390),
			Common::JA_JPN,
			Common::kPlatformWindows,
			ADGF_NO_FLAGS,
				GUIO4(COMPLET_GUIO, GUIO_MIDIGM, GAMEOPTION_MIDI_MUSIC, GAMEOPTION_NAME_FRAME)
		},
		800, 600, kCompletEngineC4, true, kPlainC4MenuStyle, kMessageDialogue, 100, false, kC4Gallery, kCompletMapNone
	},
	{
		{
			"mamatuma",
			"Updated",
			AD_ENTRY1s("mpx/startup.mpx", "05b01d4bec391bfc0ee93465a4954cb5", 2453),
			Common::JA_JPN,
			Common::kPlatformWindows,
			ADGF_NO_FLAGS,
				GUIO4(COMPLET_GUIO, GUIO_MIDIGM, GAMEOPTION_MIDI_MUSIC, GAMEOPTION_NAME_FRAME)
		},
		800, 600, kCompletEngineC4, true, kPlainC4MenuStyle, kMessageDialogue, 100, false, kC4Gallery, kCompletMapNone
	},
	{
		{
			"mousugunatsuyasumi",
			nullptr,
			AD_ENTRY1s("mpx/startup.mpx", "46b53766121e969b1cef1dbd123dcc4b", 2479),
			Common::JA_JPN,
			Common::kPlatformWindows,
			ADGF_NO_FLAGS,
				GUIO4(COMPLET_GUIO, GUIO_MIDIGM, GAMEOPTION_MIDI_MUSIC, GAMEOPTION_NAME_FRAME)
		},
		800, 600, kCompletEngineC4, true, kPlainC4MenuStyle, kMessageDialogue, 100, false, kC4Gallery, kCompletMapNone
	},
	{
		AD_TABLE_END_MARKER,
		0, 0, kCompletEngineC4, false, {}, kMessageDialogue, 0, false, {}, kCompletMapNone
	}
};

} // End of namespace Complet

#endif // ENGINES_COMPLET_DETECTION_TABLES_H
