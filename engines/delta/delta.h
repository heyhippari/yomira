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

	Common::Error initialize();
	Common::Error loadImage(const Common::String &name, Graphics::Surface &image);
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

	/**
	 * Gets a random number
	 */
	uint32 getRandomNumber(uint maxNum) {
		return _randomSource.getRandomNumber(maxNum);
	}

	bool hasFeature(EngineFeature f) const override {
		return f == kSupportsReturnToLauncher;
	}
};

} // End of namespace Delta

#endif // DELTA_H
