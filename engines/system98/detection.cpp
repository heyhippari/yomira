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
#include "engines/advancedDetector.h"

namespace System98 {

static const PlainGameDescriptor system98Games[] = {
	{"mugenyasoukyoku", "Mugen Yasoukyoku (Nocturnal Illusion)"},
	{nullptr, nullptr}
};

static const ADGameDescription gameDescriptions[] = {
	{
		"mugenyasoukyoku",
		"Game playback is not implemented yet.",
		AD_ENTRY2s("DISK_A.CAT", "1e05843bcf8d6b35d632e01c069e7cd2", 1715,
		           "DISK_A.LIB", "2aaac3d7a5c53bbf7181c6e2c5169786", 102400),
		Common::JA_JPN,
		Common::kPlatformPC98,
		ADGF_UNSUPPORTED,
		GUIO0()
	},
	AD_TABLE_END_MARKER
};

class System98MetaEngineDetection : public AdvancedMetaEngineDetection<ADGameDescription> {
public:
	System98MetaEngineDetection() : AdvancedMetaEngineDetection(gameDescriptions, system98Games) {
	}

	const char *getName() const override {
		return "system98";
	}

	const char *getEngineName() const override {
		return "System-98";
	}

	const char *getOriginalCopyright() const override {
		return "System-98 (C) Four\xEF\xBD\xA5Nine";
	}
};

} // namespace System98

REGISTER_PLUGIN_STATIC(SYSTEM98_DETECTION, PLUGIN_TYPE_ENGINE_DETECTION, System98::System98MetaEngineDetection);
