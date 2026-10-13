#ifndef ENGINES_COMPLET_COMPLET_H
#define ENGINES_COMPLET_COMPLET_H

#include "common/array.h"
#include "common/ustr.h"
#include "engines/engine.h"
#include "graphics/font.h"
#include "graphics/pixelformat.h"

#include "complet/audio.h"
#include "complet/gd.h"
#include "complet/script.h"

namespace Graphics {
struct Surface;
}

namespace Complet {

enum {
	kCgCount = 124
};

class CompletEngine : public Engine {
public:
	CompletEngine(OSystem *system);
	~CompletEngine() override;

	Common::Error run() override;
	bool hasFeature(EngineFeature f) const override;

	// Runs a script text, including its % commands, until it has finished
	void runText(Script &script, const byte *text);
	// Shows the selection window; returns the chosen item or -1 on quit
	int selectMenu(const Common::Array<MenuItem> &items);

private:
	struct MenuLayout {
		int left, top, width, height, rows;
	};

	bool initGame();
	void loadFont();
	bool loadFrameImage();
	void loadCommonData();
	void saveCommonData();
	void recordSeenCg(const Common::String &name);
	bool runGallery();
	void blitFrame(Common::Array<byte> &dst, int sx, int sy, int w, int h, int dx, int dy) const;
	bool waitAnyInput();

	void presentBuffer(const byte *bgr, int brightness = 255);
	void present();
	void drawOverlays(Graphics::Surface &screen);
	void drawMessage(Graphics::Surface &screen);
	void drawMenu(Graphics::Surface &screen);
	void drawText(Graphics::Surface &screen, const Common::U32String &text, int x, int y, uint32 color, int advance);
	MenuLayout getMenuLayout() const;

	bool showLogo();
	bool showFullImage(const Common::String &name, bool fade);
	void stageBackground(const Common::String &name);
	bool showForeground(const Common::String &args);
	bool loadImage(const Common::String &name, Common::Array<byte> &out);
	void overlaySprite(Common::Array<byte> &dst, const Common::Array<byte> &sprite);
	bool fadeTo(const Common::Array<byte> &target);
	bool delay(uint32 ms);
	bool waitClick();

	const byte *runCommand(Script &script, const byte *p);
	void printChar(uint32 ch);
	void clearMessage();

	Graphics::PixelFormat _format;
	GdImage _frameImage;
	// Image currently on screen, same layout as GD pixel data
	Common::Array<byte> _scene;
	// Latest %gb background and whether it is still waiting to be shown
	Common::Array<byte> _bg;
	Common::String _bgName;
	bool _bgPending = false;
	const Graphics::Font *_font = nullptr;
	bool _ownFont = false;

	bool _messageVisible = false;
	Common::Array<Common::U32String> _messageLines;

	const Common::Array<MenuItem> *_menu = nullptr;
	int _hover = -1;

	AudioPlayer _audio;
	Script _script;
	// Layout of the original "common" file: settings, seen-CG bits, font, flag bits, volumes
	byte _commonData[41];
	bool _seenCg[kCgCount];
};

} // End of namespace Complet

#endif // ENGINES_COMPLET_COMPLET_H
