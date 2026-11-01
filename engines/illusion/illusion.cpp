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
	_fadeImage(0), _pointerImage(0), _startupPhase(kMoviePending), _startupElapsed(0), _menuSelection(0),
	_menuAction(-1), _linearFiltering(false), _day(1), _currentGirl(0), _portraitImage(0), _profileImage(0),
	_selectPhase(kSelectFadeIn), _selectElapsed(0), _portraitElapsed(0), _selectedSlot(0), _shownSlot(0),
	_selectButton(0), _selectConfirmed(false), _currentSwimsuit(-1), _suitPhase(kSelectFadeIn), _suitElapsed(0),
	_suitClock(0), _suitPage(0), _suitButton(0), _suitConfirmed(false), _suitDirty(true),
	_suitArchivesOpen(false) {
	for (uint index = 0; index < ARRAYSIZE(_menuImages); ++index)
		_menuImages[index] = 0;
	for (uint index = 0; index < ARRAYSIZE(_selectImages); ++index)
		_selectImages[index] = 0;
	for (uint index = 0; index < ARRAYSIZE(_suitImages); ++index)
		_suitImages[index] = 0;
	resetNewGame();
	g_engine = this;
}

IllusionEngine::~IllusionEngine() {
	_mixer->stopHandle(_musicHandle);
	_mixer->stopHandle(_effectHandle);
	if (_renderer)
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
	// The white fader is stretched over the screen for scene fades.
	_fadeImage = _renderer->createImage(*fadeBackground.getSurface(), fadeBackground.getPalette());
	if (!_fadeImage)
		return Common::Error(Common::kReadingFailed, "Unable to upload fadeW.bmp");

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

	// The game draws this pointer instead of the system cursor in every 2D scene.
	if (!archive.readMember("icon_17.tga", imageData))
		return Common::Error(Common::kReadingFailed, "Unable to read icon_17.tga from sb2_007.pp");
	Common::MemoryReadStream pointerStream(imageData.data(), imageData.size());
	Image::TGADecoder pointer;
	if (!pointer.loadStream(pointerStream))
		return Common::Error(Common::kReadingFailed, "Invalid icon_17.tga in sb2_007.pp");
	_pointerImage = _renderer->createImage(*pointer.getSurface(), pointer.getPalette());
	if (!_pointerImage)
		return Common::Error(Common::kReadingFailed, "Unable to upload icon_17.tga");

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
	clearSwimsuitModel();
	_suitModelArchive.close();
	_suitTextureArchive.close();
	_suitArchivesOpen = false;
	for (uint index = 0; index < _sceneImages.size(); ++index)
		_renderer->destroyImage(_sceneImages[index]);
	_sceneImages.clear();
	_renderer->invalidateScene();
	_selectArchive.close();
	_renderer->destroyImage(_portraitImage);
	_renderer->destroyImage(_profileImage);
	_portraitImage = 0;
	_profileImage = 0;
	for (uint index = 0; index < ARRAYSIZE(_selectImages); ++index)
		_selectImages[index] = 0;
	for (uint index = 0; index < ARRAYSIZE(_suitImages); ++index)
		_suitImages[index] = 0;
}

uint IllusionEngine::loadSceneImage(IllusionArchive &archive, const Common::String &name, bool keepBackgroundCopy) {
	Common::Array<byte> data;
	if (!archive.readMember(name, data)) {
		warning("Illusion: unable to read %s", name.c_str());
		return 0;
	}
	Common::MemoryReadStream stream(data.data(), data.size());
	uint handle = 0;
	if (name.hasSuffixIgnoreCase(".bmp")) {
		Image::BitmapDecoder image;
		if (image.loadStream(stream))
			handle = _renderer->createImage(*image.getSurface(), image.getPalette(), keepBackgroundCopy);
	} else {
		Image::TGADecoder image;
		if (image.loadStream(stream))
			handle = _renderer->createImage(*image.getSurface(), image.getPalette());
	}
	if (!handle)
		warning("Illusion: invalid image %s", name.c_str());
	else
		_sceneImages.push_back(handle);
	return handle;
}

// Selection slots map to the girl numbers used in resource names.
static int girlForSlot(int slot) {
	static const int girls[] = { 0, 1, 2, 5, 7, 8 };
	return slot >= 0 && slot < (int)ARRAYSIZE(girls) ? girls[slot] : 0;
}

