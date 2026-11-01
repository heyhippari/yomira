/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <cxxtest/TestSuite.h>

#include "common/array.h"
#include "common/endian.h"
#include "engines/illusion/model.h"

class IllusionModelTestSuite : public CxxTest::TestSuite {
	typedef Common::Array<byte> Bytes;

	static void append(Bytes &bytes, const byte *data, uint size) {
		for (uint i = 0; i < size; ++i)
			bytes.push_back(data[i]);
	}

	static void putU32(Bytes &bytes, uint32 value) {
		byte data[4];
		WRITE_LE_UINT32(data, value);
		append(bytes, data, 4);
	}

	static void putFloat(Bytes &bytes, float value) {
		byte data[4];
		WRITE_LE_FLOAT32(data, value);
		append(bytes, data, 4);
	}

	static void putZeros(Bytes &bytes, uint count) {
		for (uint i = 0; i < count; ++i)
			bytes.push_back(0);
	}

	static void putString(Bytes &bytes, const char *text, uint size) {
		const uint length = strlen(text);
		for (uint i = 0; i < size; ++i)
			bytes.push_back(i < length ? text[i] : 0);
	}

	static void beginChunk(Bytes &bytes, const char *tag, uint &start) {
		for (int i = 0; i < 4; ++i)
			bytes.push_back(tag[i]);
		putU32(bytes, 0);
		start = bytes.size();
	}

	static void endChunk(Bytes &bytes, uint start) {
		WRITE_LE_UINT32(&bytes[start - 4], bytes.size() - start);
	}

