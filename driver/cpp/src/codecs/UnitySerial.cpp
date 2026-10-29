// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "UnitySerial.h"

#include <cstring>

#include "UnityTrees.h"

namespace Dud
{
namespace
{
struct BadSerial
{
};

constexpr int64_t MaxMeta = 64 * 1024 * 1024;
constexpr int MaxTypes = 20000;
constexpr int MaxNodes = 100000;
constexpr int MaxObjects = 2000000;
constexpr int64_t MaxArray = 100000000;
constexpr int Budget = 500000;

constexpr const char *CommonList[110] = {
    "AABB", "AnimationClip", "AnimationCurve", "AnimationState", "Array", "Base", "BitField",
    "bitset", "bool", "char", "ColorRGBA", "Component", "data", "deque", "double",
    "dynamic_array", "FastPropertyName", "first", "float", "Font", "GameObject", "Generic Mono",
    "GradientNEW", "GUID", "GUIStyle", "int", "list", "long long", "map", "Matrix4x4f", "MdFour",
    "MonoBehaviour", "MonoScript", "m_ByteSize", "m_Curve", "m_EditorClassIdentifier",
    "m_EditorHideFlags", "m_Enabled", "m_ExtensionPtr", "m_GameObject", "m_Index", "m_IsArray",
    "m_IsStatic", "m_MetaFlag", "m_Name", "m_ObjectHideFlags", "m_PrefabInternal",
    "m_PrefabParentObject", "m_Script", "m_StaticEditorFlags", "m_Type", "m_Version", "Object",
    "pair", "PPtr<Component>", "PPtr<GameObject>", "PPtr<Material>", "PPtr<MonoBehaviour>",
    "PPtr<MonoScript>", "PPtr<Object>", "PPtr<Prefab>", "PPtr<Sprite>", "PPtr<TextAsset>",
    "PPtr<Texture>", "PPtr<Texture2D>", "PPtr<Transform>", "Prefab", "Quaternionf", "Rectf",
    "RectInt", "RectOffset", "second", "set", "short", "size", "SInt16", "SInt32", "SInt64",
    "SInt8", "staticvector", "string", "TextAsset", "TextMesh", "Texture", "Texture2D",
    "Transform", "TypelessData", "UInt16", "UInt32", "UInt64", "UInt8", "unsigned int",
    "unsigned long long", "unsigned short", "vector", "Vector2f", "Vector3f", "Vector4f",
    "m_ScriptingClassIdentifier", "Gradient", "Type*", "int2_storage", "int3_storage",
    "BoundsInt", "m_CorrespondingSourceObject", "m_PrefabInstance", "m_PrefabAsset", "FileSize",
    "Hash128", "RenderingLayerMask"};

struct Template
{
    int ClassID = 0;
    std::vector<UsNode> Nodes;
};

// 'level;type;name;size;flags,...' to the nodes
auto ParseTemplate(const std::string &text, size_t count) -> std::vector<UsNode>
{
    std::vector<UsNode> nodes(count);
    std::vector<int> levels(count);
    std::vector<size_t> stack(count + 1);
    size_t n = 0, p = 0;
    while (p < text.size())
    {
        int field = 0;
        std::string row[5];
        size_t q = p;
        while (q < text.size() && text[q] != ',')
        {
            if (text[q] == ';')
                ++field;
            else if (field <= 4)
                row[field] += text[q];
            ++q;
        }
        nodes[n].TypeName = row[1];
        nodes[n].Name = row[2];
        nodes[n].Size = std::stoi(row[3]);
        nodes[n].Flags = static_cast<uint32_t>(std::stoll(row[4]));
        nodes[n].EndIdx = static_cast<int>(n) + 1;
        levels[n] = std::stoi(row[0]);
        ++n;
        p = q + 1;
    }
    size_t sp = 0;
    for (size_t i = 0; i < n; ++i)
    {
        while (sp > 0 && levels[i] <= levels[stack[sp - 1]])
        {
            nodes[stack[sp - 1]].EndIdx = static_cast<int>(i);
            --sp;
        }
        stack[sp++] = i;
    }
    while (sp > 0)
    {
        nodes[stack[sp - 1]].EndIdx = static_cast<int>(n);
        --sp;
    }
    return nodes;
}

auto Templates() -> const std::vector<Template> &
{
    static const std::vector<Template> templates = [] {
        std::vector<Template> list(TreeCount);
        for (int i = 0; i < TreeCount; ++i)
        {
            const std::string text = TreeText(i);
            size_t count = 1;
            for (const char c : text)
                if (c == ',')
                    ++count;
            list[static_cast<size_t>(i)].ClassID = TreeClass(i);
            list[static_cast<size_t>(i)].Nodes = ParseTemplate(text, count);
        }
        return list;
    }();
    return templates;
}

auto CommonStrings() -> const std::string &
{
    static const std::string strings = [] {
        std::string s;
        for (const char *item : CommonList)
        {
            s += item;
            s += '\0';
        }
        return s;
    }();
    return strings;
}

// ---- bounded cursor on the metadata buffer ----

struct Cursor
{
    const uint8_t *B;
    int64_t Len;
    int64_t P;

