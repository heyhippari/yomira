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
#include "illusion/renderer.h"
#include "common/debug.h"
#include "common/memstream.h"
#include "common/system.h"
#include "image/bmp.h"
#include "image/tga.h"

namespace Illusion {

static const uint32 kFadeTime = 1000;
static const uint32 kNewIconShown = 1500;
static const uint32 kNewIconHidden = 500;
static const int kSuitButtonX = 708;
static const int kSuitButtonY = 626;
static const int kSuitButtonSpacing = 74;
static const int kSuitItemY = 84;
static const int kSuitItemSpacing = 35;

// Camera of the swimsuit preview in Direct3D coordinates, looking along +Z.
static const float kSuitCamera[3] = { -6.3f, 3.7f, -30.0f };

// Swimsuit each girl owns at the start of a new game.
static const int kDefaultSwimsuits[] = { 13, 41, 20, 20, 44, 11, 14, 31, 0 };

// List entries that do not exist for some girls.
static const int kMissingSwimsuits2[] = { 9, 34, 59 };
static const int kMissingSwimsuits5[] = { 7, 8, 15, 16, 17, 25, 26, 30, 39, 40, 54, 55, 57, 58 };
static const int kMissingSwimsuits78[] = { 8, 13, 18, 19, 26, 27, 38, 54, 57 };

// Body types 0-3 are the 'a', 'm', 'b' and 'c' bodies. Girls who share one share the swimsuit
// list images, the preview models and the swimsuit textures.
static const char kBodyLetters[] = { 'a', 'm', 'b', 'c' };

static int swimsuitBodyType(int girl) {
	if (girl <= 1)
		return 0;
	if (girl == 2)
		return 1;
	return girl <= 5 ? 2 : 3;
}

static char swimsuitListLetter(int girl) {
	return kBodyLetters[swimsuitBodyType(girl)];
}

// Preview model number (e20_<type * 3 + model>) of each swimsuit per body type, from the table at
// 0x4eda80 of the executable. -1 and 3 mean that there is none.
static const int8 kSwimsuitModels[4][72] = {
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 3 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 3 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 3 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 3 }
};

// The texture index of a swimsuit equals its number (the second byte of each table pair at
// 0x4eda80). The texture names at 0x4ec840 are <letter><model>_<n>.<ext> with n counting from the
// first swimsuit of that model; this marks the ones stored as .tga ('t') instead of .bmp ('b').
static const char *const kSwimsuitTextureTypes[4] = {
	"bbbbbbbbbbbtbbbtbtttbbtbbbbbbbtbttbbbtbbbbbtbbbbbbbbbbtttttb",
	"bbbbbbbbbbbtbbbtbtttbbbbbbbbbbtbttbbbtbbbbbtbbbbbbbbbbttttbb",
	"bbbtbttttbbbbbtttttbbtttttttbbtbbtbtttttttbbbbbbbbbbbttttttt",
	"bbtbtttbtttbbbbtttttttttbtttttbbtttbbtbttbtbtbbbbbtbtttttttt"
};

static void markMissing(int8 *swimsuits, const int *entries, uint count) {
	for (uint index = 0; index < count; ++index)
		swimsuits[entries[index]] = -1;
}

void IllusionEngine::resetNewGame() {
	_day = 1;
	_currentGirl = 0;
	_currentSwimsuit = -1;
	for (int girl = 0; girl < kGirlCount; ++girl) {
		int8 *swimsuits = _swimsuits[girl];
		for (int index = 0; index < kSwimsuitCount; ++index)
			swimsuits[index] = index < 60 ? kSwimsuitLocked : kSwimsuitMissing;
		if (girl == 2)
			markMissing(swimsuits, kMissingSwimsuits2, ARRAYSIZE(kMissingSwimsuits2));
		else if (girl == 5)
			markMissing(swimsuits, kMissingSwimsuits5, ARRAYSIZE(kMissingSwimsuits5));
		else if (girl == 7 || girl == 8)
			markMissing(swimsuits, kMissingSwimsuits78, ARRAYSIZE(kMissingSwimsuits78));
		swimsuits[kDefaultSwimsuits[girl]] = kSwimsuitOwned;
	}
}

