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

#ifndef ENGINES_COMPLET_AUDIO_H
#define ENGINES_COMPLET_AUDIO_H

#include "audio/midiplayer.h"
#include "audio/mixer.h"
#include "common/str.h"

namespace Audio {
class SeekableAudioStream;
}

namespace Complet {

// Plays the Standard MIDI File versions of the music tracks
class MusicMidiPlayer : public Audio::MidiPlayer {
public:
	MusicMidiPlayer();

	bool isOpen() const { return _open; }
	bool play(const Common::String &name, bool loop);

private:
	bool _open = false;
};

// Voices are MP3 streams stored as "VMD" files; music is either a VMD or a MIDI file
class AudioPlayer {
public:
	explicit AudioPlayer(Audio::Mixer *mixer);
	~AudioPlayer();

	// The original only falls back to MIDI when the sound card cannot play 44.1 kHz stereo
	void setUseMidi(bool useMidi);
	void syncVolume();
	void pause(bool pause);

	void playMusic(const Common::String &name, bool loop);
	void stopMusic();
	void playVoice(const Common::String &name, bool loop);
	void stopVoice();
	bool isVoicePlaying() const;

	// Name of the music still playing, empty if none
	Common::String getMusic(bool &loop) const;

private:
	Audio::SeekableAudioStream *openStream(const Common::String &name);

	Audio::Mixer *_mixer;
	MusicMidiPlayer *_midi = nullptr;
	bool _useMidi = false;
	bool _midiPaused = false;
	Audio::SoundHandle _musicHandle;
	Audio::SoundHandle _voiceHandle;
	Common::String _musicName;
	bool _musicLoop = false;
};

} // End of namespace Complet

#endif // ENGINES_COMPLET_AUDIO_H
