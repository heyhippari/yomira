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

#include "xex/xex.h"
#include "xex/detection.h"

static const ADExtraGuiOptionsMap optionsList[] = {
	{
		GAMEOPTION_TEXT_ANIMATION,
		{
			_s("Animate text"),
			_s("Show messages one character at a time instead of all at once like the original"),
			"text_animation",
			false,
			0,
			0
		}
	},
	{
		GAMEOPTION_MIDI_MUSIC,
		{
			_s("Use MIDI music"),
			_s("Play the MIDI versions of the music, which the original only used when digital playback was unavailable"),
			"midi_music",
			false,
			0,
			0
		}
	},
	{
		GAMEOPTION_NAME_FRAME,
		{
			_s("Show speaker name frame"),
			_s("Display speaker names in a separate frame above dialogue text"),
			"name_frame",
			true,
			0,
			0
		}
	},
	AD_EXTRA_GUI_OPTIONS_TERMINATOR
};

class XexMetaEngine : public AdvancedMetaEngine<Xex::XexGameDescription> {
public:
	Common::Error createInstance(OSystem *system, Engine **engine, const Xex::XexGameDescription *description) const override {
		*engine = new Xex::XexEngine(system, *description);
		return Common::Error(Common::kNoError);
	}

	const char *getName() const override {
		return "xex";
	}

	const ADExtraGuiOptionsMap *getAdvancedExtraGuiOptions() const override {
		return optionsList;
	}

	Common::KeymapArray initKeymaps(const char *target) const override;
};

Common::KeymapArray XexMetaEngine::initKeymaps(const char *target) const {
	using namespace Common;
	using namespace Xex;

	Keymap *keymap = new Keymap(Keymap::kKeymapTypeGame, "xex-default", _("Default keymappings"));

	// The controller's A button advances instead, as a click at the pointer could hit a menu item or button
	Action *act = new Action(kStandardActionLeftClick, _("Left click"));
	act->setLeftClickEvent();
	act->addDefaultInputMapping("MOUSE_LEFT");
	keymap->addAction(act);

	act = new Action(kStandardActionRightClick, _("Right click"));
	act->setRightClickEvent();
	act->addDefaultInputMapping("MOUSE_RIGHT");
	keymap->addAction(act);

	struct ActionDef {
		const char *id;
		const char *description;
		XexAction action;
		const char *mappings[4];
	};
	static const ActionDef kActions[] = {
		{"ADVANCE", _s("Next message / Select"), kActionAdvance, {"RETURN", "SPACE", "KP_ENTER", "JOY_A"}},
		{"SKIP", _s("Skip messages (hold)"), kActionSkip, {"LCTRL", "RCTRL", "JOY_RIGHT_TRIGGER"}},
		{"AUTO", _s("Toggle auto mode"), kActionAutoMode, {"a", "JOY_LEFT_TRIGGER", nullptr}},
		{"HIDE", _s("Hide message window / Back"), kActionHide, {"ESCAPE", "JOY_B", nullptr}},
		{"BACKLOG", _s("Message history / Previous page"), kActionBacklog, {"PAGEUP", "JOY_LEFT_SHOULDER", nullptr}},
		{"BACKLOGNEXT", _s("Next history page"), kActionBacklogForward, {"PAGEDOWN", "JOY_RIGHT_SHOULDER", nullptr}},
		{"UP", _s("Up"), kActionUp, {"UP", "JOY_UP", nullptr}},
		{"DOWN", _s("Down"), kActionDown, {"DOWN", "JOY_DOWN", nullptr}},
		{"LEFT", _s("Left"), kActionLeft, {"LEFT", "JOY_LEFT", nullptr}},
		{"RIGHT", _s("Right"), kActionRight, {"RIGHT", "JOY_RIGHT", nullptr}},
		{"QSAVE", _s("Quick save"), kActionQuickSave, {"s", "JOY_X", nullptr}},
		{"QLOAD", _s("Quick load"), kActionQuickLoad, {"l", "JOY_Y", nullptr}}
	};

	for (const ActionDef &def : kActions) {
		act = new Action(def.id, _(def.description));
		act->setCustomEngineActionEvent(def.action);
		for (const char *mapping : def.mappings) {
			if (mapping)
				act->addDefaultInputMapping(mapping);
		}
		keymap->addAction(act);
	}

	return Keymap::arrayOf(keymap);
}

#if PLUGIN_ENABLED_DYNAMIC(XEX)
REGISTER_PLUGIN_DYNAMIC(XEX, PLUGIN_TYPE_ENGINE, XexMetaEngine);
#else
REGISTER_PLUGIN_STATIC(XEX, PLUGIN_TYPE_ENGINE, XexMetaEngine);
#endif