static int slotForGirl(int girl) {
	for (int slot = 0; slot < 6; ++slot) {
		if (girlForSlot(slot) == girl)
			return slot;
	}
	return 0;
}

// Grid cells of kyarawaku.tga, in 1024x768 layout units: three cells on each of two staggered rows.
static Common::Rect selectionCell(int slot) {
	const int x = slot < 3 ? 598 + slot * 102 : 649 + (slot - 3) * 102;
	const int y = slot < 3 ? 435 : 551;
	return Common::Rect(x, y, x + 104, y + 118);
}

static const int kSelectButtonX = 495;
static const int kSelectButtonY = 699;
static const int kSelectButtonSpacing = 265;
static const int kPortraitSlideDistance = 512;
static const uint32 kPortraitSlideTime = 200;
static const uint32 kPortraitFadeTime = 400;
static const uint32 kSceneFadeTime = 1000;

Common::Error IllusionEngine::loadCharacterSelection() {
	// The archive stays open so that portraits can be reloaded without parsing its table again.
	IllusionArchive &archive = _selectArchive;
	if (!archive.open("Data/sb2_000.pp", "Data/sb2_000.tbl"))
		return Common::Error(Common::kReadingFailed, "Unable to open character-selection archive");

	const char *const names[] = {
		"character.bmp", "select00.tga", "select01.tga", "kyarawaku.tga", "kyarasen.tga",
		"botan00_00.tga", "botan00_01.tga", "botan01_00.tga", "botan01_01.tga"
	};
	for (uint index = 0; index < ARRAYSIZE(names); ++index)
		_selectImages[kSelectBackground + index] = loadSceneImage(archive, names[index], index == 0);
	for (uint index = 0; index < 7; ++index)
		_selectImages[kSelectBadge + index] = loadSceneImage(archive, Common::String::format("time%02u.tga", index));

	IllusionArchive hud;
	if (!hud.open("Data/sb2_028.pp", "Data/sb2_028.tbl"))
		return Common::Error(Common::kReadingFailed, "Unable to open day display archive");
	for (uint index = 0; index < 10; ++index)
		_selectImages[kSelectDayDigit + index] = loadSceneImage(hud, Common::String::format("time_moji%02u.tga", index));
	for (uint index = 0; index < 6; ++index)
		_selectImages[kSelectTimeDigit + index] = loadSceneImage(hud, Common::String::format("time2_moji%02u.tga", index + 1));
	for (uint index = 0; index < 9; ++index)
		_selectImages[kSelectName + index] = loadSceneImage(hud, Common::String::format("time03_%02u.tga", index + 1));

	for (uint index = 0; index < kSelectBadge; ++index) {
		if (!_selectImages[index])
			return Common::Error(Common::kReadingFailed, "Unable to load the character-selection images");
	}

	_selectedSlot = slotForGirl(_currentGirl);
	_shownSlot = _selectedSlot;
	const Common::Error result = loadCharacterPortrait(_shownSlot);
	if (result.getCode() != Common::kNoError)
		return result;
	_portraitElapsed = kPortraitFadeTime;
	_selectButton = 0;
	_selectConfirmed = false;
	_selectPhase = kSelectFadeIn;
	_selectElapsed = 0;
	// Entering the character selection forgets the swimsuit chosen for the previous girl.
	_currentSwimsuit = -1;

	// The game starts with the pointer on the confirm button.
	const Common::Rect ok = menuRectangle(kSelectButtonX, kSelectButtonY, 248, 59);
	_system->warpMouse((ok.left + ok.right) / 2, (ok.top + ok.bottom) / 2);
	setMousePosition(Common::Point((ok.left + ok.right) / 2, (ok.top + ok.bottom) / 2));
	debug(1, "Illusion: initialized %u character-selection images for scene state 2", _sceneImages.size());
	return Common::kNoError;
}

Common::Error IllusionEngine::loadCharacterPortrait(int slot) {
	IllusionArchive &archive = _selectArchive;
	const int girl = girlForSlot(slot);
	const uint portrait = loadSceneImage(archive, Common::String::format("kao%02d.tga", girl));
	const uint profile = loadSceneImage(archive, Common::String::format("profile%02d.tga", girl));
	// The portrait and profile are owned separately so a new selection can replace them.
	for (uint index = 0; index < _sceneImages.size();) {
		if (_sceneImages[index] == portrait || _sceneImages[index] == profile)
			_sceneImages.remove_at(index);
		else
			++index;
	}
	if (!portrait || !profile) {
		_renderer->destroyImage(portrait);
		_renderer->destroyImage(profile);
		return Common::Error(Common::kReadingFailed, "Unable to load the character portrait");
	}
	_renderer->destroyImage(_portraitImage);
	_renderer->destroyImage(_profileImage);
	_portraitImage = portrait;
	_profileImage = profile;
	return Common::kNoError;
}

