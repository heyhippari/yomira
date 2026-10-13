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
