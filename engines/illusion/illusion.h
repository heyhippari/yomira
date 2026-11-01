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

#ifndef ILLUSION_H
#define ILLUSION_H

#include "common/scummsys.h"
#include "common/system.h"
#include "common/error.h"
#include "common/events.h"
#include "common/fs.h"
#include "common/random.h"
#include "common/rect.h"
#include "common/util.h"
#include "engines/engine.h"
#include "audio/mixer.h"

#include "illusion/archive.h"
#include "illusion/detection.h"

namespace Illusion {

struct IllusionInputState {
	bool keys[Common::KEYCODE_LAST];
	bool keysPressed[Common::KEYCODE_LAST];
	Common::Point mousePosition;
	Common::Point mouseDelta;
	uint32 mouseButtons;
	uint32 mouseButtonsPressed;
	int32 mouseWheelDelta;
	bool redrawRequested;
	bool quitRequested;

	IllusionInputState();
};

struct IllusionColorChannelMask {
	uint32 mask;
	uint8 bitCount;
	uint8 shift;
};

struct IllusionRendererState {
	Common::Rect viewport;
	Common::Point screenCenter;
	float uiScale;
	float projectionAspect;
	float projectionFieldOfView;
	float projectionNear;
	float projectionFar;
	float projectionMatrix[16];
	float sineTable[360];
	float cosineTable[360];
	IllusionColorChannelMask colorChannels[3];
	bool perspectiveCorrectTexturing;
	bool depthBufferEnabled;
	bool ditheringEnabled;
	bool alphaBlendingEnabled;
	uint8 cullMode;
	uint8 sourceBlendFactor;
	uint8 destinationBlendFactor;
	uint8 minificationFilter;
	uint8 magnificationFilter;
	uint8 textureColorOperation;
	uint8 textureColorArgument1;
	uint8 textureColorArgument2;
	uint8 textureAlphaOperation;
	uint8 textureAlphaArgument1;
	uint8 textureAlphaArgument2;

	IllusionRendererState();
};

class IllusionRenderer;

class IllusionEngine : public Engine {
private:
	enum StartupPhase {
		kMoviePending,
		kMenuBackground,
		kMenuButtons,
		kMenuReady,
		kMenuLeaving
	};

	enum CharacterSelectPhase {
		kSelectFadeIn,
		kSelectReady,
		kSelectFadeOut
	};

	enum CharacterSelectImage {
		kSelectBackground,
		kSelectTitle,
		kSelectProfileLabel,
		kSelectGrid,
		kSelectFrame,
		kSelectOkOn,
		kSelectOkOff,
		kSelectBackOn,
		kSelectBackOff,
		kSelectBadge,
		kSelectDayDigit = kSelectBadge + 7,
		kSelectTimeDigit = kSelectDayDigit + 10,
		kSelectName = kSelectTimeDigit + 6,
		kSelectImageCount = kSelectName + 9
	};

	const ADGameDescription *_gameDescription;
	Common::RandomSource _randomSource;
	IllusionInputState _inputState;
	IllusionRendererState _rendererState;
	IllusionRenderer *_renderer;
	IllusionArchive _soundArchive;
	Audio::SoundHandle _musicHandle;
	Audio::SoundHandle _effectHandle;
	uint _menuImages[12];
	uint _fadeImage;
	uint _pointerImage;
	Common::Array<uint> _sceneImages;
	StartupPhase _startupPhase;
	uint32 _startupElapsed;
	int _menuSelection;
	int _menuAction;
	bool _linearFiltering;

	// New-game state. The day counter and the per-girl day counts and clear flags start at
	// 1, 0 and false; save games are not supported yet.
	int _day;
	int _currentGirl;
	uint _selectImages[kSelectImageCount];
	IllusionArchive _selectArchive;
	uint _portraitImage;
	uint _profileImage;
	CharacterSelectPhase _selectPhase;
	uint32 _selectElapsed;
	uint32 _portraitElapsed;
	int _selectedSlot;
	int _shownSlot;
	int _selectButton;
	bool _selectConfirmed;
protected:
	// Engine APIs
	Common::Error run() override;

private:
	/** Checks for the minimum Sexy Beach 2 files before anything is started. */
	Common::Error validateGameData() const;
	Common::Error initDisplay();
	void initRendererState();
	Common::Error loadStartupResources();
	Common::Error loadMenuResources();
	Common::Error initCharacterSelection();
	Common::Error loadCharacterSelection();
	Common::Error loadCharacterPortrait(int slot);
	uint loadSceneImage(IllusionArchive &archive, const Common::String &name, bool keepBackgroundCopy = false);
	void clearSceneImages();
	Common::Error runStartup();
	Common::Error runCharacterSelection(bool &returnToTitle);
	void updateCharacterSelection(uint32 delta);
	void drawCharacterSelection();
	void drawPointer();
	void playSound(const Common::String &name, bool loop);
	void drawMenu();
	Common::Rect menuRectangle(int x, int y, int width, int height) const;
	void pumpEvents();
	void setMousePosition(const Common::Point &position);

public:
	IllusionEngine(OSystem *syst, const ADGameDescription *gameDesc);
	~IllusionEngine() override;

	uint32 getFeatures() const;

	/**
	 * Returns the game Id
	 */
	Common::String getGameId() const;
	const IllusionInputState &getInputState() const { return _inputState; }
	const IllusionRendererState &getRendererState() const { return _rendererState; }

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

extern IllusionEngine *g_engine;
#define SHOULD_QUIT ::Illusion::g_engine->shouldQuit()

} // End of namespace Illusion

#endif // ILLUSION_H
