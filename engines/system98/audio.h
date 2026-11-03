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

#ifndef SYSTEM98_AUDIO_H
#define SYSTEM98_AUDIO_H

#include "audio/midiplayer.h"
#include "audio/softsynth/fmtowns_pc98/pc98_audio.h"
#include "common/array.h"
#include "system98/pmd.h"

namespace System98 {

class Resources;

class Sound : public Audio::MidiPlayer {
public:
	Sound();
	~Sound() override;
	bool init();
	bool playMusic(const Common::Array<byte> &data);
	bool playEffect(const Common::Array<byte> &bank, uint effect);
	void stopEffect();
	void fadeOut(uint milliseconds);
	void syncSettings();
	void setPaused(bool paused);
	uint16 measure();
	bool playing();
	void send(uint32 message) override;
	void sysEx(const byte *message, uint16 length) override;
	void metaEvent(byte type, const byte *data, uint16 length) override;

private:
	void onTimer() override;
	MidiParser *prepare(const Common::Array<byte> &data, Common::Array<byte> &midi);
	MidiParser *_effectParser;
	Common::Array<byte> _musicMidi;
	Common::Array<byte> _effectMidi;
	MidiChannel *_effectChannels[16];
	byte _effectVolumes[16];
	bool _open;
	bool _sendingEffect;
	bool _effectPlaying;
	bool _paused;
	uint32 _fadeRemaining;
	uint32 _fadeDuration;
	int _musicVolume;
	int _effectVolume;
};

// P.M.D. playback on an emulated PC-9801-26K (OPN) or PC-9801-86 (OPNA) board.
class FmSound : public PC98AudioPluginDriver, private PmdChip {
public:
	explicit FmSound(Audio::Mixer *mixer);
	~FmSound() override;
	bool init(bool opna, const Common::Array<byte> &driver, const Common::Array<byte> &effects);
	void shutdown();
	bool isOpna() const { return _opna; }
	bool playMusic(const Common::Array<byte> &data);
	void stop();
	void fadeOut(uint8 speed);
	void playFmEffect(uint8 effect);
	void playSsgEffect(uint8 effect);
	void syncSettings();
	void setPaused(bool paused);
	uint16 measure();
	uint16 status();
	void timerCallbackA() override;
	void timerCallbackB() override;

private:
	void writeRegister(uint8 port, uint8 reg, uint8 value) override;
	void updateEffectChannels();
	Audio::Mixer *_mixer;
	PC98AudioCore *_core;
	PmdPlayer *_player;
	bool _opna;
	bool _paused;
	int _effectChannels;
};

} // namespace System98

#endif
