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

#ifndef SYSTEM98_PMD_H
#define SYSTEM98_PMD_H

#include "common/array.h"

namespace System98 {

// Receives OPN/OPNA register writes; port 1 is the OPNA extension port.
class PmdChip {
public:
	virtual ~PmdChip() {}
	virtual void writeRegister(uint8 port, uint8 reg, uint8 value) = 0;
};

// Sequencer for compiled P.M.D. 4.8 data (.M songs and .EFC FM effects).
// Timer B drives music; Timer A (fixed at its slowest period) drives effects and fades.
class PmdPlayer {
public:
	PmdPlayer(PmdChip &chip, bool opna);

	void reset();
	// Locates the built-in SSG effect table (drums and effects) in PMD.COM or PMDB2.COM.
	bool loadSsgEffects(const Common::Array<byte> &driver);
	bool loadFmEffects(const Common::Array<byte> &data);
	bool playMusic(const Common::Array<byte> &data);
	void stopMusic();
	void fadeOut(uint8 speed);
	void playSsgEffect(uint8 number);
	void playFmEffect(uint8 number);
	void stopFmEffect();

	void timerA();
	void timerB();

	uint8 timerBValue() const { return _timerB; }
	uint16 measure() const { return _measure; }
	// High byte: script status set by the song; low byte: loop count, 0xff once every part has ended.
	uint16 status() const { return (_status << 8) | _loopCount; }
	bool musicPlaying() const { return _playing; }
	bool fmEffectPlaying() const { return _fmEffectActive; }
	bool ssgEffectPlaying() const { return _ssgEffectPriority != 0; }

private:
	enum PartType {
		kPartFm,
		kPartSsg,
		kPartRhythm
	};

	struct Part {
		PartType type;
		uint8 channel;
		Common::Array<byte> *data;
		uint base;
		uint position;
		uint loop;
		bool active;
		bool hasLoop;
		uint8 length;
		uint8 gate;
		uint8 q;
		uint8 qPercent;
		uint8 keyoffFlag;
		uint8 loopCheck;
		uint8 mask;
		uint8 volume;
		int8 transpose;
		uint8 voice;
		uint8 note;
		uint16 fnum;
		int16 detune;
		int16 lfoData;
		uint8 lfoSwitch;
		uint8 lfoDelay, lfoSpeed, lfoTime;
		int8 lfoStep;
		uint8 lfoDelay2, lfoSpeed2, lfoTime2;
		int8 lfoStep2;
		uint8 lfoVolumeMask;
		uint8 slotMask;
		uint8 carrier;
		uint8 algorithm;
		uint8 tl[4];
		uint8 pan;
		uint8 envFlag;
		int8 envVolume;
		uint8 envAttack, envAttack2;
		int8 envDecay;
		uint8 envSustain, envSustain2;
		uint8 envRelease, envRelease2;
		uint8 ssgPattern;
	};

	struct SsgEffect {
		uint8 priority;
		uint offset;
	};

	uint8 read(const Part &part, uint position) const;
	uint16 readWord(const Part &part, uint position) const;
	void initPart(Part &part, PartType type, uint8 channel, Common::Array<byte> *data, uint base, uint16 address);
	void silenceAll();

	void runPart(Part &part);
	bool readCommands(Part &part, bool masked);
	void startNote(Part &part, uint8 note);
	void skipNote(Part &part);
	void endOfData(Part &part, bool &stop);
	bool command(Part &part, uint8 command);
	void loopStart(Part &part);
	void loopEnd(Part &part);
	void loopExit(Part &part);
	void tempoCommand(Part &part);
	void setTimerB(uint8 value);
	uint8 transposeNote(const Part &part, uint8 note) const;
	void setFrequency(Part &part, uint8 note);
	void calcGate(Part &part);
	void lfoNoteInit(Part &part, uint8 note);
	void lfoReset(Part &part);
	bool lfoTick(Part &part);
	bool envelopeTick(Part &part);

	void fmWrite(const Part &part, uint8 reg, uint8 value);
	void fmVoice(Part &part, bool write);
	const byte *findVoice(const Part &part, uint8 number, bool effect) const;
	void fmSilence(Part &part);
	void fmVolume(Part &part);
	void fmFrequency(Part &part);
	void fmKeyOn(Part &part);
	void fmKeyOff(Part &part);
	void ssgVolume(Part &part);
	void ssgFrequency(Part &part);
	void ssgKeyOn(Part &part);
	void ssgKeyOff(Part &part);
	void keyOff(Part &part);

	void rhythmRun(Part &part);
	void rhythmShot(uint16 bits);

	void ssgEffectStart(uint8 number);
	bool ssgEffectFrame();
	void ssgEffectTick();
	void ssgEffectEnd();
	void ssgWrite(uint8 reg, uint8 value);

	PmdChip &_chip;
	const bool _opna;
	Common::Array<byte> _music;
	Common::Array<byte> _fmEffects;
	Common::Array<byte> _ssgEffectData;
	Common::Array<SsgEffect> _ssgEffects;
	Part _parts[10];
	uint _partCount;
	Part _fmEffect;
	uint _voiceTable;
	bool _hasVoices;
	uint _rhythmTable;
	uint _rhythmPosition;
	bool _rhythmPattern;
	uint8 _keys[6];
	uint8 _ssgMixer;
	uint8 _noise;
	uint8 _rhythmLevel;
	uint8 _timerB;
	uint8 _timerBWritten;
	uint8 _timerACount;
	uint8 _tickCount;
	uint8 _measureLength;
	uint16 _measure;
	uint8 _status;
	uint8 _loopCount;
	uint8 _loopWork;
	bool _tie;
	bool _playing;
	int8 _fadeSpeed;
	uint8 _fadeVolume;
	uint8 _fmVolumeDown;
	bool _fmEffectActive;
	uint8 _ssgEffectPriority;
	uint _ssgEffectPosition;
	uint8 _ssgEffectCount;
	uint16 _ssgEffectTone;
	int16 _ssgEffectSweep;
	uint8 _ssgEffectNoise;
	uint8 _ssgEffectNoiseSweep;
	uint8 _ssgEffectNoiseCount;
};

} // namespace System98

#endif
