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

#include "audio/audiostream.h"
#include "audio/decoders/mp3.h"
#include "audio/midiparser.h"
#include "common/debug.h"
#include "common/file.h"
#include "common/memstream.h"

#include "xex/audio.h"
#include "xex/detection.h"

namespace Xex {

// The VMD files are MP3 data with every byte XORed with this value
static const byte kVmdKey = 0xe5;

MusicMidiPlayer::MusicMidiPlayer() {
	createDriver();
	if (_driver->open() != 0)
		return;

	if (_nativeMT32)
		_driver->sendMT32Reset();
	else
		_driver->sendGMReset();
	_driver->setTimerCallback(this, &timerCallback);
	_open = true;
}

bool MusicMidiPlayer::play(const Common::String &name, bool loop) {
	stop();

	Common::File file;
	if (!file.open(Common::Path(name + ".mid")))
		return false;
	debugC(1, kDebugAudio, "Playing '%s.mid'", name.c_str());

	uint32 size = file.size();
	byte *data = (byte *)malloc(size);
	if (!data || file.read(data, size) != size) {
		free(data);
		return false;
	}

	MidiParser *parser = MidiParser::createParser_SMF();
	if (!parser->loadMusic(data, size)) {
		delete parser;
		free(data);
		return false;
	}

	Common::StackLock lock(_mutex);
	_midiData = data;
	parser->setTrack(0);
	parser->setMidiDriver(this);
	parser->setTimerRate(_driver->getBaseTempo());
	parser->property(MidiParser::mpCenterPitchWheelOnUnload, 1);
	// The tracks hold notes with the sustain pedal, which All Notes Off alone does not release
	parser->property(MidiParser::mpSendSustainOffOnNotesOff, 1);
	_parser = parser;
	_isLooping = loop;
	_isPlaying = true;
	return true;
}

AudioPlayer::AudioPlayer(Audio::Mixer *mixer) : _mixer(mixer) {
}

AudioPlayer::~AudioPlayer() {
	stopMusic();
	stopVoice();
	delete _midi;
}

void AudioPlayer::setUseMidi(bool useMidi) {
	if (useMidi == _useMidi)
		return;

	_useMidi = useMidi;
	if (useMidi && !_midi) {
		_midi = new MusicMidiPlayer();
		syncVolume();
	}

	// Restart the current track with the newly chosen kind of music
	bool loop;
	Common::String music = getMusic(loop);
	if (!music.empty()) {
		stopMusic();
		playMusic(music, loop);
	}
}

void AudioPlayer::syncVolume() {
	if (_midi)
		_midi->syncVolume();
}

void AudioPlayer::pause(bool pause) {
	if (!_midi)
		return;
	if (pause) {
		_midiPaused = _midi->isPlaying();
		if (_midiPaused)
			_midi->pause();
	} else if (_midiPaused) {
		_midiPaused = false;
		_midi->resume();
	}
}

Audio::SeekableAudioStream *AudioPlayer::openStream(const Common::String &name) {
	debugC(1, kDebugAudio, "Opening '%s.vmd'", name.c_str());
	Common::File file;
	if (!file.open(Common::Path(name + ".vmd")))
		return nullptr;

	uint32 size = file.size();
	byte *data = (byte *)malloc(size);
	if (!data || file.read(data, size) != size) {
		free(data);
		return nullptr;
	}

	for (uint32 i = 0; i < size; i++)
		data[i] ^= kVmdKey;

	return Audio::makeMP3Stream(new Common::MemoryReadStream(data, size, DisposeAfterUse::YES), DisposeAfterUse::YES);
}

void AudioPlayer::playMusic(const Common::String &name, bool loop) {
	bool unused;
	if (name == getMusic(unused))
		return;

	stopMusic();
	_musicName = name;
	_musicLoop = loop;

	// Not every digital track has a MIDI version
	if (_useMidi && _midi && _midi->isOpen() && _midi->play(name, loop))
		return;

	Audio::SeekableAudioStream *stream = openStream(name);
	if (!stream) {
		warning("Xex: cannot play music '%s'", name.c_str());
		_musicName.clear();
		return;
	}

	Audio::AudioStream *source = loop ? Audio::makeLoopingAudioStream(stream, 0) : (Audio::AudioStream *)stream;
	_mixer->playStream(Audio::Mixer::kMusicSoundType, &_musicHandle, source);
}

void AudioPlayer::stopMusic() {
	debugC(1, kDebugAudio, "Stopping music '%s'", _musicName.c_str());
	_mixer->stopHandle(_musicHandle);
	if (_midi)
		_midi->stop();
	_midiPaused = false;
	_musicName.clear();
}

void AudioPlayer::playVoice(const Common::String &name, bool loop) {
	stopVoice();

	// A missing file is how scripts silence the voice channel
	Audio::SeekableAudioStream *stream = openStream(name);
	if (!stream)
		return;

	Audio::AudioStream *source = loop ? Audio::makeLoopingAudioStream(stream, 0) : (Audio::AudioStream *)stream;
	_mixer->playStream(Audio::Mixer::kSpeechSoundType, &_voiceHandle, source);
}

void AudioPlayer::stopVoice() {
	_mixer->stopHandle(_voiceHandle);
}

bool AudioPlayer::isVoicePlaying() const {
	return _mixer->isSoundHandleActive(_voiceHandle);
}

Common::String AudioPlayer::getMusic(bool &loop) const {
	loop = _musicLoop;
	bool playing = _mixer->isSoundHandleActive(_musicHandle) || (_midi && (_midi->isPlaying() || _midiPaused));
	return playing ? _musicName : Common::String();
}

} // End of namespace Xex