Common::Error IllusionEngine::runNewGame(bool &returnToTitle) {
	returnToTitle = false;
	// Every new game starts from the initial day, girl and swimsuit state, as in the game's new-game init.
	resetNewGame();
	playSound("BG0002", true);
	int step = 0;
	while (!shouldQuit() && !_inputState.quitRequested) {
		bool confirmed = false;
		Common::Error result = Common::kNoError;
		if (step == 0) {
			result = initCharacterSelection();
			if (result.getCode() == Common::kNoError)
				result = runCharacterSelection(confirmed);
		} else {
			result = loadSwimsuitSelection();
			if (result.getCode() == Common::kNoError)
				result = runSwimsuitSelection(confirmed);
			else
				clearSceneImages();
		}
		if (result.getCode() != Common::kNoError || shouldQuit() || _inputState.quitRequested)
			return result;

		if (step == 0 && !confirmed) {
			returnToTitle = true;
			return Common::kNoError;
		}
		if (step == 1 && confirmed) {
			debug(1, "Illusion: girl %d wears swimsuit %d; accessory selection (scene 2, step 2) requested",
				_currentGirl, _currentSwimsuit);
			return Common::Error(Common::kUnsupportedGameidError, "The accessory selection is not implemented yet");
		}
		step = confirmed ? step + 1 : step - 1;
	}
	return Common::kNoError;
}

uint IllusionEngine::loadModelTexture(IllusionArchive &archive, const Common::String &name) {
	Common::Array<byte> data;
	if (!archive.readMember(name, data) || data.empty()) {
		warning("Illusion: unable to read model texture %s", name.c_str());
		return 0;
	}
	Common::MemoryReadStream stream(data.data(), data.size());
	uint handle = 0;
	if (name.hasSuffixIgnoreCase(".bmp")) {
		Image::BitmapDecoder image;
		if (image.loadStream(stream))
			handle = _renderer->createTexture(*image.getSurface(), image.getPalette());
	} else {
		Image::TGADecoder image;
		if (image.loadStream(stream))
			handle = _renderer->createTexture(*image.getSurface(), image.getPalette());
	}
	if (!handle)
		warning("Illusion: invalid model texture %s", name.c_str());
	return handle;
}

void IllusionEngine::clearSwimsuitModel() {
	if (_renderer) {
		for (uint index = 0; index < _suitTextures.size(); ++index)
			_renderer->destroyTexture(_suitTextures[index]);
	}
	_suitTextures.clear();
	_suitModel.clear();
}

// Loads the preview model of the current swimsuit (FUN_00484aa3) and starts its turntable clip.
void IllusionEngine::loadSwimsuitModel() {
	clearSwimsuitModel();
	if (!_suitArchivesOpen || _currentSwimsuit < 0 || _currentSwimsuit >= kSwimsuitCount)
		return;
	const int type = swimsuitBodyType(_currentGirl);
	const int model = kSwimsuitModels[type][_currentSwimsuit];
	if (model < 0 || model > 2)
		return;

	// Texture name: letter, model number and the number among the swimsuits of that model.
	int first = _currentSwimsuit;
	while (first > 0 && kSwimsuitModels[type][first - 1] == model)
		--first;
	const Common::String texture = Common::String::format("%c%02d_%02d.%s", kBodyLetters[type], model,
		_currentSwimsuit - first, kSwimsuitTextureTypes[type][_currentSwimsuit] == 't' ? "tga" : "bmp");

	const Common::String base = Common::String::format("e20_%02d", type * 3 + model);
	Common::Array<byte> data;
	if (!_suitModelArchive.readMember(base + ".xan", data) || !_suitClips.load(data.data(), data.size())) {
		warning("Illusion: unable to load %s.xan", base.c_str());
		return;
	}
	const Common::String file = _suitClips.getModelFile().empty() ? base + ".x" : _suitClips.getModelFile();
	if (!_suitModelArchive.readMember(file, data) || !_suitModel.load(data.data(), data.size())) {
		warning("Illusion: unable to load %s", file.c_str());
		_suitModel.clear();
		return;
	}
	_suitModel.replaceTexture("zzz.bmp", texture);

	const Common::Array<Model::Texture> &textures = _suitModel.getTextures();
	for (uint index = 0; index < textures.size(); ++index) {
		const Common::String &name = textures[index].file;
		_suitTextures.push_back(loadModelTexture(name == texture ? _suitTextureArchive : _suitModelArchive, name));
	}
	_suitPlayer.play(_suitClips, 0);
	_suitModel.setTime(_suitPlayer.getTime());
	debug(1, "Illusion: swimsuit preview %s with %s", file.c_str(), texture.c_str());
}

