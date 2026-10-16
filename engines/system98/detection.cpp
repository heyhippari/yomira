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
		"Main menu and MIDI audio support. Full gameplay is not implemented yet.",
		{
			{"DISK_A.CAT", 0, "1e05843bcf8d6b35d632e01c069e7cd2", 1715},
			{"DISK_A.LIB", 0, "2aaac3d7a5c53bbf7181c6e2c5169786", 693902},
			{"DISK_B.CAT", 0, "7997c5324a0507eb407ae56376289414", 3646},
			{"DISK_B.LIB", 0, "7f4f81d188b09e978bed3afa1b64cdc0", 1082613},
			{"DISK_C.CAT", 0, "84934180a9439590e4a9e733d42611a1", 2351},
			{"DISK_C.LIB", 0, "6ca058b4581861e16d6ac11019053525", 869244},
			{"DISK_D.CAT", 0, "4142099d26b6f40e4a2b96935743158c", 816},
			{"DISK_D.LIB", 0, "a1c690c0d29c30aceaf279cae7e15fa4", 940274},
			AD_LISTEND
		},
		Common::JA_JPN,
		Common::kPlatformPC98,
		ADGF_WARNING,
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