    void Need(int64_t n) const
    {
        if (n < 0 || P + n > Len)
            throw BadSerial();
    }
    auto U8() -> uint8_t
    {
        Need(1);
        return B[P++];
    }
    auto U16() -> uint16_t
    {
        Need(2);
        const auto v = static_cast<uint16_t>(B[P] | (B[P + 1] << 8));
        P += 2;
        return v;
    }
    auto U32() -> uint32_t
    {
        Need(4);
        const uint32_t v = static_cast<uint32_t>(B[P]) | (static_cast<uint32_t>(B[P + 1]) << 8) |
                           (static_cast<uint32_t>(B[P + 2]) << 16) | (static_cast<uint32_t>(B[P + 3]) << 24);
        P += 4;
        return v;
    }
    auto U64() -> int64_t
    {
        const uint32_t lo = U32();
        return static_cast<int64_t>((static_cast<uint64_t>(U32()) << 32) | lo);
    }
    auto ZString(int maxLen) -> std::string
    {
        const int64_t s = P;
        while (P < Len && B[P] != 0)
        {
            ++P;
            if (P - s > maxLen)
                throw BadSerial();
        }
        if (P >= Len)
            throw BadSerial();
        std::string text(reinterpret_cast<const char *>(&B[s]), static_cast<size_t>(P - s));
        ++P;
        return text;
    }
};

auto BEAt(const uint8_t *b, int pos) -> uint32_t
{
    return (static_cast<uint32_t>(b[pos]) << 24) | (static_cast<uint32_t>(b[pos + 1]) << 16) |
           (static_cast<uint32_t>(b[pos + 2]) << 8) | b[pos + 3];
}

auto BE64At(const uint8_t *b, int pos) -> int64_t
{
    return static_cast<int64_t>((static_cast<uint64_t>(BEAt(b, pos)) << 32) | BEAt(b, pos + 4));
}

// Text at an offset of the text table of a type tree (or of the common table)
auto TextAt(const uint8_t *tab, int64_t tabLen, uint32_t offs) -> std::string
{
    if ((offs & 0x80000000) != 0)
    {
        const std::string &common = CommonStrings();
        const int64_t s = offs & 0x7FFFFFFF;
        if (s >= static_cast<int64_t>(common.size()) || (s > 0 && common[static_cast<size_t>(s) - 1] != '\0'))
            throw BadSerial();
        int64_t e = s;
        while (common[static_cast<size_t>(e)] != '\0')
            ++e;
        return common.substr(static_cast<size_t>(s), static_cast<size_t>(e - s));
    }
    const int64_t s = offs;
    if (s >= tabLen)
        throw BadSerial();
    int64_t e = s;
    while (e < tabLen && tab[e] != 0)
        ++e;
    if (e >= tabLen)
        throw BadSerial();
    return std::string(reinterpret_cast<const char *>(&tab[s]), static_cast<size_t>(e - s));
}

auto Le32At(const uint8_t *b) -> uint32_t
{
    return static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) | (static_cast<uint32_t>(b[2]) << 16) |
           (static_cast<uint32_t>(b[3]) << 24);
}
} // namespace

