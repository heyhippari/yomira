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

#include "illusion/model.h"

#include "common/endian.h"
#include "common/util.h"

#include <math.h>
#include <string.h>

namespace Illusion {

static const uint32 kHeaderSize = 12;
static const uint32 kObjectHeaderSize = 0x48;
static const uint32 kMaterialSize = 0x8C;
static const uint32 kTextureSize = 0xCC;
static const uint32 kLightSize = 0xAC;
static const uint32 kFrameSize = 0x18C;
static const uint32 kMeshInfoSize = 0x14C;
static const uint32 kFileVertexSize = 60;
static const uint32 kTrackHeaderSize = 24;
static const uint32 kKeySize = 0xDC;
static const uint32 kMaxVertices = 0x10000;

static const uint32 kXanSize = 0x5190;
static const uint32 kXanModelFileOffset = 0x100;
static const uint32 kXanModelFileSize = 0x100;
static const uint32 kXanClipOffset = 0x200 + 0x190;
static const uint32 kXanClipSize = 0x9C;

/** Bounded little-endian reader over one chunk. */
class Model::Reader {
	const byte *_data;
	uint32 _position;
	uint32 _end;

public:
	Reader(const byte *data, uint32 start, uint32 end) : _data(data), _position(start), _end(end) {}

	uint32 left() const { return _end - _position; }
	bool has(uint64 bytes) const { return bytes <= left(); }
	const byte *current() const { return _data + _position; }
	void skip(uint32 bytes) { _position += bytes; }
	uint32 position() const { return _position; }
};

static Common::String readString(const byte *data, uint32 maxLength) {
	uint32 length = 0;
	while (length < maxLength && data[length])
		++length;
	return Common::String((const char *)data, length);
}

static void copyFloats(float *destination, const byte *source, uint count) {
	for (uint index = 0; index < count; ++index)
		destination[index] = READ_LE_FLOAT32(source + index * 4);
}

static void multiplyMatrices(const float *a, const float *b, float *result) {
	float temp[16];
	for (int row = 0; row < 4; ++row) {
		for (int column = 0; column < 4; ++column) {
			float sum = 0.0f;
			for (int k = 0; k < 4; ++k)
				sum += a[row * 4 + k] * b[k * 4 + column];
			temp[row * 4 + column] = sum;
		}
	}
	memcpy(result, temp, sizeof(temp));
}

// D3DXMatrixTransformation(NULL, NULL, scale, NULL, rotation, translation) = S * R * T.
static void buildMatrix(const float *scale, const float *q, const float *position, float *m) {
	const float x = q[0], y = q[1], z = q[2], w = q[3];
	const float rotation[9] = {
		1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w),
		2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w),
		2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y)
	};
	for (int row = 0; row < 3; ++row) {
		for (int column = 0; column < 3; ++column)
			m[row * 4 + column] = rotation[row * 3 + column] * scale[row];
		m[row * 4 + 3] = 0.0f;
	}
	m[12] = position[0];
	m[13] = position[1];
	m[14] = position[2];
	m[15] = 1.0f;
}

// The D3DX slerp: the shorter arc, falling back to a linear blend for nearly equal rotations.
static void slerp(const float *a, const float *b, float t, float *result) {
	float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
	float sign = 1.0f;
	if (dot < 0.0f) {
		sign = -1.0f;
		dot = -dot;
	}
	float ka = 1.0f - t, kb = t;
	if (1.0f - dot > 1e-5f) {
		const float theta = acosf(MIN(1.0f, dot));
		const float s = sinf(theta);
		ka = sinf((1.0f - t) * theta) / s;
		kb = sinf(t * theta) / s;
	}
	for (int i = 0; i < 4; ++i)
		result[i] = ka * a[i] + kb * sign * b[i];
}

static void lerp3(const float *a, const float *b, float t, float *result) {
	for (int i = 0; i < 3; ++i)
		result[i] = a[i] + (b[i] - a[i]) * t;
}

static void tangent(const float *p, float *result) {
	const float length = sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
	const float scale = length > 0.0f ? 1.0f / length : 1.0f;
	result[0] = -p[2] * scale;
	result[1] = p[1] * scale;
	result[2] = p[0] * scale;
}

