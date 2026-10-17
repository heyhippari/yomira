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

#include "illusion/illusion.h"
#include "illusion/detection.h"
#include "illusion/console.h"
#include "illusion/renderer.h"
#include "audio/audiostream.h"
#include "audio/decoders/wave.h"
#include "common/scummsys.h"
#include "common/debug.h"
#include "common/events.h"
#include "common/file.h"
#include "common/memstream.h"
#include "common/system.h"
#include "engines/util.h"
#include "image/bmp.h"
#include "image/tga.h"
#include "math/utils.h"

#include <math.h>

namespace Illusion {

IllusionEngine *g_engine;

static const int kDefaultScreenWidth = 640;
static const int kDefaultScreenHeight = 480;
static const int kUiBaseWidth = 640;
static const uint32 kSettingsDisplayOffset = 0x20;
static const uint32 kSettingsScaleOffset = 0x40;
static const uint32 kSettingsTextureFilterOffset = 0x10;
static const uint32 kSettingsMinimumSize = 0x44;
static const float kUiScaleFactors[] = { 1.0f, 1.5f, 2.0f };
static const float kProjectionAspect = 0.75f;
static const float kProjectionFieldOfView = 0.5f;
static const float kProjectionNearPlane = 1.0f;
static const float kProjectionFarPlane = 100000.0f;
static const uint32 kMouseLeftMask = 1U << 0;
static const uint32 kMouseRightMask = 1U << 1;
static const uint32 kMouseMiddleMask = 1U << 2;
static const uint32 kMouseX1Mask = 1U << 3;
static const uint8 kTextureFilterPoint = 1;
static const uint8 kTextureFilterLinear = 2;
static const uint8 kTextureOperationModulate = 4;
static const uint8 kTextureArgumentTexture = 2;
static const uint8 kTextureArgumentDiffuse = 0;

static const int kArchiveCount = 36;
static const int kIndexCount = 3;
static const int kMenuButtonY[] = { 337, 424, 510, 596, 682 };

// Archives that ship without a .tbl in the supplied data set.
static bool archiveHasTable(int archive) {
	return archive < 31 || archive > 33;
}

static bool readSettingsWord(Common::SeekableReadStream &stream, uint32 offset, uint32 &value) {
	if (!stream.seek(offset))
		return false;
	value = stream.readUint32LE();
	return !stream.err() && !stream.eos();
}

IllusionInputState::IllusionInputState() : mousePosition(0, 0), mouseDelta(0, 0),
	mouseButtons(0), mouseButtonsPressed(0), mouseWheelDelta(0), redrawRequested(false), quitRequested(false) {
	for (uint i = 0; i < ARRAYSIZE(keys); ++i) {
		keys[i] = false;
		keysPressed[i] = false;
	}
}

IllusionRendererState::IllusionRendererState() : viewport(0, 0, 0, 0), screenCenter(0, 0),
	uiScale(1.0f), projectionAspect(kProjectionAspect),
	projectionFieldOfView(kProjectionFieldOfView), projectionNear(kProjectionNearPlane),
	projectionFar(kProjectionFarPlane), perspectiveCorrectTexturing(true), depthBufferEnabled(true),
	ditheringEnabled(true), alphaBlendingEnabled(true), cullMode(3), sourceBlendFactor(5),
	destinationBlendFactor(6), minificationFilter(kTextureFilterPoint), magnificationFilter(kTextureFilterPoint),
	textureColorOperation(kTextureOperationModulate), textureColorArgument1(kTextureArgumentTexture),
	textureColorArgument2(kTextureArgumentDiffuse), textureAlphaOperation(kTextureOperationModulate),
	textureAlphaArgument1(kTextureArgumentTexture), textureAlphaArgument2(kTextureArgumentDiffuse) {
	for (uint i = 0; i < ARRAYSIZE(projectionMatrix); ++i)
		projectionMatrix[i] = 0.0f;
	for (uint i = 0; i < ARRAYSIZE(sineTable); ++i) {
		sineTable[i] = 0.0f;
		cosineTable[i] = 0.0f;
	}
	for (uint i = 0; i < ARRAYSIZE(colorChannels); ++i) {
		colorChannels[i].mask = 0;
		colorChannels[i].bitCount = 0;
		colorChannels[i].shift = 0;
	}
}

IllusionEngine::IllusionEngine(OSystem *syst, const ADGameDescription *gameDesc) : Engine(syst),
	_gameDescription(gameDesc), _randomSource("Illusion"), _renderer(nullptr),
	_startupPhase(kMoviePending), _startupElapsed(0), _menuSelection(0), _menuAction(-1), _linearFiltering(false) {
	for (uint index = 0; index < ARRAYSIZE(_menuImages); ++index)
		_menuImages[index] = 0;
	g_engine = this;
}

IllusionEngine::~IllusionEngine() {
	_mixer->stopHandle(_musicHandle);
	_mixer->stopHandle(_effectHandle);
	clearSceneImages();
	delete _renderer;
	g_engine = nullptr;
}

uint32 IllusionEngine::getFeatures() const {
	return _gameDescription->flags;
}

Common::String IllusionEngine::getGameId() const {
	return _gameDescription->gameId;
}

Common::Error IllusionEngine::validateGameData() const {
	const char *const fixedFiles[] = { "Data/sune.rrr", "Data/000_640.FTT", "Data/000_1024.FTT" };
	for (uint i = 0; i < ARRAYSIZE(fixedFiles); ++i) {
		if (!Common::File::exists(fixedFiles[i]))
			return Common::Error(Common::kNoGameDataFoundError, Common::String::format("Missing %s", fixedFiles[i]));
	}

	for (int i = 0; i < kIndexCount; ++i) {
		const Common::String name = Common::String::format("Data/sb2_%03d.ix", i);
		if (!Common::File::exists(Common::Path(name)))
			return Common::Error(Common::kNoGameDataFoundError, "Missing " + name);
	}

	for (int i = 0; i < kArchiveCount; ++i) {
		const Common::String pp = Common::String::format("Data/sb2_%03d.pp", i);
		if (!Common::File::exists(Common::Path(pp)))
			return Common::Error(Common::kNoGameDataFoundError, "Missing " + pp);

		const Common::String tbl = Common::String::format("Data/sb2_%03d.tbl", i);
		if (archiveHasTable(i) && !Common::File::exists(Common::Path(tbl)))
			return Common::Error(Common::kNoGameDataFoundError, "Missing " + tbl);
	}

	return Common::kNoError;
}

Common::Error IllusionEngine::initDisplay() {
	int width = kDefaultScreenWidth;
	int height = kDefaultScreenHeight;
	uint32 bitsPerPixel = 32;
	uint32 uiScaleIndex = 0;
	bool displaySettingsValid = false;

	Common::File settings;
	if (settings.open("setting.cfg") && settings.size() >= kSettingsMinimumSize) {
		uint32 configuredWidth = 0, configuredHeight = 0, configuredDepth = 0, value = 0;
		const bool displayFieldsRead =
			readSettingsWord(settings, kSettingsDisplayOffset, configuredWidth) &&
			readSettingsWord(settings, kSettingsDisplayOffset + 4, configuredHeight) &&
			readSettingsWord(settings, kSettingsDisplayOffset + 8, configuredDepth);
		if (readSettingsWord(settings, kSettingsTextureFilterOffset, value))
			_linearFiltering = value != 0;
		if (readSettingsWord(settings, kSettingsScaleOffset, value) && value < ARRAYSIZE(kUiScaleFactors))
			uiScaleIndex = value;
		if (displayFieldsRead && configuredWidth >= 320 && configuredWidth <= 4096 &&
			configuredHeight >= 200 && configuredHeight <= 2160 &&
			(configuredDepth == 16 || configuredDepth == 24 || configuredDepth == 32)) {
			width = configuredWidth;
			height = configuredHeight;
			bitsPerPixel = configuredDepth;
			displaySettingsValid = true;
		}
	}
	if (!displaySettingsValid)
		debug(1, "Illusion: using default display mode %dx%dx32", width, height);

	// TinyGL only renders to 16 and 32 bits per pixel, so 24-bit modes use a 32-bit surface.
	const Graphics::PixelFormat format = bitsPerPixel == 16 ?
		Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0) :
		Graphics::PixelFormat::createFormatBGRA32();
	initGraphics(width, height, &format);
	const Graphics::PixelFormat &actualFormat = _system->getScreenFormat();
	if (!(actualFormat == format))
		warning("Illusion: backend pixel format differs from the requested format");
	if (actualFormat.bytesPerPixel != 2 && actualFormat.bytesPerPixel != 4)
		return Common::kUnsupportedColorMode;

