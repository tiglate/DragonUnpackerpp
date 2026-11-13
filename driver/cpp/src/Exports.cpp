// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The C ABI of the driver (DUDI 7): status codes only, no exception crosses the boundary.
#include <algorithm>
#include <cstring>
#include <memory>
#include <new>
#include <string>

#include "Registry.h"
#include "dudi7.h"

#if defined(_WIN32)
// dudi7.h declares the functions without dllexport (it is the contract shared with the hosts): drv_default.def exports them
#define DUD_EXPORT extern "C"
#else
#define DUD_EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace
{
constexpr const char *DriverName = "Default Driver";
constexpr const char *DriverAuthor = "Alexandre Devilliers";
constexpr const char *DriverVersion = "6.0.0";
constexpr int32_t DriverVersionId = 1;
constexpr const char *DriverComment = "Built-in formats of Dragon UnPACKer++";

// The last error of a call without archive, per calling thread
thread_local std::string GlobalError;

auto Fail(Dud::Archive *archive, int32_t code, const std::string &message) -> int32_t
{
    if (archive != nullptr)
        archive->LastError = message;
    else
        GlobalError = message;
    return code;
}

auto Identify(const std::string &path, std::unique_ptr<Dud::Reader> &stream) -> const Dud::FormatDef *
{
    auto reader = std::make_unique<Dud::Reader>(path);
    for (const auto &format : Dud::Formats())
    {
        if (format.RequiresExtension && !Dud::ExtensionMatches(format.Extensions, path))
            continue;
        try
        {
            if (format.Check(*reader))
            {
                stream = std::move(reader);
                return &format;
            }
        }
        catch (const Dud::Error &)
        {
        }
        catch (const Dud::ReadError &)
        {
        }
    }
    return nullptr;
}
} // namespace

DUD_EXPORT int32_t Dudi7_GetLastError(void *archive, char *buf, int32_t buflen)
{
    try
    {
        const std::string &message = archive != nullptr ? static_cast<Dud::Archive *>(archive)->LastError : GlobalError;
        if (buf != nullptr && buflen > 0)
        {
            const auto n = std::min<size_t>(message.size(), static_cast<size_t>(buflen) - 1);
            std::memcpy(buf, message.data(), n);
            buf[n] = '\0';
        }
        return static_cast<int32_t>(message.size());
    }
    catch (...)
    {
        return DUDI7_ERR_GENERIC;
    }
}

DUD_EXPORT int32_t Dudi7_Version(int32_t host_version, int32_t *plugin_version)
{
    if (plugin_version == nullptr)
        return Fail(nullptr, DUDI7_ERR_BADARG, "pluginVersion is nil");
    *plugin_version = DUDI7_CONTRACT_VERSION;
    if (host_version != DUDI7_CONTRACT_VERSION)
        return Fail(nullptr, DUDI7_ERR_VERSION, "Host DUDI version not supported");
    return DUDI7_OK;
}

DUD_EXPORT int32_t Dudi7_Init(const dudi7_host *host)
{
    // The default driver does not call back into the host yet
    if (host == nullptr || host->struct_size < sizeof(dudi7_host))
        return Fail(nullptr, DUDI7_ERR_BADARG, "Invalid host structure");
    return DUDI7_OK;
}

DUD_EXPORT int32_t Dudi7_Shutdown(void)
{
    return DUDI7_OK;
}

DUD_EXPORT int32_t Dudi7_GetDriverInfo(dudi7_driver_info *info)
{
    if (info == nullptr)
        return Fail(nullptr, DUDI7_ERR_BADARG, "info is nil");
    info->struct_size = sizeof(dudi7_driver_info);
    info->version_id = DriverVersionId;
    info->name = DriverName;
    info->author = DriverAuthor;
    info->version = DriverVersion;
    info->comment = DriverComment;
    info->format_count = static_cast<int32_t>(Dud::Formats().size());
    return DUDI7_OK;
}

DUD_EXPORT int32_t Dudi7_GetFormatInfo(int32_t index, dudi7_format_info *info)
{
    const auto &formats = Dud::Formats();
    if (info == nullptr || index < 0 || static_cast<size_t>(index) >= formats.size())
        return Fail(nullptr, DUDI7_ERR_BADARG, "Invalid format index");
    info->struct_size = sizeof(dudi7_format_info);
    info->extensions = formats[static_cast<size_t>(index)].Extensions.c_str();
    info->name = formats[static_cast<size_t>(index)].Name.c_str();
    return DUDI7_OK;
}