static void hermite(const float *p0, const float *p1, float t, float *result) {
	float t0[3], t1[3];
	tangent(p0, t0);
	tangent(p1, t1);
	const float t2 = t * t, t3 = t2 * t;
	const float h00 = 2 * t3 - 3 * t2 + 1, h10 = t3 - 2 * t2 + t, h01 = -2 * t3 + 3 * t2, h11 = t3 - t2;
	for (int i = 0; i < 3; ++i)
		result[i] = h00 * p0[i] + h10 * t0[i] + h01 * p1[i] + h11 * t1[i];
}

Model::Model() {
}

void Model::clear() {
	_materials.clear();
	_materialIds.clear();
	_textures.clear();
	_textureIds.clear();
	_lights.clear();
	_lightIds.clear();
	_meshes.clear();
	_pending.clear();
	_groups.clear();
	_groupById.clear();
	_frames.clear();
	_frameIds.clear();
	_frameVisuals.clear();
	_frameLights.clear();
	_frameParents.clear();
	_tracks.clear();
	_world.clear();
}

bool Model::load(const byte *data, uint32 size) {
	clear();
	if (!data || size < kHeaderSize || memcmp(data, "OBJM", 4) != 0)
		return false;

	uint32 position = kHeaderSize;
	bool ok = true;
	while (ok && position < size) {
		if (size - position < 8) {
			ok = false;
			break;
		}
		const uint32 tag = READ_BE_UINT32(data + position);
		const uint32 chunkSize = READ_LE_UINT32(data + position + 4);
		if (chunkSize > size - position - 8) {
			ok = false;
			break;
		}
		Reader reader(data, position + 8, position + 8 + chunkSize);
		bool known = true;
		switch (tag) {
		case MKTAG('M', 'A', 'T', 'E'):
			ok = parseMaterials(reader);
			break;
		case MKTAG('T', 'E', 'X', 'T'):
			ok = parseTextures(reader);
			break;
		case MKTAG('L', 'I', 'G', 'H'):
			ok = parseLights(reader);
			break;
		case MKTAG('M', 'E', 'S', 'H'):
			ok = parseMeshes(reader);
			break;
		case MKTAG('F', 'R', 'A', 'M'):
			ok = parseFrames(reader);
			break;
		case MKTAG('A', 'N', 'I', 'M'):
			ok = parseAnimations(reader);
			break;
		// Chunks that are not used yet. They are skipped by their size.
		case MKTAG('F', 'O', 'G', ' '):
		case MKTAG('B', 'L', 'N', 'D'):
		case MKTAG('M', 'A', 'T', 'A'):
		case MKTAG('T', 'E', 'X', 'A'):
		case MKTAG('L', 'I', 'G', 'A'):
		case MKTAG('M', 'O', 'R', 'P'):
		case MKTAG('P', 'A', 'R', 'T'):
		case MKTAG('E', 'N', 'V', 'L'):
		case MKTAG('C', 'N', 'S', 'T'):
			break;
		default:
			// The game stops reading at a tag it does not know.
			known = false;
			break;
		}
		if (!known)
			break;
		position += 8 + chunkSize;
	}

	if (!ok || !resolve()) {
		clear();
		return false;
	}
	return true;
}

bool Model::parseMaterials(Reader &r) {
	while (r.left()) {
		if (!r.has(kMaterialSize))
			return false;
		const byte *record = r.current();
		Material material;
		copyFloats(material.diffuse, record + 0x44, 4);
		copyFloats(material.ambient, record + 0x54, 4);
		copyFloats(material.specular, record + 0x64, 4);
		copyFloats(material.emissive, record + 0x74, 4);
		material.power = READ_LE_FLOAT32(record + 0x84);
		_materials.push_back(material);
		_materialIds.push_back(READ_LE_UINT32(record + 0x40));
		r.skip(kMaterialSize);
	}
	return true;
}

bool Model::parseTextures(Reader &r) {
	while (r.left()) {
		if (!r.has(kTextureSize))
			return false;
		const byte *record = r.current();
		Texture texture;
		texture.file = readString(record + 0x44, 0x88);
		_textures.push_back(texture);
		_textureIds.push_back(READ_LE_UINT32(record + 0x40));
		r.skip(kTextureSize);
	}
	return true;
}

