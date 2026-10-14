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
#include "complet/detection_tables.h"

static const DebugChannelDef debugFlagList[] = {
	{Complet::kDebugScript, "script", "Script commands"},
	{Complet::kDebugGraphics, "graphics", "Graphics loading"},
	{Complet::kDebugAudio, "audio", "Music and voices"},
	DEBUG_CHANNEL_END
};

static const PlainGameDescriptor completGames[] = {
	{"mamatuma", "Mama! Tsuma?"},
	{"yagai", "Yagai Gakushuu"},
	{"koisuruscience", "Koi Suru Science"},
	{nullptr, nullptr}
};

class CompletMetaEngineDetection : public AdvancedMetaEngineDetection<ADGameDescription> {
public:
	CompletMetaEngineDetection() : AdvancedMetaEngineDetection(Complet::gameDescriptions, completGames) {
		_flags = kADFlagMatchFullPaths;
	}

	const char *getName() const override {
		return "complet";
	}

	const char *getEngineName() const override {
		return "C4 Engine";
	}

	const char *getOriginalCopyright() const override {
		return "Complet's";
	}

	const DebugChannelDef *getDebugChannels() const override {
		return debugFlagList;
	}
};

REGISTER_PLUGIN_STATIC(COMPLET_DETECTION, PLUGIN_TYPE_ENGINE_DETECTION, CompletMetaEngineDetection);
