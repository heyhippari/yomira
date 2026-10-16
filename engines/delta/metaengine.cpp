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

#include "common/translation.h"
#include "backends/keymapper/action.h"
#include "backends/keymapper/keymap.h"
#include "backends/keymapper/standard-actions.h"

#include "delta/metaengine.h"
#include "delta/detection.h"
#include "delta/delta.h"

const char *DeltaMetaEngine::getName() const {
	return "delta";
}

const ADExtraGuiOptionsMap *DeltaMetaEngine::getAdvancedExtraGuiOptions() const {
	return nullptr;
}

Common::Error DeltaMetaEngine::createInstance(OSystem *syst, Engine **engine, const ADGameDescription *desc) const {
	*engine = new Delta::DeltaEngine(syst, desc);
	return Common::kNoError;
}

bool DeltaMetaEngine::hasFeature(MetaEngineFeature f) const {
	return checkExtendedSaves(f) || f == kSupportsLoadingDuringStartup;
}

Common::KeymapArray DeltaMetaEngine::initKeymaps(const char *target) const {
	using namespace Common;
	using namespace Delta;
	Keymap *keymap = new Keymap(Keymap::kKeymapTypeGame, "delta-default", _("Default keymappings"));
	Action *action = new Action(kStandardActionLeftClick, _("Left click"));
	action->setLeftClickEvent();
	action->addDefaultInputMapping("MOUSE_LEFT");
	keymap->addAction(action);
	action = new Action(kStandardActionRightClick, _("Right click"));
	action->setRightClickEvent();
	action->addDefaultInputMapping("MOUSE_RIGHT");
	keymap->addAction(action);

	struct ActionDef {
		const char *id;
		const char *description;
		DeltaAction action;
		const char *mappings[6];
	};
	static const ActionDef actions[] = {
		{ "SELECT", _s("Select"), kActionSelect, { "RETURN", "SPACE", "KP_ENTER", "JOY_A" } },
		{ "CANCEL", _s("Back / Cancel"), kActionCancel, { "ESCAPE", "JOY_B" } },
		{ "UP", _s("Up"), kActionUp, { "UP", "KP8", "JOY_UP", "JOY_LEFT_STICK_Y-", "JOY_RIGHT_STICK_Y-" } },
		{ "DOWN", _s("Down"), kActionDown, { "DOWN", "KP2", "JOY_DOWN", "JOY_LEFT_STICK_Y+", "JOY_RIGHT_STICK_Y+" } },
		{ "LEFT", _s("Left"), kActionLeft, { "LEFT", "KP4", "JOY_LEFT", "JOY_LEFT_STICK_X-", "JOY_RIGHT_STICK_X-" } },
		{ "RIGHT", _s("Right"), kActionRight, { "RIGHT", "KP6", "JOY_RIGHT", "JOY_LEFT_STICK_X+", "JOY_RIGHT_STICK_X+" } },
		{ "MENU", _s("Game menu"), kActionMenu, { "F5", "JOY_START" } }
	};
	for (const ActionDef &definition : actions) {
		action = new Action(definition.id, _(definition.description));
		action->setCustomEngineActionEvent(definition.action);
		for (const char *mapping : definition.mappings) {
			if (mapping)
				action->addDefaultInputMapping(mapping);
		}
		keymap->addAction(action);
	}
	return Keymap::arrayOf(keymap);
}

#if PLUGIN_ENABLED_DYNAMIC(DELTA)
REGISTER_PLUGIN_DYNAMIC(DELTA, PLUGIN_TYPE_ENGINE, DeltaMetaEngine);
#else
REGISTER_PLUGIN_STATIC(DELTA, PLUGIN_TYPE_ENGINE, DeltaMetaEngine);
#endif
