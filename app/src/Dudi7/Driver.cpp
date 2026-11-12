// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Dudi7/Driver.h"

#include <gsl/gsl>

#include <array>
#include <utility>

namespace Dup::Dudi7
{
namespace
{
auto TextOf(const char *text) -> std::string
{
    return text != nullptr ? std::string(text) : std::string();
}

/// What the driver writes goes through this callback; a false answer from the sink is a short write.
struct SinkContext
{
    const std::function<bool(std::span<const std::byte>)> *Sink;
};

auto WriteCallback(void *context, const void *buffer, std::int64_t length) -> std::int64_t
{
    const auto *sinkContext = static_cast<const SinkContext *>(context);
    const std::span<const std::byte> bytes(static_cast<const std::byte *>(buffer), static_cast<std::size_t>(length));
    return (*sinkContext->Sink)(bytes) ? length : 0;
}
} // namespace

Archive::Archive(const Driver &driver, void *handle) : _driver(driver), _handle(handle)
{
    dudi7_archive_info info{};
    info.struct_size = sizeof(info);
    if (_driver._functions.GetArchiveInfo(_handle, &info) != DUDI7_OK)
    {
        const auto message = LastError();
        _driver._functions.Close(_handle);
        throw Error(DUDI7_ERR_GENERIC, message);
    }
    _entryCount = info.entry_count;
    _formatId = TextOf(info.format_id);
    _dirSeparator = TextOf(info.dir_separator);
}

Archive::~Archive()
{
    _driver._functions.Close(_handle);
}

auto Archive::LastError() const -> std::string
{
    std::array<char, 512> buffer{};
    _driver._functions.GetLastError(_handle, buffer.data(), static_cast<std::int32_t>(buffer.size()));
    return buffer.data();
}

auto Archive::GetEntry(std::int64_t index) const -> EntryInfo
{
    dudi7_entry entry{};
    entry.struct_size = sizeof(entry);
    const auto status = _driver._functions.GetEntry(_handle, index, &entry);
    if (status != DUDI7_OK)
    {
        throw Error(status, LastError());
    }
    return EntryInfo{TextOf(entry.name), entry.offset, entry.size, entry.data_x, entry.data_y};
}

auto Archive::Extract(std::int64_t index, const std::function<bool(std::span<const std::byte>)> &sink) const -> int
{
    SinkContext context{&sink};
    return _driver._functions.Extract(_handle, index, &WriteCallback, &context);
}

auto IArchive::ExtractToMemory(std::int64_t index, std::size_t maxBytes) const -> std::vector<std::byte>
{
    std::vector<std::byte> data;
    bool tooLarge = false;
    const auto status = Extract(index, [&](std::span<const std::byte> bytes) {
        if (data.size() + bytes.size() > maxBytes)
        {
            tooLarge = true;
            return false;
        }
        data.insert(data.end(), bytes.begin(), bytes.end());
        return true;
    });
    if (tooLarge)
    {
        throw Error(DUDI7_ERR_BUFFER, "the entry is larger than the preview limit");
    }
    if (status != DUDI7_OK)
    {
        throw Error(status, "the extraction failed (status " + std::to_string(status) + ")");
    }
    return data;
}

Driver::Driver(SharedLibrary library) : _library(std::move(library))
{
    _functions.Version = _library.Function<int32_t(int32_t, int32_t *)>("Dudi7_Version");
    _functions.Init = _library.Function<int32_t(const dudi7_host *)>("Dudi7_Init");
    _functions.Shutdown = _library.Function<int32_t()>("Dudi7_Shutdown");
    _functions.GetDriverInfo = _library.Function<int32_t(dudi7_driver_info *)>("Dudi7_GetDriverInfo");
    _functions.GetFormatInfo = _library.Function<int32_t(int32_t, dudi7_format_info *)>("Dudi7_GetFormatInfo");
    _functions.Open = _library.Function<int32_t(const char *, int32_t, void **)>("Dudi7_Open");
    _functions.GetArchiveInfo = _library.Function<int32_t(void *, dudi7_archive_info *)>("Dudi7_GetArchiveInfo");
    _functions.GetEntry = _library.Function<int32_t(void *, int64_t, dudi7_entry *)>("Dudi7_GetEntry");
    _functions.Extract = _library.Function<int32_t(void *, int64_t, dudi7_write_cb, void *)>("Dudi7_Extract");
    _functions.Close = _library.Function<int32_t(void *)>("Dudi7_Close");
    _functions.GetLastError = _library.Function<int32_t(void *, char *, int32_t)>("Dudi7_GetLastError");
    if (_functions.Version == nullptr || _functions.Init == nullptr || _functions.Shutdown == nullptr ||
        _functions.GetDriverInfo == nullptr || _functions.GetFormatInfo == nullptr || _functions.Open == nullptr ||
        _functions.GetArchiveInfo == nullptr || _functions.GetEntry == nullptr || _functions.Extract == nullptr ||
        _functions.Close == nullptr || _functions.GetLastError == nullptr)
    {
        throw Error(DUDI7_ERR_UNSUPPORTED, "not a DUDI 7 driver (missing exports)");
    }

    std::int32_t pluginVersion = 0;
    if (_functions.Version(DUDI7_CONTRACT_VERSION, &pluginVersion) != DUDI7_OK)
    {
        throw Error(DUDI7_ERR_VERSION, "the driver does not support this version of the contract");
    }
    dudi7_host host{};
    host.struct_size = sizeof(host);
    host.host_version = DUDI7_CONTRACT_VERSION;
    if (_functions.Init(&host) != DUDI7_OK)
    {
        throw Error(DUDI7_ERR_GENERIC, "the driver refused to initialize");
    }

    dudi7_driver_info info{};
    info.struct_size = sizeof(info);
    if (_functions.GetDriverInfo(&info) != DUDI7_OK)
    {
        throw Error(DUDI7_ERR_GENERIC, "cannot read the driver information");
    }
    _name = TextOf(info.name);
    for (std::int32_t i = 0; i < info.format_count; ++i)
    {
        dudi7_format_info format{};
        format.struct_size = sizeof(format);
        if (_functions.GetFormatInfo(i, &format) == DUDI7_OK)
        {
            _formats.push_back({TextOf(format.extensions), TextOf(format.name)});
        }
    }
}

auto Driver::Load(const std::filesystem::path &path) -> std::unique_ptr<Driver>
{
    return std::unique_ptr<Driver>(new Driver(SharedLibrary(path)));
}

Driver::~Driver()
{
    if (_functions.Shutdown != nullptr)
    {
        _functions.Shutdown();
    }
}

auto Driver::Open(const std::filesystem::path &path) const -> std::unique_ptr<Archive>
{
    void *handle = nullptr;
    // the driver takes UTF-8 (on Windows c_str() is UTF-16)
    const std::u8string utf8 = path.u8string();
    const auto status = _functions.Open(reinterpret_cast<const char *>(utf8.c_str()), 0, &handle);
    if (status != DUDI7_OK || handle == nullptr)
    {
        std::array<char, 512> buffer{};
        _functions.GetLastError(nullptr, buffer.data(), static_cast<std::int32_t>(buffer.size()));
        throw Error(status, buffer.data());
    }
    return std::unique_ptr<Archive>(new Archive(*this, handle));
}
} // namespace Dup::Dudi7
