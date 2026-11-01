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

#include "illusion/renderer_tinygl.h"

#if defined(USE_TINYGL)

#include "illusion/illusion.h"

#include "common/algorithm.h"
#include "common/list.h"
#include "common/system.h"
#include "graphics/surface.h"

#include <string.h>

namespace Illusion {

// Room for the vertices of a few 10k-triangle models; every draw call copies the vertices it submits.
static const uint32 kDrawCallMemory = 16 * 1024 * 1024;
static const int kMaxModelLights = 8;
static const uint kMaxTextureSize = 512;

static void setCapability(TGLenum capability, bool enabled) {
	if (enabled)
		tglEnable(capability);
	else
		tglDisable(capability);
}

TinyGLRenderer::TinyGLRenderer(OSystem *system) : _system(system), _context(nullptr), _sceneCopy(nullptr),
		_sceneValid(false), _width(0), _height(0), _blendingEnabled(false) {
}

TinyGLRenderer::~TinyGLRenderer() {
	if (_sceneCopy) {
		_sceneCopy->free();
		delete _sceneCopy;
	}
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

bool TinyGLRenderer::init(int width, int height, const Graphics::PixelFormat &format,
		const IllusionRendererState &state) {
	if (_context)
		return false;

	// TinyGL only supports 16 and 32 bits per pixel, and the frustum is derived from the projection matrix.
	if ((format.bytesPerPixel != 2 && format.bytesPerPixel != 4) || width <= 0 || height <= 0 ||
			state.projectionMatrix[0] <= 0.0f || state.projectionMatrix[5] <= 0.0f ||
			state.projectionNear <= 0.0f || state.projectionFar <= state.projectionNear)
		return false;

	_context = TinyGL::createContext(width, height, format, 512, false, false, kDrawCallMemory);
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

uint TinyGLRenderer::createImage(const Graphics::Surface &surface, const Graphics::Palette &palette, bool keepBackgroundCopy) {
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

uint TinyGLRenderer::createTexture(const Graphics::Surface &surface, const Graphics::Palette &palette) {
	if (!_context || !surface.getPixels() || surface.w <= 0 || surface.h <= 0 ||
			(uint)surface.w > kMaxTextureSize || (uint)surface.h > kMaxTextureSize)
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
	Graphics::Surface *converted = surface.convertTo(Graphics::PixelFormat::createFormatRGBA32(), palette.data(), palette.size());
	if (!converted)
		return 0;
	TinyGL::setContext(_context);
	TGLuint name = 0;
	tglGenTextures(1, &name);
	tglBindTexture(TGL_TEXTURE_2D, name);
	// The filter is picked when the texture is uploaded.
	tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_MIN_FILTER, TGL_LINEAR);
	tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_MAG_FILTER, TGL_LINEAR);
	tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_WRAP_S, TGL_REPEAT);
	tglTexParameteri(TGL_TEXTURE_2D, TGL_TEXTURE_WRAP_T, TGL_REPEAT);
	tglTexImage2D(TGL_TEXTURE_2D, 0, TGL_RGBA, converted->w, converted->h, 0, TGL_RGBA, TGL_UNSIGNED_BYTE,
		converted->getPixels());
	converted->free();
	delete converted;
	return name;
}

void TinyGLRenderer::destroyTexture(uint texture) {
	if (!_context || !texture)
		return;
	TinyGL::setContext(_context);
	const TGLuint name = texture;
	tglDeleteTextures(1, &name);
}

// Lights are placed by their frames; the model view matrix must hold the camera only.
void TinyGLRenderer::setupModelLights(const Model &model) {
	const Common::Array<Model::Light> &lights = model.getLights();
	float ambient[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	int count = 0;
	for (uint i = 0; i < lights.size(); ++i) {
		const Model::Light &light = lights[i];
		if (light.type == 0xFFFFFFFF) {
			// Inferred, not traced: "Light_AMB" is taken to be the device ambient colour.
			for (int c = 0; c < 3; ++c)
				ambient[c] += light.diffuse[c];
			continue;
		}
		if (count >= kMaxModelLights || light.type < 1 || light.type > 3)
			continue;
		const float *world = light.frame >= 0 ? model.getWorldMatrix(light.frame) : nullptr;
		float position[4];
		if (light.type == 3) {
			// Directional: the direction the light travels, turned into the direction towards it.
			const float *d = light.direction;
			for (int c = 0; c < 3; ++c)
				position[c] = -(world ? d[0] * world[c] + d[1] * world[4 + c] + d[2] * world[8 + c] : d[c]);
			position[3] = 0.0f;
		} else {
			// Point lights; spot cones are not modelled.
			for (int c = 0; c < 3; ++c)
				position[c] = world ? world[12 + c] : light.position[c];
			position[3] = 1.0f;
		}
		const TGLenum id = TGL_LIGHT0 + count++;
		tglLightfv(id, TGL_AMBIENT, light.ambient);
		tglLightfv(id, TGL_DIFFUSE, light.diffuse);
		tglLightfv(id, TGL_SPECULAR, light.specular);
		tglLightfv(id, TGL_POSITION, position);
		const bool attenuated = light.attenuation[0] != 0.0f || light.attenuation[1] != 0.0f || light.attenuation[2] != 0.0f;
		// The light range is not applied; TinyGL has no per-light cutoff distance.
		tglLightf(id, TGL_CONSTANT_ATTENUATION, attenuated ? light.attenuation[0] : 1.0f);
		tglLightf(id, TGL_LINEAR_ATTENUATION, light.attenuation[1]);
		tglLightf(id, TGL_QUADRATIC_ATTENUATION, light.attenuation[2]);
		tglEnable(id);
	}
	tglLightModelfv(TGL_LIGHT_MODEL_AMBIENT, ambient);
	for (int i = count; i < kMaxModelLights; ++i)
		tglDisable(TGL_LIGHT0 + i);
}

void TinyGLRenderer::drawModelMesh(const Model &model, uint index, const uint *textures, bool alphaPass) {
	const Model::Mesh &mesh = model.getMeshes()[index];
	if (mesh.frame < 0 || mesh.indices.empty())
		return;

	float diffuse[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float zero[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float ambient[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float emissive[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	const float *specular = zero;
	float power = 0.0f;
	if (mesh.material >= 0) {
		const Model::Material &material = model.getMaterials()[mesh.material];
		memcpy(diffuse, material.diffuse, sizeof(diffuse));
		memcpy(ambient, material.ambient, sizeof(ambient));
		memcpy(emissive, material.emissive, sizeof(emissive));
		// The game only enables specular highlights for a power of at least 0.01.
		if (material.power >= 0.01f) {
			specular = material.specular;
			power = material.power;
		}
	}

	// Diffuse alpha doubles as the blend mode: (0, 1] blend, (1, 2] additive, [-1, 0) darken.
	if (alphaPass) {
		const float a = diffuse[3];
		if (a < -1e-5f) {
			// Approximation: TinyGL has no ONE_MINUS_SRC_COLOR, so the destination is scaled by alpha.
			diffuse[3] = MIN(-a, 1.0f);
			tglBlendFunc(TGL_ZERO, TGL_ONE_MINUS_SRC_ALPHA);
		} else if (a > 1.00001f) {
			diffuse[3] = MIN(a - 1.0f, 1.0f);
			tglBlendFunc(TGL_SRC_ALPHA, TGL_ONE);
		} else {
			diffuse[3] = MAX(a, 0.0f);
			tglBlendFunc(TGL_SRC_ALPHA, TGL_ONE_MINUS_SRC_ALPHA);
		}
	} else {
		diffuse[3] = 1.0f;
	}
	tglMaterialfv(TGL_FRONT_AND_BACK, TGL_AMBIENT, ambient);
	tglMaterialfv(TGL_FRONT_AND_BACK, TGL_DIFFUSE, diffuse);
	tglMaterialfv(TGL_FRONT_AND_BACK, TGL_SPECULAR, specular);
	tglMaterialfv(TGL_FRONT_AND_BACK, TGL_EMISSION, emissive);
	tglMaterialf(TGL_FRONT_AND_BACK, TGL_SHININESS, power);

	const uint texture = mesh.texture >= 0 ? textures[mesh.texture] : 0;
	if (texture) {
		tglEnable(TGL_TEXTURE_2D);
		tglBindTexture(TGL_TEXTURE_2D, texture);
	} else {
		tglDisable(TGL_TEXTURE_2D);
	}

	const float *vertices = mesh.vertices.data();
	const TGLsizei stride = Model::kVertexFloats * sizeof(float);
	tglVertexPointer(3, TGL_FLOAT, stride, vertices);
	tglNormalPointer(TGL_FLOAT, stride, vertices + 3);
	tglTexCoordPointer(2, TGL_FLOAT, stride, vertices + 6);
	tglPushMatrix();
	tglMultMatrixf(model.getWorldMatrix(mesh.frame));
	tglDrawElements(TGL_TRIANGLES, mesh.indices.size(), TGL_UNSIGNED_INT, mesh.indices.data());
	tglPopMatrix();
}

void TinyGLRenderer::drawModel(const Model &model, const uint *textures, const float *cameraPosition) {
	if (!_context || !model.isLoaded())
		return;
	TinyGL::setContext(_context);

	tglClearDepth(1.0f);
	tglClear(TGL_DEPTH_BUFFER_BIT);

	// The data is left-handed with +Z forward, so the view looks down +Z and the Z axis is mirrored
	// for TinyGL. The clockwise front faces of Direct3D stay clockwise after the mirroring.
	tglMatrixMode(TGL_MODELVIEW);
	tglPushMatrix();
	tglLoadIdentity();
	tglScalef(1.0f, 1.0f, -1.0f);
	tglTranslatef(-cameraPosition[0], -cameraPosition[1], -cameraPosition[2]);

	tglEnable(TGL_CULL_FACE);
	tglCullFace(TGL_BACK);
	tglFrontFace(TGL_CW);
	tglEnable(TGL_DEPTH_TEST);
	tglDepthFunc(TGL_LEQUAL);
	tglDepthMask(TGL_TRUE);
	tglDisable(TGL_BLEND);
	tglEnable(TGL_LIGHTING);
	tglEnable(TGL_NORMALIZE);
	tglShadeModel(TGL_SMOOTH);
	setupModelLights(model);
	tglEnableClientState(TGL_VERTEX_ARRAY);
	tglEnableClientState(TGL_NORMAL_ARRAY);
	tglEnableClientState(TGL_TEXTURE_COORD_ARRAY);

	const Common::Array<Model::Mesh> &meshes = model.getMeshes();
	_alphaMeshes.clear();
	for (uint i = 0; i < meshes.size(); ++i) {
		const Model::Mesh &mesh = meshes[i];
		if (!model.isMeshVisible(mesh) || mesh.frame < 0)
			continue;
		if (model.isMeshTranslucent(mesh)) {
			const float *world = model.getWorldMatrix(mesh.frame);
			float distance = 0.0f;
			for (int c = 0; c < 3; ++c)
				distance += (world[12 + c] - cameraPosition[c]) * (world[12 + c] - cameraPosition[c]);
			AlphaMesh entry;
			entry.mesh = i;
			entry.group = mesh.alphaGroup;
			entry.distance = sqrtf(distance) + mesh.depthBias;
			_alphaMeshes.push_back(entry);
		} else {
			drawModelMesh(model, i, textures, false);
		}
	}

	// Translucent meshes: higher draw-order group first, then back to front.
	if (!_alphaMeshes.empty()) {
		Common::sort(_alphaMeshes.begin(), _alphaMeshes.end(), [](const AlphaMesh &a, const AlphaMesh &b) {
			return a.group != b.group ? a.group > b.group : a.distance > b.distance;
		});
		tglEnable(TGL_BLEND);
		tglDepthMask(TGL_FALSE);
		for (uint i = 0; i < _alphaMeshes.size(); ++i)
			drawModelMesh(model, _alphaMeshes[i].mesh, textures, true);
		tglDepthMask(TGL_TRUE);
	}

	// Back to the state the 2D blits expect.
	tglDisableClientState(TGL_VERTEX_ARRAY);
	tglDisableClientState(TGL_NORMAL_ARRAY);
	tglDisableClientState(TGL_TEXTURE_COORD_ARRAY);
	tglDisable(TGL_LIGHTING);
	tglDisable(TGL_NORMALIZE);
	for (int i = 0; i < kMaxModelLights; ++i)
		tglDisable(TGL_LIGHT0 + i);
	tglEnable(TGL_TEXTURE_2D);
	tglBlendFunc(TGL_SRC_ALPHA, TGL_ONE_MINUS_SRC_ALPHA);
	setCapability(TGL_BLEND, _blendingEnabled);
	tglMatrixMode(TGL_MODELVIEW);
	tglPopMatrix();
}

void TinyGLRenderer::freeBackgroundCopy(uint index) {
	if (index >= _backgroundCopies.size() || !_backgroundCopies[index])
		return;
	_backgroundCopies[index]->free();
	delete _backgroundCopies[index];
	_backgroundCopies[index] = nullptr;
}

void TinyGLRenderer::destroyImage(uint image) {
	if (!_context || image == 0 || image > _images.size() || !_images[image - 1])
		return;
	TinyGL::setContext(_context);
	tglDeleteBlitImage(_images[image - 1]);
	_images[image - 1] = nullptr;
	freeBackgroundCopy(image - 1);
}

void TinyGLRenderer::drawBackground(uint image, float opacity) {
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
	if (scale >= 256) {
		// Full opacity leaves every lane unchanged, so rows can be copied as they are.
		for (int y = 0; y < _height; ++y)
			memcpy(target.getBasePtr(0, y), copy->getBasePtr(0, y), _width * 4);
		return;
	}
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

void TinyGLRenderer::drawImage(uint image, const Common::Rect &destination, float opacity, bool fadeFromBlack) {
	if (!_context || image == 0 || image > _images.size() || !_images[image - 1] || opacity <= 0.0f || destination.isEmpty())
		return;
	Common::Rect visible = destination;
	visible.clip(Common::Rect(0, 0, _width, _height));
	if (visible.isEmpty())
		return;
	TinyGL::setContext(_context);
	int sourceWidth = 0, sourceHeight = 0;
	tglGetBlitImageSize(_images[image - 1], sourceWidth, sourceHeight);
	// Map the on-screen part back to the matching source pixels.
	const int sourceLeft = (visible.left - destination.left) * sourceWidth / destination.width();
	const int sourceTop = (visible.top - destination.top) * sourceHeight / destination.height();
	const int sourceRight = (visible.right - destination.left) * sourceWidth / destination.width();
	const int sourceBottom = (visible.bottom - destination.top) * sourceHeight / destination.height();
	if (sourceRight <= sourceLeft || sourceBottom <= sourceTop)
		return;
	TinyGL::BlitTransform transform(visible.left, visible.top);
	transform.sourceRectangle(sourceLeft, sourceTop, sourceRight - sourceLeft, sourceBottom - sourceTop);
	// A zero-size destination selects TinyGL's unscaled RLE/memcpy path instead of the per-pixel scaler.
	if (visible.width() != sourceRight - sourceLeft || visible.height() != sourceBottom - sourceTop)
		transform.scale(visible.width(), visible.height());
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

void TinyGLRenderer::drawFadeToWhite(uint image, float amount) {
	amount = CLIP(amount, 0.0f, 1.0f);
	if (!_context || amount <= 0.0f)
		return;
	if (_format.bytesPerPixel != 4 || _format.rBits() != 8 || _format.gBits() != 8 || _format.bBits() != 8) {
		drawImage(image, Common::Rect(0, 0, _width, _height), amount);
		return;
	}

	// Queued draws must land in the buffer before it is modified.
	TinyGL::setContext(_context);
	Common::List<Common::Rect> dirtyAreas;
	TinyGL::presentBuffer(dirtyAreas);
	Graphics::Surface target;
	TinyGL::getSurfaceRef(target);
	const uint32 colorMask = _format.rMax() << _format.rShift | _format.gMax() << _format.gShift |
		_format.bMax() << _format.bShift;
	const uint32 scale = (uint32)(amount * 256.0f);
	for (int y = 0; y < _height; ++y) {
		uint32 *dst = (uint32 *)target.getBasePtr(0, y);
		for (int x = 0; x < _width; ++x) {
			const uint32 pixel = dst[x];
			// Each color lane moves toward 255 by scale/256; two lanes are scaled per multiply.
			const uint32 gap = ~pixel & colorMask;
			const uint32 even = (((gap & 0x00FF00FF) * scale) >> 8) & 0x00FF00FF;
			const uint32 odd = (((gap >> 8) & 0x00FF00FF) * scale) & 0xFF00FF00;
			dst[x] = pixel + ((even | odd) & colorMask);
		}
	}
}

void TinyGLRenderer::cacheScene() {
	if (!_context)
		return;
	TinyGL::setContext(_context);
	Common::List<Common::Rect> dirtyAreas;
	TinyGL::presentBuffer(dirtyAreas);
	Graphics::Surface source;
	TinyGL::getSurfaceRef(source);
	if (!_sceneCopy) {
		_sceneCopy = new Graphics::Surface();
		_sceneCopy->create(_width, _height, source.format);
	}
	for (int y = 0; y < _height; ++y)
		memcpy(_sceneCopy->getBasePtr(0, y), source.getBasePtr(0, y), _width * source.format.bytesPerPixel);
	_sceneValid = true;
}

bool TinyGLRenderer::restoreScene() {
	if (!_context || !_sceneValid)
		return false;
	TinyGL::setContext(_context);
	Graphics::Surface target;
	TinyGL::getSurfaceRef(target);
	for (int y = 0; y < _height; ++y)
		memcpy(target.getBasePtr(0, y), _sceneCopy->getBasePtr(0, y), _width * target.format.bytesPerPixel);
	return true;
}

void TinyGLRenderer::clearScreen() {
	if (!_context)
		return;
	TinyGL::setContext(_context);
	tglClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	tglClearDepth(1.0f);
	tglClear(TGL_COLOR_BUFFER_BIT | TGL_DEPTH_BUFFER_BIT);
}

void TinyGLRenderer::flipBuffer() {
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

#endif // USE_TINYGL