UnitySerialized::UnitySerialized(ReadFunc read, int64_t base, int64_t size)
    : _read(std::move(read)), _base(base), _size(size)
{
}

auto UnitySerialized::Load() -> bool
{
    try
    {
        if (_size < 48)
            return false;
        uint8_t hdr[48];
        if (!_read(_base, 48, hdr))
            return false;
        Version = static_cast<int>(BEAt(hdr, 8));
        if (Version < 17 || Version > 22)
            return false;
        int64_t fileSize = BEAt(hdr, 4);
        DataOffset = BEAt(hdr, 12);
        int headerEnd = 20;
        if (hdr[16] != 0)
            return false; // big endian data
        if (Version >= 22)
        {
            fileSize = BE64At(hdr, 24);
            DataOffset = BE64At(hdr, 32);
            headerEnd = 48;
        }
        if (DataOffset <= headerEnd || DataOffset > _size || DataOffset > MaxMeta || fileSize > _size + 16)
            return false;
        std::vector<uint8_t> buf(static_cast<size_t>(DataOffset) + 8);
        if (!_read(_base, DataOffset, buf.data()))
            return false;
        Cursor c{buf.data(), DataOffset, headerEnd};
        UnityVersion = c.ZString(64);
        c.U32(); // target platform
        const bool hasTree = c.U8() != 0;
        const int numTypes = static_cast<int>(c.U32());
        if (numTypes < 0 || numTypes > MaxTypes)
            return false;
        Types.assign(static_cast<size_t>(numTypes), UsType());
        const int nodeSize = Version >= 19 ? 32 : 24;
        for (int i = 0; i < numTypes; ++i)
        {
            UsType &type = Types[static_cast<size_t>(i)];
            type.ClassID = static_cast<int>(c.U32());
            c.U8();  // stripped
            c.U16(); // script type index
            if (type.ClassID == 114)
                c.Need(16);
            if (type.ClassID == 114)
                c.P += 16;
            c.Need(16);
            c.P += 16;
            if (!hasTree)
            {
                type.Synth = true;
                continue;
            }
            const int numNodes = static_cast<int>(c.U32());
            const int strLen = static_cast<int>(c.U32());
            if (numNodes < 0 || numNodes > MaxNodes || strLen < 0 || strLen > MaxMeta)
                throw BadSerial();
            c.Need(static_cast<int64_t>(numNodes) * nodeSize + strLen);
            const uint8_t *nodeBuf = &c.B[c.P];
            const uint8_t *tab = &c.B[c.P + static_cast<int64_t>(numNodes) * nodeSize];
            type.Nodes.assign(static_cast<size_t>(numNodes), UsNode());
            for (int j = 0; j < numNodes; ++j)
            {
                UsNode &node = type.Nodes[static_cast<size_t>(j)];
                const uint8_t *row = nodeBuf + static_cast<int64_t>(j) * nodeSize;
                node.TypeName = TextAt(tab, strLen, Le32At(row + 4));
                node.Name = TextAt(tab, strLen, Le32At(row + 8));
                node.Size = static_cast<int>(Le32At(row + 12));
                node.Flags = Le32At(row + 20);
                node.EndIdx = j + 1;
            }
            // the level of the nodes only matters to find the end of each subtree
            std::vector<int> stack(static_cast<size_t>(numNodes) + 1);
            int sp = 0;
            for (int j = 0; j < numNodes; ++j)
            {
                const int k = nodeBuf[static_cast<int64_t>(j) * nodeSize + 2]; // level
                while (sp > 0 && k <= nodeBuf[static_cast<int64_t>(stack[static_cast<size_t>(sp) - 1]) * nodeSize + 2])
                {
                    type.Nodes[static_cast<size_t>(stack[static_cast<size_t>(sp) - 1])].EndIdx = j;
                    --sp;
                }
                stack[static_cast<size_t>(sp++)] = j;
            }
            while (sp > 0)
            {
                type.Nodes[static_cast<size_t>(stack[static_cast<size_t>(sp) - 1])].EndIdx = numNodes;
                --sp;
            }
            c.P += static_cast<int64_t>(numNodes) * nodeSize + strLen;
            if (Version >= 21)
            {
                const int numDeps = static_cast<int>(c.U32());
                if (numDeps < 0 || numDeps > MaxNodes)
                    throw BadSerial();
                c.Need(static_cast<int64_t>(numDeps) * 4);
                c.P += static_cast<int64_t>(numDeps) * 4;
            }
            if (ForceTemplates)
            {
                type.Synth = true;
                type.Nodes.clear();
            }
        }
        const int numObjs = static_cast<int>(c.U32());
        if (numObjs < 0 || numObjs > MaxObjects)
            return false;
        Objects.assign(static_cast<size_t>(numObjs), UsObject());
        for (int i = 0; i < numObjs; ++i)
        {
            UsObject &o = Objects[static_cast<size_t>(i)];
            c.P = (c.P + 3) & ~static_cast<int64_t>(3);
            o.PathID = c.U64();
            if (Version >= 22)
                o.Start = c.U64();
            else
                o.Start = c.U32();
            o.Size = c.U32();
            o.TypeIdx = static_cast<int>(c.U32());
            if (o.TypeIdx < 0 || o.TypeIdx >= numTypes)
                throw BadSerial();
            o.ClassID = Types[static_cast<size_t>(o.TypeIdx)].ClassID;
            o.Start += DataOffset;
            if (o.Start < 0 || o.Start + o.Size > _size)
                throw BadSerial();
        }
        return true;
    }
    catch (...)
    {
        Types.clear();
        Objects.clear();
        return false;
    }
}

