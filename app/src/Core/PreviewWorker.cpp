// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/PreviewWorker.h"

#include "Core/DdsDecoder.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <set>
#include <utility>

namespace Dup::Core
{
namespace
{
auto ExtensionOf(const std::string &name) -> std::string
{
    const auto dot = name.rfind('.');
    if (dot == std::string::npos)
    {
        return {};
    }
    std::string extension = name.substr(dot + 1);
    for (auto &c : extension)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return extension;
}

/// Valid UTF-8 (or plain ASCII) without control characters other than tab, line feeds and escape.
auto LooksLikeText(std::span<const std::byte> bytes) -> bool
{
    if (bytes.empty())
    {
        return false;
    }
    std::size_t i = 0;
    const std::size_t checked = std::min<std::size_t>(bytes.size(), 8192);
    while (i < checked)
    {
        const auto c = static_cast<unsigned char>(bytes[i]);
        if (c < 0x80)
        {
            if (c < 0x20 && c != '\t' && c != '\n' && c != '\r' && c != 0x1b && c != '\f')
            {
                return false;
            }
            ++i;
            continue;
        }
        const std::size_t extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
        if (extra == 0 || (c >= 0xF8))
        {
            return false;
        }
        if (i + extra >= checked) // a character cut by the limit of the check is not an error
        {
            break;
        }
        for (std::size_t k = 1; k <= extra; ++k)
        {
            if ((static_cast<unsigned char>(bytes[i + k]) & 0xC0) != 0x80)
            {
                return false;
            }
        }
        i += extra + 1;
    }
    return true;
}

auto HexDump(std::span<const std::byte> bytes) -> std::string
{
    std::string text;
    char line[96];
    for (std::size_t offset = 0; offset < bytes.size(); offset += 16)
    {
        int length = std::snprintf(line, sizeof(line), "%08zx  ", offset);
        std::string ascii;
        for (std::size_t i = 0; i < 16; ++i)
        {
            if (offset + i < bytes.size())
            {
                const auto b = static_cast<unsigned>(bytes[offset + i]);
                length += std::snprintf(line + length, sizeof(line) - static_cast<std::size_t>(length), "%02x ", b);
                ascii += (b >= 0x20 && b < 0x7f) ? static_cast<char>(b) : '.';
            }
            else
            {
                length += std::snprintf(line + length, sizeof(line) - static_cast<std::size_t>(length), "   ");
            }
            if (i == 7)
            {
                length += std::snprintf(line + length, sizeof(line) - static_cast<std::size_t>(length), " ");
            }
        }
        text += line;
        text += " |" + ascii + "|\n";
    }
    return text;
}

constexpr std::size_t MAX_PALETTE_ENTRY_BYTES = 65536;
const std::set<std::string> IMAGE_FILE_EXTENSIONS{"png", "jpg", "jpeg", "bmp", "gif", "ppm", "webp"};
const std::set<std::string> AUDIO_EXTENSIONS{"wav", "mp3", "ogg", "flac", "oga", "opus"};
const std::set<std::string> TEXT_EXTENSIONS{"txt", "json", "xml", "ini", "cfg", "conf", "lua", "c", "h", "cpp", "hpp", "cs", "csv", "md", "yml", "yaml",
                                            "html", "htm", "css", "js", "py", "shader", "fnt", "obj", "mtl", "log", "bat", "sh", "toml", "def", "lst"};
} // namespace

auto PreviewWorker::Prepare(const std::string &fileName, const std::string &formatId, std::span<const std::byte> bytes, std::int64_t entrySize,
                            const EntryContext &context) -> PreviewResult
{
    PreviewResult result;
    result.EntrySize = entrySize;
    result.Extension = ExtensionOf(fileName);
    const std::string &extension = result.Extension;
    const bool truncated = static_cast<std::int64_t>(bytes.size()) < entrySize;

    const TextureEntry texture{formatId, fileName, context.DataX, context.DataY};
    if (IsTextureEntry(texture))
    {
        if (NeedsPalette(texture) && context.Palette == nullptr)
        {
            result.Message = "this picture needs a palette: choose one in the options (the archive has no PALETTE.DAT or PALETTE.LMP)";
        }
        else if (auto image = DecodeTexture(texture, bytes, context.Palette))
        {
            result.Kind = PreviewKind::IMAGE;
            result.Image = std::move(*image);
            return result;
        }
        else
        {
            result.Message = truncated ? "the image is too large to preview" : "cannot decode this texture";
        }
    }
    else if (extension == "tga")
    {
        if (auto image = DecodeTga(bytes, context.Palette))
        {
            result.Kind = PreviewKind::IMAGE;
            result.Image = std::move(*image);
            return result;
        }
        result.Message = truncated ? "the image is too large to preview" : "cannot decode this TGA file";
    }
    else if (extension == "dds")
    {
        auto dds = DecodeDds(bytes);
        if (dds.Image)
        {
            result.Kind = PreviewKind::IMAGE;
            result.Image = std::move(*dds.Image);
            // the rows of the DDS files made from Unity textures are stored bottom first
            if (formatId.rfind("Unity", 0) == 0)
            {
                FlipRows(result.Image);
            }
            return result;
        }
        result.Message = truncated ? "the image is too large to preview" : dds.Message;
    }
    else if (extension == "obj" && !truncated)
    {
        if (auto mesh = ParseObj(std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size())))
        {
            result.Kind = PreviewKind::MESH;
            result.Geometry = std::make_shared<const Mesh>(std::move(*mesh));
            return result;
        }
        // not a usable mesh: shown as text below
        if (!bytes.empty() && LooksLikeText(bytes))
        {
            result.Kind = PreviewKind::TEXT;
            result.Text.assign(reinterpret_cast<const char *>(bytes.data()), bytes.size());
            result.Message = "no triangles in this OBJ file";
            return result;
        }
    }
    else if (IMAGE_FILE_EXTENSIONS.contains(extension) && !truncated)
    {
        result.Kind = PreviewKind::IMAGE_FILE;
        result.Bytes.assign(bytes.begin(), bytes.end());
        return result;
    }
    else if (AUDIO_EXTENSIONS.contains(extension) && !truncated)
    {
        result.Kind = PreviewKind::AUDIO;
        result.Bytes.assign(bytes.begin(), bytes.end());
        return result;
    }
    else if (!bytes.empty() && (TEXT_EXTENSIONS.contains(extension) || LooksLikeText(bytes)) && LooksLikeText(bytes))
    {
        result.Kind = PreviewKind::TEXT;
        result.Text.assign(reinterpret_cast<const char *>(bytes.data()), bytes.size()); // already limited by the reader
        if (truncated)
        {
            result.Message = "only the beginning is shown";
        }
        return result;
    }