Common::Error IllusionEngine::loadSwimsuitSelection() {
	IllusionArchive archive;
	if (!archive.open("Data/sb2_001.pp", "Data/sb2_001.tbl"))
		return Common::Error(Common::kReadingFailed, "Unable to open swimsuit-selection archive");
	_suitImages[kSuitBackground] = loadSceneImage(archive, "swimsuit.bmp", true);
	_suitImages[kSuitStanding] = loadSceneImage(archive, Common::String::format("tati%02d.tga", _currentGirl));
	_suitImages[kSuitFrame] = loadSceneImage(archive, "waku.tga");
	_suitImages[kSuitNew] = loadSceneImage(archive, "new.tga");
	for (int page = 0; page < 4; ++page) {
		_suitImages[kSuitUnknown + page] = loadSceneImage(archive, Common::String::format("hatena%02d.tga", page + 1));
		_suitImages[kSuitList + page] = loadSceneImage(archive,
			Common::String::format("list_%c%02d.tga", swimsuitListLetter(_currentGirl), page + 1));
	}

	IllusionArchive buttons;
	if (!buttons.open("Data/sb2_000.pp", "Data/sb2_000.tbl"))
		return Common::Error(Common::kReadingFailed, "Unable to open character-selection archive");
	const char *const buttonNames[] = { "botan00_00.tga", "botan00_01.tga", "botan01_00.tga", "botan01_01.tga" };
	for (uint index = 0; index < ARRAYSIZE(buttonNames); ++index)
		_suitImages[kSuitOkOn + index] = loadSceneImage(buttons, buttonNames[index]);

	IllusionArchive hud;
	if (!hud.open("Data/sb2_028.pp", "Data/sb2_028.tbl"))
		return Common::Error(Common::kReadingFailed, "Unable to open day display archive");
	for (uint index = 0; index < 10; ++index)
		_suitImages[kSuitDayDigit + index] = loadSceneImage(hud, Common::String::format("time_moji%02u.tga", index));
	for (uint index = 0; index < 6; ++index)
		_suitImages[kSuitTimeDigit + index] = loadSceneImage(hud, Common::String::format("time2_moji%02u.tga", index + 1));
	for (uint index = 0; index < 9; ++index)
		_suitImages[kSuitName + index] = loadSceneImage(hud, Common::String::format("time03_%02u.tga", index + 1));

	for (uint index = 0; index < kSuitImageCount; ++index) {
		if (!_suitImages[index])
			return Common::Error(Common::kReadingFailed, "Unable to load the swimsuit-selection images");
	}

	// The preview models and the swimsuit textures stay open for swimsuit changes.
	_suitArchivesOpen = _suitModelArchive.open("Data/sb2_006.pp", "Data/sb2_006.tbl") &&
		_suitTextureArchive.open("Data/sb2_027.pp", "Data/sb2_027.tbl");
	if (!_suitArchivesOpen)
		warning("Illusion: unable to open the swimsuit model archives");

	if (_currentSwimsuit < 0)
		_currentSwimsuit = kDefaultSwimsuits[_currentGirl];
	_suitPage = _currentSwimsuit / kSwimsuitsPerPage;
	_suitButton = 0;
	_suitConfirmed = false;
	_suitPhase = kSelectFadeIn;
	_suitElapsed = 0;
	_suitClock = 0;
	_suitDirty = true;
	loadSwimsuitModel();

	const Common::Rect ok = menuRectangle(kSuitButtonX, kSuitButtonY, 248, 59);
	_system->warpMouse((ok.left + ok.right) / 2, (ok.top + ok.bottom) / 2);
	setMousePosition(Common::Point((ok.left + ok.right) / 2, (ok.top + ok.bottom) / 2));
	debug(1, "Illusion: swimsuit selection for girl %d, swimsuit %d", _currentGirl, _currentSwimsuit);
	return Common::kNoError;
}