	_rendererState.viewport = Common::Rect(0, 0, _system->getWidth(), _system->getHeight());
	_rendererState.screenCenter = Common::Point(_system->getWidth() / 2, _system->getHeight() / 2);
	_rendererState.uiScale = (_system->getWidth() / (float)kUiBaseWidth) * kUiScaleFactors[uiScaleIndex];
	_rendererState.minificationFilter = _linearFiltering ? kTextureFilterLinear : kTextureFilterPoint;
	_rendererState.magnificationFilter = _linearFiltering ? kTextureFilterLinear : kTextureFilterPoint;
	setMousePosition(_system->getEventManager()->getMousePos());
	return Common::kNoError;
}

void IllusionEngine::initRendererState() {
	const float halfFov = _rendererState.projectionFieldOfView * 0.5f;
	const float focalLength = (float)(cos(halfFov) / sin(halfFov));
	float *projection = _rendererState.projectionMatrix;
	// The shipped projection helper multiplies cot(FOV/2) by its 0.75 aspect value.
	projection[0] = focalLength * _rendererState.projectionAspect;
	projection[5] = focalLength;
	projection[10] = _rendererState.projectionFar / (_rendererState.projectionFar - _rendererState.projectionNear);
	projection[11] = 1.0f;
	projection[14] = -(_rendererState.projectionNear * _rendererState.projectionFar) /
		(_rendererState.projectionFar - _rendererState.projectionNear);

	for (uint degree = 0; degree < ARRAYSIZE(_rendererState.sineTable); ++degree) {
		const double angle = Math::deg2rad((double)degree);
		_rendererState.sineTable[degree] = (float)sin(angle);
		_rendererState.cosineTable[degree] = (float)cos(angle);
	}

	const Graphics::PixelFormat &format = _system->getScreenFormat();
	const uint8 bits[] = { (uint8)(8 - format.rLoss), (uint8)(8 - format.gLoss), (uint8)(8 - format.bLoss) };
	const uint8 shifts[] = { format.rShift, format.gShift, format.bShift };
	for (uint channel = 0; channel < ARRAYSIZE(bits); ++channel) {
		IllusionColorChannelMask &mask = _rendererState.colorChannels[channel];
		mask.bitCount = bits[channel];
		mask.shift = shifts[channel];
		mask.mask = bits[channel] ? ((1U << bits[channel]) - 1) << shifts[channel] : 0;
	}
}

