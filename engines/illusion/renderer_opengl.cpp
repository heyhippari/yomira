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

#include "illusion/renderer_opengl.h"

#if defined(USE_OPENGL_GAME)

#include "illusion/illusion.h"

#include "common/algorithm.h"
#include "common/system.h"
#include "graphics/opengl/context.h"
#include "graphics/surface.h"

#include <math.h>
#include <string.h>

namespace Illusion {

static const int kMaxModelLights = 8;

static void setCapability(GLenum capability, bool enabled) {
	if (enabled)
		glEnable(capability);
	else
		glDisable(capability);
}

static bool isPowerOfTwo(int value) {
	return value > 0 && (value & (value - 1)) == 0;
}

static int nextPowerOfTwo(int value) {
	int result = 1;
	while (result < value)
		result <<= 1;
	return result;
}

// Checks palette indices, then converts to RGBA bytes. The caller owns the result.
static Graphics::Surface *convertToRGBA(const Graphics::Surface &surface, const Graphics::Palette &palette) {
	if (!surface.getPixels() || surface.w <= 0 || surface.h <= 0)
		return nullptr;
	if (surface.format.bytesPerPixel == 1) {
		if (palette.empty() || palette.size() > 256)
			return nullptr;
		for (int row = 0; row < surface.h; ++row) {
			const byte *pixels = (const byte *)surface.getBasePtr(0, row);
			for (int column = 0; column < surface.w; ++column) {
				if (pixels[column] >= palette.size())
					return nullptr;
			}
		}
	}
	return surface.convertTo(Graphics::PixelFormat::createFormatRGBA32(), palette.data(), palette.size());
}

OpenGLRenderer::OpenGLRenderer(OSystem *system) : _system(system), _initialized(false), _width(0), _height(0),
		_nearClip(1.0f), _farClip(2.0f), _frustumRight(1.0f), _frustumTop(1.0f), _blendingEnabled(false),
		_depthTestEnabled(true), _ditheringEnabled(false), _separateSpecular(false), _frameStateValid(false) {
}

OpenGLRenderer::~OpenGLRenderer() {
	if (!_initialized)
		return;
	for (uint index = 0; index < _images.size(); ++index) {
		if (_images[index].name)
			glDeleteTextures(1, &_images[index].name);
	}
	for (uint index = 0; index < _modelTextures.size(); ++index)
		glDeleteTextures(1, &_modelTextures[index]);
}

bool OpenGLRenderer::init(int width, int height, const Graphics::PixelFormat &format,
		const IllusionRendererState &state) {
	if (_initialized)
		return false;
	if (width <= 0 || height <= 0 || state.projectionMatrix[0] <= 0.0f || state.projectionMatrix[5] <= 0.0f ||
			state.projectionNear <= 0.0f || state.projectionFar <= state.projectionNear)
		return false;
	if (!_system->hasFeature(OSystem::kFeatureOpenGLForGame) || _system->getOpenGLType() != OpenGL::kContextGL)
		return false;

	_width = width;
	_height = height;
	_modelViewport = state.viewport;
	_nearClip = state.projectionNear;
	_farClip = state.projectionFar;
	// The frustum is derived from the Direct3D projection matrix; its depth range is OpenGL's own.
	_frustumTop = _nearClip / state.projectionMatrix[5];
	_frustumRight = _nearClip / state.projectionMatrix[0];
	_blendingEnabled = state.alphaBlendingEnabled;
	_depthTestEnabled = state.depthBufferEnabled;
	_ditheringEnabled = state.ditheringEnabled;
	_separateSpecular = OpenGLContext.isGLVersionOrHigher(1, 2);
	_initialized = true;
	_frameStateValid = false;
	return true;
}

// Puts the context into the state the 2D draws expect. The backend changes the state when it presents,
// so this runs once after every flip and after every model pass.
void OpenGLRenderer::setupFrameState() {
	glViewport(0, 0, _width, _height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, _width, _height, 0.0, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// Make sure the fixed-function pipeline is active, with no buffer objects behind the client pointers.
	if (OpenGLContext.shadersSupported && GLAD_GL_VERSION_2_0)
		glUseProgram(0);
	if (GLAD_GL_VERSION_1_5) {
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
	if (GLAD_GL_VERSION_1_3) {
		glActiveTexture(GL_TEXTURE0);
		glClientActiveTexture(GL_TEXTURE0);
	}

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDisable(GL_CULL_FACE);
	glFrontFace(GL_CCW);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_FOG);
	glDisable(GL_LIGHTING);
	glDisable(GL_NORMALIZE);
	glDisable(GL_COLOR_MATERIAL);
	for (int i = 0; i < kMaxModelLights; ++i)
		glDisable(GL_LIGHT0 + i);
	if (_separateSpecular)
		glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SINGLE_COLOR);
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_FALSE);
	glShadeModel(GL_SMOOTH);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	setCapability(GL_DITHER, _ditheringEnabled);

	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	setCapability(GL_BLEND, _blendingEnabled);
	_frameStateValid = true;
}

