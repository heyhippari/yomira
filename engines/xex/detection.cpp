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

#include "base/plugins.h"
#include "common/debug-channels.h"
#include "engines/advancedDetector.h"
#include "xex/detection_tables.h"

static const DebugChannelDef debugFlagList[] = {
	{Xex::kDebugScript, "script", "Script commands"},
	{Xex::kDebugGraphics, "graphics", "Graphics loading"},
	{Xex::kDebugAudio, "audio", "Music and voices"},
	DEBUG_CHANNEL_END
};

static const PlainGameDescriptor xexGames[] = {
	{"collection", "Collection ~Shuushuu Sareta Akumu~"},
	{"case266", "Case 266 ~266 Gou Jiken~"},
	{"bokunohimitsutaiken", "Boku no Himitsu Taiken"},
	{"koisuruscience", "Koi Suru Science"},
	{"mottohimitsutaiken", "Motto! Himitsu Taiken"},
	{"boseimamaclub", "Bosei ~Mama Club~"},
	{"matty", "Matty"},
	{"yagai", "Yagai Gakushuu"},
	{"mamatuma", "Mama! Tsuma?"},
	{"mousugunatsuyasumi", "Mousugu Natsuyasumi!"},
	{nullptr, nullptr}
};

class XexMetaEngineDetection : public AdvancedMetaEngineDetection<Xex::XexGameDescription> {
public:
	XexMetaEngineDetection() : AdvancedMetaEngineDetection(Xex::gameDescriptions, xexGames) {
		_flags = kADFlagMatchFullPaths;
	}

	const char *getName() const override {
		return "xex";
	}

	const char *getEngineName() const override {
		return "XEX Engine";
	}

	const char *getOriginalCopyright() const override {
		return "Complet's";
	}

	const DebugChannelDef *getDebugChannels() const override {
		return debugFlagList;
	}
};

REGISTER_PLUGIN_STATIC(XEX_DETECTION, PLUGIN_TYPE_ENGINE_DETECTION, XexMetaEngineDetection);