Common::Error IllusionEngine::loadStartupResources() {
	Image::BitmapDecoder fadeBackground;
	Image::BitmapDecoder loadingImage;
	Image::TGADecoder startupLogo;
	IllusionArchive archive;
	if (!archive.open("Data/sb2_007.pp", "Data/sb2_007.tbl"))
		return Common::Error(Common::kNoGameDataFoundError, "Unable to open Data/sb2_007.pp/.tbl");

	Common::Array<byte> imageData;
	if (!archive.readMember("fadeW.bmp", imageData))
		return Common::Error(Common::kReadingFailed, "Unable to read fadeW.bmp from sb2_007.pp");
	Common::MemoryReadStream fadeStream(imageData.data(), imageData.size());
	if (!fadeBackground.loadStream(fadeStream))
		return Common::Error(Common::kReadingFailed, "Invalid fadeW.bmp in sb2_007.pp");
	debug(1, "Illusion startup image: fadeW.bmp (%dx%d)",
		fadeBackground.getSurface()->w, fadeBackground.getSurface()->h);

	if (!archive.readMember("loading.bmp", imageData))
		return Common::Error(Common::kReadingFailed, "Unable to read loading.bmp from sb2_007.pp");
	Common::MemoryReadStream loadingStream(imageData.data(), imageData.size());
	if (!loadingImage.loadStream(loadingStream))
		return Common::Error(Common::kReadingFailed, "Invalid loading.bmp in sb2_007.pp");
	debug(1, "Illusion startup image: loading.bmp (%dx%d)",
		loadingImage.getSurface()->w, loadingImage.getSurface()->h);

	if (!archive.readMember("f1omake_01.tga", imageData))
		return Common::Error(Common::kReadingFailed, "Unable to read f1omake_01.tga from sb2_007.pp");
	Common::MemoryReadStream logoStream(imageData.data(), imageData.size());
	if (!startupLogo.loadStream(logoStream))
		return Common::Error(Common::kReadingFailed, "Invalid f1omake_01.tga in sb2_007.pp");
	debug(1, "Illusion startup image: f1omake_01.tga (%dx%d)",
		startupLogo.getSurface()->w, startupLogo.getSurface()->h);

	return Common::kNoError;
}