void UnitySerialized::Need(int64_t pos, int64_t len) const
{
    if (len < 0 || pos < _objStart || pos + len > _objEnd)
        throw BadSerial();
}

void UnitySerialized::AddField(UsFields &f, const std::string &path, int64_t value, int64_t pos, int64_t len,
                               bool blob)
{
    if (f.size() >= 200000)
        return;
    std::string s;
    if (blob && len > 0 && len <= 1024)
    {
        s.resize(static_cast<size_t>(len));
        if (!_read(_base + pos, len, s.data()))
            throw BadSerial();
    }
    UsField field;
    field.Path = path;
    field.Int = value;
    field.Pos = pos;
    field.Len = len;
    field.Blob = blob;
    field.Str = std::move(s);
    f.push_back(std::move(field));
}

void UnitySerialized::Walk(const std::vector<UsNode> &nodes, int i, int64_t &pos, const std::string &path, bool rec,
                           int depth, UsFields &f)
{
    --_budget;
    if (_budget < 0 || depth > 24)
        throw BadSerial();
    const UsNode n = nodes[static_cast<size_t>(i)];
    const auto at = [&](int index) -> const UsNode & { return nodes[static_cast<size_t>(index)]; };
    uint8_t buf[8];
    if ((n.TypeName == "Array" || n.TypeName == "TypelessData") && n.EndIdx > i + 2 && at(i + 1).EndIdx == i + 2)
    {
        // size then the elements
        Need(pos, 4);
        if (!_read(_base + pos, 4, buf))
            throw BadSerial();
        const int64_t len = static_cast<int32_t>(Le32At(buf));
        pos += 4;
        if (len < 0 || len > MaxArray)
            throw BadSerial();
        const UsNode d = at(i + 2);
        if (at(i + 2).EndIdx == i + 3 && d.Size > 0)
        {
            const int64_t bytes = len * d.Size;
            Need(pos, bytes);
            if (rec && d.Size == 1)
            {
                std::string recPath = path;
                if (n.TypeName == "Array" && recPath.size() > 6 && recPath.compare(recPath.size() - 6, 6, ".Array") == 0)
                    recPath.resize(recPath.size() - 6);
                AddField(f, recPath, 0, pos, bytes, true);
            }
            pos += bytes;
        }
        else
        {
            // the elements of an array of structures: only the submeshes and vertex channels of a mesh are kept (the
            // others, such as the vertices of the physics shape of a sprite, would only make the search of a field
            // slow): first 64 as Name[0].field
            std::string recPath = path;
            if (recPath.size() > 6 && recPath.compare(recPath.size() - 6, 6, ".Array") == 0)
                recPath.resize(recPath.size() - 6);
            const bool keep = recPath == "m_SubMeshes" || recPath == "m_VertexData.m_Channels";
            bool fixed = d.Size > 0 && !(rec && keep);
            if (fixed)
                for (int k = i + 2; k <= d.EndIdx - 1; ++k)
                    if ((at(k).Flags & 0x4000) != 0)
                        fixed = false;
            if (fixed)
            {
                // a structure of a known size that nobody asked for (the points of a physics shape...): skipped as a block
                const int64_t bytes = len * d.Size;
                Need(pos, bytes);
                pos += bytes;
            }
            else
                for (int64_t k = 1; k <= len; ++k)
                    Walk(nodes, i + 2, pos, recPath + "[" + std::to_string(k - 1) + "]", rec && k <= 64 && keep,
                         depth + 1, f);
        }
    }
    else if (n.EndIdx == i + 1)
    {
        // a primitive value
        if (!(n.Size == 1 || n.Size == 2 || n.Size == 4 || n.Size == 8))
            throw BadSerial();
        Need(pos, n.Size);
        std::memset(buf, 0, sizeof buf);
        if (!_read(_base + pos, n.Size, buf))
            throw BadSerial();
        int64_t v = 0;
        for (int j = n.Size - 1; j >= 0; --j)
            v = static_cast<int64_t>((static_cast<uint64_t>(v) << 8) | buf[j]);
        pos += n.Size;
        if (rec)
            AddField(f, path, v, 0, 0, false);
    }
    else
    {
        int j = i + 1;
        while (j < n.EndIdx)
        {
            const std::string sub = path.empty() ? at(j).Name : path + "." + at(j).Name;
            Walk(nodes, j, pos, sub, rec, depth + 1, f);
            j = at(j).EndIdx;
        }
    }
    if ((n.Flags & 0x4000) != 0)
        pos = _objStart + (((pos - _objStart) + 3) & ~static_cast<int64_t>(3));
}