GLuint OpenGLRenderer::uploadTexture(const Graphics::Surface &surface, const Graphics::Palette &palette,
		bool repeat, int &textureWidth, int &textureHeight) {
	if (surface.w > OpenGLContext.maxTextureSize || surface.h > OpenGLContext.maxTextureSize)
		return 0;
	textureWidth = surface.w;
	textureHeight = surface.h;
	if (!OpenGLContext.NPOTSupported) {
		if (repeat && (!isPowerOfTwo(surface.w) || !isPowerOfTwo(surface.h)))
			return 0;
		textureWidth = nextPowerOfTwo(surface.w);
		textureHeight = nextPowerOfTwo(surface.h);
		if (textureWidth > OpenGLContext.maxTextureSize || textureHeight > OpenGLContext.maxTextureSize)
			return 0;
	}

	Graphics::Surface *converted = convertToRGBA(surface, palette);
	if (!converted)
		return 0;

	GLuint name = 0;
	glGenTextures(1, &name);
	glBindTexture(GL_TEXTURE_2D, name);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	const GLint wrap = repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, converted->pitch / 4);
	if (textureWidth == converted->w && textureHeight == converted->h) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
			converted->getPixels());
	} else {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, converted->w, converted->h, GL_RGBA, GL_UNSIGNED_BYTE,
			converted->getPixels());
	}
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	converted->free();
	delete converted;
	return name;
}

uint OpenGLRenderer::createImage(const Graphics::Surface &surface, const Graphics::Palette &palette, bool) {
	if (!_initialized)
		return 0;
	int textureWidth = 0, textureHeight = 0;
	const GLuint name = uploadTexture(surface, palette, false, textureWidth, textureHeight);
	if (!name)
		return 0;
	Image image;
	image.name = name;
	image.width = surface.w;
	image.height = surface.h;
	image.maxU = surface.w / (float)textureWidth;
	image.maxV = surface.h / (float)textureHeight;
	_images.push_back(image);
	return _images.size();
}

void OpenGLRenderer::destroyImage(uint image) {
	if (!_initialized || image == 0 || image > _images.size() || !_images[image - 1].name)
		return;
	glDeleteTextures(1, &_images[image - 1].name);
	_images[image - 1].name = 0;
}

uint OpenGLRenderer::createTexture(const Graphics::Surface &surface, const Graphics::Palette &palette) {
	if (!_initialized)
		return 0;
	int textureWidth = 0, textureHeight = 0;
	const GLuint name = uploadTexture(surface, palette, true, textureWidth, textureHeight);
	if (name)
		_modelTextures.push_back(name);
	return name;
}

void OpenGLRenderer::destroyTexture(uint texture) {
	if (!_initialized || !texture)
		return;
	for (uint index = 0; index < _modelTextures.size(); ++index) {
		if (_modelTextures[index] == texture) {
			_modelTextures.remove_at(index);
			const GLuint name = texture;
			glDeleteTextures(1, &name);
			return;
		}
	}
}

void OpenGLRenderer::drawQuad(const Common::Rect &destination, float maxU, float maxV) {
	glBegin(GL_TRIANGLE_STRIP);
	glTexCoord2f(0.0f, 0.0f);
	glVertex2i(destination.left, destination.top);
	glTexCoord2f(0.0f, maxV);
	glVertex2i(destination.left, destination.bottom);
	glTexCoord2f(maxU, 0.0f);
	glVertex2i(destination.right, destination.top);
	glTexCoord2f(maxU, maxV);
	glVertex2i(destination.right, destination.bottom);
	glEnd();
}

void OpenGLRenderer::drawImage(uint image, const Common::Rect &destination, float opacity, bool fadeFromBlack) {
	if (!_initialized || image == 0 || image > _images.size() || !_images[image - 1].name || opacity <= 0.0f ||
			destination.isEmpty())
		return;
	if (!_frameStateValid)
		setupFrameState();
	opacity = MIN(opacity, 1.0f);
	const Image &entry = _images[image - 1];
	glBindTexture(GL_TEXTURE_2D, entry.name);
	if (fadeFromBlack && opacity < 1.0f) {
		// Over a cleared black buffer, scaling RGB equals alpha blending.
		glDisable(GL_BLEND);
		glColor4f(opacity, opacity, opacity, 1.0f);
		drawQuad(destination, entry.maxU, entry.maxV);
		setCapability(GL_BLEND, _blendingEnabled);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		return;
	}
	glColor4f(1.0f, 1.0f, 1.0f, opacity);
	drawQuad(destination, entry.maxU, entry.maxV);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

void OpenGLRenderer::drawBackground(uint image, float opacity) {
	if (!_initialized)
		return;
	clearScreen();
	drawImage(image, Common::Rect(0, 0, _width, _height), CLIP(opacity, 0.0f, 1.0f), true);
}

void OpenGLRenderer::drawFadeToWhite(uint, float amount) {
	amount = CLIP(amount, 0.0f, 1.0f);
	if (!_initialized || amount <= 0.0f)
		return;
	if (!_frameStateValid)
		setupFrameState();
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glColor4f(1.0f, 1.0f, 1.0f, amount);
	glBegin(GL_TRIANGLE_STRIP);
	glVertex2i(0, 0);
	glVertex2i(0, _height);
	glVertex2i(_width, 0);
	glVertex2i(_width, _height);
	glEnd();
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_TEXTURE_2D);
	setCapability(GL_BLEND, _blendingEnabled);
}

