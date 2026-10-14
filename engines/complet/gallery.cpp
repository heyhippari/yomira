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

namespace Complet {

enum {
	kCellColumns = 10,
	kCellsPerPage = 90,
	kNextPageCell = 98,
	kExitCell = 99
};

static const char *const kCgNames[kCgCount] = {
	"CG01", "CG02", "CG02R", "CG03", "CG03R", "CG04", "CG05", "CG05R", "CG06", "CG07",
	"CG08", "CG09", "CG09R", "CG10", "CG11", "CG12", "CG13", "CG14", "CG15", "CG16",
	"CG17", "CG18", "CG19", "CG20", "CG21", "CG22", "CG23", "CG24", "CG25", "CG26",
	"CG27", "CG28", "CG29", "CG30", "CG31", "CG32", "CG33", "CG34", "CG35", "CG36",
	"CG37", "CG37R", "CG38", "CG39", "CG40", "CG41", "CG41R", "CG42", "CG42R", "CG43",
	"CG43R", "CG44", "CG44R", "CG45", "CG46", "CG47", "CG47R", "CG48", "CG49", "CG50",
	"CG51", "CG51n", "CG51R", "CG52", "CG53", "CG54", "CG54R", "CG55", "CG56", "CG57",
	"CG58", "CG59", "CG60", "CG60R", "CG61", "CG61R", "CG62", "CG63", "CG64", "CG65",
	"CG66", "CG67", "CG68", "CG69", "CG70", "CG71", "CG72", "CG72R", "CG73", "CG74",
	"CG75", "CG76", "CG76R", "CG77", "CG78", "CG79", "CG80", "CG81", "CG82", "CG83",
	"CG83R", "CG84", "CG85", "CG86", "CG87", "CG88", "CG89", "CG90", "CG91", "CG92",
	"CG93", "CG94", "CG94R", "CG95", "CG96", "CG97", "CG98", "CG99", "CG100", "CGSP1",
	"CGSP2", "CGSP3", "CGSP4", "CGSP5"
};

// Full-screen graphics shown through the script are what unlock gallery entries
void CompletEngine::recordSeenCg(const Common::String &name) {
	for (int i = 0; i < kCgCount; i++) {
		if (name.equalsIgnoreCase(kCgNames[i])) {
			if (!_seenCg[i]) {
				_seenCg[i] = true;
				saveCommonData();
			}
			return;
		}
	}
}

void CompletEngine::unlockAllCgs() {
	for (int i = 0; i < kCgCount; i++)
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

bool CompletEngine::runGallery() {
	const int thumbWidth = _screenWidth / kCellColumns;
	const int thumbHeight = _screenHeight / kCellColumns;
	Common::Array<int> seen;
	for (int i = 0; i < kCgCount; i++) {
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
			if (!GdImage::loadThumbnail(kCgNames[seen[first + i]], thumb)) {
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
			int width = count * 500 / kCgCount;
			if (width) {
				drawBox(page, _screenWidth, _screenHeight, 0x13, 0x1b8, width + 0x15, 0x1d8, black);
				drawBox(page, _screenWidth, _screenHeight, 0x14, 0x1b9, width + 0x14, 0x1d7, yellow);
			}
			int tenths = count * 1000 / kCgCount;
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
		int width = count * 600 / kCgCount;
		if (width) {
			drawBox(page, _screenWidth, _screenHeight, 0x13, 0x240, width + 0x15, 0x24e, black);
			drawBox(page, _screenWidth, _screenHeight, 0x14, 0x241, width + 0x14, 0x24d, yellow);
		}
		blitFrame(page, 0x284, 0x239, 0x33, 0x15, 0x2de, 0x230);
		blitFrame(page, 0x2b8, 0x24c, 10, 4, 0x10, 0x23c);
		blitFrame(page, 0x2b8, 0x24c, 10, 12, width + 0xf, 0x234);

		int tenths = count * 1000 / kCgCount;
		const int digitY = 0x24f, digitSrcX = 0x250, labelY = 0x22a;
		if (tenths < 1000) {
			if (tenths >= 100)
				blitFrame(page, (tenths / 100) * 8 + digitSrcX, digitY, 8, 9, width + 1, labelY);
			blitFrame(page, ((tenths % 100) / 10) * 8 + digitSrcX, digitY, 8, 9, width + 9, labelY);
			blitFrame(page, 0x2a0, digitY, 8, 9, width + 0x11, labelY);
			blitFrame(page, (tenths % 10) * 8 + digitSrcX, digitY, 8, 9, width + 0x15, labelY);
			blitFrame(page, 0x2a8, digitY, 0x10, 9, width + 0x1d, labelY);
		} else {
			blitFrame(page, 600, digitY, 8, 9, width + 3, labelY);
			blitFrame(page, digitSrcX, digitY, 8, 9, width + 0xb, labelY);
			blitFrame(page, digitSrcX, digitY, 8, 9, width + 0x13, labelY);
			blitFrame(page, 0x2a8, digitY, 0x10, 9, width + 0x1b, labelY);
		}
		if (pageCount > 1)
			blitFrame(page, 0x250, 0x23c, 0x33, 0xc, 0x290, 0x235);
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
	bool ok = fadeTo(pages[page]);
	bool leave = false;
	int selected = -1;
	Common::Point lastMouse(-1, -1);
	show(page, selected);

	while (ok && !leave && !shouldQuit()) {
		int activated = -2;
		Common::Event event;
		while (g_system->getEventManager()->pollEvent(event)) {
			switch (event.type) {
			case Common::EVENT_MOUSEMOVE:
				if (event.mouse != lastMouse) {
					lastMouse = event.mouse;
					int cell = cellAt(page, event.mouse);
					selected = cell < kCellsPerPage ? cell : -1;
					show(page, selected);
				}
				break;
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
				show(page, selected);
				break;
			}
			default:
				break;
			}
		}

		refreshCursor();
		if (activated == kExitCell) {
			leave = true;
		} else if (activated == kNextPageCell) {
			if (pageCount > 1) {
				page ^= 1;
				selected = -1;
				ok = fadeTo(pages[page]);
				show(page, selected);
			}
		} else if (activated >= 0) {
			GdImage image;
			if (image.loadFromName(Common::String(kCgNames[seen[page * kCellsPerPage + activated]]) + ".gd")) {
				Common::Array<byte> picture(_screenWidth * _screenHeight * 3);
				memcpy(picture.data(), image.getPixels(), picture.size());
				memcpy(_scene.data(), pages[page].data(), _scene.size());
				ok = fadeTo(picture) && waitAnyInput();
				if (ok) {
					memcpy(_scene.data(), picture.data(), _scene.size());
					ok = fadeTo(pages[page]);
					show(page, selected);
				}
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