// A built-in layout that reads exactly the object can still be the wrong one (the fields shift):
// the values it gives must make sense
auto UnitySerialized::Plausible(int classId, const UsFields &f) const -> bool
{
    switch (classId)
    {
    case 28: {
        const int64_t w = IntOf(f, "m_Width", -1);
        const int64_t h = IntOf(f, "m_Height", -1);
        const int64_t fmt = IntOf(f, "m_TextureFormat", -1);
        const int64_t cis = IntOf(f, "m_CompleteImageSize", -1);
        const int64_t sz = IntOf(f, "m_StreamData.size", -1);
        int64_t im = 0;
        const int i = Find(f, "image data");
        if (i >= 0)
            im = f[static_cast<size_t>(i)].Len;
        return w >= 1 && w <= 32768 && h >= 1 && h <= 32768 && fmt >= 1 && fmt <= 64 && sz >= 0 && cis == im + sz;
    }
    case 89: {
        const int64_t w = IntOf(f, "m_Width", -1);
        const int64_t h = IntOf(f, "m_Height", -1);
        const int64_t fmt = IntOf(f, "m_TextureFormat", -1);
        return w >= 1 && w <= 16384 && h >= 1 && h <= 16384 && fmt >= 1 && fmt <= 64 &&
               IntOf(f, "m_CompleteImageSize", -1) > 0;
    }
    case 213:
        // finite rectangle of a sensible size and scale
        return FloatOf(f, "m_Rect.width", -1) >= 0 && FloatOf(f, "m_Rect.width", -1) < 1000000 &&
               FloatOf(f, "m_Rect.height", -1) >= 0 && FloatOf(f, "m_Rect.height", -1) < 1000000 &&
               FloatOf(f, "m_PixelsToUnits", -1) > 0 && FloatOf(f, "m_PixelsToUnits", -1) < 1000000 &&
               IntOf(f, "m_RD.texture.m_FileID", -1) >= -1000 && IntOf(f, "m_RD.texture.m_FileID", -1) < 1000;
    case 43:
        return IntOf(f, "m_MeshCompression", -1) >= 0 && IntOf(f, "m_MeshCompression", -1) <= 4 &&
               IntOf(f, "m_IndexFormat", 0) >= 0 && IntOf(f, "m_IndexFormat", 0) <= 1 &&
               IntOf(f, "m_VertexData.m_VertexCount", -1) >= 0 &&
               IntOf(f, "m_VertexData.m_VertexCount", -1) <= 50000000;
    case 83: {
        const int64_t sz = IntOf(f, "m_Resource.m_Size", -1);
        return sz >= 0 && IntOf(f, "m_Channels", -1) >= 0 && IntOf(f, "m_Channels", -1) <= 64;
    }
    default:
        return true;
    }
}