void IllusionEngine::updateSwimsuitSelection(uint32 delta) {
	_suitElapsed += delta;
	_suitClock = (_suitClock + delta) % (kNewIconShown + kNewIconHidden);
	if (_suitModel.isLoaded()) {
		// The game steps the preview with 0.002 s per millisecond, which makes a turn last 5 s.
		_suitPlayer.update(delta * 0.002f);
		_suitModel.setTime(_suitPlayer.getTime());
	}
	if (_suitPhase == kSelectFadeIn) {
		if (_suitElapsed >= kFadeTime) {
			_suitPhase = kSelectReady;
			_suitElapsed = 0;
		}
		return;
	}
	if (_suitPhase == kSelectFadeOut)
		return;

	const bool clicked = (_inputState.mouseButtonsPressed & kMouseLeftMask) != 0;
	for (int button = 0; button < 2; ++button) {
		const Common::Rect rect = menuRectangle(kSuitButtonX, kSuitButtonY + button * kSuitButtonSpacing, 248, 59);
		if (!rect.contains(_inputState.mousePosition))
			continue;
		if (_suitButton != button) {
			_suitButton = button;
			playSound("SE0004", false);
		}
		if (clicked) {
			_suitConfirmed = button == 0;
			playSound(_suitConfirmed ? "SE0000" : "SE0002", false);
			_suitPhase = kSelectFadeOut;
			_suitElapsed = 0;
			return;
		}
	}
	if (!clicked)
		return;

	for (int item = 0; item < kSwimsuitsPerPage; ++item) {
		if (!menuRectangle(312, 89 + item * kSuitItemSpacing, 264, 31).contains(_inputState.mousePosition))
			continue;
		const int swimsuit = _suitPage * kSwimsuitsPerPage + item;
		const int8 state = _swimsuits[_currentGirl][swimsuit];
		if (state == kSwimsuitLocked || state == kSwimsuitMissing)
			return;
		const bool changed = swimsuit != _currentSwimsuit;
		if (changed)
			debug(1, "Illusion: swimsuit %d selected", swimsuit);
		_currentSwimsuit = swimsuit;
		if (changed)
			loadSwimsuitModel();
		playSound("SE0003", false);
		_suitDirty = true;
		return;
	}
	for (int page = 0; page < 4; ++page) {
		if (!menuRectangle(583, 123 + page * 110, 30, 106).contains(_inputState.mousePosition))
			continue;
		_suitPage = page;
		playSound("SE0003", false);
		_suitDirty = true;
		return;
	}
}

