// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The textures, sounds, texts and fonts of the Unity serialized files as archive entries: common
// part of the asset bundles (UnityFs) and of the stand-alone serialized files of the
// players (UnityAssets). A container gives the bytes of the files (ReadAt, positions of its
// own) and finds the data streams (.resS, .resource) the objects refer to (ResolveStream).
// C++ port of drv_unity_objects.pas.
#pragma once

#include "Archive.h"
#include "codecs/UnityExport.h"
#include "codecs/UnityMesh.h"

namespace Dud
{
// An entry made from an object of a serialized file (DataX = index + 1 in _virt)
struct UfsVirt
{
    UxPlan Plan;
    int64_t Pos = 0;    // texture data / bytes, in the positions of ReadAt
    int64_t SrcLen = 0; // crunched textures: size of the crunch data; sprites: size of the data of the texture
    int MeshIdx = 0;    // meshes: index in _meshes
    int TFmt = 0, TW = 0, TH = 0, RX = 0, RY = 0; // sprites: format and size of the texture, position of the sprite in it
};

class UnityObjectArchive : public Archive
{
  protected:
    void AddVirt(const UfsVirt &v);
    virtual auto ReadAt(int64_t pos, int64_t len, void *dst) -> bool = 0;
    // The block of packed data that holds the byte pos (a number, -1 when reading it costs nothing special) and the
    // bytes that have to be unpacked to read it: reading an object in a compressed bundle unpacks its whole block
    virtual auto PackedBlock(int64_t pos, int64_t &unpackBytes) -> int64_t;
    // size bytes at off of the stream file named by path (its last component)
    virtual auto ResolveStream(const std::string &path, int64_t off, int64_t size, int64_t &pos) -> bool = 0;
    auto StreamRange(int64_t pos, int64_t len, const Sink &sink) -> int32_t;
    // The serialized file of size bytes at base: its objects become entries named prefix + Class/name
    void AddObjects(int64_t base, int64_t size, const std::string &prefix);
    auto ExtractVirtual(const UfsVirt &v, const Sink &sink) -> int32_t;

    std::vector<UfsVirt> _virt;
    std::string _engineVersion; // the Unity version of a bundle (its serialized files say 0.0.0)

  private:
    std::vector<MeshInfo> _meshes;
    int64_t _atlasPos = 0; // the last crunched texture that a sprite needed, unpacked
    std::vector<uint8_t> _atlas;
    // w x h pixels at (x, y) of the texture described by v, as BGRA lines (bottom line first like Unity)
    auto TexRegion(const UfsVirt &v, int x, int y, int w, int h, uint8_t *dst) -> bool;
};
} // namespace Dud
