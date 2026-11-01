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

#ifndef ILLUSION_RENDERER_TINYGL_H
#define ILLUSION_RENDERER_TINYGL_H

#include "common/scummsys.h"

#if defined(USE_TINYGL)

#include "common/array.h"
#include "graphics/tinygl/tinygl.h"
#include "illusion/renderer.h"

namespace Illusion {

/**
 * Software renderer. Because the frame lives in system memory, it can keep a copy of a static
 * scene and fade the frame in place instead of blending full-screen layers.
 */
class TinyGLRenderer : public IllusionRenderer {
private:
	OSystem *_system;
	TinyGL::ContextHandle *_context;
	Common::Array<TinyGL::BlitImage *> _images;
	Common::Array<Graphics::Surface *> _backgroundCopies;
	Graphics::Surface *_sceneCopy;
	bool _sceneValid;
	Graphics::PixelFormat _format;
	int _width;
	int _height;
	bool _blendingEnabled;

	struct AlphaMesh {
		uint mesh;
		uint group;
		float distance;
	};
	Common::Array<AlphaMesh> _alphaMeshes;

	void freeBackgroundCopy(uint index);
	void setupModelLights(const Model &model);
	void drawModelMesh(const Model &model, uint index, const uint *textures, bool alphaPass);

public:
	TinyGLRenderer(OSystem *system);
	~TinyGLRenderer() override;

	bool init(int width, int height, const Graphics::PixelFormat &format, const IllusionRendererState &state) override;
	uint createImage(const Graphics::Surface &surface, const Graphics::Palette &palette,
		bool keepBackgroundCopy = false) override;
	void destroyImage(uint image) override;
	void drawImage(uint image, const Common::Rect &destination, float opacity = 1.0f,
		bool fadeFromBlack = false) override;
	/** Fast when the image was created with keepBackgroundCopy and matches the screen size. */
	void drawBackground(uint image, float opacity) override;
	/** Falls back to drawing the white image over the screen when the pixel format does not allow the direct path. */
	void drawFadeToWhite(uint image, float amount) override;
	void cacheScene() override;
	bool restoreScene() override;
	void invalidateScene() override { _sceneValid = false; }
	/** The size is limited to the 512x512 texture size of the context. */
	uint createTexture(const Graphics::Surface &surface, const Graphics::Palette &palette) override;
	void destroyTexture(uint texture) override;
	void drawModel(const Model &model, const uint *textures, const float *cameraPosition) override;
	void clearScreen() override;
	void flipBuffer() override;
};

} // End of namespace Illusion

#endif // USE_TINYGL

#endif // ILLUSION_RENDERER_TINYGL_H
