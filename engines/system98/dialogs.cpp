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

#include "common/system.h"
#include "common/translation.h"
#include "gui/gui-manager.h"
#include "gui/ThemeEval.h"
#include "gui/widget.h"
#include "gui/widgets/edittext.h"
#include "system98/dialogs.h"

namespace System98 {

// The script centres the name in 2-byte steps up to four full-width characters
static const uint kMaxNameLength = 8;

NameDialog::NameDialog(const Common::String &name) : GUI::Dialog(0, 0, 0, 0), _name(name) {
	_label = new GUI::StaticTextWidget(this, 0, 0, 0, 0, _("Protagonist's name:"), Graphics::kTextAlignLeft);
	_edit = new GUI::EditTextWidget(this, 0, 0, 0, 0, name.decode(Common::kWindows932), Common::U32String(), 0, GUI::kOKCmd);
	_cancel = new GUI::ButtonWidget(this, 0, 0, 0, 0, _("Cancel"), Common::U32String(), GUI::kCloseCmd, Common::ASCII_ESCAPE);
	_ok = new GUI::ButtonWidget(this, 0, 0, 0, 0, _("OK"), Common::U32String(), GUI::kOKCmd);
}

void NameDialog::reflowLayout() {
	const int margin = 10;
	const int buttonWidth = g_gui.xmlEval()->getVar("Globals.Button.Width", 0);
	const int buttonHeight = g_gui.xmlEval()->getVar("Globals.Button.Height", 0);
	const int editHeight = g_gui.xmlEval()->getVar("Globals.Line.Height", kLineHeight);
	int16 screenW, screenH;
	const Common::Rect safeArea = g_system->getSafeOverlayArea(&screenW, &screenH);
	_w = MIN<int>(safeArea.width(), 2 * buttonWidth + 3 * margin + 80);
	_h = 4 * margin + kLineHeight + editHeight + buttonHeight;
	_x = (screenW - _w) / 2;
	_y = (screenH - _h) / 2;
	safeArea.constrain(_x, _y, _w, _h);
	_label->resize(margin, margin, _w - 2 * margin, kLineHeight, false);
	_edit->resize(margin, 2 * margin + kLineHeight, _w - 2 * margin, editHeight, false);
	const int buttonY = _h - margin - buttonHeight;
	_cancel->resize(_w - 2 * (margin + buttonWidth), buttonY, buttonWidth, buttonHeight, false);
	_ok->resize(_w - margin - buttonWidth, buttonY, buttonWidth, buttonHeight, false);
	GUI::Dialog::reflowLayout();
}

void NameDialog::handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) {
	if (cmd == GUI::kOKCmd) {
		Common::String name;
		if (_edit->getEditString().encode(name, Common::kWindows932, '?') != Common::kStringEncodingResultSucceeded)
			return;
		name.trim();
		if (name.empty() || name.size() > kMaxNameLength)
			return;
		_name = name;
		setResult(1);
		close();
		return;
	}
	if (cmd == GUI::kCloseCmd) {
		setResult(0);
		close();
		return;
	}
	GUI::Dialog::handleCommand(sender, cmd, data);
}

} // namespace System98
