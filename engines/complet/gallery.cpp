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

#include "common/events.h"
#include "common/system.h"

#include "complet/complet.h"
#include "complet/detection.h"

namespace Complet {

enum {
	kCellColumns = 10,
	kCellsPerPage = 90,
	kNextPageCell = 98,
	kExitCell = 99
};

// Full-screen graphics shown through the script are what unlock gallery entries
void CompletEngine::recordSeenCg(const Common::String &name) {
	for (int i = 0; i < _gallery->count; i++) {
		if (name.equalsIgnoreCase(_gallery->names[i])) {
			if (!_seenCg[i]) {
				_seenCg[i] = true;
				saveCommonData();
			}
			return;
		}
	}
}

void CompletEngine::unlockAllCgs() {
	for (int i = 0; i < _gallery->count; i++)
		_seenCg[i] = true;
	saveCommonData();
}

// Copies a rectangle of the frame image; source rows are bottom-up like the original's
void CompletEngine::blitFrame(Common::Array<byte> &dst, int sx, int sy, int w, int h, int dx, int dy) const {
	const byte *frame = _frameImage.getPixels();
	for (int k = 0; k < h; k++) {
		int topDownRow = dy + (h - 1 - k);
		int dstRow = _screenHeight - 1 - topDownRow;
		if (dstRow < 0 || dstRow >= _screenHeight || dx < 0 || dx + w > _screenWidth)
			continue;
		memcpy(&dst[(dstRow * _screenWidth + dx) * 3], frame + ((sy + k) * _screenWidth + sx) * 3, w * 3);
	}
}

static void drawBox(Common::Array<byte> &buf, int width, int height, int x0, int y0, int x1, int y1, const byte color[3]) {
	for (int y = y0; y < y1; y++) {
		int row = height - 1 - y;
		for (int x = x0; x < x1; x++)
			memcpy(&buf[(row * width + x) * 3], color, 3);
	}
}

// Wait for a click or key; false on quit
bool CompletEngine::waitAnyInput() {
	Common::Event event;
	while (!shouldQuit()) {
		while (g_system->getEventManager()->pollEvent(event)) {
			if (event.type == Common::EVENT_LBUTTONUP || event.type == Common::EVENT_RBUTTONUP ||
					event.type == Common::EVENT_KEYDOWN || event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START) {
				refreshCursor();
				return true;
			}
		}
		refreshCursor();
		g_system->delayMillis(10);
	}
	return false;
}

bool CompletEngine::runGroupedGallery(int group) {
	if (group < 0 || group >= _gallery->groupCount)
		return true;
	const int first = group ? _gallery->groupEnds[group - 1] + 1 : 0;
	const int last = _gallery->groupEnds[group];
	if (first > last || last >= _gallery->count)
		return true;

	const bool restoreMessage = _messageVisible;
	_messageVisible = false;
	Common::Array<byte> lockedOverlay;
	loadImage("cgndisp", lockedOverlay);
	int selected = first;
	int displayed = -1;
	bool leave = false;
	while (!leave && !interrupted()) {
		if (selected != displayed) {
			Common::Array<byte> image;
			if (!loadImage(_gallery->names[selected], image))
				break;
			if (!_seenCg[selected]) {
				Common::Array<byte> locked(image.size());
				memset(locked.data(), 0, locked.size());
				for (int row = 0; row < _screenHeight / 2; row++) {
					for (int column = 0; column < _screenWidth / 2; column++) {
						const byte *src = &image[(row * 2 * _screenWidth + column * 2) * 3];
						byte *dst = &locked[((row + _screenHeight / 4) * _screenWidth + column + _screenWidth / 4) * 3];
						for (int channel = 0; channel < 3; channel++)
							dst[channel] = src[channel] / 6;
					}
				}
				if (!lockedOverlay.empty())
					overlaySprite(locked, lockedOverlay);
				image = locked;
			}
			presentBuffer(image.data());
			displayed = selected;
		}
		Common::Event event;
		while (!leave && g_system->getEventManager()->pollEvent(event)) {
			int direction = 0;
			if (event.type == Common::EVENT_LBUTTONUP)
				direction = 1;
			else if (event.type == Common::EVENT_RBUTTONUP)
				direction = -1;
			else if (event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START) {
				if (event.customType == kActionAdvance || event.customType == kActionRight || event.customType == kActionBacklogForward)
					direction = 1;
				else if (event.customType == kActionLeft || event.customType == kActionBacklog)
					direction = -1;
				else if (event.customType == kActionHide)
					leave = true;
			}
			if (direction > 0 && selected == last)
				leave = true;
			else
				selected = MAX(first, selected + direction);
		}
		refreshCursor();
		g_system->delayMillis(10);
	}
	_messageVisible = restoreMessage;
	present();
	return !interrupted();
}

bool CompletEngine::runMap() {
	struct MapRegion {
		int left, top, right, bottom;
		int previewX, previewY;
	};
	const MapRegion regions[] = {
		{44, 112, 76, 164, 240, 180},
		{76, 372, 139, 418, 240, 180},
		{410, 386, 492, 424, 240, 180},
		{370, 57, 450, 114, 190, 180},
		{138, 90, 228, 138, 240, 180},
		{170, 241, 262, 310, 280, 110},
		{342, 166, 489, 249, 150, 20},
		{490, 166, 639, 249, 170, 20},
		{342, 250, 489, 328, 150, 40},
		{490, 250, 639, 328, 170, 40},
		{171, 361, 259, 414, 300, 180},
		{323, 134, 353, 161, 150, 240}
	};
	auto regionAt = [&](const Common::Point &point) {
		for (uint region = 0; region < ARRAYSIZE(regions); region++) {
			const MapRegion &bounds = regions[region];
			if (point.x >= bounds.left && point.x <= bounds.right && point.y >= bounds.top && point.y <= bounds.bottom)
				return (int)region;
		}
		return -1;
	};

	const bool restoreMessage = _messageVisible;
	_messageVisible = false;
	_skipMode = false;
	Common::Array<byte> previews;
	if (!loadImage("pt01", previews) || !showFullImage("map", true)) {
		_messageVisible = restoreMessage;
		return false;
	}
	Common::Point pointer = g_system->getEventManager()->getMousePos();
	int selected = regionAt(pointer);
	bool redraw = true;
	bool accepted = false;
	while (!accepted && !interrupted()) {
		Common::Event event;
		while (!accepted && g_system->getEventManager()->pollEvent(event)) {
			if (handleSkipAction(event))
				continue;
			if (event.type == Common::EVENT_MOUSEMOVE || event.type == Common::EVENT_LBUTTONUP) {
				pointer = event.mouse;
				selected = regionAt(pointer);
				redraw = true;
				accepted = event.type == Common::EVENT_LBUTTONUP && selected >= 0;
			} else if (event.type == Common::EVENT_CUSTOM_ENGINE_ACTION_START) {
				int direction = 0;
				if (event.customType == kActionRight || event.customType == kActionDown)
					direction = 1;
				else if (event.customType == kActionLeft || event.customType == kActionUp)
					direction = -1;
				else if (event.customType == kActionAdvance)
					accepted = selected >= 0;
				if (direction) {
					const int count = ARRAYSIZE(regions);
					selected = selected < 0 ? (direction > 0 ? 0 : count - 1) : (selected + direction + count) % count;
					const MapRegion &bounds = regions[selected];
					pointer = Common::Point((bounds.left + bounds.right) / 2, (bounds.top + bounds.bottom) / 2);
					redraw = true;
				}
			}
		}
		if (redraw && !accepted && !interrupted()) {
			Common::Array<byte> view = _scene;
			if (selected >= 0) {
				const MapRegion &region = regions[selected];
				const int sourceX = (selected % 4) * 160;
				const int sourceY = (selected / 4) * 120;
				for (int row = 0; row < 120; row++) {
					const byte *src = &previews[((_screenHeight - 1 - sourceY - row) * _screenWidth + sourceX) * 3];
					byte *dst = &view[((_screenHeight - 1 - region.previewY - row) * _screenWidth + region.previewX) * 3];
					memcpy(dst, src, 160 * 3);
				}
			}
			const byte *frame = _frameImage.getPixels();
			if (pointer.y >= 0 && pointer.y < _screenHeight)
				memcpy(&view[(_screenHeight - 1 - pointer.y) * _screenWidth * 3], frame + 240 * _screenWidth * 3, _screenWidth * 3);
			if (pointer.x >= 0 && pointer.x < _screenWidth) {
				for (int row = 0; row < _screenHeight; row++) {
					const byte *src = frame + ((399 - row % 160) * _screenWidth + 320) * 3;
					memcpy(&view[((_screenHeight - 1 - row) * _screenWidth + pointer.x) * 3], src, 3);
				}
			}
			presentBuffer(view.data());
			redraw = false;
		}
		refreshCursor();
		if (!accepted)
			g_system->delayMillis(10);
	}
	if (accepted)
		_script.setVariable(0x1fe, selected + 1);
	_messageVisible = restoreMessage;
	present();
	return !interrupted();
}

bool CompletEngine::runGallery() {
	const int thumbWidth = _screenWidth / kCellColumns;
	const int thumbHeight = _screenHeight / kCellColumns;
	Common::Array<int> seen;
	for (int i = 0; i < _gallery->count; i++) {
		if (_seenCg[i])
			seen.push_back(i);
	}
	const int count = seen.size();
	const int pageCount = count > kCellsPerPage ? 2 : 1;

	const byte yellow[3] = {0x00, 0xff, 0xff};
	const byte black[3] = {0, 0, 0};
	const byte highlight[3] = {0x33, 0xfe, 0xfe};

	// Static parts of each page, in GD pixel layout
	Common::Array<byte> pages[2];
	for (int p = 0; p < pageCount; p++) {
		Common::Array<byte> &page = pages[p];
		page.resize(_screenWidth * _screenHeight * 3);
		memset(page.data(), 0, page.size());

		int first = p * kCellsPerPage;
		for (int i = 0; i < kCellsPerPage && first + i < count; i++) {
			Common::Array<byte> thumb;
			if (!GdImage::loadThumbnail(_gallery->names[seen[first + i]], thumb)) {
				thumb.resize(thumbWidth * thumbHeight * 3);
				memset(thumb.data(), 0x7f, thumb.size());
			}

			int cellX = (i % kCellColumns) * thumbWidth;
			int cellTop = (i / kCellColumns) * thumbHeight;
			for (int row = 0; row < thumbHeight; row++) {
				int dstRow = _screenHeight - 1 - (cellTop + thumbHeight - 1 - row);
				memcpy(&page[(dstRow * _screenWidth + cellX) * 3], &thumb[row * thumbWidth * 3], thumbWidth * 3);
			}
		}

		if (_xexLayout) {
			blitFrame(page, 0, 0xc0, 640, 48, 0, 0x1af);
			int width = count * 500 / _gallery->count;
			if (width) {
				drawBox(page, _screenWidth, _screenHeight, 0x13, 0x1b8, width + 0x15, 0x1d8, black);
				drawBox(page, _screenWidth, _screenHeight, 0x14, 0x1b9, width + 0x14, 0x1d7, yellow);
			}
			int tenths = count * 1000 / _gallery->count;
			if (tenths < 1000) {
				if (tenths >= 100)
					blitFrame(page, (tenths / 100) * 8, 0xb4, 8, 12, 0x20e, 0x1cf);
				blitFrame(page, ((tenths % 100) / 10) * 8, 0xb4, 8, 12, 0x216, 0x1cf);
				blitFrame(page, 0x50, 0xb4, 8, 12, 0x21e, 0x1cf);
				blitFrame(page, (tenths % 10) * 8, 0xb4, 8, 12, 0x222, 0x1cf);
				blitFrame(page, 0x58, 0xb4, 16, 12, 0x22a, 0x1cf);
			} else {
				blitFrame(page, 8, 0xb4, 8, 12, 0x20e, 0x1cf);
				blitFrame(page, 0, 0xb4, 8, 12, 0x216, 0x1cf);
				blitFrame(page, 0, 0xb4, 8, 12, 0x21e, 0x1cf);
				blitFrame(page, 0x58, 0xb4, 16, 12, 0x226, 0x1cf);
			}
			if (pageCount > 1)
				blitFrame(page, 0, 0xa7, 0x33, 12, 0x20a, 0x1b7);
			continue;
		}

		// Progress bar showing how many CGs have been seen
		int width = count * scaleX(600) / _gallery->count;
		if (width) {
			drawBox(page, _screenWidth, _screenHeight, scaleX(0x13), scaleY(0x240), width + scaleX(0x15), scaleY(0x24e), black);
			drawBox(page, _screenWidth, _screenHeight, scaleX(0x14), scaleY(0x241), width + scaleX(0x14), scaleY(0x24d), yellow);
		}
		blitFrame(page, scaleX(0x284), scaleY(0x239), scaleX(0x33), scaleY(0x15), scaleX(0x2de), scaleY(0x230));
		blitFrame(page, scaleX(0x2b8), scaleY(0x24c), scaleX(10), scaleY(4), scaleX(0x10), scaleY(0x23c));
		blitFrame(page, scaleX(0x2b8), scaleY(0x24c), scaleX(10), scaleY(12), width + scaleX(0xf), scaleY(0x234));

		int tenths = count * 1000 / _gallery->count;
		const int digitY = scaleY(0x24f), digitSrcX = scaleX(0x250), labelY = scaleY(0x22a);
		if (tenths < 1000) {
			if (tenths >= 100)
				blitFrame(page, scaleX((tenths / 100) * 8) + digitSrcX, digitY, scaleX(8), scaleY(9), width + scaleX(1), labelY);
			blitFrame(page, scaleX(((tenths % 100) / 10) * 8) + digitSrcX, digitY, scaleX(8), scaleY(9), width + scaleX(9), labelY);
			blitFrame(page, scaleX(0x2a0), digitY, scaleX(8), scaleY(9), width + scaleX(0x11), labelY);
			blitFrame(page, scaleX((tenths % 10) * 8) + digitSrcX, digitY, scaleX(8), scaleY(9), width + scaleX(0x15), labelY);
			blitFrame(page, scaleX(0x2a8), digitY, scaleX(0x10), scaleY(9), width + scaleX(0x1d), labelY);
		} else {
			blitFrame(page, scaleX(600), digitY, scaleX(8), scaleY(9), width + scaleX(3), labelY);
			blitFrame(page, digitSrcX, digitY, scaleX(8), scaleY(9), width + scaleX(0xb), labelY);
			blitFrame(page, digitSrcX, digitY, scaleX(8), scaleY(9), width + scaleX(0x13), labelY);
			blitFrame(page, scaleX(0x2a8), digitY, scaleX(0x10), scaleY(9), width + scaleX(0x1b), labelY);
		}
		if (pageCount > 1)
			blitFrame(page, scaleX(0x250), scaleY(0x23c), scaleX(0x33), scaleY(0xc), scaleX(0x290), scaleY(0x235));
	}

	bool restoreMessage = _messageVisible;
	_messageVisible = false;

	auto cellAt = [&](int page, const Common::Point &pos) {
		int cell = pos.x / thumbWidth + (pos.y / thumbHeight) * kCellColumns;
		if (pos.x < 0 || pos.y < 0 || pos.x >= _screenWidth || pos.y >= _screenHeight)
			return -1;
		if (cell == kNextPageCell || cell == kExitCell)
			return cell;
		int onPage = count - page * kCellsPerPage;
		if (onPage > kCellsPerPage)
			onPage = kCellsPerPage;
		return (cell < onPage) ? cell : -1;
	};

	auto show = [&](int page, int selected) {
		Common::Array<byte> view = pages[page];
		if (selected >= 0 && selected < kCellsPerPage) {
			int cellX = (selected % kCellColumns) * thumbWidth;
			int cellTop = (selected / kCellColumns) * thumbHeight;
			for (int rowIndex = 0; rowIndex < thumbHeight; rowIndex++) {
				int row = _screenHeight - 1 - (cellTop + thumbHeight - 1 - rowIndex);
				bool edge = rowIndex < 2 || rowIndex >= thumbHeight - 2;
				for (int x = 0; x < thumbWidth; x++) {
					if (edge || x < 2 || (x >= thumbWidth - 4 && x < thumbWidth - 2))
						memcpy(&view[(row * _screenWidth + cellX + x) * 3], highlight, 3);
				}
			}
		}
		presentBuffer(view.data());
	};

	int page = 0;
	bool ok = true;
	bool leave = false;
	int selected = -1;
	show(page, selected);

	while (ok && !leave && !shouldQuit()) {
		int previousSelected = selected;
		int activated = -2;
		Common::Event event;
		while (activated < 0 && g_system->getEventManager()->pollEvent(event)) {
			switch (event.type) {
			case Common::EVENT_MOUSEMOVE: {
				int cell = cellAt(page, event.mouse);
				selected = cell < kCellsPerPage ? cell : -1;
				break;
			}
			case Common::EVENT_LBUTTONUP:
			case Common::EVENT_RBUTTONUP:
				activated = cellAt(page, event.mouse);
				break;
			case Common::EVENT_CUSTOM_ENGINE_ACTION_START: {
				int onPage = MIN(count - page * kCellsPerPage, (int)kCellsPerPage);
				switch (event.customType) {
				case kActionLeft:
					selected = onPage ? (selected <= 0 ? onPage - 1 : selected - 1) : -1;
					break;
				case kActionRight:
					selected = onPage ? (selected + 1) % onPage : -1;
					break;
				case kActionUp:
					selected = onPage ? (selected < 0 ? 0 : (selected >= kCellColumns ? selected - kCellColumns : selected)) : -1;
					break;
				case kActionDown:
					selected = onPage ? (selected < 0 ? 0 : (selected + kCellColumns < onPage ? selected + kCellColumns : selected)) : -1;
					break;
				case kActionAdvance:
					activated = selected;
					break;
				case kActionBacklog:
				case kActionBacklogForward:
					activated = kNextPageCell;
					break;
				case kActionHide:
					activated = kExitCell;
					break;
				default:
					break;
				}
				break;
			}
			default:
				break;
			}
		}

		if (selected != previousSelected)
			show(page, selected);
		refreshCursor();
		if (activated == kExitCell) {
			leave = true;
		} else if (activated == kNextPageCell) {
			if (pageCount > 1) {
				page ^= 1;
				selected = -1;
				show(page, selected);
			}
		} else if (activated >= 0) {
			GdImage image;
			if (image.loadFromName(Common::String(_gallery->names[seen[page * kCellsPerPage + activated]]) + ".gd")) {
				presentBuffer(image.getPixels());
				ok = waitAnyInput();
				if (ok)
					show(page, selected);
			}
		}

		g_system->delayMillis(10);
	}

	// The next transition starts from black
	memset(_scene.data(), 0, _scene.size());
	_sceneFull.clear();
	_sceneBg.clear();
	_sceneFg.clear();
	_messageVisible = restoreMessage;
	return ok;
}

} // End of namespace Complet
