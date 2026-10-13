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

// Copies a rectangle of the frame image; source rows are bottom-up like the original's
void CompletEngine::blitFrame(Common::Array<byte> &dst, int sx, int sy, int w, int h, int dx, int dy) const {
	const byte *frame = _frameImage.getPixels();
	for (int k = 0; k < h; k++) {
		int topDownRow = dy + (h - 1 - k);
		int dstRow = kScreenHeight - 1 - topDownRow;
		if (dstRow < 0 || dstRow >= kScreenHeight || dx < 0 || dx + w > kScreenWidth)
			continue;
		memcpy(&dst[(dstRow * kScreenWidth + dx) * 3], frame + ((sy + k) * kScreenWidth + sx) * 3, w * 3);
	}
}

static void drawBox(Common::Array<byte> &buf, int x0, int y0, int x1, int y1, const byte color[3]) {
	for (int y = y0; y < y1; y++) {
		int row = kScreenHeight - 1 - y;
		for (int x = x0; x < x1; x++)
			memcpy(&buf[(row * kScreenWidth + x) * 3], color, 3);
	}
}

// Wait for a click or key; false on quit
bool CompletEngine::waitAnyInput() {
	Common::Event event;
	while (!shouldQuit()) {
		while (g_system->getEventManager()->pollEvent(event)) {
			if (event.type == Common::EVENT_LBUTTONUP || event.type == Common::EVENT_RBUTTONUP || event.type == Common::EVENT_KEYDOWN)
				return true;
		}
		g_system->delayMillis(10);
	}
	return false;
}

bool CompletEngine::runGallery() {
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
		page.resize(kScreenWidth * kScreenHeight * 3);
		memset(page.data(), 0, page.size());

		int first = p * kCellsPerPage;
		for (int i = 0; i < kCellsPerPage && first + i < count; i++) {
			Common::Array<byte> thumb;
			if (!GdImage::loadThumbnail(kCgNames[seen[first + i]], thumb)) {
				thumb.resize(kThumbWidth * kThumbHeight * 3);
				memset(thumb.data(), 0x7f, thumb.size());
			}

			int cellX = (i % kCellColumns) * kThumbWidth;
			int cellTop = (i / kCellColumns) * kThumbHeight;
			for (int r = 0; r < kThumbHeight; r++) {
				int dstRow = kScreenHeight - 1 - (cellTop + kThumbHeight - 1 - r);
				memcpy(&page[(dstRow * kScreenWidth + cellX) * 3], &thumb[r * kThumbWidth * 3], kThumbWidth * 3);
			}
		}

		// Progress bar showing how many CGs have been seen
		int width = count * 600 / kCgCount;
		if (width) {
			drawBox(page, 0x13, 0x240, width + 0x15, 0x24e, black);
			drawBox(page, 0x14, 0x241, width + 0x14, 0x24d, yellow);
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
		int cell = pos.x / kThumbWidth + (pos.y / kThumbHeight) * kCellColumns;
		if (pos.x < 0 || pos.y < 0 || pos.x >= kScreenWidth || pos.y >= kScreenHeight)
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
			int cellX = (selected % kCellColumns) * kThumbWidth;
			int cellTop = (selected / kCellColumns) * kThumbHeight;
			for (int k = 0; k < kThumbHeight; k++) {
				int row = kScreenHeight - 1 - (cellTop + kThumbHeight - 1 - k);
				bool edge = k < 2 || k > 0x39;
				for (int x = 0; x < kThumbWidth; x++) {
					if (edge || x < 2 || (x >= 76 && x < 78))
						memcpy(&view[(row * kScreenWidth + cellX + x) * 3], highlight, 3);
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
			case Common::EVENT_KEYDOWN: {
				int onPage = MIN(count - page * kCellsPerPage, (int)kCellsPerPage);
				switch (event.kbd.keycode) {
				case Common::KEYCODE_LEFT:
					selected = onPage ? (selected <= 0 ? onPage - 1 : selected - 1) : -1;
					break;
				case Common::KEYCODE_RIGHT:
					selected = onPage ? (selected + 1) % onPage : -1;
					break;
				case Common::KEYCODE_UP:
					selected = onPage ? (selected < 0 ? 0 : (selected >= kCellColumns ? selected - kCellColumns : selected)) : -1;
					break;
				case Common::KEYCODE_DOWN:
					selected = onPage ? (selected < 0 ? 0 : (selected + kCellColumns < onPage ? selected + kCellColumns : selected)) : -1;
					break;
				case Common::KEYCODE_RETURN:
				case Common::KEYCODE_KP_ENTER:
				case Common::KEYCODE_SPACE:
					activated = selected;
					break;
				case Common::KEYCODE_PAGEUP:
				case Common::KEYCODE_PAGEDOWN:
					activated = kNextPageCell;
					break;
				case Common::KEYCODE_ESCAPE:
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
				Common::Array<byte> picture(kScreenWidth * kScreenHeight * 3);
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
	_messageVisible = restoreMessage;
	return ok;
}

} // End of namespace Complet
