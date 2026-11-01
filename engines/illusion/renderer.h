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

#include "common/noncopyable.h"
#include "common/rect.h"
#include "common/system.h"
#include "graphics/palette.h"
#include "graphics/pixelformat.h"
#include "graphics/surface.h"
#include "illusion/model.h"

namespace Illusion {

struct IllusionRendererState;

/**
 * Drawing interface used by the engine. Images are 2D layers in screen coordinates and textures
 * belong to models; both are referred to by handles, where 0 means none.
 */
class IllusionRenderer : public Common::NonCopyable {
public:
	virtual ~IllusionRenderer() {}

	virtual bool init(int width, int height, const Graphics::PixelFormat &format, const IllusionRendererState &state) = 0;

	/**
	 * Uploads a 2D image. keepBackgroundCopy marks an opaque full-screen image that drawBackground()
	 * may copy directly.
	 */
	virtual uint createImage(const Graphics::Surface &surface, const Graphics::Palette &palette,
		bool keepBackgroundCopy = false) = 0;
	virtual void destroyImage(uint image) = 0;
	/**
	 * Draws an image with the given opacity. With fadeFromBlack the image must be opaque and the first
	 * layer drawn after clearScreen(); it is faded by scaling its colors instead of blending.
	 */
	virtual void drawImage(uint image, const Common::Rect &destination, float opacity = 1.0f,
		bool fadeFromBlack = false) = 0;
	/**
	 * Starts a frame with an opaque full-screen image faded from black, replacing any previous
	 * contents. Must be called before any other draw of the frame.
	 */
	virtual void drawBackground(uint image, float opacity) = 0;
	/** Fades the frame drawn so far toward white by the given amount; image is the white fader. */
	virtual void drawFadeToWhite(uint image, float amount) = 0;

	/**
	 * Keeps the frame drawn so far, so a static scene can be restored with restoreScene() instead of
	 * being drawn again. Optional: renderers that redraw cheaply keep nothing.
	 */
	virtual void cacheScene() {}
	/** Replaces the frame with the cached scene. Returns false when none is cached; the caller then redraws. */
	virtual bool restoreScene() { return false; }
	virtual void invalidateScene() {}

	/** Uploads a model texture. Returns 0 when it cannot be used. */
	virtual uint createTexture(const Graphics::Surface &surface, const Graphics::Palette &palette) = 0;
	virtual void destroyTexture(uint texture) = 0;
	/**
	 * Draws a posed model over what has been drawn so far, using the game's perspective and a
	 * camera at the given Direct3D position looking along +Z. textures holds one texture handle
	 * per Model::getTextures() entry (0 for none). The depth buffer is cleared first, and 2D
	 * draws issued afterwards land on top.
	 */
	virtual void drawModel(const Model &model, const uint *textures, const float *cameraPosition) = 0;

	virtual void clearScreen() = 0;
	/** Presents the frame. */
	virtual void flipBuffer() = 0;
};

/**
 * Creates the renderer selected by the "renderer" setting, falling back to the best one this
 * build supports. Returns nullptr when none is available.
 */
IllusionRenderer *createRenderer(OSystem *system);

} // End of namespace Illusion

#endif // ILLUSION_RENDERER_H
