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
#include "illusion/renderer_opengl.h"
#include "illusion/renderer_tinygl.h"

#include "common/config-manager.h"
#include "common/debug.h"
#include "graphics/renderer.h"

namespace Illusion {

Graphics::RendererType selectRendererType() {
	const Graphics::RendererType desired = Graphics::Renderer::parseTypeCode(ConfMan.get("renderer"));
	uint32 supported = 0;
#if defined(USE_TINYGL)
	supported |= Graphics::kRendererTypeTinyGL;
#endif
#if defined(USE_OPENGL_GAME)
	// getAvailableTypes() only reports OpenGL when the backend has kFeatureOpenGLForGame.
	supported |= Graphics::kRendererTypeOpenGL;
#endif
	if (!(Graphics::Renderer::getAvailableTypes() & supported))
		return Graphics::kRendererTypeDefault;
	const Graphics::RendererType type = Graphics::Renderer::getBestMatchingAvailableType(desired, supported);
	debug(1, "Illusion: using the %s renderer", Graphics::Renderer::getTypeCode(type).c_str());
	return type;
}

IllusionRenderer *createRenderer(OSystem *system, Graphics::RendererType type) {
#if defined(USE_OPENGL_GAME)
	if (type == Graphics::kRendererTypeOpenGL)
		return new OpenGLRenderer(system);
#endif
#if defined(USE_TINYGL)
	if (type == Graphics::kRendererTypeTinyGL)
		return new TinyGLRenderer(system);
#endif
	return nullptr;
}

} // End of namespace Illusion