Common::Error IllusionEngine::loadMenuResources() {
	Image::BitmapDecoder background;
	Image::TGADecoder logo;
	Image::TGADecoder buttons[5][2];
	if (!_soundArchive.openEmbedded("Data/sb2_032.pp"))
		return Common::Error(Common::kReadingFailed, "Unable to open title audio archive");
	IllusionArchive archive;
	if (!archive.open("Data/sb2_029.pp", "Data/sb2_029.tbl"))
		return Common::Error(Common::kReadingFailed, "Unable to open title menu archive");

	Common::Array<byte> imageData;
	if (!archive.readMember("taitoru.bmp", imageData))
		return Common::Error(Common::kReadingFailed, "Unable to read taitoru.bmp");
	Common::MemoryReadStream backgroundStream(imageData.data(), imageData.size());
	if (!background.loadStream(backgroundStream))
		return Common::Error(Common::kReadingFailed, "Invalid taitoru.bmp");
	if (!archive.readMember("taitoru04.tga", imageData))
		return Common::Error(Common::kReadingFailed, "Unable to read taitoru04.tga");
	Common::MemoryReadStream logoStream(imageData.data(), imageData.size());
	if (!logo.loadStream(logoStream))
		return Common::Error(Common::kReadingFailed, "Invalid taitoru04.tga");

	const uint buttonIds[] = { 0, 1, 4, 2, 3 };
	for (uint button = 0; button < ARRAYSIZE(buttonIds); ++button) {
		for (uint variant = 0; variant < 2; ++variant) {
			const Common::String name = Common::String::format("taitoru%02u_%02u.tga", buttonIds[button], variant);
			if (!archive.readMember(name, imageData))
				return Common::Error(Common::kReadingFailed, "Unable to read " + name);
			Common::MemoryReadStream buttonStream(imageData.data(), imageData.size());
			if (!buttons[button][variant].loadStream(buttonStream))
				return Common::Error(Common::kReadingFailed, "Invalid " + name);
		}
	}
	debug(1, "Illusion: decoded title background, logo and five menu button pairs");
	_menuImages[0] = _renderer->createImage(*background.getSurface(), background.getPalette(), true);
	_menuImages[1] = _renderer->createImage(*logo.getSurface(), logo.getPalette());
	for (uint button = 0; button < 5; ++button) {
		for (uint variant = 0; variant < 2; ++variant)
			_menuImages[2 + button * 2 + variant] = _renderer->createImage(
				*buttons[button][variant].getSurface(), buttons[button][variant].getPalette());
	}
	for (uint index = 0; index < ARRAYSIZE(_menuImages); ++index) {
		if (!_menuImages[index])
			return Common::Error(Common::kReadingFailed, "Unable to upload title menu image");
	}
	return Common::kNoError;
}