    if (bytes.empty())
    {
        result.Kind = PreviewKind::NONE;
        result.Message = "empty";
        return result;
    }
    // anything else: a hex dump of the beginning
    result.Kind = PreviewKind::HEX;
    result.Text = HexDump(bytes.first(std::min(bytes.size(), HEX_BYTES)));
    if (!result.Message.empty())
    {
        result.Message += "; ";
    }
    result.Message += "hex dump of the first " + std::to_string(std::min(bytes.size(), HEX_BYTES)) + " bytes";
    return result;
}

PreviewWorker::PreviewWorker(ResultCallback callback) : _callback(std::move(callback))
{
    _thread = std::jthread([this](const std::stop_token &stop) { Run(stop); });
}

PreviewWorker::~PreviewWorker()
{
    _thread.request_stop();
    _condition.notify_all();
}

void PreviewWorker::SetFallbackPalette(std::optional<PaletteRgb> palette)
{
    const std::scoped_lock lock(_paletteMutex);
    _fallbackPalette = std::move(palette);
}

void PreviewWorker::SetLimits(std::size_t imageBytes, std::size_t textBytes)
{
    _imageLimit = imageBytes;
    _textLimit = textBytes;
}

void PreviewWorker::Submit(PreviewRequest request)
{
    {
        const std::scoped_lock lock(_mutex);
        _pending = std::move(request);
    }
    _condition.notify_one();
}

