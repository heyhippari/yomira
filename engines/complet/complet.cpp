#include "common/config-manager.h"
#include "common/events.h"
#include "common/fs.h"
#include "common/system.h"
#include "engines/util.h"
#include "graphics/surface.h"

#include "complet/complet.h"

namespace Complet {

enum {
	// Glyph sheet inside the frame image (GDS\MESFRAME.GD)
	kGlyphSourceRow = 472,
	kGlyphRows = 32,
	kLogoFadeSteps = 32,
	kLogoFadeDelay = 8,
	kLogoHoldMs = 4000
};

CompletEngine::CompletEngine(OSystem *system) : Engine(system) {
}

bool CompletEngine::hasFeature(EngineFeature f) const {
	return f == kSupportsReturnToLauncher;
}

Common::Error CompletEngine::run() {
	// The original uses a fixed 800x600 window with 24bpp BGR DIBs
	_format = Graphics::PixelFormat::createFormatBGRA32();
	initGraphics(kScreenWidth, kScreenHeight, &_format);

	if (!initGame())
		return Common::Error(Common::kReadingFailed, "Missing game data (gds/mpx folders)");

	g_system->fillScreen(0);
	g_system->updateScreen();

	if (!showLogo() || shouldQuit())
		return Common::kNoError;

	warning("Complet: startup scenario '%s' loaded (%u command bytes), script interpreter is not implemented yet",
		_startup.getName().c_str(), _startup.getCommands().size());
	return Common::kNoError;
}

bool CompletEngine::initGame() {
	const Common::FSNode gameDataDir(ConfMan.getPath("path"));
	SearchMan.addSubDirectoryMatching(gameDataDir, "gds");
	SearchMan.addSubDirectoryMatching(gameDataDir, "mpx");
	SearchMan.addSubDirectoryMatching(gameDataDir, "vmd");

	if (!loadFrameImage())
		return false;

	// The original runs the "startup" scenario on a worker thread
	return _startup.load("startup");
}

bool CompletEngine::loadFrameImage() {
	// MESFRAME.GD holds the message frame and glyph sheet, NULL.GD is the fallback
	if (!_frameImage.loadFromName("mesframe.gd") && !_frameImage.loadFromName("null.gd"))
		return false;

	// Rebuild the sheet's transparency mask: white where the glyph row is black
	const uint32 maskBytes = kScreenWidth * kGlyphRows * 3;
	const byte *src = _frameImage.getPixels() + kGlyphSourceRow * kScreenWidth * 3;
	_glyphMask.resize(maskBytes);
	for (uint32 i = 0; i < maskBytes; i += 3) {
		bool empty = !src[i] && !src[i + 1] && !src[i + 2];
		memset(&_glyphMask[i], empty ? 0xff : 0, 3);
	}
	return true;
}

void CompletEngine::drawImage(const GdImage &image, int brightness) {
	Graphics::Surface *screen = g_system->lockScreen();
	const byte *src = image.getPixels();

	// Source rows are bottom-up
	for (int y = 0; y < kScreenHeight; y++) {
		const byte *row = src + (kScreenHeight - 1 - y) * kScreenWidth * 3;
		uint32 *dst = (uint32 *)screen->getBasePtr(0, y);
		for (int x = 0; x < kScreenWidth; x++, row += 3)
			*dst++ = _format.RGBToColor(row[2] * brightness / 255, row[1] * brightness / 255, row[0] * brightness / 255);
	}

	g_system->unlockScreen();
	g_system->updateScreen();
}

// Returns false on quit; a click or key ends the wait early if allowSkip is set
bool CompletEngine::waitDelay(uint32 ms, bool allowSkip) {
	uint32 end = g_system->getMillis() + ms;
	Common::Event event;

	while (g_system->getMillis() < end) {
		while (g_system->getEventManager()->pollEvent(event)) {
			if (allowSkip && (event.type == Common::EVENT_LBUTTONUP || event.type == Common::EVENT_KEYDOWN))
				return true;
		}
		if (shouldQuit())
			return false;
		g_system->delayMillis(2);
	}
	return true;
}

// Company logo: fade in, hold up to four seconds (or until click/key), fade out
bool CompletEngine::showLogo() {
	GdImage logo;
	if (!logo.loadFromName("cplogo.gd"))
		return true;

	for (int i = 1; i <= kLogoFadeSteps; i++) {
		drawImage(logo, i * 255 / kLogoFadeSteps);
		if (!waitDelay(kLogoFadeDelay, false))
			return false;
	}

	if (!waitDelay(kLogoHoldMs, true))
		return false;

	for (int i = kLogoFadeSteps - 1; i >= 0; i--) {
		drawImage(logo, i * 255 / kLogoFadeSteps);
		if (!waitDelay(kLogoFadeDelay, false))
			return false;
	}
	return true;
}

} // End of namespace Complet