Common::Error IllusionEngine::initCharacterSelection() {
	clearSceneImages();
	const Common::Error result = loadCharacterSelection();
	if (result.getCode() != Common::kNoError)
		clearSceneImages();
	return result;
}

void IllusionEngine::clearSceneImages() {
	for (uint index = 0; index < _sceneImages.size(); ++index)
		_renderer->destroyImage(_sceneImages[index]);
	_sceneImages.clear();
}

Common::Error IllusionEngine::loadCharacterSelection() {
	IllusionArchive archive;
	if (!archive.open("Data/sb2_000.pp", "Data/sb2_000.tbl"))
		return Common::Error(Common::kReadingFailed, "Unable to open character-selection archive");
	Common::Array<byte> data;
	if (!archive.readMember("character.bmp", data))
		return Common::Error(Common::kReadingFailed, "Unable to read character.bmp");
	Common::MemoryReadStream backgroundStream(data.data(), data.size());
	Image::BitmapDecoder background;
	if (!background.loadStream(backgroundStream))
		return Common::Error(Common::kReadingFailed, "Invalid character.bmp");
	const uint backgroundImage = _renderer->createImage(*background.getSurface(), background.getPalette());
	if (!backgroundImage)
		return Common::Error(Common::kReadingFailed, "Unable to upload character.bmp");
	_sceneImages.push_back(backgroundImage);

	const char *const names[] = {
		"select00.tga", "select01.tga", "kao00.tga", "kyarasen.tga",
		"botan00_00.tga", "botan00_01.tga", "botan01_00.tga", "botan01_01.tga",
		"kyarawaku.tga", "profile00.tga"
	};
	for (uint index = 0; index < ARRAYSIZE(names) + 7; ++index) {
		const Common::String name = index < ARRAYSIZE(names) ? Common::String(names[index]) :
			Common::String::format("time%02u.tga", index - ARRAYSIZE(names));
		if (!archive.readMember(name, data))
			return Common::Error(Common::kReadingFailed, "Unable to read " + name);
		Common::MemoryReadStream stream(data.data(), data.size());
		Image::TGADecoder image;
		if (!image.loadStream(stream))
			return Common::Error(Common::kReadingFailed, "Invalid " + name);
		const uint handle = _renderer->createImage(*image.getSurface(), image.getPalette());
		if (!handle)
			return Common::Error(Common::kReadingFailed, "Unable to upload " + name);
		_sceneImages.push_back(handle);
	}
	playSound("BG0002", true);
	debug(1, "Illusion: initialized %u character-selection images for scene state 2", _sceneImages.size());
	return Common::kNoError;
}

void IllusionEngine::playSound(const Common::String &name, bool loop) {
	Common::Array<byte> data;
	if (!_soundArchive.readMember(name + ".wav", data)) {
		warning("Illusion: unable to read sound %s", name.c_str());
		return;
	}
	Common::MemoryReadStream memory(data.data(), data.size());
	Common::SeekableReadStream *stream = memory.readStream(data.size());
	if (!stream)
		return;
	Audio::SeekableAudioStream *sound = Audio::makeWAVStream(stream, DisposeAfterUse::YES);
	if (!sound) {
		warning("Illusion: invalid sound %s", name.c_str());
		return;
	}
	Audio::SoundHandle &handle = loop ? _musicHandle : _effectHandle;
	_mixer->stopHandle(handle);
	_mixer->playStream(loop ? Audio::Mixer::kMusicSoundType : Audio::Mixer::kSFXSoundType,
		&handle, loop ? Audio::makeLoopingAudioStream(sound, 0) : sound);
	debug(1, "Illusion: playing %s%s", name.c_str(), loop ? " (looping)" : "");
}

