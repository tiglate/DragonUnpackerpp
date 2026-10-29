// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "UnityMesh.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace Dud
{
namespace
{
auto TrianglesOf(const MeshSub &s) -> int64_t
{
    switch (s.Topology)
    {
    case 0:
        return s.IndexCount / 3;
    case 1:
        return s.IndexCount >= 3 ? s.IndexCount - 2 : 0;
    case 2:
        return (s.IndexCount / 4) * 2;
    default:
        return 0;
    }
}

auto Half(uint16_t h) -> float
{
    const int e = (h >> 10) & 0x1F;
    const int m = h & 0x3FF;
    if (e == 31)
        return 0; // not a finite number
    float s;
    if (e == 0)
        s = std::ldexp(static_cast<float>(m), -24); // 2^-24
    else
    {
        const uint32_t bits = (static_cast<uint32_t>(e - 15 + 127) << 23) | (static_cast<uint32_t>(m) << 13);
        std::memcpy(&s, &bits, 4);
    }
    if ((h & 0x8000) != 0)
        s = -s;
    return s;
}

auto Comp(const uint8_t *p, int fmt) -> float
{
    float result;
    const uint32_t word = static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
                          (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
    const auto half = static_cast<uint16_t>(p[0] | (p[1] << 8));
    switch (fmt)
    {
    case 0:
        std::memcpy(&result, p, 4);
        break;
    case 1:
        result = Half(half);
        break;
    case 2:
        result = static_cast<float>(p[0] / 255.0);
        break;
    case 3:
        result = static_cast<int8_t>(p[0]) == -128 ? -1.0F : static_cast<float>(static_cast<int8_t>(p[0]) / 127.0);
        break;
    case 4:
        result = static_cast<float>(half / 65535.0);
        break;
    case 5:
        result = static_cast<int16_t>(half) == -32768 ? -1.0F : static_cast<float>(static_cast<int16_t>(half) / 32767.0);
        break;
    case 6:
        result = static_cast<float>(p[0]);
        break;
    case 7:
        result = static_cast<float>(static_cast<int8_t>(p[0]));
        break;
    case 8:
        result = static_cast<float>(half);
        break;
    case 9:
        result = static_cast<float>(static_cast<int16_t>(half));
        break;
    case 10:
        result = static_cast<float>(word);
        break;
    case 11:
        result = static_cast<float>(static_cast<int32_t>(word));
        break;
    default:
        result = 0;
        break;
    }
    if (fmt == 0)
    {
        uint32_t bits;
        std::memcpy(&bits, &result, 4);
        if (((bits >> 23) & 0xFF) == 0xFF)
            result = 0; // not a finite number
    }
    return result;
}

// a number in a field of 13 characters
auto Num(float f) -> std::string
{
    double d = f;
    if (d > 99999.999999)
        d = 99999.999999;
    else if (d < -99999.999999)
        d = -99999.999999;
    int64_t v = static_cast<int64_t>(std::nearbyint(d * 1000000.0));
    const bool neg = v < 0;
    if (neg)
        v = -v;
    char digits[32];
    std::snprintf(digits, sizeof digits, "%lld.%06lld", static_cast<long long>(v / 1000000),
                  static_cast<long long>(v % 1000000));
    std::string a = digits;
    if (neg)
        a = "-" + a;
    return std::string(a.size() < 13 ? 13 - a.size() : 0, ' ') + a;
}
} // namespace

auto MeshFormatSize(int fmt) -> int
{
    switch (fmt)
    {
    case 0:
    case 10:
    case 11:
        return 4;
    case 1:
    case 4:
    case 5:
    case 8:
    case 9:
        return 2;
    case 2:
    case 3:
    case 6:
    case 7:
        return 1;
    default:
        return 0;
    }
}

auto MeshOldFormat(int fmt) -> int
{
    switch (fmt)
    {
    case 0:
        return 0;
    case 1:
        return 1;
    case 2:
        return 2;
    case 3:
        return 6;
    case 4:
        return 10;
    default:
        return -1;
    }
}

auto MeshPrepare(MeshInfo &m, const std::vector<MeshChannel> &channels) -> bool
{
    bool used[4] = {};
    for (int &stride : m.Stride)
        stride = 0;
    for (int s = 0; s <= 3; ++s)
    {
        int sum = 0, endOf = 0;
        for (const MeshChannel &ch : channels)
            if (ch.Dim > 0 && ch.Stream == s)
            {
                const int sz = MeshFormatSize(ch.Format) * ch.Dim;
                if (sz == 0 || ch.Dim > 4)
                    return false;
                sum += sz;
                if (ch.Offset + sz > endOf)
                    endOf = ch.Offset + sz;
                used[s] = true;
            }
        if (endOf > sum)
            sum = endOf;
        m.Stride[s] = sum;
    }
    for (const MeshChannel &ch : channels)
        if (ch.Dim > 0 && ch.Stream > 3)
            return false;
    int64_t pos = 0;
    for (int s = 0; s <= 3; ++s)
    {
        m.StreamPos[s] = pos;
        pos += static_cast<int64_t>(m.Stride[s]) * m.VertexCount;
        pos = (pos + 15) & ~static_cast<int64_t>(15);
    }
    m.Width = static_cast<int>(std::to_string(m.VertexCount).size());
    const auto fits = [&](const MeshChannel &ch) {
        return ch.Stream >= 0 && ch.Stream <= 3 &&
               m.StreamPos[ch.Stream] + static_cast<int64_t>(m.Stride[ch.Stream]) * m.VertexCount <= m.VertLen;
    };
    bool result = m.Pos.Dim >= 3 && m.VertexCount > 0 && m.Pos.Stream >= 0 && m.Pos.Stream <= 3 &&
                  used[m.Pos.Stream] && m.VertLen >= m.StreamPos[3] && fits(m.Pos);
    if (result && m.Nrm.Dim > 0)
        result = fits(m.Nrm);
    if (result && m.Uv.Dim > 0)
        result = fits(m.Uv);
    return result;
}

auto MeshObjSize(const MeshInfo &m) -> int64_t
{
    int64_t result = 2 + static_cast<int64_t>(m.Name.size()) + 1; // o <name>
    result += static_cast<int64_t>(m.VertexCount) * 44;
    if (m.Uv.Dim > 0)
        result += static_cast<int64_t>(m.VertexCount) * 31;
    if (m.Nrm.Dim > 0)
        result += static_cast<int64_t>(m.VertexCount) * 45;
    int f;
    if (m.Uv.Dim > 0 && m.Nrm.Dim > 0)
        f = 3 * m.Width + 2;
    else if (m.Uv.Dim > 0)
        f = 2 * m.Width + 1;
    else if (m.Nrm.Dim > 0)
        f = 2 * m.Width + 2;
    else
        f = m.Width;
    for (size_t i = 0; i < m.Subs.size(); ++i)
    {
        const int64_t faces = TrianglesOf(m.Subs[i]);
        if (faces == 0)
            continue;
        result += 5 + static_cast<int64_t>(std::to_string(i).size()) + 1; // g sub<i>
        result += faces * (1 + 3 * (1 + f) + 1);
    }
    return result;
}

auto MeshWriteObj(const MeshInfo &m, const MeshRead &read, std::string &out) -> bool
{
    std::vector<uint8_t> vd(static_cast<size_t>(m.VertLen) + 16), id(static_cast<size_t>(m.IdxLen) + 16);
    if (m.VertLen > 0 && !read(m.VertPos, m.VertLen, vd.data()))
        return false;
    if (m.IdxLen > 0 && !read(m.IdxPos, m.IdxLen, id.data()))
        return false;

    const auto vec = [&](const MeshChannel &ch, int v, int k) -> float {
        if (k >= ch.Dim)
            return 0;
        const int64_t base = m.StreamPos[ch.Stream] + static_cast<int64_t>(v) * m.Stride[ch.Stream] + ch.Offset +
                             static_cast<int64_t>(k) * MeshFormatSize(ch.Format);
        return Comp(&vd[static_cast<size_t>(base)], ch.Format);
    };
    const auto idx = [&](int64_t pos, int64_t base) -> int64_t {
        const uint8_t *p = &id[static_cast<size_t>(pos)];
        int64_t result;
        if (m.Idx32)
            result = static_cast<int64_t>(static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
                                          (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24));
        else
            result = p[0] | (p[1] << 8);
        result += base;
        if (result < 0 || result >= m.VertexCount)
            result = 0;
        return result;
    };
    const auto pad = [&](int64_t n) {
        std::string s = std::to_string(n + 1);
        return std::string(m.Width > static_cast<int>(s.size()) ? static_cast<size_t>(m.Width) - s.size() : 0, '0') + s;
    };
    const auto one = [&](const std::string &x) {
        if (m.Uv.Dim > 0 && m.Nrm.Dim > 0)
            return " " + x + "/" + x + "/" + x;
        if (m.Uv.Dim > 0)
            return " " + x + "/" + x;
        if (m.Nrm.Dim > 0)
            return " " + x + "//" + x;
        return " " + x;
    };
    // reversed: the mesh is mirrored
    const auto tri = [&](int64_t a, int64_t b, int64_t c) { out += "f" + one(pad(c)) + one(pad(b)) + one(pad(a)) + "\n"; };
    const auto faces = [&](const MeshSub &sub) {
        const int64_t i0 = sub.FirstByte;
        switch (sub.Topology)
        {
        case 0:
            for (int64_t k = 0; k <= sub.IndexCount / 3 - 1; ++k)
            {
                if (m.Idx32)
                    tri(idx(i0 + k * 12, sub.BaseVertex), idx(i0 + k * 12 + 4, sub.BaseVertex),
                        idx(i0 + k * 12 + 8, sub.BaseVertex));
                else
                    tri(idx(i0 + k * 6, sub.BaseVertex), idx(i0 + k * 6 + 2, sub.BaseVertex),
                        idx(i0 + k * 6 + 4, sub.BaseVertex));
            }
            break;
        case 1:
            for (int64_t k = 0; k <= sub.IndexCount - 3; ++k)
            {
                int64_t a, b, c;
                if (m.Idx32)
                {
                    a = idx(i0 + k * 4, sub.BaseVertex);
                    b = idx(i0 + k * 4 + 4, sub.BaseVertex);
                    c = idx(i0 + k * 4 + 8, sub.BaseVertex);
                }
                else
                {
                    a = idx(i0 + k * 2, sub.BaseVertex);
                    b = idx(i0 + k * 2 + 2, sub.BaseVertex);
                    c = idx(i0 + k * 2 + 4, sub.BaseVertex);
                }
                if ((k & 1) == 0)
                    tri(a, b, c);
                else
                    tri(b, a, c);
            }
            break;
        case 2:
            for (int64_t k = 0; k <= sub.IndexCount / 4 - 1; ++k)
            {
                int64_t a, b, c, d;
                if (m.Idx32)
                {
                    a = idx(i0 + k * 16, sub.BaseVertex);
                    b = idx(i0 + k * 16 + 4, sub.BaseVertex);
                    c = idx(i0 + k * 16 + 8, sub.BaseVertex);
                    d = idx(i0 + k * 16 + 12, sub.BaseVertex);
                }
                else
                {
                    a = idx(i0 + k * 8, sub.BaseVertex);
                    b = idx(i0 + k * 8 + 2, sub.BaseVertex);
                    c = idx(i0 + k * 8 + 4, sub.BaseVertex);
                    d = idx(i0 + k * 8 + 6, sub.BaseVertex);
                }
                tri(a, b, c);
                tri(a, c, d);
            }
            break;
        default:
            break;
        }
    };

    out += "o " + m.Name + "\n";
    for (int i = 0; i < m.VertexCount; ++i)
        out += "v " + Num(-vec(m.Pos, i, 0)) + " " + Num(vec(m.Pos, i, 1)) + " " + Num(vec(m.Pos, i, 2)) + "\n";
    if (m.Uv.Dim > 0)
        for (int i = 0; i < m.VertexCount; ++i)
            out += "vt " + Num(vec(m.Uv, i, 0)) + " " + Num(vec(m.Uv, i, 1)) + "\n";
    if (m.Nrm.Dim > 0)
        for (int i = 0; i < m.VertexCount; ++i)
            out += "vn " + Num(-vec(m.Nrm, i, 0)) + " " + Num(vec(m.Nrm, i, 1)) + " " + Num(vec(m.Nrm, i, 2)) + "\n";
    for (size_t j = 0; j < m.Subs.size(); ++j)
    {
        if (TrianglesOf(m.Subs[j]) == 0)
            continue;
        out += "g sub" + std::to_string(j) + "\n";
        faces(m.Subs[j]);
    }
    return true;
}
} // namespace Dud
