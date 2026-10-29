// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "UnityObjects.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <unordered_map>

#include "PathUtil.h"
#include "codecs/Crunch.h"
#include "codecs/TexDecode.h"
#include "codecs/UnitySerial.h"

namespace Dud
{
namespace
{
constexpr size_t MaxObjectEntries = 500000;
constexpr int64_t MaxUnpack = 256 * 1024 * 1024; // bytes of packed data that the second pass may unpack to list the objects
constexpr int64_t Chunk = 1024 * 1024;

// Shell sort of the indexes ix by the keys they point to (not stable: the order of equal keys matters)
void SortIdx(std::vector<int> &ix, const std::vector<int64_t> &keys)
{
    int gap = static_cast<int>(ix.size()) / 2;
    const int high = static_cast<int>(ix.size()) - 1;
    while (gap > 0)
    {
        for (int i = gap; i <= high; ++i)
        {
            const int t = ix[static_cast<size_t>(i)];
            int j = i;
            while (j >= gap && keys[static_cast<size_t>(ix[static_cast<size_t>(j - gap)])] > keys[static_cast<size_t>(t)])
            {
                ix[static_cast<size_t>(j)] = ix[static_cast<size_t>(j - gap)];
                j -= gap;
            }
            ix[static_cast<size_t>(j)] = t;
        }
        gap /= 2;
    }
}

auto Sanitize(const std::string &t) -> std::string
{
    std::string result = t;
    for (char &c : result)
        if (c == '/' || c == '\\' || c == ':' || static_cast<unsigned char>(c) < ' ')
            c = '_';
    // Trim: characters up to the space on both ends
    size_t first = 0, last = result.size();
    while (first < last && static_cast<unsigned char>(result[first]) <= ' ')
        ++first;
    while (last > first && static_cast<unsigned char>(result[last - 1]) <= ' ')
        --last;
    result = result.substr(first, last - first);
    if (result.empty())
        result = "unnamed";
    return result;
}

auto Round(float f) -> int
{
    return static_cast<int>(std::nearbyint(f));
}

struct TexInfo
{
    int Fmt = 0, W = 0, H = 0;
    int64_t Pos = 0, Len = 0;
    bool Ok = false;
};

struct Pending
{
    std::string Fo, Nme;
    UfsVirt V;
    int64_t PathID = 0;
    int ObjIdx = 0, Seq = 0;
};
} // namespace

auto UnityObjectArchive::StreamRange(int64_t pos, int64_t len, const Sink &sink) -> int32_t
{
    if (len <= 0)
        return DUDI7_OK;
    std::vector<uint8_t> buf(static_cast<size_t>(std::min(len, Chunk)));
    while (len > 0)
    {
        const int64_t n = std::min(len, Chunk);
        if (!ReadAt(pos, n, buf.data()))
        {
            LastError = "Cannot unpack the data at " + std::to_string(pos);
            return DUDI7_ERR_IO;
        }
        const int32_t result = ExtractBytes(buf.data(), n, sink);
        if (result != DUDI7_OK)
            return result;
        pos += n;
        len -= n;
    }
    return DUDI7_OK;
}

auto UnityObjectArchive::PackedBlock(int64_t, int64_t &unpackBytes) -> int64_t
{
    unpackBytes = 0;
    return -1;
}

void UnityObjectArchive::AddVirt(const UfsVirt &v)
{
    _virt.push_back(v);
}

void UnityObjectArchive::AddObjects(int64_t base, int64_t size, const std::string &prefix)
{
    static const char *const folders[4] = {"Texture2D", "TextAsset", "AudioClip", "Font"};
    static const char *const faceNames[6] = {"px", "nx", "py", "ny", "pz", "nz"};
    UnitySerialized s([this](int64_t pos, int64_t len, void *dst) { return ReadAt(pos, len, dst); }, base, size);
    std::set<std::string> used; // (case insensitive: the names are in lower case)
    std::unordered_map<std::string, int> texCache; // path id of a texture -> index in texInfos (-1: unusable)
    std::vector<TexInfo> texInfos;
    std::unordered_map<int64_t, int> idTab;
    bool idTabBuilt = false;
    std::vector<Pending> pend;
    {
        const char *templates = std::getenv("DUDI7_UNITY_TEMPLATES");
        s.ForceTemplates = templates != nullptr && std::string(templates) == "1"; // test hook
    }

    const auto textureData = [&](const UsFields &fl, int64_t &p, int64_t &l) -> bool {
        if (s.IntOf(fl, "m_StreamData.size") > 0 &&
            ResolveStream(s.StrOf(fl, "m_StreamData.path"), s.IntOf(fl, "m_StreamData.offset"),
                          s.IntOf(fl, "m_StreamData.size"), p))
        {
            l = s.IntOf(fl, "m_StreamData.size");
            return true;
        }
        const int fd = s.Find(fl, "image data");
        if (fd >= 0 && fl[static_cast<size_t>(fd)].Blob && fl[static_cast<size_t>(fd)].Len > 0)
        {
            p = s.AbsPos(fl[static_cast<size_t>(fd)]);
            l = fl[static_cast<size_t>(fd)].Len;
            return true;
        }
        return false;
    };

    // index of the object with this path id (-1: not in this file)
    const auto findObject = [&](int64_t pathId) -> int {
        if (!idTabBuilt)
        {
            idTabBuilt = true;
            for (size_t k = 0; k < s.Objects.size(); ++k)
                idTab.try_emplace(s.Objects[k].PathID, static_cast<int>(k));
        }
        const auto it = idTab.find(pathId);
        return it == idTab.end() ? -1 : it->second;
    };

    // can the pixels of this texture format be read for a sprite?
    const auto spriteFormatOk = [&](int fm, int wd, int ht, int64_t l, int64_t p) -> bool {
        int bw, bh, bb;
        if (SourceBytesPerPixel(fm) > 0)
            return static_cast<int64_t>(wd) * ht * SourceBytesPerPixel(fm) <= l;
        if (DecodableFormatInfo(fm, bw, bh, bb))
            return static_cast<int64_t>((wd + bw - 1) / bw) * ((ht + bh - 1) / bh) * bb <= l;
        if (fm == 28 || fm == 29)
        {
            std::string hd(256, '\0');
            if (l < 256)
                hd.resize(static_cast<size_t>(l));
            if (hd.empty() || !ReadAt(p, static_cast<int64_t>(hd.size()), hd.data()))
                return false;
            CrunchHeaderInfo ci;
            return hd.size() >= 74 &&
                   CrunchInfo(reinterpret_cast<const uint8_t *>(hd.data()), static_cast<int64_t>(hd.size()), l, ci) &&
                   ci.Width == wd && ci.Height == ht && ((fm == 28 && ci.Format == 0) || (fm == 29 && ci.Format != 0));
        }
        return false;
    };

    // the fields of a Mesh object (Unity 2018 and later, not compressed) as what MeshWriteObj needs
    const auto meshFromObject = [&](const UsFields &fl, MeshInfo &m) -> bool {
        m = MeshInfo();
        int ver = s.UnityMajor();
        if (ver == 0) // (the bundles may hide the version: the format of the file tells)
            ver = s.Version >= 19 ? 2019 : 2018;
        if (ver < 2018 || s.IntOf(fl, "m_MeshCompression") != 0)
            return false;
        m.VertexCount = static_cast<int>(s.IntOf(fl, "m_VertexData.m_VertexCount"));
        if (m.VertexCount < 1 || m.VertexCount > 2000000)
            return false;
        int fd = s.Find(fl, "m_VertexData.m_DataSize");
        if (fd < 0 || !fl[static_cast<size_t>(fd)].Blob || fl[static_cast<size_t>(fd)].Len == 0)
            return false;
        m.VertPos = s.AbsPos(fl[static_cast<size_t>(fd)]);
        m.VertLen = fl[static_cast<size_t>(fd)].Len;
        fd = s.Find(fl, "m_IndexBuffer");
        if (fd < 0 || !fl[static_cast<size_t>(fd)].Blob || fl[static_cast<size_t>(fd)].Len == 0)
            return false;
        m.IdxPos = s.AbsPos(fl[static_cast<size_t>(fd)]);
        m.IdxLen = fl[static_cast<size_t>(fd)].Len;
        if (s.Find(fl, "m_IndexFormat") < 0)
            return false;
        m.Idx32 = s.IntOf(fl, "m_IndexFormat") != 0;
        std::vector<MeshChannel> ch(14);
        for (int k = 0; k <= 13; ++k)
        {
            const std::string b = "m_VertexData.m_Channels[" + std::to_string(k) + "].";
            if (s.Find(fl, b + "dimension") < 0)
                break;
            MeshChannel &c = ch[static_cast<size_t>(k)];
            c.Stream = static_cast<int>(s.IntOf(fl, b + "stream"));
            c.Offset = static_cast<int>(s.IntOf(fl, b + "offset"));
            c.Format = static_cast<int>(s.IntOf(fl, b + "format"));
            c.Dim = static_cast<int>(s.IntOf(fl, b + "dimension") & 0xF);
            if (ver < 2019)
                c.Format = MeshOldFormat(c.Format);
        }
        if (s.Find(fl, "m_VertexData.m_Channels[13].dimension") < 0)
            return false; // not the 14 channels of Unity 2018 and later
        m.Pos = ch[0];
        m.Nrm = ch[1];
        m.Uv = ch[4];
        for (int k = 0; k <= 63; ++k)
        {
            const std::string b = "m_SubMeshes[" + std::to_string(k) + "].";
            if (s.Find(fl, b + "indexCount") < 0)
                break;
            MeshSub sub;
            sub.FirstByte = s.IntOf(fl, b + "firstByte");
            sub.IndexCount = s.IntOf(fl, b + "indexCount");
            sub.Topology = s.IntOf(fl, b + "topology");
            sub.BaseVertex = s.IntOf(fl, b + "baseVertex");
            if (sub.FirstByte < 0 || sub.IndexCount < 0 ||
                sub.FirstByte + sub.IndexCount * (2 + 2 * (m.Idx32 ? 1 : 0)) > m.IdxLen)
                return false;
            m.Subs.push_back(sub);
        }
        if (s.Find(fl, "m_SubMeshes[64].indexCount") >= 0)
            return false;
        m.Name = Sanitize(s.StrOf(fl, "m_Name"));
        return MeshPrepare(m, ch);
    };

    // one more entry: Folder/Name+extension of the plan, unique
    const auto emit = [&](const std::string &fo, const std::string &nme, const UfsVirt &vi, int64_t pathId) {
        std::string nn = nme;
        if (used.count(ToLower(fo + "/" + nn + vi.Plan.Ext)) > 0)
            nn += "_" + std::to_string(pathId);
        used.insert(ToLower(fo + "/" + nn + vi.Plan.Ext));
        AddVirt(vi);
        Add(prefix + fo + "/" + nn + vi.Plan.Ext, vi.Pos, vi.Plan.OutSize, static_cast<int64_t>(_virt.size()));
    };

    // the entries of the second pass wait here until they can be added in the order of the table
    const auto queue = [&](const std::string &fo, const std::string &nme, const UfsVirt &vi, int64_t pathId,
                           int objIdx) {
        Pending p;
        p.Fo = fo;
        p.Nme = nme;
        p.V = vi;
        p.PathID = pathId;
        p.ObjIdx = objIdx;
        p.Seq = static_cast<int>(pend.size());
        pend.push_back(std::move(p));
    };

    const auto flushPending = [&] {
        std::vector<int> ix(pend.size());
        std::vector<int64_t> keys(pend.size());
        for (size_t k = 0; k < pend.size(); ++k)
        {
            ix[k] = static_cast<int>(k);
            keys[k] = static_cast<int64_t>(pend[k].ObjIdx) * 1000000000 + pend[k].Seq; // (the sequence keeps the faces of a cubemap in order)
        }
        SortIdx(ix, keys);
        for (size_t k = 0; k < pend.size(); ++k)
        {
            const Pending &p = pend[static_cast<size_t>(ix[k])];
            emit(p.Fo, p.Nme, p.V, p.PathID);
        }
        pend.clear();
    };

    const auto peek = [&](int64_t pos, int64_t len) -> std::string {
        if (len > 256)
            len = 256;
        if (len <= 0)
            return "";
        std::string result(static_cast<size_t>(len), '\0');
        if (!ReadAt(pos, len, result.data()))
            return "";
        return result;
    };

    try
    {
        if (!s.Load())
            return;
        if (s.UnityMajor() == 0 && !_engineVersion.empty())
            s.UnityVersion = _engineVersion;
        UsFields f;
        for (size_t o = 0; o < s.Objects.size(); ++o)
        {
            if (_entries.size() >= MaxObjectEntries)
                break;
            const UsObject &obj = s.Objects[o];
            std::string folder;
            switch (obj.ClassID)
            {
            case 28:
                folder = folders[0];
                break;
            case 49:
                folder = folders[1];
                break;
            case 83:
                folder = folders[2];
                break;
            case 128:
                folder = folders[3];
                break;
            default:
                continue;
            }
            if (!s.ReadObject(obj, f))
                continue;
            bool haveData = false;
            int64_t dataPos = 0, dataLen = 0;
            UfsVirt v;
            v.Plan.Kind = UxKind::Raw;
            switch (obj.ClassID)
            {
            case 28: {
                if (s.IntOf(f, "m_StreamData.size") > 0 &&
                    ResolveStream(s.StrOf(f, "m_StreamData.path"), s.IntOf(f, "m_StreamData.offset"),
                                  s.IntOf(f, "m_StreamData.size"), dataPos))
                {
                    dataLen = s.IntOf(f, "m_StreamData.size");
                    haveData = true;
                }
                else
                {
                    const int fld = s.Find(f, "image data");
                    if (fld >= 0 && f[static_cast<size_t>(fld)].Blob && f[static_cast<size_t>(fld)].Len > 0)
                    {
                        dataPos = s.AbsPos(f[static_cast<size_t>(fld)]);
                        dataLen = f[static_cast<size_t>(fld)].Len;
                        haveData = true;
                    }
                }
                if (!haveData)
                    continue;
                const int tf = static_cast<int>(s.IntOf(f, "m_TextureFormat"));
                const int tw = static_cast<int>(s.IntOf(f, "m_Width"));
                const int th = static_cast<int>(s.IntOf(f, "m_Height"));
                UxPlan plan = PlanTexture(tf, tw, th, dataLen);
                if (tf == 28 || tf == 29)
                {
                    // crunched: the header of the crunch data tells what it is
                    const std::string head = peek(dataPos, 256);
                    CrunchHeaderInfo ci;
                    if (head.size() >= 74 &&
                        CrunchInfo(reinterpret_cast<const uint8_t *>(head.data()), static_cast<int64_t>(head.size()),
                                   dataLen, ci))
                        plan = PlanCrunch(tf, tw, th, ci.Format, ci.Width, ci.Height, dataLen);
                }
                v.Plan = plan;
                v.SrcLen = dataLen;
                break;
            }
            case 83: {
                if (s.IntOf(f, "m_Resource.m_Size") > 0 &&
                    ResolveStream(s.StrOf(f, "m_Resource.m_Source"), s.IntOf(f, "m_Resource.m_Offset"),
                                  s.IntOf(f, "m_Resource.m_Size"), dataPos))
                {
                    dataLen = s.IntOf(f, "m_Resource.m_Size");
                    haveData = true;
                }
                else
                {
                    const int fld = s.Find(f, "m_AudioData");
                    if (fld >= 0 && f[static_cast<size_t>(fld)].Blob && f[static_cast<size_t>(fld)].Len > 0)
                    {
                        dataPos = s.AbsPos(f[static_cast<size_t>(fld)]);
                        dataLen = f[static_cast<size_t>(fld)].Len;
                        haveData = true;
                    }
                }
                if (!haveData)
                    continue;
                v.Plan.Ext = AudioExt(peek(dataPos, 4));
                break;
            }
            default: { // 49, 128
                const int fld = s.Find(f, obj.ClassID == 49 ? "m_Script" : "m_FontData");
                if (fld < 0 || !f[static_cast<size_t>(fld)].Blob)
                    continue;
                dataPos = s.AbsPos(f[static_cast<size_t>(fld)]);
                dataLen = f[static_cast<size_t>(fld)].Len;
                const std::string head = peek(dataPos, dataLen);
                if (obj.ClassID == 49)
                    v.Plan.Ext = TextExt(head, dataLen);
                else
                {
                    if (dataLen == 0)
                        continue;
                    v.Plan.Ext = FontExt(head);
                }
                break;
            }
            }
            if (v.Plan.Kind == UxKind::Raw)
            {
                v.Plan.PayloadLen = dataLen;
                v.Plan.OutSize = dataLen;
            }
            v.Pos = dataPos;
            std::string nm = Sanitize(s.StrOf(f, "m_Name"));
            const std::string key = folder + "/" + nm + v.Plan.Ext;
            if (used.count(ToLower(key)) > 0)
                nm += "_" + std::to_string(obj.PathID);
            used.insert(ToLower(folder + "/" + nm + v.Plan.Ext));
            AddVirt(v);
            Add(prefix + folder + "/" + nm + v.Plan.Ext, dataPos, v.Plan.OutSize, static_cast<int64_t>(_virt.size()));
        }
        // second pass: cubemaps (6 textures), sprites (a rectangle of a texture) and meshes. The objects are read in the
        // order of their data (in a compressed bundle every block is unpacked once instead of once per object, the
        // table is not in that order) and the entries are added afterwards in the order of the table
        std::vector<int> order;
        for (size_t o = 0; o < s.Objects.size(); ++o)
            if (s.Objects[o].ClassID == 89 || s.Objects[o].ClassID == 213 || s.Objects[o].ClassID == 43)
                order.push_back(static_cast<int>(o));
        {
            std::vector<int64_t> keys(s.Objects.size());
            for (size_t i = 0; i < s.Objects.size(); ++i)
                keys[i] = s.Objects[i].Start;
            SortIdx(order, keys);
        }
        // Reading the sprites and meshes of a file with a lot of them in a packed bundle unpacks nearly all of it (Ara Fell:
        // 777 MB of LZMA to unpack: 3 minutes): over the limit they are left out of the listing (DUDI7_UNITY_DEEP=1 reads them anyway)
        // (every block that an object touches, from its first byte to its last, counted once: the objects are in order)
        int64_t cost = 0;
        int64_t lastBlock = -1;
        for (const int oi : order)
        {
            int64_t blockBytes, blockBytes2;
            const UsObject &obj = s.Objects[static_cast<size_t>(oi)];
            const int64_t blockNo = PackedBlock(base + obj.Start, blockBytes);
            if (blockNo >= 0)
            {
                if (blockNo > lastBlock)
                    cost += blockBytes;
                const int64_t blockNo2 = PackedBlock(base + obj.Start + obj.Size - 1, blockBytes2);
                if (blockNo2 > blockNo)
                    cost += (blockNo2 - blockNo) * blockBytes2; // (the blocks between have about the size of the last)
                if (blockNo2 > lastBlock)
                    lastBlock = blockNo2;
                if (blockNo > lastBlock)
                    lastBlock = blockNo;
            }
        }
        const char *deep = std::getenv("DUDI7_UNITY_DEEP");
        if (cost > MaxUnpack && !(deep != nullptr && std::string(deep) == "1"))
            order.clear();
        for (const int oi : order)
        {
            const size_t o = static_cast<size_t>(oi);
            if (_entries.size() + pend.size() >= MaxObjectEntries)
                break;
            const UsObject &obj = s.Objects[o];
            if (!s.ReadObject(obj, f))
                continue;
            UfsVirt v;
            v.Plan.Kind = UxKind::Raw;
            int64_t dataPos = 0, dataLen = 0;
            if (obj.ClassID == 89)
            {
                // data of the faces: +X -X +Y -Y +Z -Z, each with all its mip levels (m_CompleteImageSize bytes)
                if (!textureData(f, dataPos, dataLen))
                    continue;
                const int fmt = static_cast<int>(s.IntOf(f, "m_TextureFormat"));
                const int64_t cis = s.IntOf(f, "m_CompleteImageSize");
                if (cis <= 0 || dataLen < 6 * cis)
                    continue;
                for (int face = 0; face <= 5; ++face)
                {
                    v.Plan = PlanTexture(fmt, static_cast<int>(s.IntOf(f, "m_Width")),
                                         static_cast<int>(s.IntOf(f, "m_Height")), cis);
                    v.SrcLen = cis;
                    v.Pos = dataPos + face * cis;
                    if (v.Plan.Kind == UxKind::Raw)
                    {
                        v.Plan.PayloadLen = cis;
                        v.Plan.OutSize = cis;
                    }
                    queue("Cubemap", Sanitize(s.StrOf(f, "m_Name")) + "_" + faceNames[face], v, obj.PathID,
                          static_cast<int>(o));
                }
            }
            else if (obj.ClassID == 43)
            {
                // the mesh as OBJ
                MeshInfo mi;
                if (!meshFromObject(f, mi))
                    continue;
                _meshes.push_back(mi);
                v.Plan.Kind = UxKind::Mesh;
                v.Plan.Ext = ".obj";
                v.Plan.OutSize = MeshObjSize(mi);
                v.MeshIdx = static_cast<int>(_meshes.size()) - 1;
                v.Pos = mi.VertPos;
                queue("Mesh", mi.Name, v, obj.PathID, static_cast<int>(o));
            }
            else
            {
                // the sprite: m_RD.textureRect of the texture m_RD.texture (of this file)
                if (s.IntOf(f, "m_RD.texture.m_FileID") != 0 || s.IntOf(f, "m_RD.texture.m_PathID") == 0)
                    continue;
                // the texture is looked up once: a file has a few atlases and thousands of sprites, and reading a far
                // object of a compressed bundle again and again unpacks the same blocks over and over
                const std::string cKey = std::to_string(s.IntOf(f, "m_RD.texture.m_PathID"));
                int ci;
                const auto cached = texCache.find(cKey);
                if (cached != texCache.end())
                    ci = cached->second;
                else
                {
                    ci = -1;
                    const int t = findObject(s.IntOf(f, "m_RD.texture.m_PathID"));
                    UsFields tf;
                    if (t >= 0 && s.Objects[static_cast<size_t>(t)].ClassID == 28 &&
                        s.ReadObject(s.Objects[static_cast<size_t>(t)], tf) && textureData(tf, dataPos, dataLen))
                    {
                        TexInfo info;
                        info.Fmt = static_cast<int>(s.IntOf(tf, "m_TextureFormat"));
                        info.W = static_cast<int>(s.IntOf(tf, "m_Width"));
                        info.H = static_cast<int>(s.IntOf(tf, "m_Height"));
                        info.Pos = dataPos;
                        info.Len = dataLen;
                        info.Ok = info.W <= 16384 && info.H <= 16384 &&
                                  spriteFormatOk(info.Fmt, info.W, info.H, dataLen, dataPos);
                        texInfos.push_back(info);
                        ci = static_cast<int>(texInfos.size()) - 1;
                    }
                    texCache[cKey] = ci;
                }
                if (ci < 0 || !texInfos[static_cast<size_t>(ci)].Ok)
                    continue;
                const TexInfo &ti = texInfos[static_cast<size_t>(ci)];
                dataPos = ti.Pos;
                dataLen = ti.Len;
                const int rw = Round(s.FloatOf(f, "m_RD.textureRect.width"));
                const int rh = Round(s.FloatOf(f, "m_RD.textureRect.height"));
                const int rx = Round(s.FloatOf(f, "m_RD.textureRect.x"));
                const int ry = Round(s.FloatOf(f, "m_RD.textureRect.y"));
                if (rw < 1 || rh < 1 || rx < 0 || ry < 0 || rx + rw > ti.W || ry + rh > ti.H)
                    continue;
                v.Plan.Kind = UxKind::Sprite;
                v.Plan.Ext = ".tga";
                v.Plan.Fmt = ti.Fmt;
                v.Plan.W = rw;
                v.Plan.H = rh;
                v.Plan.HeaderLen = 18;
                v.Plan.OutSize = 18 + static_cast<int64_t>(rw) * rh * 4;
                v.Plan.PayloadLen = v.Plan.OutSize - 18;
                v.TFmt = ti.Fmt;
                v.TW = ti.W;
                v.TH = ti.H;
                v.RX = rx;
                v.RY = ry;
                v.Pos = dataPos;
                v.SrcLen = dataLen;
                queue("Sprite", Sanitize(s.StrOf(f, "m_Name")), v, obj.PathID, static_cast<int>(o));
            }
        }
        flushPending();
    }
    catch (...)
    {
        // keep what was read: the raw files are always there
    }
}

auto UnityObjectArchive::TexRegion(const UfsVirt &v, int x, int y, int w, int h, uint8_t *dst) -> bool
{
    const int fmt = v.TFmt;
    int fmtDec = fmt;
    bool fromAtlas = false;
    std::vector<uint8_t> src, rows;
    if (fmt == 28 || fmt == 29)
    {
        // crunched: unpack once (the sprites of an atlas follow each other)
        if (_atlasPos != v.Pos || _atlas.empty())
        {
            _atlasPos = -1;
            src.assign(static_cast<size_t>(v.SrcLen) + 8, 0);
            if (!ReadAt(v.Pos, v.SrcLen, src.data()))
                return false;
            CrunchHeaderInfo ci;
            if (!CrunchInfo(src.data(), v.SrcLen, v.SrcLen, ci))
                return false;
            _atlas.assign(static_cast<size_t>(CrunchLevelSize(ci, 0)) + 8, 0);
            if (!CrunchUnpack(src.data(), v.SrcLen, 0, _atlas.data(), CrunchLevelSize(ci, 0)))
                return false;
            _atlasPos = v.Pos;
        }
        fromAtlas = true;
        fmtDec = fmt == 28 ? 10 : 12;
    }
    // bytes of the texture data (for a crunched texture: its unpacked blocks)
    const auto fetch = [&](int64_t pos, int64_t len, void *into) -> bool {
        if (fromAtlas)
        {
            if (pos < 0 || pos + len > static_cast<int64_t>(_atlas.size()))
                return false;
            std::memcpy(into, &_atlas[static_cast<size_t>(pos)], static_cast<size_t>(len));
            return true;
        }
        return ReadAt(v.Pos + pos, len, into);
    };
    const int bpp = SourceBytesPerPixel(fmtDec);
    if (bpp > 0)
    {
        rows.assign(static_cast<size_t>(w) * static_cast<size_t>(bpp) + 8, 0);
        for (int r = 0; r < h; ++r)
        {
            if (!fetch((static_cast<int64_t>(y + r) * v.TW + x) * bpp, static_cast<int64_t>(w) * bpp, rows.data()))
                return false;
            uint8_t *p = dst + static_cast<int64_t>(r) * w * 4;
            for (int c = 0; c < w; ++c)
            {
                const uint8_t *row = rows.data();
                switch (fmtDec)
                {
                case 1: // alpha only
                    p[0] = 255;
                    p[1] = 255;
                    p[2] = 255;
                    p[3] = row[c];
                    break;
                case 63:
                    p[0] = row[c];
                    p[1] = row[c];
                    p[2] = row[c];
                    p[3] = 255;
                    break;
                case 3:
                    p[0] = row[c * 3 + 2];
                    p[1] = row[c * 3 + 1];
                    p[2] = row[c * 3];
                    p[3] = 255;
                    break;
                case 4: // RGBA
                    p[0] = row[c * 4 + 2];
                    p[1] = row[c * 4 + 1];
                    p[2] = row[c * 4];
                    p[3] = row[c * 4 + 3];
                    break;
                case 5: // ARGB
                    p[0] = row[c * 4 + 3];
                    p[1] = row[c * 4 + 2];
                    p[2] = row[c * 4 + 1];
                    p[3] = row[c * 4];
                    break;
                case 14: // BGRA
                    p[0] = row[c * 4];
                    p[1] = row[c * 4 + 1];
                    p[2] = row[c * 4 + 2];
                    p[3] = row[c * 4 + 3];
                    break;
                default: {
                    const uint32_t px = row[c * 2] | (static_cast<uint32_t>(row[c * 2 + 1]) << 8);
                    if (fmtDec == 2)
                    {
                        p[0] = static_cast<uint8_t>((px & 15) * 17);
                        p[1] = static_cast<uint8_t>(((px >> 4) & 15) * 17);
                        p[2] = static_cast<uint8_t>(((px >> 8) & 15) * 17);
                        p[3] = static_cast<uint8_t>((px >> 12) * 17);
                    }
                    else if (fmtDec == 13)
                    {
                        p[0] = static_cast<uint8_t>(((px >> 4) & 15) * 17);
                        p[1] = static_cast<uint8_t>(((px >> 8) & 15) * 17);
                        p[2] = static_cast<uint8_t>((px >> 12) * 17);
                        p[3] = static_cast<uint8_t>((px & 15) * 17);
                    }
                    else
                    {
                        const uint32_t a = px & 31, g = (px >> 5) & 63, rr = px >> 11;
                        p[0] = static_cast<uint8_t>((a << 3) | (a >> 2));
                        p[1] = static_cast<uint8_t>((g << 2) | (g >> 4));
                        p[2] = static_cast<uint8_t>((rr << 3) | (rr >> 2));
                        p[3] = 255;
                    }
                    break;
                }
                }
                p += 4;
            }
        }
        return true;
    }
    int bw, bh, bb;
    if (!DecodableFormatInfo(fmtDec, bw, bh, bb))
        return false;
    // block formats: the rows of blocks that cover the region are decoded, then cut
    const int bx = (v.TW + bw - 1) / bw;
    const int row0 = y / bh;
    const int row1 = (y + h - 1) / bh;
    src.assign(static_cast<size_t>(bx) * static_cast<size_t>(bb) + 8, 0);
    rows.assign(static_cast<size_t>(v.TW) * static_cast<size_t>(bh) * 4 + 8, 0);
    for (int bRow = row0; bRow <= row1; ++bRow)
    {
        if (!fetch(static_cast<int64_t>(bRow) * bx * bb, static_cast<int64_t>(bx) * bb, src.data()))
            return false;
        DecodeBlockRow(fmtDec, src.data(), v.TW, v.TH, bRow, rows.data());
        int lines = v.TH - bRow * bh;
        if (lines > bh)
            lines = bh;
        for (int r = 0; r < lines; ++r)
            if (bRow * bh + r >= y && bRow * bh + r < y + h)
                std::memmove(dst + static_cast<int64_t>(bRow * bh + r - y) * w * 4,
                             &rows[static_cast<size_t>((static_cast<int64_t>(r) * v.TW + x) * 4)],
                             static_cast<size_t>(w) * 4);
    }
    // the formats without alpha
    if (fmtDec >= 48 && fmtDec <= 53)
        for (int64_t r = 0; r <= static_cast<int64_t>(w) * h - 1; ++r)
            dst[r * 4 + 3] = 255;
    return true;
}

auto UnityObjectArchive::ExtractVirtual(const UfsVirt &v, const Sink &sink) -> int32_t
{
    uint8_t hdr[160];
    std::vector<uint8_t> buf, cv;
    int32_t result = DUDI7_OK;
    switch (v.Plan.Kind)
    {
    case UxKind::Raw:
        return StreamRange(v.Pos, v.Plan.PayloadLen, sink);
    case UxKind::Dds:
        BuildHeader(v.Plan, hdr);
        result = ExtractBytes(hdr, v.Plan.HeaderLen, sink);
        if (result == DUDI7_OK)
            result = StreamRange(v.Pos, v.Plan.PayloadLen, sink);
        return result;
    case UxKind::Mesh: {
        std::string obj;
        const MeshRead read = [this](int64_t pos, int64_t len, void *dst) { return ReadAt(pos, len, dst); };
        if (!MeshWriteObj(_meshes[static_cast<size_t>(v.MeshIdx)], read, obj) ||
            static_cast<int64_t>(obj.size()) != v.Plan.OutSize)
        {
            LastError = "Cannot read the mesh: wrote " + std::to_string(obj.size()) + " of " +
                        std::to_string(v.Plan.OutSize);
            return DUDI7_ERR_IO;
        }
        return ExtractBytes(obj.data(), static_cast<int64_t>(obj.size()), sink);
    }
    case UxKind::Sprite: {
        BuildHeader(v.Plan, hdr);
        result = ExtractBytes(hdr, v.Plan.HeaderLen, sink);
        if (result != DUDI7_OK)
            return result;
        // by blocks of lines so that a large sprite does not need a large buffer
        int64_t n = 1;
        while (n < v.Plan.H && n * 2 * v.Plan.W * 4 <= Chunk)
            n *= 2;
        cv.assign(static_cast<size_t>(n * v.Plan.W * 4 + 8), 0);
        int64_t pos = 0;
        while (pos < v.Plan.H)
        {
            int64_t left = v.Plan.H - pos;
            if (left > n)
                left = n;
            if (!TexRegion(v, v.RX, v.RY + static_cast<int>(pos), v.Plan.W, static_cast<int>(left), cv.data()))
            {
                LastError = "Cannot read the texture of the sprite";
                return DUDI7_ERR_IO;
            }
            result = ExtractBytes(cv.data(), left * v.Plan.W * 4, sink);
            if (result != DUDI7_OK)
                return result;
            pos += left;
        }
        return result;
    }
    case UxKind::Crunch: {
        // the crunch data is unpacked in memory (a few MB)
        buf.assign(static_cast<size_t>(v.SrcLen) + 8, 0);
        cv.assign(static_cast<size_t>(v.Plan.PayloadLen) + 8, 0);
        if (v.SrcLen <= 0 || !ReadAt(v.Pos, v.SrcLen, buf.data()))
        {
            LastError = "Cannot read the crunched texture at " + std::to_string(v.Pos);
            return DUDI7_ERR_IO;
        }
        if (!CrunchUnpack(buf.data(), v.SrcLen, 0, cv.data(), v.Plan.PayloadLen))
        {
            LastError = "Cannot unpack the crunched texture";
            return DUDI7_ERR_IO;
        }
        BuildHeader(v.Plan, hdr);
        result = ExtractBytes(hdr, v.Plan.HeaderLen, sink);
        if (result == DUDI7_OK)
            result = ExtractBytes(cv.data(), v.Plan.PayloadLen, sink);
        return result;
    }
    case UxKind::Decode: {
        BuildHeader(v.Plan, hdr);
        result = ExtractBytes(hdr, v.Plan.HeaderLen, sink);
        if (result != DUDI7_OK)
            return result;
        int bw, bh, bb;
        BlockFormatInfo(v.Plan.Fmt, bw, bh, bb);
        const int bx = (v.Plan.W + bw - 1) / bw;
        const int by = (v.Plan.H + bh - 1) / bh;
        buf.assign(static_cast<size_t>(bx) * static_cast<size_t>(bb) + 8, 0);
        cv.assign(static_cast<size_t>(v.Plan.W) * static_cast<size_t>(bh) * 4 + 8, 0);
        for (int64_t n = 0; n <= by - 1; ++n)
        {
            if (!ReadAt(v.Pos + n * bx * bb, static_cast<int64_t>(bx) * bb, buf.data()))
            {
                LastError = "Cannot read the texture data";
                return DUDI7_ERR_IO;
            }
            DecodeBlockRow(v.Plan.Fmt, buf.data(), v.Plan.W, v.Plan.H, static_cast<int>(n), cv.data());
            int64_t left = v.Plan.H - n * bh;
            if (left > bh)
                left = bh;
            if (v.Plan.Fmt >= 48 && v.Plan.Fmt <= 53) // formats without alpha
            {
                int64_t pos = 3;
                while (pos < left * v.Plan.W * 4)
                {
                    cv[static_cast<size_t>(pos)] = 255;
                    pos += 4;
                }
            }
            result = ExtractBytes(cv.data(), left * v.Plan.W * 4, sink);
            if (result != DUDI7_OK)
                return result;
        }
        return result;
    }
    default: {
        BuildHeader(v.Plan, hdr);
        result = ExtractBytes(hdr, v.Plan.HeaderLen, sink);
        if (result != DUDI7_OK)
            return result;
        const int bpp = SourceBytesPerPixel(v.Plan.Fmt);
        const int obpp = OutBytesPerPixel(v.Plan.Fmt);
        buf.assign(static_cast<size_t>((Chunk / bpp) * bpp), 0);
        if (obpp != bpp)
            cv.assign(static_cast<size_t>(((static_cast<int64_t>(buf.size()) / bpp) * obpp) + 8), 0);
        int64_t pos = v.Pos;
        int64_t left = v.Plan.PayloadLen;
        while (left > 0)
        {
            int64_t n = static_cast<int64_t>(buf.size());
            if (n > left)
                n = left;
            if (!ReadAt(pos, n, buf.data()))
            {
                LastError = "Cannot unpack the texture data at " + std::to_string(pos);
                return DUDI7_ERR_IO;
            }
            if (obpp != bpp)
            {
                ConvertPixels(v.Plan.Fmt, buf.data(), cv.data(), static_cast<int>(n / bpp));
                result = ExtractBytes(cv.data(), (n / bpp) * obpp, sink);
            }
            else
            {
                ConvertPixels(v.Plan.Fmt, buf.data(), buf.data(), static_cast<int>(n / bpp));
                result = ExtractBytes(buf.data(), n, sink);
            }
            if (result != DUDI7_OK)
                return result;
            pos += n;
            left -= n;
        }
        return result;
    }
    }
}
} // namespace Dud