Common::Rect IllusionEngine::menuRectangle(int x, int y, int width, int height) const {
	const int screenWidth = _rendererState.viewport.width();
	const int screenHeight = _rendererState.viewport.height();
	return Common::Rect(x * screenWidth / 1024, y * screenHeight / 768,
		(x + width) * screenWidth / 1024, (y + height) * screenHeight / 768);
}

void IllusionEngine::drawMenu() {
	float opacity = 1.0f;
	if (_startupPhase == kMenuBackground)
		opacity = MIN(1.0f, _startupElapsed / 1000.0f);
	else if (_startupPhase == kMenuLeaving)
		opacity = MAX(0.0f, 1.0f - _startupElapsed / 1000.0f);
	_renderer->drawBackground(_menuImages[0], opacity);
	_renderer->drawImage(_menuImages[1], menuRectangle(0, 0, 560, 256), opacity);
	if (_startupPhase != kMenuBackground && _startupPhase != kMoviePending) {
		for (uint button = 0; button < 5; ++button) {
			float buttonOpacity = opacity;
			int x = 731;
			if (_startupPhase == kMenuButtons) {
				const int elapsed = (int)_startupElapsed - (int)button * 100;
				if (elapsed <= 0)
					continue;
				buttonOpacity = MIN(1.0f, elapsed / 500.0f);
				x = 1016 - 285 * MIN(elapsed, 200) / 200;
			}
			const uint variant = _startupPhase == kMenuReady && _menuSelection == (int)button ? 0 : 1;
			_renderer->drawImage(_menuImages[2 + button * 2 + variant],
				menuRectangle(x, kMenuButtonY[button], 285, 67), buttonOpacity);
		}
	}
	_renderer->flipBuffer();
}

Common::Error IllusionEngine::runStartup() {
	uint32 previousTime = _system->getMillis();
	while (!shouldQuit() && !_inputState.quitRequested) {
		pumpEvents();
		if (shouldQuit() || _inputState.quitRequested)
			break;
		if (_inputState.keysPressed[Common::KEYCODE_ESCAPE])
			return Common::kNoError;
		const uint32 now = _system->getMillis();
		if (_startupPhase != kMenuReady)
			_startupElapsed += MIN<uint32>(100, now - previousTime);
		previousTime = now;
		const int previousSelection = _menuSelection;
		const StartupPhase previousPhase = _startupPhase;
		bool redraw = _startupPhase != kMenuReady || _inputState.redrawRequested;
		switch (_startupPhase) {
		case kMoviePending:
			debug(1, "Illusion: startup movie playback deferred; entering title menu");
			playSound("BG0000", true);
			_startupPhase = kMenuBackground;
			_startupElapsed = 0;
			break;
		case kMenuBackground:
			if (_startupElapsed >= 1000) {
				_startupPhase = kMenuButtons;
				_startupElapsed = 0;
				debug(1, "Illusion: title background reveal complete; revealing menu buttons");
			}
			break;
		case kMenuButtons:
			if (_startupElapsed >= 900) {
				_startupPhase = kMenuReady;
				_startupElapsed = 0;
				debug(1, "Illusion: title menu ready");
			}
			break;
		case kMenuReady:
			if (_inputState.mouseDelta != Common::Point(0, 0) || _inputState.mouseButtonsPressed) {
				_menuSelection = -1;
				for (uint button = 0; button < 5; ++button) {
					if (menuRectangle(731, kMenuButtonY[button], 285, 67).contains(_inputState.mousePosition)) {
						_menuSelection = button;
						break;
					}
				}
			}
			if (_inputState.keysPressed[Common::KEYCODE_DOWN])
				_menuSelection = (_menuSelection + 1) % 5;
			if (_inputState.keysPressed[Common::KEYCODE_UP])
				_menuSelection = _menuSelection < 0 ? 4 : (_menuSelection + 4) % 5;
			if (_menuSelection >= 0 && ((_inputState.mouseButtonsPressed & kMouseLeftMask) ||
					_inputState.keysPressed[Common::KEYCODE_RETURN] || _inputState.keysPressed[Common::KEYCODE_KP_ENTER])) {
				_menuAction = _menuSelection;
				playSound("SE0000", false);
				_startupPhase = kMenuLeaving;
				_startupElapsed = 0;
				debug(1, "Illusion: title menu action %d selected", _menuAction);
			}
			break;
		case kMenuLeaving:
			if (_startupElapsed >= 1000) {
				if (_menuAction == 4)
					return Common::kNoError;
				const uint sceneStates[] = { 2, 0xc, 0x11, 0xd };
				debug(1, "Illusion: title menu requests scene state %u", sceneStates[_menuAction]);
				if (_menuAction == 0) {
					const Common::Error result = initCharacterSelection();
					if (result.getCode() != Common::kNoError)
						return result;
					return Common::Error(Common::kUnsupportedGameidError,
						"Character-selection resources are initialized, but scene interaction is not implemented yet");
				}
				return Common::Error(Common::kUnsupportedGameidError,
					"The selected title-menu scene is not implemented yet");
			}
			break;
		}
		redraw = redraw || _startupPhase != previousPhase || _menuSelection != previousSelection;
		if (redraw)
			drawMenu();
		else
			_system->updateScreen();
		_system->delayMillis(16);
	}
	return Common::kNoError;
}

