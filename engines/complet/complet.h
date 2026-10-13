#ifndef ENGINES_COMPLET_COMPLET_H
#define ENGINES_COMPLET_COMPLET_H

#include "engines/engine.h"
#include "graphics/pixelformat.h"

#include "complet/gd.h"
#include "complet/scenario.h"

namespace Complet {

class CompletEngine : public Engine {
public:
	CompletEngine(OSystem *system);
	Common::Error run() override;

	bool hasFeature(EngineFeature f) const override;

private:
	bool initGame();
	bool showLogo();
	bool loadFrameImage();
	void drawImage(const GdImage &image, int brightness);
	bool waitDelay(uint32 ms, bool allowSkip);

	Graphics::PixelFormat _format;
	GdImage _frameImage;
	// Glyph sheet transparency mask built from the frame image's glyph rows
	Common::Array<byte> _glyphMask;
	Scenario _startup;
};

} // End of namespace Complet

#endif // ENGINES_COMPLET_COMPLET_H