void IllusionEngine::drawSwimsuitSelection() {
	// The list only changes on clicks, so it is drawn once and restored afterwards.
	if (_suitDirty || !_renderer->restoreScene()) {
		_renderer->drawBackground(_suitImages[kSuitBackground], 1.0f);
		_renderer->drawImage(_suitImages[kSuitStanding], menuRectangle(17, 63, 270, 690));
		drawDayDisplay(_suitImages + kSuitDayDigit, _suitImages + kSuitTimeDigit, _suitImages + kSuitName + _currentGirl);
		_renderer->drawImage(_suitImages[kSuitList + _suitPage], menuRectangle(301, 54, 314, 706));
		for (int item = 0; item < kSwimsuitsPerPage; ++item) {
			if (_swimsuits[_currentGirl][_suitPage * kSwimsuitsPerPage + item] == kSwimsuitLocked)
				_renderer->drawImage(_suitImages[kSuitUnknown + _suitPage],
					menuRectangle(314, 91 + item * kSuitItemSpacing, 260, 27));
		}
		if (_currentSwimsuit / kSwimsuitsPerPage == _suitPage) {
			const int item = _currentSwimsuit % kSwimsuitsPerPage;
			_renderer->drawImage(_suitImages[kSuitFrame], menuRectangle(306, kSuitItemY + item * kSuitItemSpacing, 275, 39));
		}
		_renderer->cacheScene();
		_suitDirty = false;
	}

	if (_suitModel.isLoaded()) {
		_renderer->drawModel(_suitModel, _suitTextures.data(), kSuitCamera);
	}

	for (int button = 0; button < 2; ++button) {
		const bool highlighted = _suitPhase == kSelectReady && _suitButton == button;
		const uint image = _suitImages[(button ? kSuitBackOn : kSuitOkOn) + (highlighted ? 0 : 1)];
		_renderer->drawImage(image, menuRectangle(kSuitButtonX, kSuitButtonY + button * kSuitButtonSpacing, 248, 59));
	}
	if (_suitClock < kNewIconShown) {
		for (int item = 0; item < kSwimsuitsPerPage; ++item) {
			if (_swimsuits[_currentGirl][_suitPage * kSwimsuitsPerPage + item] == kSwimsuitNew)
				_renderer->drawImage(_suitImages[kSuitNew], menuRectangle(285, kSuitItemY + item * kSuitItemSpacing, 40, 40));
		}
	}

	if (_suitPhase == kSelectReady)
		drawPointer();
	float fade = 0.0f;
	if (_suitPhase == kSelectFadeIn)
		fade = 1.0f - MIN(1.0f, _suitElapsed / (float)kFadeTime);
	else if (_suitPhase == kSelectFadeOut)
		fade = MIN(1.0f, _suitElapsed / (float)kFadeTime);
	_renderer->drawFadeToWhite(_fadeImage, fade);
	_renderer->flipBuffer();
}

Common::Error IllusionEngine::runSwimsuitSelection(bool &confirmed) {
	confirmed = false;
	uint32 previousTime = _system->getMillis();
	while (!shouldQuit() && !_inputState.quitRequested) {
		const uint32 frameStart = _system->getMillis();
		pumpEvents();
		if (shouldQuit() || _inputState.quitRequested)
			break;
		const uint32 now = _system->getMillis();
		const uint32 delta = MIN<uint32>(100, now - previousTime);
		previousTime = now;
		const CharacterSelectPhase previousPhase = _suitPhase;
		const bool iconWasShown = _suitClock < kNewIconShown;
		updateSwimsuitSelection(delta);
		if (_suitPhase == kSelectFadeOut && _suitElapsed >= kFadeTime) {
			clearSceneImages();
			confirmed = _suitConfirmed;
			return Common::kNoError;
		}
		const bool redraw = _suitDirty || _suitModel.isLoaded() || _suitPhase != kSelectReady || _suitPhase != previousPhase ||
			iconWasShown != (_suitClock < kNewIconShown) || _inputState.mouseDelta != Common::Point(0, 0) ||
			_inputState.mouseButtonsPressed || _inputState.redrawRequested;
		if (redraw)
			drawSwimsuitSelection();
		const uint32 spent = _system->getMillis() - frameStart;
		_system->delayMillis(spent < 16 ? 16 - spent : 1);
	}
	return Common::kNoError;
}

} // End of namespace Illusion