void IllusionEngine::pumpEvents() {
	_inputState.mouseDelta = Common::Point(0, 0);
	_inputState.mouseWheelDelta = 0;
	_inputState.mouseButtonsPressed = 0;
	_inputState.redrawRequested = false;
	for (uint key = 0; key < ARRAYSIZE(_inputState.keysPressed); ++key)
		_inputState.keysPressed[key] = false;
	Common::Event event;
	while (_system->getEventManager()->pollEvent(event)) {
		switch (event.type) {
		case Common::EVENT_KEYDOWN: {
			const int keyCode = event.kbd.keycode;
			if (keyCode > Common::KEYCODE_INVALID && keyCode < Common::KEYCODE_LAST) {
				if (!_inputState.keys[keyCode])
					_inputState.keysPressed[keyCode] = true;
				_inputState.keys[keyCode] = true;
			}
			break;
		}
		case Common::EVENT_KEYUP: {
			const int keyCode = event.kbd.keycode;
			if (keyCode > Common::KEYCODE_INVALID && keyCode < Common::KEYCODE_LAST)
				_inputState.keys[keyCode] = false;
			break;
		}
		case Common::EVENT_MOUSEMOVE: {
			const Common::Point previous = _inputState.mousePosition;
			setMousePosition(event.mouse);
			Common::Point relativeDelta = event.relMouse;
			if (relativeDelta.x == 0 && relativeDelta.y == 0)
				relativeDelta = _inputState.mousePosition - previous;
			_inputState.mouseDelta += relativeDelta;
			break;
		}
		case Common::EVENT_LBUTTONDOWN:
			setMousePosition(event.mouse);
			_inputState.mouseButtonsPressed |= kMouseLeftMask;
			_inputState.mouseButtons |= kMouseLeftMask;
			break;
		case Common::EVENT_LBUTTONUP:
			setMousePosition(event.mouse);
			_inputState.mouseButtons &= ~kMouseLeftMask;
			break;
		case Common::EVENT_RBUTTONDOWN:
			setMousePosition(event.mouse);
			_inputState.mouseButtonsPressed |= kMouseRightMask;
			_inputState.mouseButtons |= kMouseRightMask;
			break;
		case Common::EVENT_RBUTTONUP:
			setMousePosition(event.mouse);
			_inputState.mouseButtons &= ~kMouseRightMask;
			break;
		case Common::EVENT_MBUTTONDOWN:
			setMousePosition(event.mouse);
			_inputState.mouseButtonsPressed |= kMouseMiddleMask;
			_inputState.mouseButtons |= kMouseMiddleMask;
			break;
		case Common::EVENT_MBUTTONUP:
			setMousePosition(event.mouse);
			_inputState.mouseButtons &= ~kMouseMiddleMask;
			break;
		case Common::EVENT_X1BUTTONDOWN:
			setMousePosition(event.mouse);
			_inputState.mouseButtonsPressed |= kMouseX1Mask;
			_inputState.mouseButtons |= kMouseX1Mask;
			break;
		case Common::EVENT_X1BUTTONUP:
			setMousePosition(event.mouse);
			_inputState.mouseButtons &= ~kMouseX1Mask;
			break;
		case Common::EVENT_WHEELUP:
			_inputState.mouseWheelDelta += 120;
			break;
		case Common::EVENT_WHEELDOWN:
			_inputState.mouseWheelDelta -= 120;
			break;
		case Common::EVENT_FOCUS_LOST:
			for (uint key = 0; key < ARRAYSIZE(_inputState.keys); ++key) {
				_inputState.keys[key] = false;
				_inputState.keysPressed[key] = false;
			}
			_inputState.mouseButtons = 0;
			_inputState.mouseButtonsPressed = 0;
			break;
		case Common::EVENT_FOCUS_GAINED:
		case Common::EVENT_SCREEN_CHANGED:
			_inputState.redrawRequested = true;
			break;
		case Common::EVENT_QUIT:
			_inputState.quitRequested = true;
			break;
		default:
			break;
		}
	}
}

