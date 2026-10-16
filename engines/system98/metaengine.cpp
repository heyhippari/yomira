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

#include "backends/keymapper/action.h"
#include "backends/keymapper/keymap.h"
#include "backends/keymapper/standard-actions.h"
#include "base/plugins.h"
#include "common/translation.h"
#include "engines/advancedDetector.h"
#include "system98/system98.h"

class System98MetaEngine : public AdvancedMetaEngine<ADGameDescription> {
public:
	const char *getName() const override {
		return "system98";
	}

	Common::Error createInstance(OSystem *system, Engine **engine, const ADGameDescription *description) const override {
		*engine = new System98::System98Engine(system);
		return Common::kNoError;
	}

	Common::KeymapArray initKeymaps(const char *target) const override;
};

Common::KeymapArray System98MetaEngine::initKeymaps(const char *target) const {
	using namespace Common;
	using namespace System98;

	Keymap *keymap = new Keymap(Keymap::kKeymapTypeGame, "system98-default", _("Default keymappings"));
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
		System98Action action;
		const char *mappings[5];
	};
	static const ActionDef actions[] = {
		{"ADVANCE", _s("Next message / Select"), kActionAdvance, {"RETURN", "SPACE", "KP_ENTER", "x", "JOY_A"}},
		{"CANCEL", _s("Back / Cancel"), kActionCancel, {"ESCAPE", "z", "KP_DIVIDE", "KP0", "JOY_B"}},
		{"UP", _s("Up"), kActionUp, {"UP", "KP8", "JOY_UP", nullptr, nullptr}},
		{"DOWN", _s("Down"), kActionDown, {"DOWN", "KP2", "JOY_DOWN", nullptr, nullptr}},
		{"LEFT", _s("Left"), kActionLeft, {"LEFT", "KP4", "JOY_LEFT", nullptr, nullptr}},
		{"RIGHT", _s("Right"), kActionRight, {"RIGHT", "KP6", "JOY_RIGHT", nullptr, nullptr}},
		{"SKIP", _s("Skip messages (hold)"), kActionSkip, {"LCTRL", "RCTRL", "JOY_RIGHT_TRIGGER", nullptr, nullptr}}
	};
	for (const ActionDef &definition : actions) {
		action = new Action(definition.id, _(definition.description));
		action->setCustomEngineActionEvent(definition.action);
		for (const char *mapping : definition.mappings)
			if (mapping)
				action->addDefaultInputMapping(mapping);
		keymap->addAction(action);
	}
	return Keymap::arrayOf(keymap);
}

#if PLUGIN_ENABLED_DYNAMIC(SYSTEM98)
REGISTER_PLUGIN_DYNAMIC(SYSTEM98, PLUGIN_TYPE_ENGINE, System98MetaEngine);
#else
REGISTER_PLUGIN_STATIC(SYSTEM98, PLUGIN_TYPE_ENGINE, System98MetaEngine);
#endif