void IllusionEngine::updateCharacterSelection(uint32 delta) {
	_selectElapsed += delta;
	if (_selectPhase == kSelectFadeIn) {
		if (_selectElapsed >= kSceneFadeTime) {
			_selectPhase = kSelectReady;
			_selectElapsed = 0;
		}
		return;
	}
	if (_selectPhase == kSelectFadeOut)
		return;

	_portraitElapsed = MIN(_portraitElapsed + delta, kPortraitFadeTime);
	const bool clicked = (_inputState.mouseButtonsPressed & kMouseLeftMask) != 0;

	// Both groups keep their last highlighted entry while the pointer is outside them.
	for (int button = 0; button < 2; ++button) {
		const Common::Rect rect = menuRectangle(kSelectButtonX + button * kSelectButtonSpacing, kSelectButtonY, 248, 59);
		if (!rect.contains(_inputState.mousePosition))
			continue;
		if (_selectButton != button) {
			_selectButton = button;
			playSound("SE0004", false);
		}
		if (clicked) {
			_selectConfirmed = button == 0;
			playSound(_selectConfirmed ? "SE0000" : "SE0002", false);
			_selectPhase = kSelectFadeOut;
			_selectElapsed = 0;
			if (_selectConfirmed)
				_currentGirl = girlForSlot(_shownSlot);
			return;
		}
	}

	if (!clicked)
		return;
	for (int slot = 0; slot < 6; ++slot) {
		const Common::Rect cell = selectionCell(slot);
		if (!menuRectangle(cell.left, cell.top, cell.width(), cell.height()).contains(_inputState.mousePosition))
			continue;
		playSound("SE0003", false);
		_selectedSlot = slot;
		_renderer->invalidateScene();
		if (slot != _shownSlot) {
			if (loadCharacterPortrait(slot).getCode() == Common::kNoError) {
				_shownSlot = slot;
				_portraitElapsed = 0;
			}
		}
		break;
	}
}

void IllusionEngine::drawCharacterSelection() {
	// Once the portrait has settled the scene below the buttons is static, so it is drawn only once.
	const bool settled = _portraitElapsed >= kPortraitFadeTime;
	if (!settled || !_renderer->restoreScene()) {
		_renderer->drawBackground(_selectImages[kSelectBackground], 1.0f);

		// A new portrait slides in from the left while it fades in.
		const int slide = _portraitElapsed >= kPortraitSlideTime ? 0 :
			-kPortraitSlideDistance + kPortraitSlideDistance * (int)_portraitElapsed / (int)kPortraitSlideTime;
		_renderer->drawImage(_portraitImage, menuRectangle(slide, 0, 1024, 768), _portraitElapsed / (float)kPortraitFadeTime);
		_renderer->drawImage(_selectImages[kSelectTitle], menuRectangle(388, 30, 420, 56));
		_renderer->drawImage(_selectImages[kSelectProfileLabel], menuRectangle(680, 165, 160, 72));

		drawDayDisplay(_selectImages + kSelectDayDigit, _selectImages + kSelectTimeDigit,
			_selectImages + kSelectName + girlForSlot(_selectedSlot));

		_renderer->drawImage(_profileImage, menuRectangle(495, 235, 513, 190));
		_renderer->drawImage(_selectImages[kSelectGrid], menuRectangle(494, 433, 516, 238));
		const Common::Rect cell = selectionCell(_selectedSlot);
		_renderer->drawImage(_selectImages[kSelectFrame], menuRectangle(cell.left, cell.top, cell.width(), cell.height()));

		// The game shows each girl's day count or "Clear"; without saves every girl is on day 1.
		for (int slot = 0; slot < 6; ++slot) {
			const int x = slot < 3 ? 626 + slot * 102 : 677 + (slot - 3) * 102;
			const int y = slot < 3 ? 527 : 643;
			_renderer->drawImage(_selectImages[kSelectBadge + 1], menuRectangle(x, y, 48, 20));
		}
		if (settled)
			_renderer->cacheScene();
	}

	// The buttons do not overlap the layers above, so they can follow them.
	for (int button = 0; button < 2; ++button) {
		const bool highlighted = _selectPhase == kSelectReady && _selectButton == button;
		const uint image = _selectImages[(button ? kSelectBackOn : kSelectOkOn) + (highlighted ? 0 : 1)];
		_renderer->drawImage(image, menuRectangle(kSelectButtonX + button * kSelectButtonSpacing, kSelectButtonY, 248, 59));
	}

	if (_selectPhase == kSelectReady)
		drawPointer();
	float fade = 0.0f;
	if (_selectPhase == kSelectFadeIn)
		fade = 1.0f - MIN(1.0f, _selectElapsed / (float)kSceneFadeTime);
	else if (_selectPhase == kSelectFadeOut)
		fade = MIN(1.0f, _selectElapsed / (float)kSceneFadeTime);
	_renderer->drawFadeToWhite(_fadeImage, fade);
	_renderer->flipBuffer();
}

