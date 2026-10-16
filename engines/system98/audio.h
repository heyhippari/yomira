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
#include "common/array.h"

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

} // namespace System98

#endif
