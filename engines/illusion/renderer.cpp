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

#include "illusion/renderer.h"
#include "illusion/illusion.h"

#include "common/list.h"
#include "common/system.h"
#include "graphics/surface.h"

namespace Illusion {

static void setCapability(TGLenum capability, bool enabled) {
	if (enabled)
		tglEnable(capability);
	else
		tglDisable(capability);
}

IllusionRenderer::IllusionRenderer(OSystem *system) : _system(system), _context(nullptr), _width(0), _height(0),
		_blendingEnabled(false) {
}

IllusionRenderer::~IllusionRenderer() {
	for (uint index = 0; index < _backgroundCopies.size(); ++index)
		freeBackgroundCopy(index);
	if (_context) {
		TinyGL::setContext(_context);
		for (uint index = 0; index < _images.size(); ++index) {
			if (_images[index])
				tglDeleteBlitImage(_images[index]);
		}
		TinyGL::destroyContext(_context);
	}
}

bool IllusionRenderer::init(int width, int height, const Graphics::PixelFormat &format,
		const IllusionRendererState &state) {
	if (_context)
		return false;

	// TinyGL only supports 16 and 32 bits per pixel, and the frustum is derived from the projection matrix.
	if ((format.bytesPerPixel != 2 && format.bytesPerPixel != 4) || width <= 0 || height <= 0 ||
			state.projectionMatrix[0] <= 0.0f || state.projectionMatrix[5] <= 0.0f ||
			state.projectionNear <= 0.0f || state.projectionFar <= state.projectionNear)
		return false;

	_context = TinyGL::createContext(width, height, format, 512, true, false);
	if (!_context)
		return false;
	_format = format;
	_width = width;
	_height = height;
	TinyGL::setContext(_context);

	tglViewport(state.viewport.left, height - state.viewport.bottom,
		state.viewport.width(), state.viewport.height());

	const float nearClip = state.projectionNear;
	const float top = nearClip / state.projectionMatrix[5];
	const float right = nearClip / state.projectionMatrix[0];
	tglMatrixMode(TGL_PROJECTION);
	tglLoadIdentity();
	tglFrustumf(-right, right, -top, top, nearClip, state.projectionFar);

	tglMatrixMode(TGL_MODELVIEW);
	tglLoadIdentity();
	tglScalef(1.0f, 1.0f, -1.0f);
	tglEnable(TGL_TEXTURE_2D);
	setCapability(TGL_DEPTH_TEST, state.depthBufferEnabled);
	tglDepthFunc(TGL_LEQUAL);
	tglDepthMask(TGL_TRUE);
	setCapability(TGL_DITHER, state.ditheringEnabled);
	tglEnable(TGL_CULL_FACE);
	tglCullFace(TGL_BACK);
	tglFrontFace(TGL_CW);
	_blendingEnabled = state.alphaBlendingEnabled;
	setCapability(TGL_BLEND, _blendingEnabled);
	tglBlendFunc(TGL_SRC_ALPHA, TGL_ONE_MINUS_SRC_ALPHA);
	tglTexEnvi(TGL_TEXTURE_ENV, TGL_TEXTURE_ENV_MODE, TGL_MODULATE);

	return true;
}

uint IllusionRenderer::createImage(const Graphics::Surface &surface, const Graphics::Palette &palette, bool keepBackgroundCopy) {
	if (!_context || !surface.getPixels() || surface.w <= 0 || surface.h <= 0)
		return 0;
	if (surface.format.bytesPerPixel == 1) {
		if (palette.empty() || palette.size() > 256)
			return 0;
		for (int row = 0; row < surface.h; ++row) {
			const byte *pixels = (const byte *)surface.getBasePtr(0, row);
			for (int column = 0; column < surface.w; ++column) {
				if (pixels[column] >= palette.size())
					return 0;
			}
		}
	}
	TinyGL::setContext(_context);
	Graphics::Surface *converted = surface.convertTo(Graphics::PixelFormat::createFormatRGBA32(), palette.data(), palette.size());
	if (!converted)
		return 0;
	TinyGL::BlitImage *image = tglGenBlitImage();
	if (image)
		tglUploadBlitImage(image, *converted, 0, false);
	converted->free();
	delete converted;
	if (!image)
		return 0;
	_images.push_back(image);
	_backgroundCopies.push_back(nullptr);
	if (keepBackgroundCopy && _format.bytesPerPixel == 4 && _format.rBits() == 8 && _format.gBits() == 8 &&
			_format.bBits() == 8) {
		Graphics::Surface *background = surface.convertTo(_format, palette.data(), palette.size());
		if (background && (background->w != _width || background->h != _height)) {
			Graphics::Surface *scaled = background->scale(_width, _height);
			background->free();
			delete background;
			background = scaled;
		}
		_backgroundCopies.back() = background;
	}
	return _images.size();
}

void IllusionRenderer::freeBackgroundCopy(uint index) {
	if (index >= _backgroundCopies.size() || !_backgroundCopies[index])
		return;
	_backgroundCopies[index]->free();
	delete _backgroundCopies[index];
	_backgroundCopies[index] = nullptr;
}

void IllusionRenderer::destroyImage(uint image) {
	if (!_context || image == 0 || image > _images.size() || !_images[image - 1])
		return;
	TinyGL::setContext(_context);
	tglDeleteBlitImage(_images[image - 1]);
	_images[image - 1] = nullptr;
	freeBackgroundCopy(image - 1);
}

void IllusionRenderer::drawBackground(uint image, float opacity) {
	if (!_context || image == 0 || image > _images.size() || !_images[image - 1])
		return;
	opacity = CLIP(opacity, 0.0f, 1.0f);
	const Graphics::Surface *copy = _backgroundCopies[image - 1];
	if (!copy || copy->w != _width || copy->h != _height) {
		clearScreen();
		drawImage(image, Common::Rect(0, 0, _width, _height), opacity, true);
		return;
	}

	// Every pixel is replaced, so no TinyGL clear is needed. Draw calls queued later run after this write.
	TinyGL::setContext(_context);
	Graphics::Surface target;
	TinyGL::getSurfaceRef(target);
	const uint32 colorMask = _format.rMax() << _format.rShift | _format.gMax() << _format.gShift |
		_format.bMax() << _format.bShift;
	const uint32 scale = (uint32)(opacity * 256.0f);
	for (int y = 0; y < _height; ++y) {
		const uint32 *src = (const uint32 *)copy->getBasePtr(0, y);
		uint32 *dst = (uint32 *)target.getBasePtr(0, y);
		for (int x = 0; x < _width; ++x) {
			const uint32 pixel = src[x];
			// Scale two color lanes per multiply; the alpha lane is kept as is.
			const uint32 even = (((pixel & 0x00FF00FF) * scale) >> 8) & 0x00FF00FF;
			const uint32 odd = (((pixel >> 8) & 0x00FF00FF) * scale) & 0xFF00FF00;
			dst[x] = ((even | odd) & colorMask) | (pixel & ~colorMask);
		}
	}
}

void IllusionRenderer::drawImage(uint image, const Common::Rect &destination, float opacity, bool fadeFromBlack) {
	if (!_context || image == 0 || image > _images.size() || !_images[image - 1] || opacity <= 0.0f || destination.isEmpty() ||
			destination.left < 0 || destination.top < 0)
		return;
	TinyGL::setContext(_context);
	int sourceWidth = 0, sourceHeight = 0;
	tglGetBlitImageSize(_images[image - 1], sourceWidth, sourceHeight);
	TinyGL::BlitTransform transform(destination.left, destination.top);
	transform.sourceRectangle(0, 0, sourceWidth, sourceHeight);
	// A zero-size destination selects TinyGL's unscaled RLE/memcpy path instead of the per-pixel scaler.
	if (destination.width() != sourceWidth || destination.height() != sourceHeight)
		transform.scale(destination.width(), destination.height());
	opacity = MIN(opacity, 1.0f);
	if (fadeFromBlack && opacity < 1.0f && _blendingEnabled) {
		// Over a cleared black buffer, scaling RGB equals alpha blending without the per-pixel blend path.
		transform.tint(1.0f, opacity, opacity, opacity);
		tglDisable(TGL_BLEND);
		tglBlit(_images[image - 1], transform);
		tglEnable(TGL_BLEND);
		return;
	}
	transform.tint(opacity);
	tglBlit(_images[image - 1], transform);
}

void IllusionRenderer::clearScreen() {
	if (!_context)
		return;
	TinyGL::setContext(_context);
	tglClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	tglClearDepth(1.0f);
	tglClearStencil(0);
	tglClear(TGL_COLOR_BUFFER_BIT | TGL_DEPTH_BUFFER_BIT | TGL_STENCIL_BUFFER_BIT);
}

void IllusionRenderer::flipBuffer() {
	if (!_context)
		return;
	TinyGL::setContext(_context);

	Common::List<Common::Rect> dirtyAreas;
	TinyGL::presentBuffer(dirtyAreas);
	Graphics::Surface surface;
	TinyGL::getSurfaceRef(surface);
	for (Common::List<Common::Rect>::const_iterator it = dirtyAreas.begin(); it != dirtyAreas.end(); ++it) {
		_system->copyRectToScreen(surface.getBasePtr(it->left, it->top), surface.pitch,
			it->left, it->top, it->width(), it->height());
	}
	_system->updateScreen();
}

} // End of namespace Illusion
