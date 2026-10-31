// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/ThumbnailWorker.h"

#include "Core/DdsDecoder.h"
#include "Core/PreviewWorker.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace Dup::Core
{
namespace
{
auto ExtensionOf(std::string_view name) -> std::string
{
    const auto dot = name.rfind('.');
    if (dot == std::string_view::npos)
    {
        return {};
    }
    std::string extension(name.substr(dot + 1));
    for (auto &c : extension)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return extension;
}
} // namespace

auto HasThumbnail(std::string_view fileName) -> bool
{
    const auto extension = ExtensionOf(fileName);
    return extension == "tga" || extension == "dds" || extension == "png" || extension == "jpg" || extension == "jpeg" || extension == "bmp" ||
           extension == "gif" || extension == "webp" || extension == "ogf" || extension == "oaf" || HasTextureExtension(fileName);
}

auto Downscale(const DecodedImage &image, std::uint32_t maxSide) -> DecodedImage
{
    if (image.Width == 0 || image.Height == 0 || (image.Width <= maxSide && image.Height <= maxSide))
    {
        return image;
    }
    const double scale = static_cast<double>(maxSide) / std::max(image.Width, image.Height);
    DecodedImage result;
    result.Width = std::max<std::uint32_t>(1, static_cast<std::uint32_t>(image.Width * scale));
    result.Height = std::max<std::uint32_t>(1, static_cast<std::uint32_t>(image.Height * scale));
    result.Rgba.resize(static_cast<std::size_t>(result.Width) * result.Height * 4);
    for (std::uint32_t y = 0; y < result.Height; ++y)
    {
        const std::uint32_t y0 = static_cast<std::uint32_t>(static_cast<std::uint64_t>(y) * image.Height / result.Height);
        const std::uint32_t y1 = std::max(y0 + 1, static_cast<std::uint32_t>(static_cast<std::uint64_t>(y + 1) * image.Height / result.Height));
        for (std::uint32_t x = 0; x < result.Width; ++x)
        {
            const std::uint32_t x0 = static_cast<std::uint32_t>(static_cast<std::uint64_t>(x) * image.Width / result.Width);
            const std::uint32_t x1 = std::max(x0 + 1, static_cast<std::uint32_t>(static_cast<std::uint64_t>(x + 1) * image.Width / result.Width));
            std::uint64_t sum[4] = {0, 0, 0, 0};
            for (std::uint32_t sy = y0; sy < y1; ++sy)
            {
                for (std::uint32_t sx = x0; sx < x1; ++sx)
                {
                    const std::size_t at = (static_cast<std::size_t>(sy) * image.Width + sx) * 4;
                    // premultiplied by alpha so that transparent pixels do not darken the edges
                    const unsigned alpha = image.Rgba[at + 3];
                    sum[0] += image.Rgba[at] * alpha;
                    sum[1] += image.Rgba[at + 1] * alpha;
                    sum[2] += image.Rgba[at + 2] * alpha;
                    sum[3] += alpha;
                }
            }
            const std::uint64_t count = static_cast<std::uint64_t>(y1 - y0) * (x1 - x0);
            std::uint8_t *out = &result.Rgba[(static_cast<std::size_t>(y) * result.Width + x) * 4];
            if (sum[3] == 0)
            {
                out[0] = out[1] = out[2] = out[3] = 0;
            }
            else
            {
                out[0] = static_cast<std::uint8_t>(sum[0] / sum[3]);
                out[1] = static_cast<std::uint8_t>(sum[1] / sum[3]);
                out[2] = static_cast<std::uint8_t>(sum[2] / sum[3]);
                out[3] = static_cast<std::uint8_t>(sum[3] / count);
            }
        }
    }
    return result;
}

ThumbnailWorker::ThumbnailWorker(std::shared_ptr<const ArchiveSource> source, std::shared_ptr<const ArchiveIndex> index,
                                 std::string formatId, std::uint32_t maxSide, ResultCallback callback)
    : _source(std::move(source)), _index(std::move(index)), _formatId(std::move(formatId)), _maxSide(maxSide),
      _callback(std::move(callback))
{
    _thread = std::jthread([this](const std::stop_token &stop) { Run(stop); });
}

ThumbnailWorker::~ThumbnailWorker()
{
    _thread.request_stop();
    _condition.notify_all();
}

void ThumbnailWorker::Request(std::uint32_t entry)
{
    {
        const std::scoped_lock lock(_mutex);
        if (std::find(_pending.begin(), _pending.end(), entry) == _pending.end())
        {
            _pending.push_back(entry);
        }
    }
    _condition.notify_one();
}

void ThumbnailWorker::SetFallbackPalette(std::optional<PaletteRgb> palette)
{
    const std::scoped_lock lock(_paletteMutex);
    _fallbackPalette = std::move(palette);
}

void ThumbnailWorker::ClearPending()
{
    const std::scoped_lock lock(_mutex);
    _pending.clear();
}

void ThumbnailWorker::Run(const std::stop_token &stop)
{
    std::unique_ptr<Dudi7::IArchive> archive;
    PaletteLookup palettes;
    while (true)
    {
        std::uint32_t entry = 0;
        {
            std::unique_lock lock(_mutex);
            _condition.wait(lock, stop, [this] { return !_pending.empty(); });
            if (stop.stop_requested())
            {
                return;
            }
            entry = _pending.back();
            _pending.pop_back();
        }
        ThumbnailResult result;
        result.Entry = entry;
        try
        {
            if (archive == nullptr)
            {
                archive = _source->Open();
            }
            const std::string name(_index->GetFileName(entry));
            std::vector<std::byte> bytes;
            archive->Extract(entry, [&](std::span<const std::byte> piece) {
                bytes.insert(bytes.end(), piece.begin(), piece.end());
                return bytes.size() <= MAX_BYTES;
            });
            if (bytes.size() <= MAX_BYTES)
            {
                const auto info = archive->GetEntry(entry);
                EntryContext context{info.DataX, info.DataY, nullptr};
                if (NeedsPalette({_formatId, name, info.DataX, info.DataY}))
                {
                    std::optional<PaletteRgb> fallback;
                    {
                        const std::scoped_lock lock(_paletteMutex);
                        fallback = _fallbackPalette;
                    }
                    context.Palette = palettes.Get(*archive, fallback);
                }
                auto prepared = PreviewWorker::Prepare(name, _formatId, bytes, static_cast<std::int64_t>(bytes.size()), context);
                result.Extension = prepared.Extension;
                if (prepared.Kind == PreviewKind::IMAGE)
                {
                    result.Image = Downscale(prepared.Image, _maxSide);
                }
                else if (prepared.Kind == PreviewKind::IMAGE_FILE)
                {
                    result.EncodedBytes = std::move(prepared.Bytes);
                }
            }
        }
        catch (const std::exception &)
        {
            archive.reset();
        }
        try
        {
            _callback(std::move(result));
        }
        catch (const std::exception &)
        {
        }
    }
}
} // namespace Dup::Core
