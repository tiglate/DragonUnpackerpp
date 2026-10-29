// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/HrfFile.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace Dup::Core
{
namespace
{
constexpr char MAGIC[5] = {'H', 'R', 'F', 'i', 26};
constexpr std::size_t HEADER_V12 = 114; // ID 5, version 1, name 98, size 4, program version 2, entries 4
constexpr std::size_t HEADER_V3 = 284;  // see the specification
constexpr std::size_t INFO_V12 = 256;
constexpr std::size_t INFO_V3 = 257;
constexpr std::size_t ENTRY_V1 = 41;
constexpr std::size_t ENTRY_V2 = 89;
constexpr std::size_t ENTRY_V3 = 271;
constexpr std::int64_t MAX_ENTRIES = 50'000'000;

class Output
{
  public:
    void Bytes(const void *data, std::size_t length)
    {
        const auto *p = static_cast<const std::uint8_t *>(data);
        _data.insert(_data.end(), p, p + length);
    }
    void U8(std::uint8_t v)
    {
        _data.push_back(v);
    }
    void U16(std::uint32_t v)
    {
        Int(v, 2);
    }
    void U32(std::uint32_t v)
    {
        Int(v, 4);
    }
    void I64(std::int64_t v)
    {
        Int(static_cast<std::uint64_t>(v), 8);
    }
    /// A zero padded field; the text is cut to leave room for the terminating zero.
    void Field(const std::string &text, std::size_t width)
    {
        std::string cut = text.substr(0, width - 1);
        _data.insert(_data.end(), cut.begin(), cut.end());
        _data.insert(_data.end(), width - cut.size(), 0);
    }
    [[nodiscard]] auto Data() const -> const std::vector<std::uint8_t> &
    {
        return _data;
    }

  private:
    void Int(std::uint64_t v, int bytes)
    {
        for (int i = 0; i < bytes; ++i)
        {
            _data.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
        }
    }
    std::vector<std::uint8_t> _data;
};

class Input
{
  public:
    explicit Input(std::vector<std::uint8_t> data) : _data(std::move(data))
    {
    }
    [[nodiscard]] auto Left() const -> std::size_t
    {
        return _data.size() - _pos;
    }
    void Skip(std::size_t n)
    {
        Need(n);
        _pos += n;
    }
    auto U8() -> std::uint8_t
    {
        Need(1);
        return _data[_pos++];
    }
    auto Int(int bytes) -> std::uint64_t
    {
        Need(static_cast<std::size_t>(bytes));
        std::uint64_t v = 0;
        for (int i = 0; i < bytes; ++i)
        {
            v |= static_cast<std::uint64_t>(_data[_pos++]) << (8 * i);
        }
        return v;
    }
    auto Field(std::size_t width) -> std::string
    {
        Need(width);
        const char *p = reinterpret_cast<const char *>(_data.data()) + _pos;
        _pos += width;
        return {p, strnlen(p, width)};
    }

  private:
    void Need(std::size_t n) const
    {
        if (n > Left())
        {
            throw std::runtime_error("the HRF file is truncated");
        }
    }
    std::vector<std::uint8_t> _data;
    std::size_t _pos{0};
};

void CheckFits(const HrfFile &hrf)
{
    for (const auto &e : hrf.Entries)
    {
        if (e.Offset < 0 || e.Size < 0 || e.Offset + 1 > INT32_MAX || e.Size > INT32_MAX)
        {
            throw std::runtime_error("versions 1 and 2 of the HRF format cannot hold offsets beyond 2 GiB: use version 3");
        }
    }
    if (hrf.SourceSize > INT32_MAX)
    {
        throw std::runtime_error("versions 1 and 2 of the HRF format cannot hold a companion file beyond 2 GiB: use version 3");
    }
}

void WriteV12(Output &out, const HrfFile &hrf)
{
    CheckFits(hrf);
    out.Bytes(MAGIC, 5);
    out.U8(static_cast<std::uint8_t>(hrf.Version));
    out.Field(hrf.SourceName, 98);
    out.U32(static_cast<std::uint32_t>(hrf.SourceSize));
    out.U8(static_cast<std::uint8_t>(hrf.ProgramVersion / 10000));
    out.U8(static_cast<std::uint8_t>(hrf.ProgramVersion / 1000 % 10));
    out.U32(static_cast<std::uint32_t>(hrf.Entries.size()));
    if (hrf.Version == 2)
    {
        out.U8(hrf.Info ? 1 : 0); // version of the information chunk
        out.U8(0);                // size of the security data
        if (hrf.Info)
        {
            out.Field(hrf.Info->Author, 64);
            out.Field(hrf.Info->Url, 128);
            out.Field(hrf.Info->Title, 64);
        }
    }
    for (const auto &e : hrf.Entries)
    {
        out.Field(e.Name, hrf.Version == 1 ? 32 : 64);
        out.U8(199);
        out.U32(static_cast<std::uint32_t>(e.Offset + 1));
        out.U32(static_cast<std::uint32_t>(e.Size));
        if (hrf.Version == 2)
        {
            const std::array<std::uint8_t, 16> security{};
            out.Bytes(security.data(), security.size());
        }
    }
}

void WriteV3(Output &out, const HrfFile &hrf)
{
    out.Bytes(MAGIC, 5);
    out.U8(3);
    out.U8(0);
    out.U32(static_cast<std::uint32_t>(hrf.ProgramVersion));
    out.U8(hrf.ProgramId);
    out.U8(hrf.Info ? 1 : 0);
    out.Field(hrf.SourceName, 255);
    out.I64(hrf.SourceSize);
    out.U32(static_cast<std::uint32_t>(hrf.Entries.size()));
    out.U32(static_cast<std::uint32_t>(HEADER_V3 + (hrf.Info ? INFO_V3 : 0)));
    if (hrf.Info)
    {
        out.U8(0);
        out.Field(hrf.Info->Author, 64);
        out.Field(hrf.Info->Url, 128);
        out.Field(hrf.Info->Title, 64);
    }
    for (const auto &e : hrf.Entries)
    {
        out.Field(e.Name, 255);
        out.I64(e.Offset);
        out.I64(e.Size);
    }
}

void ReadInfo(Input &in, HrfFile &hrf, bool typeByte)
{
    if (typeByte)
    {
        in.U8();
    }
    HrfInfo info;
    info.Author = in.Field(64);
    info.Url = in.Field(128);
    info.Title = in.Field(64);
    hrf.Info = std::move(info);
}
} // namespace

void WriteHrf(const std::string &path, const HrfFile &hrf)
{
    if (hrf.Version < 1 || hrf.Version > 3)
    {
        throw std::runtime_error("unknown version of the HRF format");
    }
    Output out;
    if (hrf.Version == 3)
    {
        WriteV3(out, hrf);
    }
    else
    {
        WriteV12(out, hrf);
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        throw std::runtime_error("cannot create " + path);
    }
    file.write(reinterpret_cast<const char *>(out.Data().data()), static_cast<std::streamsize>(out.Data().size()));
    if (!file)
    {
        throw std::runtime_error("cannot write " + path);
    }
}

auto ReadHrf(const std::string &path) -> HrfFile
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("cannot open " + path);
    }
    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (data.size() < 6 || std::memcmp(data.data(), MAGIC, 5) != 0)
    {
        throw std::runtime_error("not an HRF file");
    }
    Input in(std::move(data));
    in.Skip(5);
    const int version = in.U8();
    HrfFile hrf;
    if (version == 1 || version == 2)
    {
        hrf.Version = version;
        hrf.SourceName = in.Field(98);
        hrf.SourceSize = static_cast<std::int32_t>(in.Int(4));
        const int major = in.U8();
        const int minor = in.U8();
        hrf.ProgramVersion = major * 10000 + minor * 1000;
        hrf.ProgramId = 1;
        const auto count = static_cast<std::int32_t>(in.Int(4));
        if (count < 0 || count > MAX_ENTRIES)
        {
            throw std::runtime_error("the HRF file is damaged");
        }
        if (version == 2)
        {
            const int infoVersion = in.U8();
            in.U8();
            if (infoVersion == 1)
            {
                ReadInfo(in, hrf, false);
            }
        }
        // the size of an entry of version 2 depends on the security data: the rest of the file tells it
        const std::size_t entrySize = version == 1 ? ENTRY_V1 : (count > 0 ? in.Left() / static_cast<std::size_t>(count) : ENTRY_V2);
        const std::size_t nameWidth = version == 1 ? 32 : 64;
        if (count > 0 && (entrySize < nameWidth + 9 || in.Left() < entrySize * static_cast<std::size_t>(count)))
        {
            throw std::runtime_error("the HRF file is damaged");
        }
        for (std::int32_t i = 0; i < count; ++i)
        {
            HrfEntry e;
            e.Name = in.Field(nameWidth);
            in.U8();
            e.Offset = static_cast<std::int64_t>(static_cast<std::int32_t>(in.Int(4))) - 1;
            e.Size = static_cast<std::int32_t>(in.Int(4));
            in.Skip(entrySize - (nameWidth + 9));
            hrf.Entries.push_back(std::move(e));
        }
    }
    else if (version == 3)
    {
        hrf.Version = 3;
        const int minor = in.U8();
        if (minor != 0)
        {
            throw std::runtime_error("unsupported version of the HRF format");
        }
        hrf.ProgramVersion = static_cast<std::int32_t>(in.Int(4));
        hrf.ProgramId = in.U8();
        const int attributes = in.U8();
        hrf.SourceName = in.Field(255);
        hrf.SourceSize = static_cast<std::int64_t>(in.Int(8));
        const auto count = static_cast<std::int32_t>(in.Int(4));
        const auto indexAt = static_cast<std::uint32_t>(in.Int(4));
        if (count < 0 || count > MAX_ENTRIES || indexAt < HEADER_V3)
        {
            throw std::runtime_error("the HRF file is damaged");
        }
        if ((attributes & 1) != 0)
        {
            ReadInfo(in, hrf, true);
        }
        in.Skip(indexAt - HEADER_V3 - ((attributes & 1) != 0 ? INFO_V3 : 0));
        if (in.Left() < ENTRY_V3 * static_cast<std::size_t>(count))
        {
            throw std::runtime_error("the HRF file is damaged");
        }
        for (std::int32_t i = 0; i < count; ++i)
        {
            HrfEntry e;
            e.Name = in.Field(255);
            e.Offset = static_cast<std::int64_t>(in.Int(8));
            e.Size = static_cast<std::int64_t>(in.Int(8));
            hrf.Entries.push_back(std::move(e));
        }
    }
    else
    {
        throw std::runtime_error("unsupported version of the HRF format");
    }
    for (const auto &e : hrf.Entries)
    {
        if (e.Offset < 0 || e.Size < 0)
        {
            throw std::runtime_error("the HRF file is damaged");
        }
    }
    return hrf;
}
} // namespace Dup::Core
