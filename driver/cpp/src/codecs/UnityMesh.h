// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Unity Mesh objects as Wavefront OBJ files (positions, normals and the first UV set, one group per
// triangle submesh). C++ port of drv_unity_mesh.pas. The size of the file is known when the archive is opened
// without writing it: the numbers are printed in fields of a fixed width (13 characters, 6 decimals, values
// clamped to +-99999.999999) and the indexes of the faces are zero padded to the digits of the number of
// vertices. Unity is left handed: x is negated and the triangles are written in the opposite order, so the OBJ
// looks as in Unity.
// The vertex data is the one of Unity 2018 and later: channels (stream, offset, format, dimension) in 4 streams
// laid one after the other (each starting on a 16 bytes boundary), index buffer of 16 or 32 bits and submeshes
// (first byte, number of indexes, topology, base vertex). Not done: compressed meshes, skinning, blend shapes.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Dud
{
struct MeshChannel
{
    int Stream = 0, Offset = 0, Format = 0, Dim = 0; // Dim = 0: the mesh has no such channel
};

struct MeshSub
{
    int64_t FirstByte = 0, IndexCount = 0, Topology = 0, BaseVertex = 0;
};

struct MeshInfo
{
    std::string Name;
    int VertexCount = 0;
    MeshChannel Pos, Nrm, Uv;
    int64_t StreamPos[4] = {}; // offset of each stream in the vertex data
    int Stride[4] = {};
    int64_t VertPos = 0, VertLen = 0; // the vertex data (position in the data the archive reads)
    int64_t IdxPos = 0, IdxLen = 0;
    bool Idx32 = false;
    std::vector<MeshSub> Subs;
    int Width = 0; // digits of the indexes of the faces
};

using MeshRead = std::function<bool(int64_t pos, int64_t len, void *dst)>;

// Size in bytes of a component of a vertex format (the numbering of Unity 2019: 0 float, 1 half, 2 unorm8,
// 3 snorm8, 4 unorm16, 5 snorm16, 6 uint8, 7 sint8, 8 uint16, 9 sint16, 10 uint32, 11 sint32); 0: unknown
auto MeshFormatSize(int fmt) -> int;
// The format numbers of Unity 2017/2018 as the ones above
auto MeshOldFormat(int fmt) -> int;
// Lays out the streams (needs the channels to be set) and checks that everything fits; false: not exportable
auto MeshPrepare(MeshInfo &m, const std::vector<MeshChannel> &channels) -> bool;
// Size of the OBJ file
auto MeshObjSize(const MeshInfo &m) -> int64_t;
// Makes the OBJ file (MeshObjSize bytes)
auto MeshWriteObj(const MeshInfo &m, const MeshRead &read, std::string &out) -> bool;
} // namespace Dud