	// A model with one textured triangle in a child frame that moves from x = 0 to x = 10.
	static void buildModel(Bytes &bytes, uint32 index2 = 2, bool unknownChunk = false) {
		const char header[] = { 'O', 'B', 'J', 'M', 0, 0, 0, 0, 0, 0, 0, 0 };
		append(bytes, (const byte *)header, sizeof(header));
		uint start;

		beginChunk(bytes, "MATE", start);
		putString(bytes, "mat", 0x40);
		putU32(bytes, 0x100);
		for (int i = 0; i < 4; ++i)
			putFloat(bytes, 1.0f);
		putZeros(bytes, 0x8C - 0x54);
		endChunk(bytes, start);

		beginChunk(bytes, "TEXT", start);
		putString(bytes, "zzz.bmp", 0x40);
		putU32(bytes, 0x200);
		putString(bytes, "zzz.bmp", 0x88);
		endChunk(bytes, start);

		beginChunk(bytes, "MESH", start);
		putString(bytes, "mesh", 0x40);
		putU32(bytes, 0x300);
		putU32(bytes, 1);
		Bytes info(0x14C, 0);
		WRITE_LE_UINT32(&info[0x04], 0x100);
		WRITE_LE_UINT32(&info[0x08], 0x200);
		WRITE_LE_UINT32(&info[0x40], 3);
		WRITE_LE_UINT32(&info[0x44], 3);
		WRITE_LE_UINT32(&info[0x48], 1);
		append(bytes, info.data(), info.size());
		for (int v = 0; v < 3; ++v) {
			putFloat(bytes, (float)v);
			putFloat(bytes, 0.0f);
			putFloat(bytes, 0.0f);
			putFloat(bytes, 0.0f);
			putFloat(bytes, 0.0f);
			putFloat(bytes, 0.0f);
			putFloat(bytes, 2.0f); // not normalized in the file
			putFloat(bytes, 0.5f);
			putFloat(bytes, 0.25f);
			putZeros(bytes, 24);
		}
		bytes.push_back(0);
		bytes.push_back(0);
		bytes.push_back(1);
		bytes.push_back(0);
		bytes.push_back(index2 & 0xFF);
		bytes.push_back(index2 >> 8);
		endChunk(bytes, start);

		beginChunk(bytes, "FRAM", start);
		for (int f = 0; f < 2; ++f) {
			Bytes frame(0x18C, 0);
			WRITE_LE_UINT32(&frame[0x40], 0x400 + f);
			for (int i = 0; i < 4; ++i)
				WRITE_LE_FLOAT32(&frame[0x44 + i * 20], 1.0f);
			WRITE_LE_UINT32(&frame[0xAC], f ? 0x400 : 0);
			WRITE_LE_UINT32(&frame[0xB0], f ? 0x300 : 0);
			append(bytes, frame.data(), frame.size());
		}
		endChunk(bytes, start);

		beginChunk(bytes, "ANIM", start);
		putZeros(bytes, 0x44);
		putU32(bytes, 1);
		putU32(bytes, 0x401);
		putZeros(bytes, 16);
		putU32(bytes, 2);
		for (int k = 0; k < 2; ++k) {
			Bytes key(0xDC, 0);
			WRITE_LE_FLOAT32(&key[0x00], k * 10.0f);
			WRITE_LE_UINT32(&key[0x04], 1);
			WRITE_LE_FLOAT32(&key[0x08], k * 10.0f);
			// Rotation and scale are left to the prepare step; the first key has only a scale flag.
			WRITE_LE_UINT32(&key[0x14], k == 0 ? 1 : 0);
			WRITE_LE_FLOAT32(&key[0x74 + 3 * 4], 1.0f);
			WRITE_LE_UINT32(&key[0x24], 1);
			for (int i = 0; i < 3; ++i)
				WRITE_LE_FLOAT32(&key[0x28 + i * 4], 1.0f);
			append(bytes, key.data(), key.size());
		}
		endChunk(bytes, start);

		beginChunk(bytes, "FOG ", start);
		putZeros(bytes, 32);
		endChunk(bytes, start);

		if (unknownChunk) {
			beginChunk(bytes, "ZZZZ", start);
			endChunk(bytes, start);
			// Anything after an unknown tag is ignored, even garbage.
			putZeros(bytes, 3);
		}
	}

public:
	void testParseSyntheticModel() {
		Bytes data;
		buildModel(data);
		Illusion::Model model;
		TS_ASSERT(model.load(data.data(), data.size()));
		TS_ASSERT_EQUALS(model.getFrameCount(), 2u);
		TS_ASSERT_EQUALS(model.getMeshes().size(), 1u);
		const Illusion::Model::Mesh &mesh = model.getMeshes()[0];
		TS_ASSERT_EQUALS(mesh.frame, 1);
		TS_ASSERT_EQUALS(mesh.material, 0);
		TS_ASSERT_EQUALS(mesh.texture, 0);
		TS_ASSERT_EQUALS(mesh.indices.size(), 3u);
		TS_ASSERT_EQUALS(mesh.vertices.size(), 3u * Illusion::Model::kVertexFloats);
		// The normal is normalized and the uv is kept.
		TS_ASSERT_DELTA(mesh.vertices[Illusion::Model::kVertexFloats + 5], 1.0f, 1e-6f);
		TS_ASSERT_DELTA(mesh.vertices[6], 0.5f, 1e-6f);
		TS_ASSERT(model.isMeshVisible(mesh));
		TS_ASSERT(!model.isMeshTranslucent(mesh));
		model.replaceTexture("ZZZ.BMP", "a00_00.tga");
		TS_ASSERT_EQUALS(model.getTextures()[0].file, "a00_00.tga");
		TS_ASSERT(model.isMeshTranslucent(mesh));
	}

	void testAnimationUsesPrepareAndInterpolation() {
		Bytes data;
		buildModel(data);
		Illusion::Model model;
		TS_ASSERT(model.load(data.data(), data.size()));
		// The child frame follows its animated position; the parent matrix is the identity.
		model.setTime(5.0f);
		TS_ASSERT_DELTA(model.getWorldMatrix(1)[12], 5.0f, 1e-4f);
		TS_ASSERT_DELTA(model.getWorldMatrix(1)[0], 1.0f, 1e-4f);
		model.setTime(10.0f);
		TS_ASSERT_DELTA(model.getWorldMatrix(1)[12], 10.0f, 1e-4f);
		// Past the last key the last pose is kept.
		model.setTime(50.0f);
		TS_ASSERT_DELTA(model.getWorldMatrix(1)[12], 10.0f, 1e-4f);
		TS_ASSERT_DELTA(model.getWorldMatrix(0)[12], 0.0f, 1e-4f);
	}

