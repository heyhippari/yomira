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

#ifndef SYSTEM98_SYSTEM98_H
#define SYSTEM98_SYSTEM98_H

#include "engines/engine.h"
#include "common/events.h"
#include "graphics/surface.h"
#include "system98/resource.h"

namespace Graphics {
class Font;
class FontSJIS;
}

namespace System98 {

enum System98Action {
	kActionNone,
	kActionAdvance,
	kActionCancel,
	kActionUp,
	kActionDown,
	kActionLeft,
	kActionRight,
	kActionSkip
};

class Sound;

class System98Engine : public Engine {
public:
	explicit System98Engine(OSystem *system);
	~System98Engine() override;
	Common::Error run() override;
	bool hasFeature(EngineFeature feature) const override;
	void syncSoundSettings() override;

private:
	void pauseEngineIntern(bool paused) override;
	Common::Error initialize();
	bool loadScene(const Common::String &name, uint bank);
	// A script offset's bit 14 and above select the secondary bank; the low 14 bits address within it
	static const uint kSceneBankSize = 0x4000;
	static uint sceneBankOf(uint offset) { return offset >= kSceneBankSize; }
	static uint sceneBankOffset(uint offset) { return offset & (kSceneBankSize - 1); }
	const Common::Array<byte> &sceneBank(uint bank) const { return bank ? _secondaryScene : _scene; }
	const Common::String &sceneBankName(uint bank) const { return bank ? _secondaryName : _sceneName; }
	uint16 fetchWord();
	uint16 sceneWord(uint offset) const;
	Common::String sceneString(uint offset, bool fileName = false) const;
	uint16 &reg(uint index);
	bool execute();
	void present();
	bool loadImage(const Common::String &name, int x, int y, uint page, bool palette, bool transparent);
	void copyPage(uint source, const Common::Rect &rect, uint destination, int x, int y, bool transparent);
	bool patternedBlit(const Common::Rect &sourceRect, const Common::Point &position, uint pattern, uint delay);
	void fadePalette(bool out, uint delay, bool monochrome);
	bool waitFrames(uint frames);
	bool waitForTextAdvance();
	bool waitTextDelay(uint frames, bool &fastText);
	void pollInput();
	void printText(uint box, uint offset);
	void drawCharacter(uint16 character, int x, int y, byte color, bool graphics = false);
	void openTextBox(uint box, const Common::Rect &frame, uint style);
	void clearTextBox(uint box, bool close);
	void drawTextBox(uint box);
	void updateMenu(const Common::Event &event);
	struct TextBox {
		Common::Rect rect;
		Common::Rect frame;
		Common::Array<byte> backing;
		Common::Array<byte> backingMask;
		uint16 style = 0;
		bool active = false;
		int x = 0;
		int y = 0;
		byte color = 7;
	};
	struct MenuItem {
		uint16 id;
		Common::Rect rect;
	};
	Resources _resources;
	Sound *_sound;
	Common::Array<byte> _scene;
	Common::Array<byte> _secondaryScene;
	Common::String _sceneName;
	Common::String _secondaryName;
	Common::Array<byte> _font;
	Common::Array<byte> _soundEffects;
	Graphics::Surface _pages[3];
	Graphics::Surface _text;
	Common::Array<byte> _glyphMask;
	Graphics::Surface _screen;
	Graphics::Surface _windowStyles;
	Graphics::FontSJIS *_sjisFont;
	Graphics::Font *_fallbackFont;
	TextBox _textBoxes[16];
	Common::Array<MenuItem> _menu;
	Common::Rect _mouseBounds;
	Common::Point _mouse;
	uint16 _menuRegisters[2];
	uint16 _menuJump;
	byte _menuBank;
	bool _menuEnabled;
	bool _coordinateMenu;
	uint16 _menuStyle;
	uint16 _mouseActionRegister;
	byte _activeBank;
	byte _drawPage;
	bool _scriptValid;
	bool _keyPressed;
	bool _skipMode;
	bool _skipRequested;
	byte _textActionsHeld;
	bool _textMouseHeld;
	uint32 _frameEpoch;
	Common::Array<byte> _patterns[4];
	uint16 _idleCharacters[8];
	uint16 _idleDelays[8];
	byte _targetPalette[16 * 3];
	byte _palette[16 * 3];
	uint16 _registers[1040];
	uint32 _longRegisters[10];
	byte _strings[50][80];
	uint16 _returnStack[8];
	byte _returnBanks[8];
	uint16 _scriptOffset;
	uint16 _stackSize;
	uint16 _paletteLock;
	uint16 _frameCounter;
	uint16 _characterDelay;
	byte _musicMode;
	byte _displayPage;
	byte _compareResult;
};

} // namespace System98

#endif