void IllusionEngine::drawDayDisplay(const uint *dayDigits, const uint *timeDigits, const uint *name) {
	// Day counter and time-of-day slot of the girl; a new game starts at day 1, slot 1.
	_renderer->drawImage(timeDigits[0], menuRectangle(977, 70, 19, 29));
	_renderer->drawImage(dayDigits[(_day / 10) % 10], menuRectangle(833, 26, 29, 35));
	_renderer->drawImage(dayDigits[_day % 10], menuRectangle(853, 26, 29, 35));
	_renderer->drawImage(*name, menuRectangle(875, 74, 84, 26));
}

void IllusionEngine::drawPointer() {
	const Common::Rect size = menuRectangle(0, 0, 64, 64);
	_renderer->drawImage(_pointerImage, Common::Rect(_inputState.mousePosition.x, _inputState.mousePosition.y,
		_inputState.mousePosition.x + size.width(), _inputState.mousePosition.y + size.height()));
}

Common::Error IllusionEngine::runCharacterSelection(bool &confirmed) {
	confirmed = false;
	uint32 previousTime = _system->getMillis();
	bool redraw = true;
	while (!shouldQuit() && !_inputState.quitRequested) {
		const uint32 frameStart = _system->getMillis();
		pumpEvents();
		if (shouldQuit() || _inputState.quitRequested)
			break;
		const uint32 now = _system->getMillis();
		const uint32 delta = MIN<uint32>(100, now - previousTime);
		previousTime = now;
		const CharacterSelectPhase previousPhase = _selectPhase;
		const bool wasAnimating = _portraitElapsed < kPortraitFadeTime;
		updateCharacterSelection(delta);
		if (_selectPhase == kSelectFadeOut && _selectElapsed >= kSceneFadeTime) {
			clearSceneImages();
			confirmed = _selectConfirmed;
			if (confirmed)
				debug(1, "Illusion: girl %d chosen", _currentGirl);
			return Common::kNoError;
		}
		// A portrait that was just loaded starts its animation now, not after the load time.
		if (_portraitElapsed == 0 && delta != 0)
			previousTime = _system->getMillis();
		// Nothing changes on screen while idle, so only redraw when something moved or is animating.
		redraw = redraw || _selectPhase != kSelectReady || _selectPhase != previousPhase || wasAnimating ||
			_portraitElapsed < kPortraitFadeTime || _inputState.mouseDelta != Common::Point(0, 0) ||
			_inputState.mouseButtonsPressed || _inputState.redrawRequested;
		if (redraw)
			drawCharacterSelection();
		redraw = false;
		const uint32 spent = _system->getMillis() - frameStart;
		_system->delayMillis(spent < 16 ? 16 - spent : 1);
	}
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
	if (_startupPhase == kMenuReady)
		drawPointer();
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
					bool returnToTitle = false;
					const Common::Error result = runNewGame(returnToTitle);
					if (!returnToTitle)
						return result;
					playSound("BG0000", true);
					_startupPhase = kMenuBackground;
					_startupElapsed = 0;
					previousTime = _system->getMillis();
					continue;
				}
				return Common::Error(Common::kUnsupportedGameidError,
					"The selected title-menu scene is not implemented yet");
			}
			break;
		}
		redraw = redraw || _startupPhase != previousPhase || _menuSelection != previousSelection ||
			_inputState.mouseDelta != Common::Point(0, 0);
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
