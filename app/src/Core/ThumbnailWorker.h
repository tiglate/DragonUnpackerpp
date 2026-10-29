// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/ArchiveIndex.h"
#include "Core/TextureConverters.h"
#include "Core/ArchiveSource.h"
#include "Core/TgaDecoder.h"
#include "Dudi7/Driver.h"

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Dup::Core
{
/// Area averaging reduction so that neither side exceeds maxSide (the image is returned unchanged when it already fits).
[[nodiscard]] auto Downscale(const DecodedImage &image, std::uint32_t maxSide) -> DecodedImage;

/// Can the entry have a thumbnail (an image the preview code decodes: tga, dds, png, jpg, bmp, gif)?
[[nodiscard]] auto HasThumbnail(std::string_view fileName) -> bool;

struct ThumbnailResult
{
    std::uint64_t Generation{0};
    std::uint32_t Entry{0};
    DecodedImage Image; // empty (Width = 0) when the entry could not be decoded
    std::vector<std::byte> EncodedBytes; // png/jpg/... files that the interface decodes (Image is empty then)
    std::string Extension;
};

/// Makes thumbnails on its own thread with its own archive handle. Requests are served newest first (what is visible now
/// matters more than what was visible a moment ago). The callback runs on the worker thread. No Qt here.
class ThumbnailWorker
{
  public:
    using ResultCallback = std::function<void(ThumbnailResult)>;

    ThumbnailWorker(std::shared_ptr<const ArchiveSource> source, std::shared_ptr<const ArchiveIndex> index, std::string formatId,
                    std::uint32_t maxSide, ResultCallback callback);
    ~ThumbnailWorker();
    ThumbnailWorker(const ThumbnailWorker &) = delete;
    auto operator=(const ThumbnailWorker &) -> ThumbnailWorker & = delete;

    void Request(std::uint32_t entry);
    /// Forgets the requests that did not start (the visible items changed).
    void ClearPending();
    /// The palette for 8 bits pictures when the archive has none.
    void SetFallbackPalette(std::optional<PaletteRgb> palette);

    static constexpr std::size_t MAX_BYTES = 32U * 1024 * 1024;

  private:
    void Run(const std::stop_token &stop);
    std::mutex _paletteMutex;
    std::optional<PaletteRgb> _fallbackPalette;

    std::shared_ptr<const ArchiveSource> _source;
    std::shared_ptr<const ArchiveIndex> _index;
    std::string _formatId;
    std::uint32_t _maxSide;
    ResultCallback _callback;
    std::mutex _mutex;
    std::condition_variable_any _condition;
    std::vector<std::uint32_t> _pending; // a stack
    std::jthread _thread;
};
} // namespace Dup::Core