	void testUnknownChunkStopsParsing() {
		Bytes data;
		buildModel(data, 2, true);
		Illusion::Model model;
		TS_ASSERT(model.load(data.data(), data.size()));
		TS_ASSERT_EQUALS(model.getFrameCount(), 2u);
	}

	void testInvalidDataFailsGracefully() {
		Illusion::Model model;
		TS_ASSERT(!model.load(nullptr, 0));
		Bytes data;
		buildModel(data);

		// An index past the vertex array.
		Bytes bad = data;
		Bytes outOfRange;
		buildModel(outOfRange, 3);
		TS_ASSERT(!model.load(outOfRange.data(), outOfRange.size()));
		TS_ASSERT(!model.isLoaded());

		// Wrong magic.
		bad[3] = 'X';
		TS_ASSERT(!model.load(bad.data(), bad.size()));

		// Every truncation must be rejected or parsed without reading out of bounds.
		for (uint size = 0; size < data.size(); ++size) {
			Bytes cut;
			append(cut, data.data(), size);
			if (!model.load(cut.data(), cut.size()))
				TS_ASSERT(!model.isLoaded());
		}

		// A mesh claiming more vertices than the chunk holds.
		Bytes huge = data;
		for (uint i = 12; i + 8 < huge.size(); ++i) {
			if (memcmp(&huge[i], "MESH", 4) == 0) {
				WRITE_LE_UINT32(&huge[i + 8 + 0x48 + 0x40], 0xFFF0);
				break;
			}
		}
		TS_ASSERT(!model.load(huge.data(), huge.size()));
	}

	void testClipPlayer() {
		Bytes data(0x5190, 0);
		memcpy(&data[0x100], "e20_00.x", 8);
		const uint base = 0x200 + 0x190;
		// Clip 0: loops, 600 ticks over animation time 0..60.
		WRITE_LE_UINT32(&data[base + 0x00], 1);
		WRITE_LE_UINT32(&data[base + 0x50], 600);
		WRITE_LE_FLOAT32(&data[base + 0x54], 0.0f);
		WRITE_LE_FLOAT32(&data[base + 0x58], 60.0f);
		// Clip 1: plays once over 60 ticks, then chains to clip 0.
		WRITE_LE_UINT32(&data[base + 0x9C + 0x50], 60);
		WRITE_LE_FLOAT32(&data[base + 0x9C + 0x58], 10.1f);
		WRITE_LE_UINT32(&data[base + 0x9C + 0x70], 1);
		WRITE_LE_UINT32(&data[base + 0x9C + 0x74], 0);

		Illusion::ModelClips clips;
		TS_ASSERT(clips.load(data.data(), data.size()));
		TS_ASSERT_EQUALS(clips.getModelFile(), "e20_00.x");
		TS_ASSERT(clips.getClip(0) != nullptr);
		TS_ASSERT(clips.getClip(2) == nullptr);
		TS_ASSERT_DELTA(clips.getClip(0)->step, 59.9f / 600.0f, 1e-6f);

		Illusion::ModelClipPlayer player;
		player.play(clips, 0);
		player.update(5.0f / 60.0f * 60.0f / 60.0f); // 5 ticks
		TS_ASSERT_DELTA(player.getTime(), 59.9f / 600.0f * 5.0f, 1e-3f);
		// 600 ticks restart the loop from the beginning.
		player.update(10.0f - 5.0f / 60.0f);
		TS_ASSERT(player.isPlaying());
		TS_ASSERT_LESS_THAN(player.getTime(), 1.0f);

		player.play(clips, 1);
		player.update(1.5f);
		TS_ASSERT(player.isPlaying());
		// The chain started clip 0 again.
		TS_ASSERT_LESS_THAN(player.getTime(), 1.0f);

		Bytes shortData(100, 0);
		TS_ASSERT(!clips.load(shortData.data(), shortData.size()));
	}
};