bool Model::parseLights(Reader &r) {
	while (r.left()) {
		if (!r.has(kLightSize))
			return false;
		const byte *record = r.current();
		Light light;
		light.type = READ_LE_UINT32(record + 0x44);
		copyFloats(light.diffuse, record + 0x48, 4);
		copyFloats(light.specular, record + 0x58, 4);
		copyFloats(light.ambient, record + 0x68, 4);
		copyFloats(light.position, record + 0x78, 3);
		copyFloats(light.direction, record + 0x84, 3);
		light.range = READ_LE_FLOAT32(record + 0x90);
		copyFloats(light.attenuation, record + 0x98, 3);
		light.frame = -1;
		_lights.push_back(light);
		_lightIds.push_back(READ_LE_UINT32(record + 0x40));
		r.skip(kLightSize);
	}
	return true;
}

bool Model::parseMeshes(Reader &r) {
	while (r.left()) {
		if (!r.has(kObjectHeaderSize))
			return false;
		const uint32 id = READ_LE_UINT32(r.current() + 0x40);
		const uint32 count = READ_LE_UINT32(r.current() + 0x44);
		r.skip(kObjectHeaderSize);

		// A single mesh is its own group; a group is followed by its sub-meshes, each with a header.
		const bool single = count == 1;
		if (!single && count > r.left() / (kObjectHeaderSize + kMeshInfoSize))
			return false;
		Group group;
		group.first = _meshes.size();
		group.count = count;
		for (uint32 index = 0; index < count; ++index) {
			if (!single) {
				if (!r.has(kObjectHeaderSize))
					return false;
				r.skip(kObjectHeaderSize);
			}
			if (!parseMeshBody(r))
				return false;
		}
		_groupById.setVal(id, _groups.size());
		_groups.push_back(group);
	}
	return true;
}

bool Model::parseMeshBody(Reader &r) {
	if (!r.has(kMeshInfoSize))
		return false;
	const byte *info = r.current();
	const uint32 materialId = READ_LE_UINT32(info + 0x04);
	const uint32 textureId = READ_LE_UINT32(info + 0x08);
	const uint32 primitive = READ_LE_UINT32(info + 0x38);
	const uint32 vertexCount = READ_LE_UINT32(info + 0x40);
	const uint32 indexCount = READ_LE_UINT32(info + 0x44);
	const uint32 stages = READ_LE_UINT32(info + 0x48);
	if (vertexCount >= kMaxVertices || indexCount >= kMaxVertices)
		return false;
	if (!r.has((uint64)kMeshInfoSize + (uint64)vertexCount * kFileVertexSize + (uint64)indexCount * 2))
		return false;

	_meshes.push_back(Mesh());
	Mesh &mesh = _meshes.back();
	mesh.material = -1;
	mesh.texture = -1;
	mesh.frame = -1;
	mesh.alphaGroup = READ_LE_UINT32(info + 0x4C);
	mesh.depthBias = (int32)READ_LE_UINT32(info + 0x50);
	Pending pending;
	pending.material = materialId;
	pending.texture = stages ? textureId : 0;
	_pending.push_back(pending);

	const byte *vertex = info + kMeshInfoSize;
	mesh.vertices.resize(vertexCount * kVertexFloats);
	for (uint32 index = 0; index < vertexCount; ++index, vertex += kFileVertexSize) {
		float *out = &mesh.vertices[index * kVertexFloats];
		copyFloats(out, vertex, 3);
		copyFloats(out + 3, vertex + 0x10, 3);
		copyFloats(out + 6, vertex + 0x1C, 2);
		const float length = sqrtf(out[3] * out[3] + out[4] * out[4] + out[5] * out[5]);
		if (length > 0.0f) {
			for (int i = 3; i < 6; ++i)
				out[i] /= length;
		}
	}

	const byte *indices = vertex;
	if (primitive == 0) {
		const uint32 triangles = indexCount / 3;
		mesh.indices.resize(triangles * 3);
		for (uint32 index = 0; index < triangles * 3; ++index) {
			const uint32 value = READ_LE_UINT16(indices + index * 2);
			if (value >= vertexCount)
				return false;
			mesh.indices[index] = value;
		}
	} else if (primitive == 1 && vertexCount >= 3) {
		for (uint32 index = 0; index + 2 < vertexCount; ++index) {
			const bool odd = index & 1;
			mesh.indices.push_back(odd ? index + 1 : index);
			mesh.indices.push_back(odd ? index : index + 1);
			mesh.indices.push_back(index + 2);
		}
	} else if (primitive == 2 && vertexCount >= 3) {
		for (uint32 index = 1; index + 1 < vertexCount; ++index) {
			mesh.indices.push_back(0);
			mesh.indices.push_back(index);
			mesh.indices.push_back(index + 1);
		}
	}
	r.skip(kMeshInfoSize + vertexCount * kFileVertexSize + indexCount * 2);
	return true;
}

