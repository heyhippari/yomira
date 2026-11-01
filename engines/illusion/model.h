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

#ifndef ILLUSION_MODEL_H
#define ILLUSION_MODEL_H

#include "common/array.h"
#include "common/hashmap.h"
#include "common/noncopyable.h"
#include "common/str.h"

namespace Illusion {

/**
 * A Sexy Beach 2 model (".x", "OBJM" version 8). Coordinates are left-handed Direct3D ones and
 * matrices are 4x4 row-major with row vectors, like the original. Skinning (ENVL) and morphs
 * (MORP) are not parsed yet; MESH vertices stay in rest space so they can be added later.
 */
class Model : public Common::NonCopyable {
public:
	enum { kVertexFloats = 8 }; // position, normal, uv

	struct Material {
		float diffuse[4];
		float ambient[4];
		float specular[4];
		float emissive[4];
		float power;
	};

	struct Texture {
		Common::String file;
	};

	struct Light {
		uint32 type; // 1 point, 2 spot, 3 directional, 0xFFFFFFFF ambient
		float diffuse[4];
		float specular[4];
		float ambient[4];
		float position[3];
		float direction[3];
		float range;
		float attenuation[3];
		int frame;
	};

	struct Mesh {
		int material; // -1 when none
		int texture;  // -1 when the mesh has no texture stage
		int frame;    // -1 when no frame displays the mesh
		uint32 alphaGroup;
		int32 depthBias;
		Common::Array<float> vertices;  // kVertexFloats per vertex
		Common::Array<uint32> indices;  // triangle list, clockwise front faces
	};

	struct Frame {
		int parent; // -1 for a root
		float local[16];
	};

	Model();

	/** Parses a model. On failure the model is left empty and false is returned. */
	bool load(const byte *data, uint32 size);
	void clear();
	bool isLoaded() const { return !_frames.empty(); }

	/** Poses the frames at the given animation time (in frames, 60 per second). */
	void setTime(float time);

	const Common::Array<Material> &getMaterials() const { return _materials; }
	const Common::Array<Texture> &getTextures() const { return _textures; }
	const Common::Array<Light> &getLights() const { return _lights; }
	const Common::Array<Mesh> &getMeshes() const { return _meshes; }
	uint getFrameCount() const { return _frames.size(); }
	/** World matrix of a frame at the current pose. */
	const float *getWorldMatrix(uint frame) const { return _world[frame].m; }
	/** False when the material makes the game skip the mesh (diffuse alpha of exactly 0). */
	bool isMeshVisible(const Mesh &mesh) const;
	/** Points every texture entry named oldName (case-insensitive) at newName, like the game does for "zzz.bmp". */
	void replaceTexture(const Common::String &oldName, const Common::String &newName);
	/** True when the mesh is drawn in the sorted alpha pass because of its material or a .tga/.dds texture. */
	bool isMeshTranslucent(const Mesh &mesh) const;

private:
	struct Key {
		float time;
		float position[3];
		float rotation[4];
		float scale[3];
		bool hasPosition;
		bool hasRotation;
		bool hasScale;
		bool hermite;
	};

	struct Track {
		uint32 frameId;
		int frame;
		Common::Array<Key> keys;
	};

	struct Matrix {
		float m[16];
	};

	struct Group {
		uint first;
		uint count;
	};

	struct Pending {
		uint32 material;
		uint32 texture;
	};

	class Reader;

	Common::Array<Material> _materials;
	Common::Array<uint32> _materialIds;
	Common::Array<Texture> _textures;
	Common::Array<uint32> _textureIds;
	Common::Array<Light> _lights;
	Common::Array<uint32> _lightIds;
	Common::Array<Mesh> _meshes;
	Common::Array<Pending> _pending;
	Common::Array<Group> _groups;
	Common::HashMap<uint32, uint> _groupById;
	Common::Array<Frame> _frames;
	Common::Array<uint32> _frameIds;
	Common::Array<uint32> _frameVisuals;
	Common::Array<uint32> _frameLights;
	Common::Array<uint32> _frameParents;
	Common::Array<Track> _tracks;
	Common::Array<Matrix> _world;

	bool parseMaterials(Reader &r);
	bool parseTextures(Reader &r);
	bool parseLights(Reader &r);
	bool parseMeshes(Reader &r);
	bool parseMeshBody(Reader &r);
	bool parseFrames(Reader &r);
	bool parseAnimations(Reader &r);
	bool resolve();
	static void prepareTrack(Track &track);
	static void evaluateTrack(const Track &track, float time, float *matrix);
};

/** The clip table of a ".xan" animation file. */
class ModelClips : public Common::NonCopyable {
public:
	enum { kClipCount = 128 };

	struct Clip {
		bool used;
		bool loop;
		uint32 loopStart;
		uint32 ticks;
		float start;
		float end;
		float step; // animation time per tick
		bool chain;
		uint32 next;
		uint32 repeat;
	};

	ModelClips();

	/** Parses a ".xan" file (a fixed size). Returns false for anything else. */
	bool load(const byte *data, uint32 size);
	const Common::String &getModelFile() const { return _modelFile; }
	const Clip *getClip(uint index) const;

private:
	Common::String _modelFile;
	Clip _clips[kClipCount];
};

/** Plays the clips of a ModelClips like the game does. */
class ModelClipPlayer {
public:
	ModelClipPlayer();

	void play(const ModelClips &clips, uint index);
	/** Advances by the given number of seconds. */
	void update(float seconds);
	float getTime() const { return _time; }
	bool isPlaying() const { return _clips && _playing; }

private:
	const ModelClips *_clips;
	uint _index;
	float _tick;
	float _time;
	uint32 _loops;
	bool _playing;
};

} // End of namespace Illusion

#endif // ILLUSION_MODEL_H