auto UnitySerialized::ReadObject(const UsObject &o, UsFields &f) -> bool
{
    f.clear();
    try
    {
        UsType &type = Types[static_cast<size_t>(o.TypeIdx)];
        if (type.Nodes.empty() && !type.Synth)
            return false;
        _objStart = o.Start;
        _objEnd = o.Start + o.Size;
        _budget = Budget;
        int64_t pos = o.Start;
        if (type.Synth)
        {
            // the layout of the Unity version is unknown: the right one reads exactly the whole object
            // the layout that worked for the previous object of this type is tried first
            const std::vector<Template> &templates = Templates();
            for (int j = -1; j < static_cast<int>(templates.size()); ++j)
            {
                int k;
                if (j == -1)
                {
                    k = type.Chosen - 1;
                    if (k < 0)
                        continue;
                }
                else
                {
                    k = j;
                    if (k == type.Chosen - 1)
                        continue;
                }
                if (templates[static_cast<size_t>(k)].ClassID == o.ClassID)
                {
                    f.clear();
                    pos = o.Start;
                    _budget = Budget;
                    try
                    {
                        Walk(templates[static_cast<size_t>(k)].Nodes, 0, pos, "", true, 0, f);
                    }
                    catch (const BadSerial &)
                    {
                        continue;
                    }
                    if (pos == _objEnd && Plausible(o.ClassID, f))
                    {
                        type.Chosen = k + 1;
                        return true;
                    }
                }
            }
            f.clear();
            return false;
        }
        Walk(type.Nodes, 0, pos, "", true, 0, f);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

auto UnitySerialized::Find(const UsFields &f, const std::string &path) -> int
{
    for (size_t i = 0; i < f.size(); ++i)
        if (f[i].Path == path)
            return static_cast<int>(i);
    return -1;
}

auto UnitySerialized::IntOf(const UsFields &f, const std::string &path, int64_t def) -> int64_t
{
    const int i = Find(f, path);
    if (i >= 0 && !f[static_cast<size_t>(i)].Blob)
        return f[static_cast<size_t>(i)].Int;
    return def;
}

auto UnitySerialized::StrOf(const UsFields &f, const std::string &path) -> std::string
{
    const int i = Find(f, path);
    if (i >= 0 && f[static_cast<size_t>(i)].Blob)
        return f[static_cast<size_t>(i)].Str;
    return "";
}

auto UnitySerialized::FloatOf(const UsFields &f, const std::string &path, float def) -> float
{
    const int i = Find(f, path);
    if (i < 0 || f[static_cast<size_t>(i)].Blob)
        return def;
    const auto bits = static_cast<uint32_t>(f[static_cast<size_t>(i)].Int & 0xFFFFFFFF);
    float result;
    std::memcpy(&result, &bits, 4);
    if (((bits >> 23) & 0xFF) == 0xFF)
        result = def; // not a finite number
    return result;
}

auto UnitySerialized::UnityMajor() const -> int
{
    int result = 0;
    for (const char c : UnityVersion)
    {
        if (c < '0' || c > '9')
            break;
        result = result * 10 + (c - '0');
    }
    return result;
}

auto UnitySerialized::AbsPos(const UsField &field) const -> int64_t
{
    return _base + field.Pos;
}
} // namespace Dud
