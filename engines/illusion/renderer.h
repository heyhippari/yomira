/* ScummVM - Graphic Adventure Engine
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

#ifndef ILLUSION_RENDERER_H
#define ILLUSION_RENDERER_H

#include "common/array.h"
#include "common/noncopyable.h"
#include "common/system.h"
#include "graphics/palette.h"
#include "graphics/pixelformat.h"
#include "graphics/tinygl/tinygl.h"

namespace Illusion {

struct IllusionRendererState;

class IllusionRenderer : public Common::NonCopyable {
private:
	OSystem *_system;
	TinyGL::ContextHandle *_context;
	Common::Array<TinyGL::BlitImage *> _images;
	Common::Array<Graphics::Surface *> _backgroundCopies;
	Graphics::PixelFormat _format;
	int _width;
	int _height;
	bool _blendingEnabled;

	void freeBackgroundCopy(uint index);

public:
	IllusionRenderer(OSystem *system);
	~IllusionRenderer();

	bool init(int width, int height, const Graphics::PixelFormat &format, const IllusionRendererState &state);
	uint createImage(const Graphics::Surface &surface, const Graphics::Palette &palette, bool keepBackgroundCopy = false);
	void destroyImage(uint image);
	/**
	 * Draws an image with the given opacity. With fadeFromBlack the image must be opaque and the first
	 * layer drawn after clearScreen(); it is faded by scaling its colors instead of blending.
	 */
	void drawImage(uint image, const Common::Rect &destination, float opacity = 1.0f, bool fadeFromBlack = false);
	/**
	 * Starts a frame with an opaque full-screen image faded from black, replacing any previous
	 * contents. Must be called before any other draw of the frame. Fast when the image was created
	 * with keepBackgroundCopy and matches the screen size.
	 */
	void drawBackground(uint image, float opacity);
	void clearScreen();
	void flipBuffer();
};

} // End of namespace Illusion

#endif // ILLUSION_RENDERER_H
