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

#include "audio/midiparser.h"
#include "common/config-manager.h"
#include "system98/audio.h"
#include "system98/mmd.h"

namespace System98 {

Sound::Sound() : _effectParser(nullptr), _open(false), _sendingEffect(false),
	_effectPlaying(false), _paused(false), _fadeRemaining(0), _fadeDuration(0),
	_musicVolume(255), _effectVolume(255) {
	memset(_effectChannels, 0, sizeof(_effectChannels));
	memset(_effectVolumes, 127, sizeof(_effectVolumes));
}

Sound::~Sound() {
	if (_driver)
		_driver->setTimerCallback(nullptr, nullptr);
	stopEffect();
	stop();
}

bool Sound::init() {
	if (_open) return true;
	createDriver();
	if (_driver->open() != 0) {
		// createDriver() would overwrite (and leak) it on the next retry
		delete _driver;
		_driver = nullptr;
		return false;
	}
	if (_nativeMT32) _driver->sendMT32Reset();
	else _driver->sendGMReset();
	_open = true;
	syncSettings();
	_driver->setTimerCallback(this, &timerCallback);
	return true;
}

MidiParser *Sound::prepare(const Common::Array<byte> &data, Common::Array<byte> &midi) {
	if (!convertMmdToMidi(data, midi)) return nullptr;
	MidiParser *parser = MidiParser::createParser_SMF();
	if (!parser->loadMusic(midi.data(), midi.size())) { delete parser; return nullptr; }
	parser->setMidiDriver(this);
	parser->setTimerRate(_driver->getBaseTempo());
	parser->property(MidiParser::mpCenterPitchWheelOnUnload, 1);
	parser->property(MidiParser::mpSendSustainOffOnNotesOff, 1);
	parser->setTrack(0);
	return parser;
}

bool Sound::playMusic(const Common::Array<byte> &data) {
	if (!_open) return false;
	Common::StackLock lock(_mutex);
	stop();
	_fadeRemaining = 0;
	setVolume(_musicVolume);
	_parser = prepare(data, _musicMidi);
	_isPlaying = _parser != nullptr;
	_isLooping = true;
	return _isPlaying;
}

void Sound::stopEffect() {
	Common::StackLock lock(_mutex);
	_sendingEffect = true;
	if (_effectParser) {
		_effectParser->unloadMusic();
		delete _effectParser;
		_effectParser = nullptr;
	}
	for (MidiChannel *&channel : _effectChannels) {
		if (channel) { channel->release(); channel = nullptr; }
	}
	_effectMidi.clear();
	_effectPlaying = false;
	_sendingEffect = false;
}

bool Sound::playEffect(const Common::Array<byte> &bank, uint effect) {
	if (!_open) return false;
	Common::Array<byte> data;
	if (!extractMmdEffect(bank, effect, data)) return false;
	Common::StackLock lock(_mutex);
	stopEffect();
	_sendingEffect = true;
	_effectParser = prepare(data, _effectMidi);
	_effectPlaying = _effectParser != nullptr;
	_sendingEffect = false;
	return _effectPlaying;
}

void Sound::fadeOut(uint milliseconds) {
	Common::StackLock lock(_mutex);
	if (!milliseconds) { stop(); return; }
	_fadeDuration = milliseconds * 1000;
	_fadeRemaining = _fadeDuration;
}

void Sound::syncSettings() {
	Common::StackLock lock(_mutex);
	const bool mute = ConfMan.getBool("mute");
	_musicVolume = mute || ConfMan.getBool("music_mute") ? 0 : CLIP(ConfMan.getInt("music_volume"), 0, 255);
	_effectVolume = mute || ConfMan.getBool("sfx_mute") ? 0 : CLIP(ConfMan.getInt("sfx_volume"), 0, 255);
	setVolume(_fadeRemaining ? uint64(_musicVolume) * _fadeRemaining / _fadeDuration : _musicVolume);
	for (uint channel = 0; channel < 16; ++channel)
		if (_effectChannels[channel]) _effectChannels[channel]->volume(_effectVolumes[channel] * _effectVolume / 255);
}

void Sound::setPaused(bool paused) {
	Common::StackLock lock(_mutex);
	_paused = paused;
	if (_parser) {
		if (paused) _parser->pausePlaying();
		else _parser->resumePlaying();
	}
	_sendingEffect = true;
	if (_effectParser) {
		if (paused) _effectParser->pausePlaying();
		else _effectParser->resumePlaying();
	}
	_sendingEffect = false;
}

uint16 Sound::measure() {
	Common::StackLock lock(_mutex);
	return _parser && _isPlaying ? _parser->getTick() / 192 : 0;
}

bool Sound::playing() {
	Common::StackLock lock(_mutex);
	return _isPlaying;
}

void Sound::send(uint32 message) {
	if (!_sendingEffect) { Audio::MidiPlayer::send(message); return; }
	const byte channel = message & 15;
	if ((message & 0xfff0) == 0x07b0) {
		_effectVolumes[channel] = (message >> 16) & 127;
		message = (message & 0xff00ffff) | (uint32(_effectVolumes[channel] * _effectVolume / 255) << 16);
	}
	if (!_effectChannels[channel]) {
		if ((message & 0xfff0) == 0x7bb0) return;
		_effectChannels[channel] = channel == 9 ? _driver->getPercussionChannel() : _driver->allocateChannel();
		if (_effectChannels[channel]) _effectChannels[channel]->volume(_effectVolumes[channel] * _effectVolume / 255);
	}
	if (_effectChannels[channel]) _effectChannels[channel]->send(message);
}

void Sound::sysEx(const byte *message, uint16 length) {
	if (_open) _driver->sysEx(message, length);
}

void Sound::metaEvent(byte type, const byte *data, uint16 length) {
	if (_sendingEffect) {
		if (type == 0x2f) _effectPlaying = false;
	} else Audio::MidiPlayer::metaEvent(type, data, length);
}

void Sound::onTimer() {
	Common::StackLock lock(_mutex);
	if (_paused || !_open) return;
	if (_fadeRemaining) {
		_fadeRemaining -= MIN<uint32>(_fadeRemaining, _driver->getBaseTempo());
		setVolume(uint64(_musicVolume) * _fadeRemaining / _fadeDuration);
		if (!_fadeRemaining) stop();
	}
	if (_parser && _isPlaying) _parser->onTimer();
	if (_effectParser && _effectPlaying) {
		_sendingEffect = true;
		_effectParser->onTimer();
		_sendingEffect = false;
	}
}

} // namespace System98
