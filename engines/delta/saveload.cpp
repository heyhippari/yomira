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

#include "delta/delta.h"
#include "common/memstream.h"
#include "common/ptr.h"

namespace Delta {

static void writeSaveString(Common::WriteStream &stream, const Common::String &text) {
	stream.writeUint16LE(text.size());
	stream.writeString(text);
}

static bool readSaveString(Common::SeekableReadStream &stream, Common::String &text) {
	const uint16 length = stream.readUint16LE();
	char buffer[256];
	if (length >= sizeof(buffer) || stream.size() - stream.pos() < length || stream.read(buffer, length) != length)
		return false;
	text = Common::String(buffer, length);
	return !stream.err();
}

bool DeltaEngine::canLoadGameStateCurrently(Common::U32String *msg) {
	return _script && _atMenu && !_optionsActive && !_loadPending && !_transitionDuration;
}

bool DeltaEngine::canSaveGameStateCurrently(Common::U32String *msg) {
	return canLoadGameStateCurrently(msg);
}

Common::Error DeltaEngine::saveGameStream(Common::WriteStream *stream, bool isAutosave) {
	if (!canSaveGameStateCurrently())
		return Common::kWritingFailed;
	stream->writeUint32BE(MKTAG('D', 'L', 'T', 'A'));
	stream->writeByte(1);
	writeSaveString(*stream, getGameId());
	stream->writeUint32LE(_scenario.size());
	stream->writeUint32LE(_configuration.scenarioNumber);
	stream->writeUint32LE(_randomSource.getSeed());
	stream->writeUint16LE(_menuResultVariable);
	stream->writeSint16LE(_menuHover);
	if (!_script->saveState(*stream))
		return Common::kWritingFailed;
	for (uint index = 0; index < MenuLayout::kArgumentCount; ++index)
		stream->writeSint32LE(_menuConfig.arguments[index]);
	for (uint index = 0; index < 2; ++index)
		writeSaveString(*stream, _menuConfig.strings[index]);
	return stream->err() ? Common::kWritingFailed : Common::kNoError;
}

bool DeltaEngine::restoreSaveStream(Common::SeekableReadStream &stream, bool apply) {
	if (stream.readUint32BE() != MKTAG('D', 'L', 'T', 'A') || stream.readByte() != 1)
		return false;
	Common::String game;
	if (!readSaveString(stream, game) || game != getGameId() || stream.readUint32LE() != _scenario.size() ||
		stream.readUint32LE() != _configuration.scenarioNumber)
		return false;
	const uint32 seed = stream.readUint32LE();
	const uint16 variable = stream.readUint16LE();
	const int16 hover = stream.readSint16LE();
	Common::ScopedPtr<Script> script(new Script(_scenario, getGameId() == "onegaitasukete", &_randomSource));
	if (!script->loadState(stream))
		return false;
	ScriptCommand menu;
	for (uint index = 0; index < MenuLayout::kArgumentCount; ++index)
		menu.arguments.push_back(stream.readSint32LE());
	for (uint index = 0; index < 2; ++index) {
		Common::String name;
		if (!readSaveString(stream, name) || name.empty() || name.contains('/') || name.contains('\\') || name.contains(':'))
			return false;
		menu.strings.push_back(name);
	}
	const MenuLayout layout(menu.arguments);
	bool valid = !stream.err() && !stream.eos() && variable < 1024 && layout.hasButtons() &&
		hover >= -1 && hover < layout.count;
	for (uint index = 2; index < MenuLayout::kArgumentCount; ++index)
		valid = valid && menu.arguments[index] >= 0 && menu.arguments[index] <= 800;
	valid = valid && layout.step > 0 &&
		layout.left + layout.width <= 800 &&
		layout.top + (layout.count - 1) * layout.step + layout.height <= 600;
	if (!valid || !apply)
		return valid;
	delete _script;
	_script = script.release();
	_scriptPosition = _script->position();
	_randomSource.setSeed(seed);
	_menuConfig = menu;
	_menuResultVariable = variable;
	_menuHover = hover;
	_menuPressed = -1;
	_transitionDuration = 0;
	_waiting = false;
	_atMenu = false;
	return true;
}

Common::Error DeltaEngine::loadGameStream(Common::SeekableReadStream *stream) {
	const int64 start = stream->pos();
	if (!restoreSaveStream(*stream, false))
		return Common::Error(Common::kReadingFailed, "Invalid or incompatible Delta savegame");
	const int64 length = stream->pos() - start;
	if (length <= 0 || length > 65536 || !stream->seek(start))
		return Common::kReadingFailed;
	Common::Array<byte> state;
	state.resize(length);
	if (stream->read(state.begin(), state.size()) != state.size())
		return Common::kReadingFailed;
	_pendingState = state;
	_loadPending = true;
	return Common::kNoError;
}

Common::Error DeltaEngine::applyPendingLoad() {
	_loadPending = false;
	Common::MemoryReadStream stream(_pendingState.begin(), _pendingState.size());
	if (!restoreSaveStream(stream, true))
		return Common::kReadingFailed;
	_uiBusy = true;
	const Common::Error result = enterMenu();
	_uiBusy = false;
	return result;
}

} // End of namespace Delta
