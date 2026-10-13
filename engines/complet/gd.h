#ifndef ENGINES_COMPLET_GD_H
#define ENGINES_COMPLET_GD_H

#include "common/array.h"
#include "common/stream.h"
#include "common/str.h"
#include "common/types.h"

namespace Complet {

enum {
	kScreenWidth = 800,
	kScreenHeight = 600,
	kThumbWidth = 80,
	kThumbHeight = 60
};

// Decoder for the C4 engine's "GD3" images (GDS\*.GD)
class GdImage {
public:
	GdImage();

	bool load(Common::SeekableReadStream &stream);
	bool loadFromName(const Common::String &name);

	// 24bpp, B,G,R byte order, bottom-up rows, kScreenWidth x kScreenHeight
	const byte *getPixels() const { return _pixels.data(); }

	// The 80x60 preview stored in the file header, same pixel layout
	static bool loadThumbnail(const Common::String &name, Common::Array<byte> &out);

private:
	bool decodeLz(const byte *data, uint32 size, uint32 pos);
	bool decodePredictive(const byte *data, uint32 size, uint32 pos);

	Common::Array<byte> _pixels;
};

} // End of namespace Complet

#endif // ENGINES_COMPLET_GD_H
