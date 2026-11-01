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

#ifndef ILLUSION_RENDERER_OPENGL_H
#define ILLUSION_RENDERER_OPENGL_H

#include "common/scummsys.h"

#if defined(USE_OPENGL_GAME)

#include "common/array.h"
#include "graphics/opengl/system_headers.h"
#include "illusion/renderer.h"

namespace Illusion {

/**
 * Fixed-function OpenGL renderer. Scenes are redrawn every frame, so no scene cache is kept.
 */
class OpenGLRenderer : public IllusionRenderer {
private:
	struct Image {
		GLuint name;
		int width;
		int height;
		float maxU;
		float maxV;
	};

	struct AlphaMesh {
		uint mesh;
		uint group;
		float distance;
	};

	OSystem *_system;
	bool _initialized;
	Common::Array<Image> _images;
	Common::Array<AlphaMesh> _alphaMeshes;
	Common::Array<GLuint> _modelTextures;
	int _width;
	int _height;
	Common::Rect _modelViewport;
	float _nearClip;
	float _farClip;
	float _frustumRight;
	float _frustumTop;
	bool _blendingEnabled;
	bool _depthTestEnabled;
	bool _ditheringEnabled;
	bool _separateSpecular;
	bool _frameStateValid;

	void setupFrameState();
	void setupModelLights(const Model &model);
	void drawModelMesh(const Model &model, uint index, const uint *textures, bool alphaPass);
	void drawQuad(const Common::Rect &destination, float maxU, float maxV);
	GLuint uploadTexture(const Graphics::Surface &surface, const Graphics::Palette &palette, bool repeat,
		int &textureWidth, int &textureHeight);

public:
	OpenGLRenderer(OSystem *system);
	~OpenGLRenderer() override;

	bool init(int width, int height, const Graphics::PixelFormat &format, const IllusionRendererState &state) override;
	uint createImage(const Graphics::Surface &surface, const Graphics::Palette &palette,
		bool keepBackgroundCopy = false) override;
	void destroyImage(uint image) override;
	void drawImage(uint image, const Common::Rect &destination, float opacity = 1.0f,
		bool fadeFromBlack = false) override;
	void drawBackground(uint image, float opacity) override;
	/** Draws an untextured white quad; the image is not used. */
	void drawFadeToWhite(uint image, float amount) override;
	/** Fails for non-power-of-two sizes when the context lacks NPOT textures. */
	uint createTexture(const Graphics::Surface &surface, const Graphics::Palette &palette) override;
	void destroyTexture(uint texture) override;
	void drawModel(const Model &model, const uint *textures, const float *cameraPosition) override;
	void clearScreen() override;
	void flipBuffer() override;
};

} // End of namespace Illusion

#endif // USE_OPENGL_GAME

#endif // ILLUSION_RENDERER_OPENGL_H