bool Model::parseFrames(Reader &r) {
	while (r.left()) {
		if (!r.has(kFrameSize))
			return false;
		const byte *record = r.current();
		Frame frame;
		copyFloats(frame.local, record + 0x44, 16);
		frame.parent = -1;
		_frames.push_back(frame);
		_frameIds.push_back(READ_LE_UINT32(record + 0x40));
		// Parent, mesh and light references are resolved once every chunk has been read.
		_frameVisuals.push_back(READ_LE_UINT32(record + 0xB0));
		_frameLights.push_back(READ_LE_UINT32(record + 0xB4));
		_frameParents.push_back(READ_LE_UINT32(record + 0xAC));
		_world.push_back(Matrix());
		r.skip(kFrameSize);
	}
	return true;
}

bool Model::parseAnimations(Reader &r) {
	if (!r.has(kObjectHeaderSize))
		return false;
	const uint32 trackCount = READ_LE_UINT32(r.current() + 0x44);
	r.skip(kObjectHeaderSize);
	for (uint32 index = 0; index < trackCount; ++index) {
		if (!r.has(kTrackHeaderSize))
			return false;
		const uint32 frameId = READ_LE_UINT32(r.current());
		const uint32 keyCount = READ_LE_UINT32(r.current() + 20);
		r.skip(kTrackHeaderSize);
		if (keyCount > r.left() / kKeySize)
			return false;

		_tracks.push_back(Track());
		Track &track = _tracks.back();
		track.frameId = frameId;
		track.frame = -1;
		track.keys.resize(keyCount);
		for (uint32 k = 0; k < keyCount; ++k, r.skip(kKeySize)) {
			const byte *record = r.current();
			Key &key = track.keys[k];
			key.time = READ_LE_FLOAT32(record);
			key.hasPosition = READ_LE_UINT32(record + 0x04) != 0;
			copyFloats(key.position, record + 0x08, 3);
			key.hasRotation = READ_LE_UINT32(record + 0x14) != 0;
			key.hasScale = READ_LE_UINT32(record + 0x24) != 0;
			copyFloats(key.scale, record + 0x28, 3);
			copyFloats(key.rotation, record + 0x74, 4);
			key.hermite = READ_LE_UINT32(record + 0xA0) != 0;
		}
		prepareTrack(track);
	}
	return true;
}

// Fills the channels a key does not carry from the neighbouring keys that do (FUN_00406879).
void Model::prepareTrack(Track &track) {
	const uint count = track.keys.size();
	if (!count)
		return;
	for (int channel = 0; channel < 3; ++channel) {
		const uint size = channel == 0 ? 4 : 3;
		// Index of the next key that carries the channel, or count when there is none.
		Common::Array<uint> next(count);
		uint following = count;
		for (uint i = count; i-- > 0;) {
			const Key &key = track.keys[i];
			const bool flagged = channel == 0 ? key.hasRotation : (channel == 1 ? key.hasScale : key.hasPosition);
			if (flagged)
				following = i;
			next[i] = following;
		}
		uint previous = 0;
		for (uint i = 0; i < count; ++i) {
			Key &key = track.keys[i];
			float *value = channel == 0 ? key.rotation : (channel == 1 ? key.scale : key.position);
			const bool flagged = channel == 0 ? key.hasRotation : (channel == 1 ? key.hasScale : key.hasPosition);
			if (flagged) {
				previous = i;
				continue;
			}
			const Key &before = track.keys[previous];
			const float *beforeValue = channel == 0 ? before.rotation : (channel == 1 ? before.scale : before.position);
			if (next[i] >= count || track.keys[next[i]].time == before.time) {
				for (uint j = 0; j < size; ++j)
					value[j] = beforeValue[j];
				continue;
			}
			const Key &after = track.keys[next[i]];
			const float *afterValue = channel == 0 ? after.rotation : (channel == 1 ? after.scale : after.position);
			const float f = (key.time - before.time) / (after.time - before.time);
			if (channel == 0) {
				float result[4];
				slerp(beforeValue, afterValue, f, result);
				memcpy(value, result, sizeof(result));
			} else {
				float result[3];
				lerp3(beforeValue, afterValue, f, result);
				memcpy(value, result, sizeof(result));
			}
		}
	}
}

