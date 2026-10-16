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

#ifndef DELTA_DATA_H
#define DELTA_DATA_H

#include "common/array.h"
#include "common/func.h"
#include "common/hash-str.h"
#include "common/rect.h"
#include "common/stream.h"
#include "common/str-array.h"
#include "common/ustr.h"

namespace Graphics {
class Font;
class ManagedSurface;
struct Surface;
struct PixelFormat;
}

namespace Delta {

struct StartupConfig {
	uint32 scenarioNumber = 0;
	Common::String title;
	Common::String resourcePrefix;
	Common::String scenarioFile;
	Common::String optionsFile;
	Common::String saveSignature;
	uint32 saveSignatureSize = 0;
	uint32 saveSize = 0;
	Common::String soundPrefix;
	Common::String systemVoicePrefix;
	Common::String defaultNames[2];
	Common::Rect messageRect;
	Common::Point textPosition;
	Common::HashMap<Common::String, Common::StringArray> stringTables;
};

typedef Common::Functor0<bool> LoadProgress;

bool readPackedData(Common::SeekableReadStream &stream, Common::Array<byte> &data, LoadProgress *progress = nullptr);
bool readIafData(Common::SeekableReadStream &stream, Common::Array<byte> &data, LoadProgress *progress = nullptr);
bool readIafImage(Common::SeekableReadStream &stream, Graphics::Surface &surface, const Graphics::PixelFormat &format, LoadProgress *progress = nullptr);
bool findIafRecord(Common::SeekableReadStream &stream, const Common::String &name, uint32 &offset, uint32 &size, LoadProgress *progress = nullptr);
bool readSpriteBankImage(Common::SeekableReadStream &stream, const Common::String &name, Graphics::Surface &color, Graphics::Surface &mask, const Graphics::PixelFormat &format, LoadProgress *progress = nullptr, Common::Point *position = nullptr);
bool readSpriteInfo(Common::SeekableReadStream &stream, const Common::String &name, Common::StringArray &layers, LoadProgress *progress = nullptr);
bool compositeMasked(Graphics::Surface &destination, const Graphics::Surface &color, const Graphics::Surface &mask, LoadProgress *progress = nullptr, const Common::Point &position = Common::Point(), byte frameTransparency = 1);
void drawShadowedString(const Graphics::Font &font, Graphics::ManagedSurface &destination, const Common::U32String &text, int x, int y, int width, uint32 foreground, uint32 shadow);
bool readStartupConfig(Common::SeekableReadStream &stream, StartupConfig &config, LoadProgress *progress = nullptr);
bool readScenarioData(Common::SeekableReadStream &stream, Common::Array<byte> &data, LoadProgress *progress = nullptr);

} // End of namespace Delta

#endif