void PreviewWorker::Run(const std::stop_token &stop)
{
    std::unique_ptr<Dudi7::IArchive> archive;
    const ArchiveSource *archiveSource = nullptr;
    PaletteLookup palettes;
    while (true)
    {
        PreviewRequest request;
        {
            std::unique_lock lock(_mutex);
            _condition.wait(lock, stop, [this] { return _pending.has_value(); });
            if (stop.stop_requested())
            {
                return;
            }
            request = std::move(*_pending);
            _pending.reset();
        }
        PreviewResult result;
        try
        {
            if (archive == nullptr || archiveSource != request.Source.get())
            {
                archive.reset();
                archive = request.Source->Open();
                archiveSource = request.Source.get();
                palettes.Reset();
            }
            const auto info = archive->GetEntry(request.Entry);
            const std::string extension = ExtensionOf(request.FileName);
            std::size_t limit = TEXT_EXTENSIONS.contains(extension) ? _textLimit.load() : HEX_BYTES;
            if (extension == "tga" || extension == "dds" || extension == "obj" || IMAGE_FILE_EXTENSIONS.contains(extension) || HasTextureExtension(request.FileName))
            {
                limit = _imageLimit.load();
            }
            else if (AUDIO_EXTENSIONS.contains(extension))
            {
                limit = MAX_AUDIO_BYTES;
            }
            // read at most `limit` bytes: a sink that says no ends the extraction early
            std::vector<std::byte> bytes;
            archive->Extract(request.Entry, [&](std::span<const std::byte> piece) {
                const std::size_t room = limit > bytes.size() ? limit - bytes.size() : 0;
                bytes.insert(bytes.end(), piece.begin(), piece.begin() + static_cast<std::ptrdiff_t>(std::min(room, piece.size())));
                return bytes.size() < limit;
            });
            EntryContext context{info.DataX, info.DataY, nullptr};
            if (IsTextureEntry({request.FormatId, request.FileName, info.DataX, info.DataY}) && NeedsPalette({request.FormatId, request.FileName, info.DataX, info.DataY}))
            {
                std::optional<PaletteRgb> fallback;
                {
                    const std::scoped_lock lock(_paletteMutex);
                    fallback = _fallbackPalette;
                }
                context.Palette = palettes.Get(*archive, fallback);
            }
            // the palette the user chose for the colour mapped pictures (an entry of this archive)
            std::optional<PaletteRgb> chosen;
            if (request.PaletteEntry >= 0 && extension == "tga")
            {
                try
                {
                    std::vector<std::byte> paletteBytes;
                    archive->Extract(request.PaletteEntry, [&](std::span<const std::byte> piece) {
                        paletteBytes.insert(paletteBytes.end(), piece.begin(), piece.end());
                        return paletteBytes.size() < MAX_PALETTE_ENTRY_BYTES;
                    });
                    chosen = ParsePalette(paletteBytes);
                }
                catch (const std::exception &error)
                {
                    spdlog::warn("cannot read the palette entry {}: {}", request.PaletteEntry, error.what());
                }
                if (chosen && context.Palette == nullptr)
                {
                    context.Palette = &*chosen;
                }
            }
            result = Prepare(request.FileName, request.FormatId, bytes, info.Size, context);
            result.Detected = IdentifyFile(std::span<const std::byte>(bytes).first(std::min(bytes.size(), FILE_TYPE_HEAD_BYTES)), bytes.size() < FILE_TYPE_HEAD_BYTES ? static_cast<std::int64_t>(bytes.size()) : info.Size, request.FileName);
        }
        catch (const std::exception &error)
        {
            result.Kind = PreviewKind::ERROR_MESSAGE;
            result.Message = error.what();
            archive.reset();
        }
        result.Generation = request.Generation;
        try
        {
            _callback(std::move(result));
        }
        catch (const std::exception &error)
        {
            spdlog::error("preview callback failed: {}", error.what());
        }
    }
}
} // namespace Dup::Core
