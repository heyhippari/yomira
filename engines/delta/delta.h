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

#ifndef DELTA_H
#define DELTA_H

#include "common/error.h"
#include "common/random.h"
#include "engines/engine.h"
#include "graphics/screen.h"
#include "graphics/font.h"

#include "delta/data.h"
#include "delta/input.h"
#include "delta/script.h"

struct ADGameDescription;

namespace Delta {

class DeltaEngine : public Engine {
private:
	const ADGameDescription *_gameDescription;
	Common::RandomSource _randomSource;
	StartupConfig _configuration;
	Common::Array<byte> _scenario;
	uint32 _scriptPosition = 0;
	Graphics::Screen *_screen = nullptr;
	Script *_script = nullptr;
	ScriptCommand _menuConfig;
	Graphics::Surface _transitionFrom;
	Graphics::Surface _transitionTo;
	Graphics::Surface _transitionFrame;
	Graphics::Surface _menuBackground;
	Graphics::Surface _menuButtons;
	uint32 _transitionStart = 0;
	uint32 _transitionDuration = 0;
	uint32 _waitUntil = 0;
	bool _waiting = false;
	bool _atMenu = false;
	bool _cursorInstalled = false;
	InputState _input;
	int _menuHover = -1;
	int _menuPressed = -1;
	uint16 _menuResultVariable = 0;
	bool _uiBusy = false;
	bool _loadPending = false;
	Common::Array<byte> _pendingState;
	Common::Array<byte> _titleState;
	Graphics::Font *_messageFont = nullptr;
	Graphics::Surface _messageBackground;
	Graphics::Surface _messageFrameColor[2];
	Graphics::Surface _messageFrameMask[2];
	Common::Point _messageFramePosition[2];
	Common::StringArray _messageTokens;
	uint _messageTokenIndex = 0;
	Common::Array<Common::U32String> _messageLines;
	uint _messageLineIndex = 0;
	int _speaker = -1;
	bool _messageVisible = false;
	bool _messageWaiting = false;
	bool _messageBoundary = false;

	Common::Error initialize();
	Common::Error loadImage(const Common::String &name, Graphics::Surface &image);
	Common::Error compositeForeground(Graphics::Surface &image, const Common::String &name);
	Common::Error compositeLayers(Graphics::Surface &image, const Common::StringArray &layers, const Common::Path &bankName);
	Common::Error advanceScript();
	Common::Error enterMenu();
	void drawMenu();
	void handleInput(const Common::Event &event);
	bool pollInput();
	bool serviceInput();
	uint32 _nextInputUpdate = 0;
	uint32 _nextPresent = 0;
	void startTransition(const Graphics::Surface &image, uint32 duration);
	bool updateTransition();
	int menuItemAt(const Common::Point &point) const;
	void activateMenuItem(int item);
	void showExtras();
	bool restoreSaveStream(Common::SeekableReadStream &stream, bool apply);
	Common::Error applyPendingLoad();
	Common::Error presentMessage();
	Common::Error loadMessageFrame(uint frame);
	void drawMessagePage();
	void advanceMessage();

protected:
	Common::Error run() override;

public:
	DeltaEngine(OSystem *syst, const ADGameDescription *gameDesc);
	~DeltaEngine() override;

	uint32 getFeatures() const;

	/**
	 * Returns the game Id
	 */
	Common::String getGameId() const;
	bool canLoadGameStateCurrently(Common::U32String *msg = nullptr) override;
	bool canSaveGameStateCurrently(Common::U32String *msg = nullptr) override;
	Common::Error saveGameStream(Common::WriteStream *stream, bool isAutosave = false) override;
	Common::Error loadGameStream(Common::SeekableReadStream *stream) override;

	/**
	 * Gets a random number
	 */
	uint32 getRandomNumber(uint maxNum) {
		return _randomSource.getRandomNumber(maxNum);
	}

	bool hasFeature(EngineFeature f) const override {
		return f == kSupportsReturnToLauncher || f == kSupportsLoadingDuringRuntime ||
			f == kSupportsSavingDuringRuntime || f == kSupportsChangingOptionsDuringRuntime;
	}
};

} // End of namespace Delta

#endif // DELTA_H
