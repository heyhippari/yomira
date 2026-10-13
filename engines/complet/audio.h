#ifndef ENGINES_COMPLET_AUDIO_H
#define ENGINES_COMPLET_AUDIO_H

#include "audio/mixer.h"
#include "common/str.h"

namespace Audio {
class SeekableAudioStream;
}

namespace Complet {

// Music and voice playback; both come from MP3 streams stored as "VMD" files
class AudioPlayer {
public:
	explicit AudioPlayer(Audio::Mixer *mixer);
	~AudioPlayer();

	void playMusic(const Common::String &name, bool loop);
	void stopMusic();
	void playVoice(const Common::String &name, bool loop);
	void stopVoice();

private:
	Audio::SeekableAudioStream *openStream(const Common::String &name);

	Audio::Mixer *_mixer;
	Audio::SoundHandle _musicHandle;
	Audio::SoundHandle _voiceHandle;
	Common::String _musicName;
};

} // End of namespace Complet

#endif // ENGINES_COMPLET_AUDIO_H