void Model::evaluateTrack(const Track &track, float time, float *matrix) {
	const Common::Array<Key> &keys = track.keys;
	const Key &last = keys.back();
	if (time > last.time) {
		buildMatrix(last.scale, last.rotation, last.position, matrix);
		return;
	}
	uint first = 0;
	int second = -1;
	for (uint i = 0; i < keys.size(); ++i) {
		const Key &key = keys[i];
		if (key.time == time) {
			buildMatrix(key.scale, key.rotation, key.position, matrix);
			return;
		}
		if (key.time < time) {
			first = i;
		} else {
			second = i;
			break;
		}
	}
	const Key &k0 = keys[first];
	if (second < 0 || (uint)second == first) {
		buildMatrix(k0.scale, k0.rotation, k0.position, matrix);
		return;
	}
	const Key &k1 = keys[second];
	const float f = (time - k0.time) / (k1.time - k0.time);
	float rotation[4], position[3], scale[3];
	slerp(k0.rotation, k1.rotation, f, rotation);
	if (k0.hermite)
		hermite(k0.position, k1.position, f, position);
	else
		lerp3(k0.position, k1.position, f, position);
	lerp3(k0.scale, k1.scale, f, scale);
	buildMatrix(scale, rotation, position, matrix);
}

bool Model::resolve() {
	Common::HashMap<uint32, uint> frameIndex;
	for (uint i = 0; i < _frames.size(); ++i)
		frameIndex.setVal(_frameIds[i], i);
	for (uint i = 0; i < _frames.size(); ++i) {
		const uint32 parentId = _frameParents[i];
		_frames[i].parent = -1;
		// Frames are stored parents first; anything else is treated as a root.
		if (parentId && frameIndex.contains(parentId)) {
			const uint parent = frameIndex[parentId];
			if (parent < i)
				_frames[i].parent = parent;
		}
	}

	for (uint i = 0; i < _frames.size(); ++i) {
		const uint32 visual = _frameVisuals[i];
		if (visual && _groupById.contains(visual)) {
			const Group &group = _groups[_groupById[visual]];
			for (uint j = 0; j < group.count; ++j)
				_meshes[group.first + j].frame = i;
		}
		const uint32 lightId = _frameLights[i];
		if (lightId) {
			for (uint j = 0; j < _lights.size(); ++j) {
				if (_lightIds[j] == lightId)
					_lights[j].frame = i;
			}
		}
	}

	for (uint i = 0; i < _meshes.size(); ++i) {
		for (uint j = 0; j < _materials.size() && _pending[i].material; ++j) {
			if (_materialIds[j] == _pending[i].material) {
				_meshes[i].material = j;
				break;
			}
		}
		for (uint j = 0; j < _textures.size() && _pending[i].texture; ++j) {
			if (_textureIds[j] == _pending[i].texture) {
				_meshes[i].texture = j;
				break;
			}
		}
	}

	for (uint i = 0; i < _tracks.size(); ++i) {
		if (frameIndex.contains(_tracks[i].frameId))
			_tracks[i].frame = frameIndex[_tracks[i].frameId];
	}

	_pending.clear();
	_frameVisuals.clear();
	_frameLights.clear();
	_frameParents.clear();
	setTime(0.0f);
	return true;
}

void Model::setTime(float time) {
	// Local matrices first: the rest pose, replaced by animated frames.
	for (uint i = 0; i < _frames.size(); ++i)
		memcpy(_world[i].m, _frames[i].local, sizeof(_frames[i].local));
	for (uint i = 0; i < _tracks.size(); ++i) {
		const Track &track = _tracks[i];
		if (track.frame >= 0 && !track.keys.empty())
			evaluateTrack(track, time, _world[track.frame].m);
	}
	// world = local * parentWorld, parents first.
	for (uint i = 0; i < _frames.size(); ++i) {
		if (_frames[i].parent >= 0)
			multiplyMatrices(_world[i].m, _world[_frames[i].parent].m, _world[i].m);
	}
}