DUD_EXPORT int32_t Dudi7_IsFormat(const char *path, int32_t, int32_t *supported)
{
    try
    {
        if (path == nullptr || supported == nullptr)
            return Fail(nullptr, DUDI7_ERR_BADARG, "Nil argument");
        *supported = 0;
        std::unique_ptr<Dud::Reader> stream;
        try
        {
            if (Identify(path, stream) != nullptr)
                *supported = 1;
        }
        catch (const Dud::Error &e)
        {
            return Fail(nullptr, e.Code(), e.what());
        }
        return DUDI7_OK;
    }
    catch (const std::exception &e)
    {
        return Fail(nullptr, DUDI7_ERR_GENERIC, e.what());
    }
}

DUD_EXPORT int32_t Dudi7_Open(const char *path, int32_t, void **archive)
{
    try
    {
        if (path == nullptr || archive == nullptr)
            return Fail(nullptr, DUDI7_ERR_BADARG, "Nil argument");
        *archive = nullptr;
        std::unique_ptr<Dud::Reader> stream;
        const Dud::FormatDef *format = nullptr;
        try
        {
            format = Identify(path, stream);
        }
        catch (const Dud::Error &e)
        {
            return Fail(nullptr, e.Code(), e.what());
        }
        if (format == nullptr)
            return Fail(nullptr, DUDI7_ERR_NOTFORMAT, "File format not supported by this driver");
        auto instance = format->Create();
        instance->SetStream(std::move(stream));
        try
        {
            instance->Parse();
        }
        catch (const Dud::Error &e)
        {
            return Fail(nullptr, e.Code(), e.what());
        }
        catch (const Dud::ReadError &e)
        {
            return Fail(nullptr, DUDI7_ERR_NOTFORMAT, std::string("Truncated or damaged file: ") + e.what());
        }
        *archive = instance.release();
        return DUDI7_OK;
    }
    catch (const std::bad_alloc &)
    {
        return Fail(nullptr, DUDI7_ERR_GENERIC, "Out of memory");
    }
    catch (const std::exception &e)
    {
        return Fail(nullptr, DUDI7_ERR_GENERIC, e.what());
    }
}

DUD_EXPORT int32_t Dudi7_GetArchiveInfo(void *archive, dudi7_archive_info *info)
{
    if (archive == nullptr || info == nullptr)
        return Fail(nullptr, DUDI7_ERR_BADARG, "Nil argument");
    const auto *a = static_cast<Dud::Archive *>(archive);
    info->struct_size = sizeof(dudi7_archive_info);
    info->extract_internal = a->ExtractInternal() ? 1 : 0;
    info->entry_count = a->Count();
    info->format_id = a->FormatId().c_str();
    info->dir_separator = a->DirSeparator().c_str();
    return DUDI7_OK;
}

DUD_EXPORT int32_t Dudi7_GetEntry(void *archive, int64_t index, dudi7_entry *entry)
{
    if (archive == nullptr || entry == nullptr)
        return Fail(nullptr, DUDI7_ERR_BADARG, "Nil argument");
    auto *a = static_cast<Dud::Archive *>(archive);
    if (index < 0 || index >= a->Count())
        return Fail(a, DUDI7_ERR_BADARG, "Entry index out of range");
    const Dud::Entry &e = a->Entries()[static_cast<size_t>(index)];
    entry->struct_size = sizeof(dudi7_entry);
    entry->name = e.Name.c_str();
    entry->offset = e.Offset;
    entry->size = e.Size;
    entry->data_x = e.DataX;
    entry->data_y = e.DataY;
    return DUDI7_OK;
}

DUD_EXPORT int32_t Dudi7_Extract(void *archive, int64_t index, dudi7_write_cb write, void *ctx)
{
    if (archive == nullptr || write == nullptr)
        return Fail(nullptr, DUDI7_ERR_BADARG, "Nil argument");
    auto *a = static_cast<Dud::Archive *>(archive);
    try
    {
        if (index < 0 || index >= a->Count())
            return Fail(a, DUDI7_ERR_BADARG, "Entry index out of range");
        return a->Extract(index, Dud::Sink(write, ctx));
    }
    catch (const Dud::Error &e)
    {
        return Fail(a, e.Code(), e.what());
    }
    catch (const Dud::ReadError &e)
    {
        return Fail(a, DUDI7_ERR_IO, e.what());
    }
    catch (const std::exception &e)
    {
        return Fail(a, DUDI7_ERR_GENERIC, e.what());
    }
}

DUD_EXPORT int32_t Dudi7_Close(void *archive)
{
    delete static_cast<Dud::Archive *>(archive);
    return DUDI7_OK;
}