void IllusionEngine::setMousePosition(const Common::Point &position) {
	const int maxX = MAX(0, _rendererState.viewport.width() - 1);
	const int maxY = MAX(0, _rendererState.viewport.height() - 1);
	_inputState.mousePosition.x = CLIP((int)position.x, 0, maxX);
	_inputState.mousePosition.y = CLIP((int)position.y, 0, maxY);
}

Common::Error IllusionEngine::run() {
	Common::Error result = validateGameData();
	if (result.getCode() != Common::kNoError)
		return result;

	result = initDisplay();
	if (result.getCode() != Common::kNoError)
		return result;
	initRendererState();
	_renderer = new IllusionRenderer(_system);
	if (!_renderer->init(_rendererState.viewport.width(), _rendererState.viewport.height(),
			_system->getScreenFormat(), _rendererState))
		return Common::Error(Common::kUnsupportedColorMode, "Unable to initialize TinyGL renderer");
	result = loadStartupResources();
	if (result.getCode() != Common::kNoError)
		return result;
	result = loadMenuResources();
	if (result.getCode() != Common::kNoError)
		return result;
	_renderer->clearScreen();
	_renderer->flipBuffer();

	setDebugger(new Console());
	syncSoundSettings();

	debug(1, "Illusion: %dx%d, %u bytes per pixel; game data present",
		_rendererState.viewport.width(), _rendererState.viewport.height(), (uint)_system->getScreenFormat().bytesPerPixel);
	debug(1, "Illusion renderer: viewport %dx%d, center %d,%d, UI scale %.2f",
		_rendererState.viewport.width(), _rendererState.viewport.height(),
		_rendererState.screenCenter.x, _rendererState.screenCenter.y, _rendererState.uiScale);
	debug(1, "Illusion projection: aspect %.2f, FOV %.2f, near %.1f, far %.0f; RGB masks %08x/%08x/%08x",
		_rendererState.projectionAspect, _rendererState.projectionFieldOfView,
		_rendererState.projectionNear, _rendererState.projectionFar,
		_rendererState.colorChannels[0].mask, _rendererState.colorChannels[1].mask,
		_rendererState.colorChannels[2].mask);

	return runStartup();
}

} // End of namespace Illusion
