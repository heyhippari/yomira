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

#include "audio/mixer.h"
#include "common/archive.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/file.h"
#include "common/system.h"
#include "graphics/cursorman.h"
#include "graphics/fonts/ttf.h"

#include "delta/delta.h"

namespace Delta {

namespace {

struct SettingKey {
	const char *key;
	int OptionsSettings::*field;
	int minimum, maximum;
};

const SettingKey kSettingKeys[] = {
	{ "delta_text_speed", &OptionsSettings::textSpeed, 0, 2 },
	{ "delta_auto_speed", &OptionsSettings::autoSpeed, 0, 2 },
	{ "delta_no_movie", &OptionsSettings::noMovie, 0, 1 },
	{ "delta_effect", &OptionsSettings::effect, 0, 2 },
	{ "delta_font_smoothing", &OptionsSettings::fontSmoothing, 0, 1 },
	{ "delta_font_shadow", &OptionsSettings::fontShadow, 0, 1 },
	{ "delta_window_transparency", &OptionsSettings::windowTransparency, 0, 255 },
	{ "delta_wheel_mode", &OptionsSettings::wheelMode, 0, 1 },
	{ "delta_volume_all", &OptionsSettings::volumeAll, 0, 100 },
	{ "delta_voice_duck", &OptionsSettings::volumeDuck, 0, 100 },
	{ "delta_voice_cut", &OptionsSettings::voiceCut, 0, 1 },
	{ "delta_no_directsound", &OptionsSettings::noDirectSound, 0, 1 },
	{ "delta_unread_skip", &OptionsSettings::unreadSkip, 0, 1 },
	{ "delta_read_skip", &OptionsSettings::readSkip, 0, 1 },
	{ "delta_skip_stop", &OptionsSettings::skipStop, 0, 1 },
	{ "delta_auto_stop", &OptionsSettings::autoStop, 0, 1 },
	{ "delta_skip_key", &OptionsSettings::skipKey, 16, 17 },
	{ "delta_auto_cursor", &OptionsSettings::autoCursor, 0, 1 },
	{ "delta_wheel_button", &OptionsSettings::wheelButton, 0, 5 },
	{ "delta_right_click", &OptionsSettings::rightClick, 0, 5 }
};

const char *const kColorKeys[] = { "delta_text_color", "delta_read_color", "delta_backlog_color" };

// The original stores music and effect levels as DirectSound attenuation in hundredths of a decibel.
int decibelsToVolume(int decibels) {
	return CLIP<int>(Audio::Mixer::kMaxMixerVolume * pow(10.0, decibels / 20.0) + 0.5, 0, Audio::Mixer::kMaxMixerVolume);
}

int volumeToDecibels(int volume) {
	if (volume <= 0)
		return -30;
	return CLIP<int>(floor(20.0 * log10((double)volume / Audio::Mixer::kMaxMixerVolume) + 0.5), -30, 0);
}

int readSetting(const char *key, int fallback, int minimum, int maximum) {
	return ConfMan.hasKey(key) ? CLIP(ConfMan.getInt(key), minimum, maximum) : fallback;
}

void blitArea(Graphics::Screen &screen, const Graphics::Surface &source, const Common::Rect &area) {
	screen.blitFrom(source, area, Common::Point(area.left, area.top));
}

// 0x414f50 draws the secondary colour first: at (+2, +2) normally, or at all four diagonals in outline mode.
void drawPreviewText(const Graphics::Font &font, Graphics::Screen &screen, const char *utf8, int x, int y, int width,
		uint32 foreground, bool shadow, bool outline) {
	const Common::U32String text(utf8, Common::kUtf8);
	const uint32 black = screen.format.RGBToColor(0, 0, 0);
	if (shadow && outline) {
		for (int dy = -1; dy <= 1; dy += 2) {
			for (int dx = -1; dx <= 1; dx += 2)
				font.drawString(&screen, text, x + dx, y + dy, width, black);
		}
	} else if (shadow) {
		font.drawString(&screen, text, x + 2, y + 2, width, black);
	}
	font.drawString(&screen, text, x, y, width, foreground);
}

} // End of anonymous namespace

Graphics::Font *DeltaEngine::loadFont(int size) {
	const char *const names[] = { "NotoSansCJK-Regular.ttc", "NotoSansCJKjp-Regular.otf", "NotoSerifCJK-Regular.ttc" };
	for (uint index = 0; index < ARRAYSIZE(names); ++index) {
		Common::SeekableReadStream *stream = SearchMan.createReadStreamForMember(names[index]);
		if (stream) {
			Graphics::Font *font = Graphics::loadTTFFont(stream, DisposeAfterUse::YES, size, Graphics::kTTFSizeModeCharacter);
			if (font)
				return font;
		}
	}
	return nullptr;
}

void DeltaEngine::loadSettings() {
	OptionsSettings &s = _settings;
	s = OptionsSettings();
	// Keep the port's existing frame default when the key is absent; the in-game reset uses 128.
	s.windowTransparency = _messageFrameTransparency;
	for (const SettingKey &setting : kSettingKeys)
		s.*setting.field = readSetting(setting.key, s.*setting.field, setting.minimum, setting.maximum);
	if (s.skipKey != 16)
		s.skipKey = 17;
	for (uint index = 0; index < ARRAYSIZE(kColorKeys); ++index)
		s.colors[index] = readSetting(kColorKeys[index], s.colors[index], 0, 0xffffff);
	for (uint row = 0; row < kOptionsCharacterVoices; ++row) {
		s.characterVoiceOff[row] = readSetting(Common::String::format("delta_character_voice_off_%u", row + 1).c_str(), 0, 0, 1);
		s.characterVolume[row] = readSetting(Common::String::format("delta_character_volume_%u", row + 1).c_str(), 0, -30, 0);
	}
	s.fullscreen = _system->hasFeature(OSystem::kFeatureFullscreenMode) &&
		_system->getFeatureState(OSystem::kFeatureFullscreenMode);
	// Music, effect and voice levels are shared with ScummVM's own volume settings.
	s.volumeMusic = volumeToDecibels(ConfMan.getInt("music_volume"));
	s.volumeEffects = volumeToDecibels(ConfMan.getInt("sfx_volume"));
	s.volumeVoice = CLIP(ConfMan.getInt("speech_volume") * 100 / Audio::Mixer::kMaxMixerVolume, 0, 100);
	s.musicOff = ConfMan.getBool("music_mute");
	s.effectsOff = ConfMan.getBool("sfx_mute");
	s.voiceOff = ConfMan.getBool("speech_mute");
	_messageFrameTransparency = s.windowTransparency;
}

void DeltaEngine::saveSettings() {
	const OptionsSettings &s = _settings;
	for (const SettingKey &setting : kSettingKeys)
		ConfMan.setInt(setting.key, s.*setting.field);
	for (uint index = 0; index < ARRAYSIZE(kColorKeys); ++index)
		ConfMan.setInt(kColorKeys[index], s.colors[index]);
	for (uint row = 0; row < kOptionsCharacterVoices; ++row) {
		ConfMan.setInt(Common::String::format("delta_character_voice_off_%u", row + 1), s.characterVoiceOff[row]);
		ConfMan.setInt(Common::String::format("delta_character_volume_%u", row + 1), s.characterVolume[row]);
	}
	// The sliders have whole-decibel steps; keep ScummVM's finer level unless the step changed.
	if (volumeToDecibels(ConfMan.getInt("music_volume")) != s.volumeMusic)
		ConfMan.setInt("music_volume", decibelsToVolume(s.volumeMusic));
	if (volumeToDecibels(ConfMan.getInt("sfx_volume")) != s.volumeEffects)
		ConfMan.setInt("sfx_volume", decibelsToVolume(s.volumeEffects));
	if (ConfMan.getInt("speech_volume") * 100 / Audio::Mixer::kMaxMixerVolume != s.volumeVoice)
		ConfMan.setInt("speech_volume", s.volumeVoice * Audio::Mixer::kMaxMixerVolume / 100);
	ConfMan.setBool("music_mute", s.musicOff != 0);
	ConfMan.setBool("sfx_mute", s.effectsOff != 0);
	ConfMan.setBool("speech_mute", s.voiceOff != 0);
	if (_system->hasFeature(OSystem::kFeatureFullscreenMode) &&
		_system->getFeatureState(OSystem::kFeatureFullscreenMode) != (s.fullscreen != 0)) {
		_system->beginGFXTransaction();
		_system->setFeatureState(OSystem::kFeatureFullscreenMode, s.fullscreen != 0);
		_system->endGFXTransaction();
		ConfMan.setBool("fullscreen", s.fullscreen != 0);
	}
	ConfMan.flushToDisk();
	syncSoundSettings();
	_messageFrameTransparency = s.windowTransparency;
	debug(1, "Delta options saved: transparency %d, text colour %06x, shadow %d", s.windowTransparency, s.colors[0], s.fontShadow);
}

bool DeltaEngine::openOptions() {
	if (!_optionsThumb.getPixels()) {
		Common::File bank;
		if (!bank.open("CG/ST.CGF") ||
			!readSpriteBankImage(bank, "CURSOR", _optionsThumb, _optionsThumbMask, _screen->format, &_progress) ||
			!readSpriteBankImage(bank, "MENUSYS1B1", _optionsDigits, _optionsDigitsMask, _screen->format, &_progress) ||
			_optionsDigits.w < 12 || _optionsDigits.h < 252 + 24 * 10) {
			warning("Delta options: missing CURSOR or MENUSYS1B1 in CG/ST.CGF");
			return false;
		}
	}
	if (!_optionsFont)
		_optionsFont = loadFont(18);
	if (!_messageFont)
		_messageFont = loadFont(24);
	loadSettings();
	const bool package = getGameId() == "onegaitasukete";
	// PHM's sound page has four character-voice rows; One Lover's artwork has none.
	_options.open(_settings, kOptionsDisplay, package, package ? 4 : 0);
	_optionsLoadedPage = -1;
	if (!loadOptionsPage())
		return false;
	_optionsActive = true;
	CursorMan.showMouse(_input.mouseActive());
	drawOptions();
	return true;
}

bool DeltaEngine::loadOptionsPage() {
	if (_optionsLoadedPage == _options.page())
		return true;
	static const char *const prefixes[] = { "MENUS_D", "MENUS_N", "MENUS_Y" };
	const bool package = getGameId() == "onegaitasukete";
	for (uint state = 0; state < ARRAYSIZE(_optionsImages); ++state) {
		_optionsImages[state].free();
		Common::String name = Common::String::format("%s%u", prefixes[_options.page()], state + 1);
		// PHM ships two sound-page layouts; A matches the recovered control table.
		if (package && _options.page() == kOptionsSound)
			name += "A";
		if (loadImage(name, _optionsImages[state]).getCode() != Common::kNoError ||
			_optionsImages[state].w != _screen->w || _optionsImages[state].h != _screen->h) {
			warning("Delta options: cannot load %s", name.c_str());
			return false;
		}
	}
	_optionsLoadedPage = _options.page();
	return true;
}

void DeltaEngine::closeOptions() {
	_optionsActive = false;
	_optionsDirty = false;
	_optionsLoadedPage = -1;
	for (uint state = 0; state < ARRAYSIZE(_optionsImages); ++state)
		_optionsImages[state].free();
	if (_atMenu)
		drawMenu();
}

void DeltaEngine::drawOptions() {
	_optionsDirty = false;
	if (!loadOptionsPage()) {
		closeOptions();
		return;
	}
	_screen->blitFrom(_optionsImages[0]);
	const Common::Array<OptionsControl> &controls = _options.controls();
	for (uint index = 0; index < controls.size(); ++index) {
		const OptionsControl &control = controls[index];
		if (control.type == kOptionsRadio && control.selected)
			blitArea(*_screen, _optionsImages[1], control.rect);
		else if ((int)index == _options.hover() && (control.type == kOptionsButton || control.type == kOptionsRadio))
			blitArea(*_screen, _optionsImages[2], control.rect);
		else if (control.type == kOptionsSlider && control.maximum > control.minimum)
			compositeMasked(*_screen->surfacePtr(), _optionsThumb, _optionsThumbMask, nullptr,
				Common::Point(_options.sliderThumb(index), control.rect.top));
	}
	if (_options.page() == kOptionsDisplay)
		drawDisplayOptions();
	// Keyboard and controller focus has no original artwork, so outline the focused control.
	if (!_input.mouseActive() && _options.hover() >= 0)
		_screen->frameRect(controls[_options.hover()].rect, _screen->format.RGBToColor(255, 255, 255));
}

void DeltaEngine::drawDisplayOptions() {
	const OptionsSettings &s = _options.working();
	Graphics::Screen &screen = *_screen;
	// 0x42e9d0: three colour swatches and a 64x48 preview in the selected colour.
	for (uint index = 0; index < ARRAYSIZE(s.colors); ++index) {
		const uint32 color = s.colors[index];
		screen.fillRect(Common::Rect(36 + index * 96, 180, 68 + index * 96, 204),
			screen.format.RGBToColor(color >> 16, (color >> 8) & 0xff, color & 0xff));
	}
	blitArea(screen, _optionsImages[2], Common::Rect(288, 216, 352, 264));
	const uint32 target = s.colors[_options.colorTarget()];
	const uint32 targetColor = screen.format.RGBToColor(target >> 16, (target >> 8) & 0xff, target & 0xff);
	if (_optionsFont) {
		drawPreviewText(*_optionsFont, screen, "\xe6\x96\x87\xe5\xad\x97\xe8\x89\xb2", 290, 220, 62, targetColor, s.fontShadow, true);
		drawPreviewText(*_optionsFont, screen, "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88", 290, 244, 62, targetColor, s.fontShadow, true);
	}
	// 0x42e4c0: shadow sample in the normal text colour.
	if (_messageFont) {
		const uint32 text = s.colors[0];
		drawPreviewText(*_messageFont, screen,
			"\xe6\x96\x87\xe5\xad\x97\xe3\x81\xab\xe5\xbd\xb1\xe6\x9c\x89\xe7\x84\xa1\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88",
			420, 264, 332, screen.format.RGBToColor(text >> 16, (text >> 8) & 0xff, text & 0xff), s.fontShadow, false);
	}
	// 0x42e5a0: percentage digits from MENUSYS1B1 and the darkened window sample.
	const int percent = s.windowTransparency * 100 / 255;
	const int digits[3] = { percent / 100, percent % 100 / 10, percent % 10 };
	for (int place = 0; place < 3; ++place) {
		if ((place == 0 && !digits[0]) || (place == 1 && !digits[0] && !digits[1]))
			continue;
		const Common::Rect source(0, 252 + digits[place] * 24, 12, 276 + digits[place] * 24);
		compositeMasked(*screen.surfacePtr(), _optionsDigits.getSubArea(source), _optionsDigitsMask.getSubArea(source),
			nullptr, Common::Point(616 + place * 12, 320));
	}
	const Common::Rect sample(416, 376, 752, 464);
	for (int y = sample.top; y < sample.bottom; ++y) {
		for (int x = sample.left; x < sample.right; ++x) {
			byte red, green, blue;
			screen.format.colorToRGB(screen.getPixel(x, y), red, green, blue);
			screen.setPixel(x, y, screen.format.RGBToColor(red * s.windowTransparency >> 8,
				green * s.windowTransparency >> 8, blue * s.windowTransparency >> 8));
		}
	}
}

void DeltaEngine::handleOptionsInput(const Common::Event &event) {
	OptionsResult result = kOptionsIdle;
	switch (event.type) {
	case Common::EVENT_MOUSEMOVE:
		result = _options.mouseMove(event.mouse);
		break;
	case Common::EVENT_LBUTTONDOWN:
		result = _options.mouseDown(event.mouse);
		break;
	case Common::EVENT_LBUTTONUP:
		result = _options.mouseUp(event.mouse);
		break;
	case Common::EVENT_CUSTOM_ENGINE_ACTION_START:
		switch (event.customType) {
		case kActionSelect: result = _options.activateFocus(); break;
		case kActionCancel: result = _options.cancel(); break;
		case kActionUp: result = _options.moveFocus(0, -1); break;
		case kActionDown: result = _options.moveFocus(0, 1); break;
		case kActionLeft: result = _options.adjustFocus(-1); break;
		case kActionRight: result = _options.adjustFocus(1); break;
		case kActionMenu:
			openMainMenuDialog();
			CursorMan.showMouse(_input.mouseActive());
			result = kOptionsRedraw;
			break;
		default: break;
		}
		break;
	default:
		break;
	}
	if (result == kOptionsCommit || result == kOptionsCommitClose) {
		_settings = _options.working();
		saveSettings();
	}
	if (result == kOptionsClose || result == kOptionsCommitClose)
		closeOptions();
	else if (result != kOptionsIdle)
		_optionsDirty = true;
}

} // End of namespace Delta