// Lights are placed by their frames; the model view matrix must hold the camera only.
void OpenGLRenderer::setupModelLights(const Model &model) {
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
		const GLenum id = GL_LIGHT0 + count++;
		glLightfv(id, GL_AMBIENT, light.ambient);
		glLightfv(id, GL_DIFFUSE, light.diffuse);
		glLightfv(id, GL_SPECULAR, light.specular);
		glLightfv(id, GL_POSITION, position);
		const bool attenuated = light.attenuation[0] != 0.0f || light.attenuation[1] != 0.0f || light.attenuation[2] != 0.0f;
		// The light range is not applied; fixed-function GL has no per-light cutoff distance.
		glLightf(id, GL_CONSTANT_ATTENUATION, attenuated ? light.attenuation[0] : 1.0f);
		glLightf(id, GL_LINEAR_ATTENUATION, light.attenuation[1]);
		glLightf(id, GL_QUADRATIC_ATTENUATION, light.attenuation[2]);
		glEnable(id);
	}
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
	for (int i = count; i < kMaxModelLights; ++i)
		glDisable(GL_LIGHT0 + i);
}

void OpenGLRenderer::drawModelMesh(const Model &model, uint index, const uint *textures, bool alphaPass) {
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
			power = MIN(material.power, 128.0f);
		}
	}

	// Diffuse alpha doubles as the blend mode: (0, 1] blend, (1, 2] additive, [-1, 0) darken.
	if (alphaPass) {
		const float a = diffuse[3];
		if (a < -1e-5f) {
			diffuse[3] = MIN(-a, 1.0f);
			glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
		} else if (a > 1.00001f) {
			diffuse[3] = MIN(a - 1.0f, 1.0f);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		} else {
			diffuse[3] = MAX(a, 0.0f);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
	} else {
		diffuse[3] = 1.0f;
	}
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
	glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emissive);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, power);

	const uint texture = mesh.texture >= 0 ? textures[mesh.texture] : 0;
	if (texture) {
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, texture);
	} else {
		glDisable(GL_TEXTURE_2D);
	}

	const float *vertices = mesh.vertices.data();
	const GLsizei stride = Model::kVertexFloats * sizeof(float);
	glVertexPointer(3, GL_FLOAT, stride, vertices);
	glNormalPointer(GL_FLOAT, stride, vertices + 3);
	glTexCoordPointer(2, GL_FLOAT, stride, vertices + 6);
	glPushMatrix();
	glMultMatrixf(model.getWorldMatrix(mesh.frame));
	glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, mesh.indices.data());
	glPopMatrix();
}

void OpenGLRenderer::drawModel(const Model &model, const uint *textures, const float *cameraPosition) {
	if (!_initialized || !model.isLoaded())
		return;
	if (!_frameStateValid)
		setupFrameState();

	glViewport(_modelViewport.left, _height - _modelViewport.bottom, _modelViewport.width(), _modelViewport.height());
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(-_frustumRight, _frustumRight, -_frustumTop, _frustumTop, _nearClip, _farClip);

	glClearDepth(1.0);
	glClear(GL_DEPTH_BUFFER_BIT);

	// The data is left-handed with +Z forward, so the view looks down +Z and the Z axis is mirrored
	// for OpenGL. The clockwise front faces of Direct3D stay clockwise after the mirroring.
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glScalef(1.0f, 1.0f, -1.0f);
	glTranslatef(-cameraPosition[0], -cameraPosition[1], -cameraPosition[2]);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CW);
	setCapability(GL_DEPTH_TEST, true);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glEnable(GL_LIGHTING);
	glEnable(GL_NORMALIZE);
	glShadeModel(GL_SMOOTH);
	// Direct3D computes specular highlights with a local viewer and adds them after texturing.
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);
	if (_separateSpecular)
		glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);
	setupModelLights(model);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

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
		glEnable(GL_BLEND);
		glDepthMask(GL_FALSE);
		for (uint i = 0; i < _alphaMeshes.size(); ++i)
			drawModelMesh(model, _alphaMeshes[i].mesh, textures, true);
		glDepthMask(GL_TRUE);
	}

	// Back to the state the 2D draws expect.
	setupFrameState();
}

void OpenGLRenderer::clearScreen() {
	if (!_initialized)
		return;
	setupFrameState();
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClearDepth(1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLRenderer::flipBuffer() {
	if (!_initialized)
		return;
	_system->updateScreen();
	_frameStateValid = false;
}

} // End of namespace Illusion

#endif // USE_OPENGL_GAME