void Model::replaceTexture(const Common::String &oldName, const Common::String &newName) {
	for (uint i = 0; i < _textures.size(); ++i) {
		if (_textures[i].file.equalsIgnoreCase(oldName))
			_textures[i].file = newName;
	}
}

bool Model::isMeshVisible(const Mesh &mesh) const {
	return mesh.material < 0 || _materials[mesh.material].diffuse[3] != 0.0f;
}

bool Model::isMeshTranslucent(const Mesh &mesh) const {
	if (mesh.texture < 0 || mesh.material < 0)
		return false;
	const float alpha = _materials[mesh.material].diffuse[3];
	if (!(alpha > 0.999999f && alpha < 1.000001f))
		return true;
	const Common::String &file = _textures[mesh.texture].file;
	return file.hasSuffixIgnoreCase(".tga") || file.hasSuffixIgnoreCase(".dds");
}

ModelClips::ModelClips() {
	memset(_clips, 0, sizeof(_clips));
}

bool ModelClips::load(const byte *data, uint32 size) {
	memset(_clips, 0, sizeof(_clips));
	_modelFile.clear();
	if (!data || size != kXanSize)
		return false;
	_modelFile = readString(data + kXanModelFileOffset, kXanModelFileSize);
	for (uint index = 0; index < kClipCount; ++index) {
		const byte *record = data + kXanClipOffset + index * kXanClipSize;
		Clip &clip = _clips[index];
		clip.ticks = READ_LE_UINT32(record + 0x50);
		if (!clip.ticks)
			continue;
		clip.used = true;
		clip.loop = READ_LE_UINT32(record) == 1;
		clip.loopStart = READ_LE_UINT32(record + 0x48);
		clip.start = READ_LE_FLOAT32(record + 0x54);
		clip.end = READ_LE_FLOAT32(record + 0x58);
		clip.step = (clip.end - 0.1f - clip.start) / clip.ticks;
		clip.chain = READ_LE_UINT32(record + 0x70) != 0;
		clip.next = READ_LE_UINT32(record + 0x74);
		clip.repeat = READ_LE_UINT32(record + 0x78);
	}
	return true;
}

const ModelClips::Clip *ModelClips::getClip(uint index) const {
	return index < kClipCount && _clips[index].used ? &_clips[index] : nullptr;
}

ModelClipPlayer::ModelClipPlayer() : _clips(nullptr), _index(0), _tick(0.0f), _time(0.0f), _loops(0), _playing(false) {
}

void ModelClipPlayer::play(const ModelClips &clips, uint index) {
	_clips = &clips;
	_index = index;
	_tick = 0.0f;
	_loops = 0;
	const ModelClips::Clip *clip = clips.getClip(index);
	_playing = clip != nullptr;
	_time = clip ? clip->start : 0.0f;
}

void ModelClipPlayer::update(float seconds) {
	const ModelClips::Clip *clip = _playing ? _clips->getClip(_index) : nullptr;
	if (!clip) {
		_playing = false;
		return;
	}
	const float ticks = seconds * 60.0f;
	const float low = MIN(clip->start, clip->end), high = MAX(clip->start, clip->end);
	_time = CLIP(_time + ticks * clip->step, low, high);
	_tick += ticks;
	if (_tick < (float)clip->ticks)
		return;

	if (!clip->loop) {
		if (clip->chain && _clips->getClip(clip->next))
			play(*_clips, clip->next);
		else
			_playing = false;
		return;
	}
	++_loops;
	if (clip->chain && _loops >= clip->repeat && _clips->getClip(clip->next)) {
		play(*_clips, clip->next);
		return;
	}
	float excess = _tick - (float)clip->ticks;
	if (clip->ticks > clip->loopStart)
		excess = fmodf(excess, (float)(clip->ticks - clip->loopStart));
	else
		excess = 0.0f;
	_tick = (float)clip->loopStart + excess;
	_time = CLIP(clip->start + _tick * clip->step, low, high);
}

} // End of namespace Illusion
