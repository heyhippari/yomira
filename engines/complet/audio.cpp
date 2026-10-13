#include "audio/audiostream.h"
#include "audio/decoders/mp3.h"
#include "common/debug.h"
#include "common/file.h"
#include "common/memstream.h"

#include "complet/audio.h"
#include "complet/detection.h"

namespace Complet {

// The VMD files are MP3 data with every byte XORed with this value
static const byte kVmdKey = 0xe5;

AudioPlayer::AudioPlayer(Audio::Mixer *mixer) : _mixer(mixer) {
}

AudioPlayer::~AudioPlayer() {
	stopMusic();
	stopVoice();
}

Audio::SeekableAudioStream *AudioPlayer::openStream(const Common::String &name) {
	debugC(1, kDebugAudio, "Opening '%s.vmd'", name.c_str());
#ifdef USE_MAD
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
#else
	return nullptr;
#endif
}

void AudioPlayer::playMusic(const Common::String &name, bool loop) {
	if (name == _musicName && _mixer->isSoundHandleActive(_musicHandle))
		return;

	stopMusic();
	Audio::SeekableAudioStream *stream = openStream(name);
	if (!stream) {
		warning("Complet: cannot play music '%s'", name.c_str());
		return;
	}

	_musicName = name;
	_musicLoop = loop;
	Audio::AudioStream *source = loop ? Audio::makeLoopingAudioStream(stream, 0) : (Audio::AudioStream *)stream;
	_mixer->playStream(Audio::Mixer::kMusicSoundType, &_musicHandle, source);
}

void AudioPlayer::stopMusic() {
	_mixer->stopHandle(_musicHandle);
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
	return _mixer->isSoundHandleActive(_musicHandle) ? _musicName : Common::String();
}

} // End of namespace Complet
