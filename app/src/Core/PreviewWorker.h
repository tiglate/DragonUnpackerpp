// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/ArchiveSource.h"
#include "Core/ObjParser.h"
#include "Core/OofRender.h"
#include "Core/RigModel.h"
#include "Core/FileTypes.h"
#include "Core/TextureConverters.h"
#include "Core/TgaDecoder.h"
#include "Dudi7/Driver.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace Dup::Core
{
enum class PreviewKind
{
    NONE,
    IMAGE,      // Image holds the pixels
    IMAGE_FILE, // Bytes hold an image file that the interface decodes (png, jpg, bmp, gif)
    TEXT,
    MESH, // Mesh holds the geometry of an OBJ file
    MODEL, // Scene holds a Descent 3 model (textures, pose and effects are drawn by the viewer)
    RIG,   // Rig holds a skinned model with animations (glTF of the Requiem models), RigTexture its picture if found
    AUDIO, // Bytes hold the whole sound file (wav, mp3, ogg...)
    HEX,   // Text holds a hex dump of the first bytes
    ERROR_MESSAGE
};

struct PreviewRequest
{
    std::uint64_t Generation{0};
    std::shared_ptr<const ArchiveSource> Source;
    std::int64_t Entry{0};
    std::string FileName;
    std::string FormatId; // of the archive
    /// An entry of the archive that holds a palette (a .pal file): the colour mapped pictures (the tiles of the console
    /// ROMs) are shown with its colours instead of their own. -1: none.
    std::int64_t PaletteEntry{-1};
};

struct PreviewResult
{
    std::uint64_t Generation{0};
    PreviewKind Kind{PreviewKind::NONE};
    std::string Extension; // lower case, without the dot
    std::string Message;   // a short note ("truncated", why there is no preview...)
    std::string Text;      // TEXT and HEX
    DecodedImage Image;
    std::shared_ptr<const Mesh> Geometry;
    std::shared_ptr<const OofScene> Scene;
    std::shared_ptr<const RigScene> Rig;
    std::shared_ptr<const DecodedImage> RigTexture;
    std::vector<std::byte> Bytes;
    std::int64_t EntrySize{0};
    /// What the first bytes of the entry say (FileTypes.h); set by the worker, not by Prepare.
    std::optional<DetectedType> Detected;
};

/// What the driver keeps for an entry and the palette of the archive, for the pictures that depend on them (TextureConverters.h).
struct EntryContext
{
    std::int64_t DataX{0};
    std::int64_t DataY{0};
    const PaletteRgb *Palette{nullptr};
    /// Finds the picture of a model texture by its name in the archive that the entry is in (empty: textures are not looked for).
    OofTextureResolver Textures;
    /// Finds the picture of a rig model by its slot in the art archive of the game (empty: not looked for).
    std::function<std::shared_ptr<const DecodedImage>(int)> RigTexture;
};

/// Prepares the preview of one entry at a time on its own thread, with its own archive handle: the newest request wins
/// (older ones that did not start are dropped), and the answer carries the generation of its request so the caller can
/// ignore stale ones. The callback runs on the worker thread. No Qt here.
class PreviewWorker
{
  public:
    using ResultCallback = std::function<void(PreviewResult)>;

    explicit PreviewWorker(ResultCallback callback);
    ~PreviewWorker();
    PreviewWorker(const PreviewWorker &) = delete;
    auto operator=(const PreviewWorker &) -> PreviewWorker & = delete;

    void Submit(PreviewRequest request);
    /// The largest image and text read for a preview (applies to the next request).
    void SetLimits(std::size_t imageBytes, std::size_t textBytes);
    /// The palette for 8 bits pictures when the archive has none (applies to the next request).
    void SetFallbackPalette(std::optional<PaletteRgb> palette);

    /// What the worker does with the bytes of an entry; exposed for the tests.
    [[nodiscard]] static auto Prepare(const std::string &fileName, const std::string &formatId, std::span<const std::byte> bytes, std::int64_t entrySize,
                                        const EntryContext &context = {}) -> PreviewResult;

    static constexpr std::size_t MAX_IMAGE_BYTES = 64U * 1024 * 1024;
    static constexpr std::size_t MAX_AUDIO_BYTES = 128U * 1024 * 1024;
    static constexpr std::size_t MAX_TEXT_BYTES = 1U * 1024 * 1024;
    static constexpr std::size_t HEX_BYTES = 16U * 1024;

  private:
    void Run(const std::stop_token &stop);

    ResultCallback _callback;
    std::atomic<std::size_t> _imageLimit{MAX_IMAGE_BYTES};
    std::atomic<std::size_t> _textLimit{MAX_TEXT_BYTES};
    std::mutex _paletteMutex;
    std::optional<PaletteRgb> _fallbackPalette;
    std::mutex _mutex;
    std::condition_variable_any _condition;
    std::optional<PreviewRequest> _pending;
    std::jthread _thread;
};
} // namespace Dup::Core
