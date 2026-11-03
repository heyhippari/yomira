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

#include "common/endian.h"
#include "system98/pmd.h"

namespace System98 {

namespace {

const uint16 kFmNumbers[12] = {0x26a, 0x28f, 0x2b6, 0x2df, 0x30b, 0x339, 0x36a, 0x39e, 0x3d5, 0x410, 0x44e, 0x48f};
const uint16 kSsgPeriods[12] = {0xee8, 0xe12, 0xd48, 0xc89, 0xbd5, 0xb2b, 0xa8a, 0x9f3, 0x964, 0x8dd, 0x85e, 0x7e6};
// Carrier slots per algorithm (bit 7 = slot 4 ... bit 4 = slot 1)
const byte kCarriers[8] = {0x80, 0x80, 0x80, 0x80, 0xa0, 0xe0, 0xe0, 0xf0};
// Modulator TLs written directly by a voice change, in register order (bit 7 = 0x40 ... bit 4 = 0x4c)
const byte kModulators[8] = {0xe0, 0xe0, 0xe0, 0xe0, 0xc0, 0x80, 0x80, 0x00};
// PMDB2 rhythm-source mapping for K/R rhythm bits: instrument register, level/pan, key bits
const byte kRhythmSounds[11][3] = {
	{0x18, 0xdf, 0x01}, {0x19, 0xdf, 0x02}, {0x1c, 0x5f, 0x10}, {0x1c, 0xdf, 0x10},
	{0x1c, 0x9f, 0x10}, {0x1d, 0xd3, 0x20}, {0x19, 0xdf, 0x02}, {0x1b, 0x9c, 0x88},
	{0x1a, 0x9d, 0x04}, {0x1a, 0xdf, 0x04}, {0x1a, 0x5e, 0x04}
};
const uint kGuard = 4096;

// Operand bytes of commands this player does not interpret; 0xff marks an invalid command
uint8 commandLength(uint8 command) {
	static const byte lengths[0x4c] = {
		1, 1, 1, 1, 0, 2, 2, 4, 2, 0, 1, 0, 0, 4, 1, 4, // ff..f0
		2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, // ef..e0
		1, 1, 1, 1, 1, 3, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, // df..d0
		1, 6, 5, 1, 1, 1, 1, 3, 3, 6, 1, 1, 2, 1, 0, 1, // cf..c0
		4, 1, 2, 1, 1, 1, 1, 2, 1, 1, 2, 16             // bf..b4
	};
	return command >= 0xb4 ? lengths[0xff - command] : 0xff;
}

} // namespace

PmdPlayer::PmdPlayer(PmdChip &chip, bool opna) : _chip(chip), _opna(opna) {
	memset(_parts, 0, sizeof(_parts));
	memset(&_fmEffect, 0, sizeof(_fmEffect));
	memset(_keys, 0, sizeof(_keys));
	_partCount = 0;
	_voiceTable = _rhythmTable = _rhythmPosition = 0;
	_hasVoices = _rhythmPattern = false;
	_ssgMixer = 0x3f;
	_noise = 0;
	_rhythmLevel = 45;
	_timerB = _timerBWritten = 200;
	_timerACount = _tickCount = 0;
	_measureLength = 96;
	_measure = 0;
	_status = 0;
	_loopCount = 0xff;
	_loopWork = 0;
	_tie = _playing = false;
	_fadeSpeed = 0;
	_fadeVolume = 0;
	// The game starts PMD.COM with /DF20 and PMDB2.COM with /DF10
	_fmVolumeDown = opna ? 10 : 20;
	_fmEffectActive = false;
	_ssgEffectPriority = 0;
	_ssgEffectPosition = 0;
	_ssgEffectCount = 0;
	_ssgEffectTone = 0;
	_ssgEffectSweep = 0;
	_ssgEffectNoise = _ssgEffectNoiseSweep = _ssgEffectNoiseCount = 0;
}

void PmdPlayer::reset() {
	_chip.writeRegister(0, 0x24, 0);
	_chip.writeRegister(0, 0x25, 0);
	_chip.writeRegister(0, 0x26, _timerB);
	_timerBWritten = _timerB;
	_chip.writeRegister(0, 0x27, 0x3f);
	_ssgMixer = 0x3f;
	ssgWrite(7, _ssgMixer);
	for (uint channel = 0; channel < 3; ++channel)
		ssgWrite(8 + channel, 0);
	if (_opna) {
		for (uint channel = 0; channel < 6; ++channel)
			_chip.writeRegister(channel / 3, 0xb4 + channel % 3, 0xc0);
		_chip.writeRegister(0, 0x10, 0xff);
		_chip.writeRegister(0, 0x11, _rhythmLevel);
		for (uint reg = 0x18; reg < 0x1e; ++reg)
			_chip.writeRegister(0, reg, 0xcf);
	}
	silenceAll();
}

bool PmdPlayer::loadSsgEffects(const Common::Array<byte> &driver) {
	_ssgEffects.clear();
	_ssgEffectData.clear();
	// The effect lookup multiplies the number by three and adds the table address
	static const byte signature[] = {0x03, 0xdb, 0x03, 0xd8, 0x81, 0xc3};
	for (uint position = 0; position + sizeof(signature) + 2 <= driver.size(); ++position) {
		if (memcmp(driver.data() + position, signature, sizeof(signature)))
			continue;
		const uint address = READ_LE_UINT16(driver.data() + position + sizeof(signature));
		if (address < 0x100 || address - 0x100 + 3 > driver.size())
			return false;
		const uint table = address - 0x100;
		const uint first = READ_LE_UINT16(driver.data() + table + 1);
		if (!first || first % 3 || table + first > driver.size())
			return false;
		_ssgEffectData = Common::Array<byte>(driver.data() + table, driver.size() - table);
		for (uint entry = 0; entry < first / 3; ++entry) {
			SsgEffect effect;
			effect.priority = _ssgEffectData[entry * 3];
			effect.offset = READ_LE_UINT16(_ssgEffectData.data() + entry * 3 + 1);
			if (!effect.priority || effect.offset >= _ssgEffectData.size()) {
				_ssgEffects.clear();
				return false;
			}
			_ssgEffects.push_back(effect);
		}
		return true;
	}
	return false;
}

bool PmdPlayer::loadFmEffects(const Common::Array<byte> &data) {
	stopFmEffect();
	_fmEffects.clear();
	if (data.size() < 0x100 || READ_LE_UINT16(data.data() + 0xfe) >= data.size())
		return false;
	_fmEffects = data;
	return true;
}

uint8 PmdPlayer::read(const Part &part, uint position) const {
	return position < part.data->size() ? (*part.data)[position] : 0x80;
}

uint16 PmdPlayer::readWord(const Part &part, uint position) const {
	return read(part, position) | (read(part, position + 1) << 8);
}

void PmdPlayer::initPart(Part &part, PartType type, uint8 channel, Common::Array<byte> *data, uint base, uint16 address) {
	memset(&part, 0, sizeof(part));
	part.type = type;
	part.channel = channel;
	part.data = data;
	part.base = base;
	part.position = base + address;
	part.active = read(part, part.position) != 0x80;
	part.length = 1;
	part.keyoffFlag = 0xff;
	part.note = 0xff;
	part.slotMask = 0xf0;
	part.pan = 0xc0;
	part.ssgPattern = 7;
	part.envVolume = 0;
	if (type == kPartFm) {
		part.volume = 0x6c;
	} else if (type == kPartSsg) {
		part.volume = 8;
		part.envFlag = 3;
	} else {
		part.volume = 15;
	}
}

bool PmdPlayer::playMusic(const Common::Array<byte> &data) {
	stopMusic();
	if (data.size() < 27)
		return false;
	_music = data;
	Part header;
	initPart(header, kPartFm, 0, &_music, 1, 0);
	_hasVoices = readWord(header, 1) == 0x1a;
	_voiceTable = _hasVoices ? 1 + readWord(header, 1 + 24) : 0;
	_rhythmTable = 1 + readWord(header, 1 + 22);
	_partCount = 0;
	const uint fmParts = _opna ? 6 : 3;
	for (uint index = 0; index < 3; ++index)
		initPart(_parts[_partCount++], kPartSsg, index, &_music, 1, readWord(header, 1 + (6 + index) * 2));
	initPart(_parts[_partCount++], kPartRhythm, 0, &_music, 1, readWord(header, 1 + 10 * 2));
	for (uint index = 0; index < fmParts; ++index)
		initPart(_parts[_partCount++], kPartFm, index, &_music, 1, readWord(header, 1 + index * 2));
	_rhythmPattern = false;
	_rhythmPosition = 0;
	_tie = false;
	_status = 0;
	_loopCount = 0;
	_measure = 0;
	_tickCount = 0;
	_measureLength = 96;
	_fadeSpeed = 0;
	_fadeVolume = 0;
	setTimerB(200);
	if (_fmEffectActive)
		_parts[4 + 2].mask |= 2;
	if (_ssgEffectPriority)
		_parts[2].mask |= 2;
	_playing = true;
	return true;
}

void PmdPlayer::stopMusic() {
	if (!_playing)
		return;
	_playing = false;
	_loopCount = 0xff;
	_fadeSpeed = 0;
	_fadeVolume = 0;
	silenceAll();
}

void PmdPlayer::silenceAll() {
	const uint fmChannels = _opna ? 6 : 3;
	for (uint channel = 0; channel < fmChannels; ++channel) {
		if (channel == 2 && _fmEffectActive)
			continue;
		const uint8 port = channel / 3, offset = channel % 3;
		for (uint slot = 0; slot < 4; ++slot) {
			_chip.writeRegister(port, 0x40 + offset + slot * 4, 0x7f);
			_chip.writeRegister(port, 0x80 + offset + slot * 4, 0x7f);
		}
		_keys[channel] = 0;
		_chip.writeRegister(0, 0x28, offset | (port << 2));
	}
	for (uint channel = 0; channel < 3; ++channel)
		if (channel != 2 || !_ssgEffectPriority)
			ssgWrite(8 + channel, 0);
	if (_opna)
		_chip.writeRegister(0, 0x10, 0xff);
}

void PmdPlayer::fadeOut(uint8 speed) {
	if (_playing)
		_fadeSpeed = speed;
}

void PmdPlayer::setTimerB(uint8 value) {
	_timerB = value;
}

void PmdPlayer::timerA() {
	if ((++_timerACount & 7) == 0 && _fadeSpeed) {
		const int volume = _fadeVolume + _fadeSpeed;
		if (_fadeSpeed > 0 && volume > 0xff) {
			_fadeVolume = 0xff;
			_fadeSpeed = 0;
			stopMusic();
		} else if (_fadeSpeed < 0 && volume < 0) {
			_fadeVolume = 0;
			_fadeSpeed = 0;
		} else {
			_fadeVolume = volume;
		}
	}
	if (_ssgEffectPriority)
		ssgEffectTick();
	if (_fmEffectActive) {
		runPart(_fmEffect);
		if (_fmEffectActive && (!_fmEffect.active || (read(_fmEffect, _fmEffect.position) == 0x80 && !_fmEffect.length)))
			stopFmEffect();
	}
}

void PmdPlayer::timerB() {
	if (_playing) {
		_loopWork = 3;
		for (uint index = 0; index < _partCount; ++index) {
			if (_parts[index].type == kPartRhythm)
				rhythmRun(_parts[index]);
			else
				runPart(_parts[index]);
		}
		if (_loopWork) {
			for (uint index = 0; index < _partCount; ++index)
				if (_parts[index].loopCheck != 3)
					_parts[index].loopCheck = 0;
			if (_loopWork == 3)
				_loopCount = 0xff;
			else if (++_loopCount == 0xff)
				_loopCount = 1;
		}
		if (++_tickCount == _measureLength) {
			_tickCount = 0;
			++_measure;
		}
	}
	if (_timerB != _timerBWritten) {
		_timerBWritten = _timerB;
		_chip.writeRegister(0, 0x26, _timerB);
	}
}

void PmdPlayer::runPart(Part &part) {
	if (!part.active)
		return;
	bool readData = false;
	if (part.mask) {
		if (--part.length) {
			_loopWork &= part.loopCheck;
			return;
		}
		if (part.type == kPartFm && (part.mask & 2) && !_fmEffectActive)
			part.mask &= ~2;
		if (part.mask) {
			readCommands(part, true);
			_loopWork &= part.loopCheck;
			return;
		}
		readData = true;
	} else {
		--part.length;
		if (!(part.keyoffFlag & 3) && part.length <= part.gate) {
			keyOff(part);
			part.keyoffFlag = 0xff;
		}
		readData = !part.length;
	}
	if (readData) {
		part.lfoSwitch &= 0xf7;
		if (readCommands(part, false)) {
			_loopWork &= part.loopCheck;
			return;
		}
	}
	uint8 changes = part.lfoSwitch & 8;
	if (part.lfoSwitch) {
		if ((part.lfoSwitch & 3) && lfoTick(part))
			changes |= part.lfoSwitch & 3;
		if (changes & 0x19) {
			if (part.type == kPartFm)
				fmFrequency(part);
			else
				ssgFrequency(part);
		}
	}
	if (part.type == kPartFm) {
		if ((changes & 0x22) || _fadeSpeed)
			fmVolume(part);
	} else {
		const bool envelope = envelopeTick(part);
		if (envelope || (changes & 0x22) || _fadeSpeed)
			ssgVolume(part);
	}
	_loopWork &= part.loopCheck;
}

void PmdPlayer::endOfData(Part &part, bool &stop) {
	part.loopCheck = 3;
	part.note = 0xff;
	if (part.hasLoop) {
		part.position = part.loop;
		part.loopCheck = 1;
		stop = false;
	} else {
		stop = true;
	}
}

// Returns true when a note started; false when the part ended, rested while masked, or failed
bool PmdPlayer::readCommands(Part &part, bool masked) {
	for (uint guard = 0; guard < kGuard; ++guard) {
		uint8 value = read(part, part.position++);
		if (value == 0x80) {
			--part.position;
			bool stop;
			endOfData(part, stop);
			if (stop)
				return false;
			continue;
		}
		if (value == 0xda) {
			// Portamento: approximated by its destination note
			const uint8 target = read(part, part.position + 1);
			part.position += 2;
			value = target;
			if (value >= 0x80)
				break;
		} else if (value > 0x80) {
			if (!command(part, value))
				break;
			continue;
		}
		if (masked) {
			// A drum effect yields SSG channel C to the next music note
			if (part.type == kPartSsg && part.channel == 2 && !(part.mask & 1) && (part.mask & 2) &&
			    _ssgEffectPriority < 2 && (value & 0x0f) != 0x0f) {
				if (_ssgEffectPriority == 1)
					ssgEffectEnd();
				part.mask &= ~2;
				if (!part.mask) {
					startNote(part, value);
					return true;
				}
			}
			skipNote(part);
			return false;
		}
		startNote(part, value);
		return true;
	}
	part.active = false;
	part.loopCheck = 3;
	keyOff(part);
	return false;
}

void PmdPlayer::skipNote(Part &part) {
	part.fnum = 0;
	part.note = 0xff;
	part.length = read(part, part.position++);
	_tie = false;
}

void PmdPlayer::startNote(Part &part, uint8 value) {
	lfoNoteInit(part, value);
	setFrequency(part, transposeNote(part, value));
	part.length = read(part, part.position++);
	calcGate(part);
	if (part.type == kPartFm) {
		fmVolume(part);
		fmFrequency(part);
		fmKeyOn(part);
	} else {
		ssgVolume(part);
		ssgFrequency(part);
		ssgKeyOn(part);
	}
	_tie = false;
	part.keyoffFlag = read(part, part.position) == 0xfb ? 2 : 0;
}

uint8 PmdPlayer::transposeNote(const Part &part, uint8 note) const {
	if (note == 0x0f)
		return note;
	if (note == 0x0c) {
		note = part.note;
		if (note >= 0x80)
			return 0x0f;
	}
	if (!part.transpose)
		return note;
	int octave = note >> 4;
	int key = (note & 0x0f) + part.transpose;
	while (key < 0) {
		key += 12;
		--octave;
	}
	while (key >= 12) {
		key -= 12;
		++octave;
	}
	return ((octave & 0x0f) << 4) | key;
}

void PmdPlayer::setFrequency(Part &part, uint8 note) {
	if ((note & 0x0f) == 0x0f || (note & 0x0f) >= 12) {
		part.note = 0xff;
		if (!(part.lfoSwitch & 0x11))
			part.fnum = 0;
		return;
	}
	part.note = note;
	const uint octave = (note >> 4) & 0x0f;
	if (part.type == kPartFm) {
		part.fnum = kFmNumbers[note & 0x0f] | ((octave & 7) << 11);
	} else {
		uint period = kSsgPeriods[note & 0x0f];
		if (octave) {
			period >>= octave - 1;
			period = (period >> 1) + (period & 1);
		}
		part.fnum = period;
	}
}

void PmdPlayer::calcGate(Part &part) {
	if (read(part, part.position) == 0xc1) {
		++part.position;
		part.gate = 0;
	} else if (part.qPercent) {
		part.gate = ((part.length * part.qPercent) >> 8) + part.q;
	} else {
		part.gate = part.q;
	}
}

void PmdPlayer::lfoNoteInit(Part &part, uint8 note) {
	if ((note & 0x0f) == 0x0f || _tie) {
		if (part.lfoSwitch & 3)
			lfoTick(part);
		return;
	}
	if (part.type == kPartSsg && part.envFlag != 0xff) {
		part.envFlag = 0;
		part.envVolume = 0;
		part.envAttack = part.envAttack2;
		if (!part.envAttack) {
			part.envFlag = 1;
			part.envVolume = part.envDecay;
		}
		part.envSustain = part.envSustain2;
		part.envRelease = part.envRelease2;
	}
	if (part.lfoSwitch & 3) {
		if (!(part.lfoSwitch & 4))
			lfoReset(part);
		else
			lfoTick(part);
	}
}

void PmdPlayer::lfoReset(Part &part) {
	part.lfoData = 0;
	part.lfoDelay = part.lfoDelay2;
	part.lfoSpeed = part.lfoSpeed2;
	part.lfoStep = part.lfoStep2;
	part.lfoTime = part.lfoTime2;
	if (part.lfoSwitch & 4) {
		++part.lfoDelay;
		++part.lfoDelay2;
	}
}

bool PmdPlayer::lfoTick(Part &part) {
	if (part.lfoDelay) {
		--part.lfoDelay;
		return false;
	}
	if (part.lfoSpeed != 1) {
		--part.lfoSpeed;
		return false;
	}
	part.lfoSpeed = part.lfoSpeed2;
	const int16 previous = part.lfoData;
	part.lfoData += part.lfoStep;
	if (part.lfoTime != 0xff && !--part.lfoTime) {
		part.lfoTime = part.lfoTime2 * 2;
		part.lfoStep = -part.lfoStep;
	}
	return part.lfoData != previous;
}

bool PmdPlayer::envelopeTick(Part &part) {
	if (part.envFlag == 0xff)
		return false;
	const int8 previous = part.envVolume;
	if (part.envFlag == 0) {
		if (!--part.envAttack) {
			part.envFlag = 1;
			part.envVolume = part.envDecay;
		}
		return part.envVolume != previous;
	}
	if (part.envFlag != 2) {
		if (!part.envSustain || --part.envSustain)
			return false;
		part.envSustain = part.envSustain2;
		--part.envVolume;
	} else if (!part.envRelease) {
		part.envVolume = -15;
	} else {
		if (--part.envRelease)
			return false;
		part.envRelease = part.envRelease2;
		--part.envVolume;
	}
	if (part.envVolume < -15 || part.envVolume >= 15)
		part.envVolume = -15;
	return part.envVolume != previous;
}

bool PmdPlayer::command(Part &part, uint8 value) {
	const bool fm = part.type == kPartFm;
	const bool ssg = part.type == kPartSsg;
	switch (value) {
	case 0xff:
		if (!fm)
			break;
		part.voice = read(part, part.position++);
		fmVoice(part, !part.mask);
		return true;
	case 0xfe:
		if (part.type == kPartRhythm)
			break;
		part.q = read(part, part.position++);
		return true;
	case 0xfd:
		part.volume = read(part, part.position++);
		return true;
	case 0xfc:
		tempoCommand(part);
		return true;
	case 0xfb:
		_tie = true;
		return true;
	case 0xfa:
		part.detune = readWord(part, part.position);
		part.position += 2;
		return true;
	case 0xf9:
		loopStart(part);
		return true;
	case 0xf8:
		loopEnd(part);
		return true;
	case 0xf7:
		loopExit(part);
		return true;
	case 0xf6:
		part.loop = part.position;
		part.hasLoop = true;
		return true;
	case 0xf5:
		if (part.type == kPartRhythm)
			break;
		part.transpose = read(part, part.position++);
		return true;
	case 0xe7:
		if (part.type == kPartRhythm)
			break;
		part.transpose += (int8)read(part, part.position++);
		return true;
	case 0xf4:
	case 0xe3: {
		const uint step = value == 0xf4 ? (fm ? 4 : 1) : read(part, part.position++);
		const uint volume = (part.volume + step) & 0xff;
		part.volume = fm ? MIN<uint>(volume, 0x7f) : MIN<uint>(volume, 15);
		return true;
	}
	case 0xf3:
	case 0xe2: {
		const uint step = value == 0xf3 ? (fm ? 4 : 1) : read(part, part.position++);
		part.volume = part.volume > step ? part.volume - step : 0;
		return true;
	}
	case 0xf2:
		if (part.type == kPartRhythm)
			break;
		part.lfoDelay = part.lfoDelay2 = read(part, part.position++);
		part.lfoSpeed = part.lfoSpeed2 = read(part, part.position++);
		part.lfoStep = part.lfoStep2 = read(part, part.position++);
		part.lfoTime = part.lfoTime2 = read(part, part.position++);
		lfoReset(part);
		return true;
	case 0xf1: {
		if (part.type == kPartRhythm)
			break;
		uint8 mode = read(part, part.position++);
		if (mode & 0xf8)
			mode = 1;
		part.lfoSwitch = (part.lfoSwitch & 0xf8) | (mode & 7);
		lfoReset(part);
		return true;
	}
	case 0xf0:
		if (!ssg)
			break;
		part.envAttack = part.envAttack2 = read(part, part.position++);
		part.envDecay = read(part, part.position++);
		part.envSustain = part.envSustain2 = read(part, part.position++);
		part.envRelease = part.envRelease2 = read(part, part.position++);
		if (part.envFlag == 0xff) {
			part.envFlag = 2;
			part.envVolume = -15;
		}
		return true;
	case 0xef: {
		const uint8 reg = read(part, part.position++);
		const uint8 data = read(part, part.position++);
		_chip.writeRegister(fm && part.channel >= 3 ? 1 : 0, reg, data);
		return true;
	}
	case 0xee:
		if (!ssg)
			break;
		_noise = read(part, part.position++);
		return true;
	case 0xed:
		if (!ssg)
			break;
		part.ssgPattern = read(part, part.position++);
		return true;
	case 0xec: {
		if (!fm || !_opna)
			break;
		part.pan = (part.pan & 0x3f) | ((read(part, part.position++) & 3) << 6);
		if (!part.mask)
			fmWrite(part, 0xb4, part.pan);
		return true;
	}
	case 0xdf:
		_measureLength = read(part, part.position++);
		return true;
	case 0xdc:
		_status = read(part, part.position++);
		return true;
	case 0xdb:
		_status += read(part, part.position++);
		return true;
	case 0xd5:
		part.detune += (int16)readWord(part, part.position);
		part.position += 2;
		return true;
	case 0xd4: {
		const uint8 number = read(part, part.position++);
		if (!part.mask)
			playSsgEffect(number);
		return true;
	}
	case 0xd3: {
		const uint8 number = read(part, part.position++);
		if (!part.mask && &part != &_fmEffect)
			playFmEffect(number);
		return true;
	}
	case 0xd2: {
		const uint8 speed = read(part, part.position++);
		if (!part.mask)
			fadeOut(speed);
		return true;
	}
	case 0xc4:
		part.qPercent = read(part, part.position++);
		return true;
	case 0xc2:
		if (part.type == kPartRhythm)
			break;
		part.lfoDelay = part.lfoDelay2 = read(part, part.position++);
		lfoReset(part);
		return true;
	case 0xc1:
		return true;
	default:
		break;
	}
	const uint8 length = commandLength(value);
	if (length == 0xff)
		return false;
	part.position += length;
	return true;
}

void PmdPlayer::loopStart(Part &part) {
	const uint target = part.base + readWord(part, part.position);
	part.position += 2;
	if (target + 1 < part.data->size())
		(*part.data)[target + 1] = 0;
}

void PmdPlayer::loopEnd(Part &part) {
	const uint8 count = read(part, part.position++);
	if (count) {
		if (part.position >= part.data->size()) {
			part.position = part.data->size();
			return;
		}
		const uint8 counter = ++(*part.data)[part.position++];
		if (count == counter) {
			part.position += 2;
			return;
		}
	} else {
		++part.position;
		part.loopCheck = 1;
	}
	part.position = part.base + readWord(part, part.position) + 2;
}

void PmdPlayer::loopExit(Part &part) {
	const uint target = part.base + readWord(part, part.position);
	part.position += 2;
	if (uint8(read(part, target) - 1) == read(part, target + 1))
		part.position = target + 4;
}

void PmdPlayer::tempoCommand(Part &part) {
	const uint8 value = read(part, part.position++);
	if (value < 0xfb) {
		setTimerB(value);
		return;
	}
	if (value == 0xff) {
		const uint8 tempo = MAX<uint8>(read(part, part.position++), 0x12);
		const uint quotient = 0x112c / tempo, remainder = 0x112c % tempo;
		setTimerB(uint8(256 - quotient - (remainder >= 0x80 ? 1 : 0)));
		return;
	}
	const int8 delta = read(part, part.position++);
	if (value == 0xfe)
		setTimerB(CLIP<int>(_timerB + delta, 0, 0xfa));
	// 0xfd adjusts the tempo in beats; its effect is approximated through Timer B
	else if (value == 0xfd)
		setTimerB(CLIP<int>(_timerB - delta, 0, 0xfa));
}

void PmdPlayer::keyOff(Part &part) {
	if (part.type == kPartFm)
		fmKeyOff(part);
	else if (part.type == kPartSsg)
		ssgKeyOff(part);
}

void PmdPlayer::fmWrite(const Part &part, uint8 reg, uint8 value) {
	_chip.writeRegister(part.channel / 3, reg + part.channel % 3, value);
}

const byte *PmdPlayer::findVoice(const Part &part, uint8 number, bool effect) const {
	if (effect) {
		const uint table = READ_LE_UINT16(_fmEffects.data() + 0xfe);
		for (uint position = table; position + 26 <= _fmEffects.size(); position += 26)
			if (_fmEffects[position] == number)
				return _fmEffects.data() + position + 1;
		return nullptr;
	}
	if (!_hasVoices)
		return nullptr;
	for (uint position = _voiceTable; position + 26 <= _music.size(); position += 26)
		if (_music[position] == number)
			return _music.data() + position + 1;
	return nullptr;
}

void PmdPlayer::fmSilence(Part &part) {
	for (uint slot = 0; slot < 4; ++slot) {
		fmWrite(part, 0x40 + slot * 4, 0x7f);
		fmWrite(part, 0x80 + slot * 4, 0x7f);
	}
	_keys[part.channel] &= ~part.slotMask;
	_chip.writeRegister(0, 0x28, _keys[part.channel] | (part.channel % 3) | ((part.channel / 3) << 2));
}

void PmdPlayer::fmVoice(Part &part, bool write) {
	const byte *voice = findVoice(part, part.voice, &part == &_fmEffect);
	if (!voice)
		return;
	part.algorithm = voice[24];
	if (!write)
		return;
	fmSilence(part);
	fmWrite(part, 0xb0, voice[24]);
	const uint8 algorithm = voice[24] & 7;
	part.carrier = kCarriers[algorithm];
	part.lfoVolumeMask = part.carrier;
	for (uint slot = 0; slot < 4; ++slot) {
		fmWrite(part, 0x30 + slot * 4, voice[slot]);
		if (kModulators[algorithm] & (0x80 >> slot))
			fmWrite(part, 0x40 + slot * 4, voice[4 + slot]);
		part.tl[slot] = voice[4 + slot];
	}
	for (uint reg = 0; reg < 16; ++reg)
		fmWrite(part, 0x50 + reg * 4, voice[8 + reg]);
}

void PmdPlayer::fmVolume(Part &part) {
	if (!part.slotMask)
		return;
	uint volume = part.volume;
	if (&part != &_fmEffect) {
		if (_fmVolumeDown)
			volume = (volume * (256 - _fmVolumeDown)) >> 8;
		if (_fadeVolume >= 2)
			volume = (volume * (256 - (_fadeVolume >> 1))) >> 8;
	}
	// Attenuation per slot, in the order slot 4, 3, 2, 1
	uint8 attenuation[4] = {0x80, 0x80, 0x80, 0x80};
	const uint8 level = ~volume & 0xff;
	uint8 slots = part.slotMask & part.carrier;
	for (uint index = 0; index < 4; ++index)
		if (slots & (0x80 >> index))
			attenuation[index] = level;
	if (level != 0xff && (part.lfoSwitch & 2)) {
		const uint8 lfoSlots = part.lfoVolumeMask & part.slotMask;
		slots |= lfoSlots;
		const int8 lfo = part.lfoData & 0xff;
		for (uint index = 0; index < 4; ++index)
			if (lfoSlots & (0x80 >> index))
				attenuation[index] = CLIP<int>(attenuation[index] - lfo, 0, 0xff);
	}
	// Slot 4, 3, 2 and 1 use TL registers 0x4c, 0x44, 0x48 and 0x40
	static const uint8 registers[4] = {3, 1, 2, 0};
	for (uint index = 0; index < 4; ++index) {
		if (!(slots & (0x80 >> index)))
			continue;
		const uint slot = registers[index];
		const int tl = MIN<int>(attenuation[index] + part.tl[slot], 0xff) - 0x80;
		fmWrite(part, 0x40 + slot * 4, MAX(tl, 0));
	}
}

void PmdPlayer::fmFrequency(Part &part) {
	if (!part.fnum || !part.slotMask)
		return;
	int block = part.fnum & 0x3800;
	int number = (part.fnum & 0x7ff) + part.detune;
	if (part.lfoSwitch & 1)
		number += part.lfoData;
	for (uint guard = 0; guard < 16; ++guard) {
		if (number >= 0x26a) {
			if (number < 0x4d4)
				break;
			block += 0x800;
			if (block == 0x4000) {
				block = 0x3800;
				number = MIN(number, 0x7ff);
				break;
			}
			number -= 0x26a;
		} else {
			block -= 0x800;
			if (block < 0) {
				block = 0;
				number = MAX(number, 8);
				break;
			}
			number += 0x26a;
		}
	}
	const uint value = block | (number & 0x7ff);
	fmWrite(part, 0xa4, value >> 8);
	fmWrite(part, 0xa0, value & 0xff);
}

void PmdPlayer::fmKeyOn(Part &part) {
	if (part.note == 0xff)
		return;
	_keys[part.channel] |= part.slotMask;
	_chip.writeRegister(0, 0x28, _keys[part.channel] | (part.channel % 3) | ((part.channel / 3) << 2));
}

void PmdPlayer::fmKeyOff(Part &part) {
	if (part.note == 0xff)
		return;
	_keys[part.channel] &= ~part.slotMask;
	_chip.writeRegister(0, 0x28, _keys[part.channel] | (part.channel % 3) | ((part.channel / 3) << 2));
}

void PmdPlayer::ssgWrite(uint8 reg, uint8 value) {
	_chip.writeRegister(0, reg, value);
}

void PmdPlayer::ssgVolume(Part &part) {
	if (part.envFlag == 3 || part.envFlag == 0xff || part.mask)
		return;
	int volume = part.volume;
	if (_fadeVolume)
		volume = (volume * (256 - _fadeVolume)) >> 8;
	if (volume) {
		volume = int8(volume + part.envVolume);
		if (volume < 0)
			volume = 0;
		else if (volume > 15)
			volume = 15;
	}
	if (volume && (part.lfoSwitch & 0x22)) {
		if (part.lfoSwitch & 2)
			volume += part.lfoData;
		volume = CLIP(volume, 0, 15);
	}
	ssgWrite(8 + part.channel, volume);
}

void PmdPlayer::ssgFrequency(Part &part) {
	if (!part.fnum || part.mask)
		return;
	int period = part.fnum - part.detune;
	if (part.lfoSwitch & 1)
		period -= part.lfoData;
	ssgWrite(part.channel * 2, period & 0xff);
	ssgWrite(part.channel * 2 + 1, (period >> 8) & 0x0f);
}

void PmdPlayer::ssgKeyOn(Part &part) {
	if (part.note == 0xff)
		return;
	const uint8 bits = (1 << part.channel) | (8 << part.channel);
	_ssgMixer = (_ssgMixer | bits) & ~(bits & part.ssgPattern);
	ssgWrite(7, _ssgMixer);
	if (!_ssgEffectPriority && _noise != _ssgEffectNoise) {
		_ssgEffectNoise = _noise;
		ssgWrite(6, _noise);
	}
}

void PmdPlayer::ssgKeyOff(Part &part) {
	if (part.note == 0xff)
		return;
	if (part.envFlag != 0xff)
		part.envFlag = 2;
}

void PmdPlayer::rhythmRun(Part &part) {
	if (!part.active)
		return;
	if (--part.length) {
		_loopWork &= part.loopCheck;
		return;
	}
	for (uint guard = 0; guard < kGuard; ++guard) {
		const uint8 value = _rhythmPattern ? read(part, _rhythmPosition++) : 0xff;
		if (value == 0xff) {
			const uint8 next = read(part, part.position++);
			if (next == 0x80) {
				--part.position;
				bool stop;
				endOfData(part, stop);
				if (stop) {
					_rhythmPattern = false;
					_tie = false;
					_loopWork &= part.loopCheck;
					return;
				}
			} else if (next > 0x80) {
				if (!command(part, next))
					break;
			} else {
				_rhythmPosition = part.base + readWord(part, _rhythmTable + next * 2);
				_rhythmPattern = true;
			}
			continue;
		}
		if ((value & 0xc0) == 0xc0) {
			// Pattern commands share the part state but read from the pattern
			const uint position = part.position;
			part.position = _rhythmPosition;
			const bool valid = command(part, value);
			_rhythmPosition = part.position;
			part.position = position;
			if (!valid)
				break;
			continue;
		}
		if (value & 0x80) {
			const uint16 bits = ((value << 8) | read(part, _rhythmPosition++)) & 0x3fff;
			if (bits && !part.mask)
				rhythmShot(bits);
		}
		part.length = read(part, _rhythmPosition++);
		_tie = false;
		_loopWork &= part.loopCheck;
		return;
	}
	part.active = false;
	part.loopCheck = 3;
	_loopWork &= part.loopCheck;
}

void PmdPlayer::rhythmShot(uint16 bits) {
	if (_opna) {
		for (uint index = 0; index < ARRAYSIZE(kRhythmSounds); ++index) {
			if (!(bits & (1 << index)))
				continue;
			_chip.writeRegister(0, kRhythmSounds[index][0], kRhythmSounds[index][1]);
			const uint8 key = kRhythmSounds[index][2];
			if (key & 0x80)
				_chip.writeRegister(0, 0x10, 0x84);
			_chip.writeRegister(0, 0x10, key & 0x7f);
		}
		if (_fadeVolume)
			_chip.writeRegister(0, 0x11, (_rhythmLevel * (256 - _fadeVolume)) >> 8);
	}
	if (_fadeVolume)
		return;
	for (uint index = 0; index < 14; ++index) {
		if (bits & (1 << index)) {
			playSsgEffect(index);
			return;
		}
	}
}

void PmdPlayer::playSsgEffect(uint8 number) {
	ssgEffectStart(number);
}

void PmdPlayer::ssgEffectStart(uint8 number) {
	if (number >= _ssgEffects.size())
		return;
	const SsgEffect &effect = _ssgEffects[number];
	if (_ssgEffectPriority > effect.priority)
		return;
	if (_partCount)
		_parts[2].mask |= 2;
	_ssgEffectPosition = effect.offset;
	if (ssgEffectFrame())
		_ssgEffectPriority = effect.priority;
}

bool PmdPlayer::ssgEffectFrame() {
	const Common::Array<byte> &data = _ssgEffectData;
	if (_ssgEffectPosition >= data.size() || data[_ssgEffectPosition] == 0xff || _ssgEffectPosition + 11 > data.size()) {
		ssgEffectEnd();
		return false;
	}
	const byte *frame = data.data() + _ssgEffectPosition;
	_ssgEffectCount = frame[0];
	_ssgEffectTone = READ_LE_UINT16(frame + 1);
	ssgWrite(4, frame[1]);
	ssgWrite(5, frame[2]);
	_ssgEffectNoise = frame[3];
	ssgWrite(6, frame[3]);
	const uint8 mix = ((frame[4] << 2) | (frame[4] >> 6)) & 0x24;
	_ssgMixer = (_ssgMixer & 0xdb) | mix;
	ssgWrite(7, _ssgMixer);
	for (uint index = 0; index < 4; ++index)
		ssgWrite(10 + index, frame[5 + index]);
	_ssgEffectSweep = (int8)frame[9];
	_ssgEffectNoiseSweep = frame[10];
	_ssgEffectNoiseCount = frame[10] & 0x0f;
	_ssgEffectPosition += 11;
	return true;
}

void PmdPlayer::ssgEffectTick() {
	if (!--_ssgEffectCount) {
		ssgEffectFrame();
		return;
	}
	_ssgEffectTone += _ssgEffectSweep;
	ssgWrite(4, _ssgEffectTone & 0xff);
	ssgWrite(5, _ssgEffectTone >> 8);
	ssgWrite(7, _ssgMixer);
	if (_ssgEffectNoiseSweep && !--_ssgEffectNoiseCount) {
		_ssgEffectNoiseCount = _ssgEffectNoiseSweep & 0x0f;
		_ssgEffectNoise += int8(_ssgEffectNoiseSweep) >> 4;
		ssgWrite(6, _ssgEffectNoise);
	}
}

void PmdPlayer::ssgEffectEnd() {
	ssgWrite(10, 0);
	_ssgMixer = (_ssgMixer & 0xdb) | 0x24;
	ssgWrite(7, _ssgMixer);
	_ssgEffectPriority = 0;
}

void PmdPlayer::playFmEffect(uint8 number) {
	if (_fmEffects.empty() || number >= 0x7f)
		return;
	stopFmEffect();
	initPart(_fmEffect, kPartFm, 2, &_fmEffects, 0, READ_LE_UINT16(_fmEffects.data() + number * 2));
	if (!_fmEffect.active)
		return;
	_fmEffectActive = true;
	if (_partCount)
		_parts[4 + 2].mask |= 2;
}

void PmdPlayer::stopFmEffect() {
	if (!_fmEffectActive)
		return;
	_fmEffectActive = false;
	fmSilence(_fmEffect);
	if (_playing && _partCount && _parts[4 + 2].active)
		fmVoice(_parts[4 + 2], true);
}

} // namespace System98
