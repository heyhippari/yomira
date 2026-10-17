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

#include "common/memstream.h"
#include "common/savefile.h"
#include "common/system.h"
#include "common/translation.h"

#include "xex/xex.h"

namespace Xex {

enum {
	kSaveVersion = 1,
	// The original's quick save has its own file next to the 20 numbered slots
	kQuickSaveSlot = 99
};

static const uint32 kSaveMagic = MKTAG('C', 'M', 'P', 'L');

void XexEngine::markResumePoint() {
	Common::MemoryWriteStreamDynamic stream(DisposeAfterUse::YES);
	_script.saveState(stream);

	writeString(stream, _sceneFull);
	writeString(stream, _sceneBg);
	writeString(stream, _sceneFg);
	writeString(stream, _bgName);
	stream.writeByte(_bgPending);

	bool loop;
	writeString(stream, _audio.getMusic(loop));
	stream.writeByte(loop);

	_resumePoint.resize(stream.size());
	memcpy(_resumePoint.data(), stream.getData(), stream.size());
}

bool XexEngine::canLoadGameStateCurrently(Common::U32String *msg) {
	return _inputWait && !_replaying;
}

bool XexEngine::canSaveGameStateCurrently(Common::U32String *msg) {
	// Like the original, the title and the recollection scenarios cannot be saved
	const Common::String &scenario = _script.getScenarioName();
	return _inputWait && !_replaying && !_resumePoint.empty() &&
		!scenario.equalsIgnoreCase("startup") && !scenario.equalsIgnoreCase("kaisou");
}

Common::Error XexEngine::saveGameStream(Common::WriteStream *stream, bool isAutosave) {
	stream->writeUint32BE(kSaveMagic);
	stream->writeByte(kSaveVersion);
	// Text position to come back to; zero when waiting in a menu
	stream->writeUint32LE(_inText ? _textPos : 0);
	stream->writeUint32LE(_resumePoint.size());
	stream->write(_resumePoint.data(), _resumePoint.size());
	return stream->err() ? Common::kWritingFailed : Common::kNoError;
}

Common::Error XexEngine::loadGameStream(Common::SeekableReadStream *stream) {
	if (stream->readUint32BE() != kSaveMagic || stream->readByte() != kSaveVersion)
		return Common::Error(Common::kReadingFailed, "Unsupported savegame");

	uint32 textPos = stream->readUint32LE();
	uint32 size = stream->readUint32LE();
	if (stream->err() || size > stream->size() - stream->pos())
		return Common::kReadingFailed;

	Common::Array<byte> state(size);
	if (stream->read(state.data(), size) != size)
		return Common::kReadingFailed;

	// The interpreter may be deep inside a text; it unwinds before the state is applied
	_pendingState = state;
	_pendingTextPos = textPos;
	_loadPending = true;
	_script.stop();
	return Common::kNoError;
}

bool XexEngine::applyPendingLoad() {
	_loadPending = false;

	Common::MemoryReadStream stream(_pendingState.data(), _pendingState.size());
	if (!_script.loadState(stream))
		return false;

	_sceneFull = readString(stream);
	_sceneBg = readString(stream);
	_sceneFg = readString(stream);
	_bgName = readString(stream);
	_bgPending = stream.readByte();
	Common::String music = readString(stream);
	bool loop = stream.readByte();
	if (stream.err() || stream.eos())
		return false;

	_resumePoint = _pendingState;

	_audio.stopVoice();
	if (music.empty())
		_audio.stopMusic();
	else
		_audio.playMusic(music, loop);

	rebuildScene();
	clearMessage();
	resetBacklog();
	_menu = nullptr;
	_hover = -1;
	_messageHidden = false;
	_fastText = false;

	// Run the interrupted text silently up to where it was saved
	_replayEnd = _pendingTextPos;
	_replaying = _replayEnd != 0;
	present();
	return true;
}

// Restores the background layer and the screen from the image names
void XexEngine::rebuildScene() {
	const uint size = _screenWidth * _screenHeight * 3;
	_bg.resize(size);
	memset(_bg.data(), 0, size);
	if (!_bgName.empty())
		loadImage(_bgName, _bg);

	_scene.resize(size);
	memset(_scene.data(), 0, size);
	if (!_sceneFull.empty()) {
		loadImage(_sceneFull, _scene);
	} else if (!_sceneFg.empty()) {
		if (!_sceneBg.empty())
			loadImage(_sceneBg, _scene);
		composeSprites(_scene, _sceneFg);
	}
}

void XexEngine::quickSave() {
	if (!canSaveGameStateCurrently()) {
		g_system->displayMessageOnOSD(_("Saving game is currently unavailable"));
		return;
	}

	if (saveGameState(kQuickSaveSlot, "Quick save").getCode() == Common::kNoError)
		g_system->displayMessageOnOSD(_("Game saved"));
	else
		g_system->displayMessageOnOSD(_("Failed to save game"));
}

void XexEngine::quickLoad() {
	if (!canLoadGameStateCurrently() || !getSaveFileManager()->exists(getSaveStateName(kQuickSaveSlot))) {
		g_system->displayMessageOnOSD(_("No quick save to load"));
		return;
	}

	if (loadGameState(kQuickSaveSlot).getCode() != Common::kNoError)
		g_system->displayMessageOnOSD(_("Failed to load saved game"));
}

} // End of namespace Xex
