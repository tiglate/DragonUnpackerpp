// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Unity serialized files (the "CAB-*" files of the asset bundles, "*.assets"): the list of the
// objects they hold and a reader of the fields of an object driven by its type tree. C++ port of
// drv_unity_serial.pas. Not in Dragon UnPACKer 5: written from the layout of the format (versions 17 to
// 22, Unity 2017.x to Unity 6, little endian data; files without type trees, as the player data of
// release builds, are read with the built-in layouts of UnityTrees).
// Header (big endian): size of the metadata, size of the file, version, offset of the data; from
// version 9 the endianness (1 byte + 3 reserved); from version 22 the sizes are repeated
// with 64 bits (metadata size 32 bits, file size 64, data offset 64, one more 64 bits value).
// Metadata (little endian): Unity version (text), target platform (32 bits), type tree flag,
// number of types and for each: class id (32), stripped flag (8), script type index (16), the
// script hash (16 bytes, class 114 only), the type hash (16 bytes) and, when there are type
// trees, a flat list of nodes (16 bits version, level, flags, offset of the type text, offset
// of the name text, size, index, meta flags and from version 19 a 64 bits hash) followed by
// the texts (an offset with the high bit set is in a table of common texts); from version 21
// the dependencies of the type (count + 32 bits values). Then the number of objects and for
// each (aligned on 4 bytes): path id (64), start in the data (32, 64 from version 22), size
// (32), index of the type (32).
// Reading an object follows its type tree: 'Array' nodes hold a size and the elements, strings
// are arrays of characters, nodes with the flag 0x4000 align the position on 4 bytes
// (relative to the start of the object).
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Dud
{
struct UsNode
{
    std::string TypeName;
    std::string Name;
    int Size = 0;
    uint32_t Flags = 0;
    int EndIdx = 0; // index of the node following the subtree of this node
};

struct UsType
{
    int ClassID = 0;
    std::vector<UsNode> Nodes;
    bool Synth = false; // no type tree in the file: ReadObject tries the built-in layouts
    int Chosen = 0;     // the layout that worked last, plus 1 (0: none yet)
};

struct UsObject
{
    int64_t PathID = 0;
    int64_t Start = 0; // position in the serialized file
    int64_t Size = 0;
    int TypeIdx = 0;
    int ClassID = 0;
};

// A field read from an object: a number, or a block of bytes (string, array of bytes)
struct UsField
{
    std::string Path; // ex: m_StreamData.path
    int64_t Int = 0;
    int64_t Pos = 0; // blobs: position in the serialized file
    int64_t Len = 0;
    bool Blob = false;
    std::string Str; // blobs of at most 1024 bytes
};

using UsFields = std::vector<UsField>;

class UnitySerialized
{
  public:
    // Read len bytes at the position pos of the (unpacked) data
    using ReadFunc = std::function<bool(int64_t pos, int64_t len, void *dst)>;

    // The serialized file is size bytes at the position base of what read gives access to
    UnitySerialized(ReadFunc read, int64_t base, int64_t size);

    // Read the types and the objects: false when the file is not supported (not a serialized
    // file, big endian...), never throws
    auto Load() -> bool;
    // Read the fields of an object (only the ones the callers need are kept)
    auto ReadObject(const UsObject &o, UsFields &fields) -> bool;
    static auto Find(const UsFields &f, const std::string &path) -> int;
    static auto IntOf(const UsFields &f, const std::string &path, int64_t def = 0) -> int64_t;
    static auto StrOf(const UsFields &f, const std::string &path) -> std::string;
    // A 32 bits float field (its bits are the value read as an integer)
    static auto FloatOf(const UsFields &f, const std::string &path, float def = 0) -> float;
    // The major number of the Unity version (0 when unknown)
    auto UnityMajor() const -> int;
    // Position in the unpacked data of the bytes of a blob field
    auto AbsPos(const UsField &field) const -> int64_t;

    int Version = 0;
    int64_t DataOffset = 0;
    std::string UnityVersion; // ex: 2019.3.10f1
    std::vector<UsType> Types;
    std::vector<UsObject> Objects;
    // Test hook: use the built-in layouts even when the file has type trees
    bool ForceTemplates = false;

  private:
    auto Plausible(int classId, const UsFields &f) const -> bool;
    void Need(int64_t pos, int64_t len) const;
    void Walk(const std::vector<UsNode> &nodes, int i, int64_t &pos, const std::string &path, bool rec, int depth,
              UsFields &f);
    void AddField(UsFields &f, const std::string &path, int64_t value, int64_t pos, int64_t len, bool blob);

    ReadFunc _read;
    int64_t _base;
    int64_t _size;
    int64_t _objStart = 0;
    int64_t _objEnd = 0;
    int _budget = 0;
};
} // namespace Dud
