// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/ArchiveSource.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace Dup::Core
{
/// A file found inside another file.
struct RippedFile
{
    std::int64_t Offset{0};
    std::int64_t Size{0};
    std::string Type;      // "PNG", "JPEG"...
    std::string Extension; // "png"...
};

struct SignatureInfo
{
    std::string Type;
    std::string Extension;
    std::string Description;
    /// Kinds of finds, to select the types by what they are (RIPPER_AUDIO | RIPPER_VIDEO | RIPPER_IMAGE; 0 = other).
    unsigned Kinds{0};
    /// Short or common magic numbers: many false positives (v5 excluded them on request).
    bool FalsePositives{false};
};

constexpr unsigned RIPPER_AUDIO = 1;
constexpr unsigned RIPPER_VIDEO = 2;
constexpr unsigned RIPPER_IMAGE = 4;

/// What the MP3 and AAC finders accept (the options of v5 for the MPEG audio).
struct MpegOptions
{
    /// Which layers are looked for in each version: [version: MPEG-1, MPEG-2, MPEG-2.5][layer: I, II, III]. Layer I is off
    /// by default (v5 had it on): it matches noise.
    bool Layers[3][3]{{false, true, true}, {false, false, true}, {false, false, true}};
    bool LimitFramesMin{true};
    std::uint32_t FramesMin{20};
    bool LimitFramesMax{false};
    std::uint32_t FramesMax{30000};
    bool LimitSizeMin{true};
    std::int64_t SizeMin{2048};
    bool LimitSizeMax{false};
    std::int64_t SizeMax{INT32_MAX};
    bool XingVbr{true}; ///< trust the size of the Xing header of a VBR file
    bool Id3Tag{true};  ///< include the ID3 tags (v2 in front, v1 at the end)
};

/// Random access to the scanned file for the code that finds where a file ends.
class RipReader
{
  public:
    virtual ~RipReader() = default;
    [[nodiscard]] virtual auto Size() const -> std::int64_t = 0;
    /// Reads up to length bytes at offset; returns how many were read.
    virtual auto Read(std::int64_t offset, void *buffer, std::size_t length) const -> std::size_t = 0;
};

/// The signature scanner of v5 ("HyperRipper"): finds files embedded in any file by their magic numbers and works out where each
/// one ends from its own structure (PNG chunks, JPEG segments, RIFF size...). No Qt here.
class HyperRipper
{
  public:
    struct Options
    {
        std::set<std::string> Types;  // empty = every known type
        std::int64_t MinSize{64};     // smaller finds are probably false positives
        bool SkipInsideFound{true};   // continue after the end of a found file
        MpegOptions Mpeg;
        std::size_t BlockSize{4U * 1024 * 1024}; // bytes read at once (between 4 KiB and 256 MiB: only the speed changes)
    };

    /// The types that can be found.
    [[nodiscard]] static auto Signatures() -> std::vector<SignatureInfo>;

    /// Scans the file; progress(bytesDone, bytesTotal) and the stop token are checked about every megabyte.
    [[nodiscard]] static auto Scan(const RipReader &reader, const Options &options, const std::function<void(std::int64_t, std::int64_t)> &progress,
                                   const std::stop_token &stop) -> std::vector<RippedFile>;
    [[nodiscard]] static auto Scan(const std::filesystem::path &file, const Options &options) -> std::vector<RippedFile>;
    /// Every known type, with the default options.
    [[nodiscard]] static auto Scan(const std::filesystem::path &file) -> std::vector<RippedFile>;
};

/// The files found, as an archive of the original file (entry names like "00012_0x0001a2b3.png").
class RippedArchiveSource : public ArchiveSource
{
  public:
    /// Default names of v5 ("file_ext-0000000001.png").
    RippedArchiveSource(std::filesystem::path file, std::vector<RippedFile> files);
    /// With names chosen by the caller (RipNaming.h, or the ones of an HRF file), one per find; "\\" between folders.
    RippedArchiveSource(std::filesystem::path file, std::vector<RippedFile> files, std::vector<std::string> names)
        : _file(std::move(file)), _files(std::make_shared<const std::vector<RippedFile>>(std::move(files))), _names(std::make_shared<const std::vector<std::string>>(std::move(names)))
    {
    }
    [[nodiscard]] auto Open() const -> std::unique_ptr<Dudi7::IArchive> override;
    [[nodiscard]] auto GetTitle() const -> std::string override
    {
        return _file.filename().string() + " (HyperRipper)";
    }
    [[nodiscard]] auto GetPath() const -> std::filesystem::path override
    {
        return {}; // not a file that can be opened again by name
    }
    [[nodiscard]] auto GetFiles() const -> const std::vector<RippedFile> &
    {
        return *_files;
    }
    [[nodiscard]] auto GetNames() const -> const std::vector<std::string> &
    {
        return *_names;
    }
    [[nodiscard]] auto GetScannedFile() const -> const std::filesystem::path &
    {
        return _file;
    }

  private:
    std::filesystem::path _file;
    std::shared_ptr<const std::vector<RippedFile>> _files;
    std::shared_ptr<const std::vector<std::string>> _names;
};

/// A scan on its own thread, for the window.
class HyperRipperJob
{
  public:
    HyperRipperJob(std::filesystem::path file, HyperRipper::Options options);
    ~HyperRipperJob();
    HyperRipperJob(const HyperRipperJob &) = delete;
    auto operator=(const HyperRipperJob &) -> HyperRipperJob & = delete;

    void Start();
    void Cancel();
    [[nodiscard]] auto IsDone() const -> bool
    {
        return _done;
    }
    [[nodiscard]] auto WasCancelled() const -> bool
    {
        return _cancelled;
    }
    [[nodiscard]] auto GetBytesDone() const -> std::int64_t
    {
        return _bytesDone;
    }
    [[nodiscard]] auto GetBytesTotal() const -> std::int64_t
    {
        return _bytesTotal;
    }
    /// Valid once IsDone() and not cancelled; empty when the file could not be read (see GetError()).
    [[nodiscard]] auto GetResults() const -> std::vector<RippedFile>;
    [[nodiscard]] auto GetError() const -> std::string;
    [[nodiscard]] auto GetFile() const -> const std::filesystem::path &
    {
        return _file;
    }

  private:
    std::filesystem::path _file;
    HyperRipper::Options _options;
    std::atomic<bool> _done{false};
    std::atomic<bool> _cancelled{false};
    std::atomic<std::int64_t> _bytesDone{0};
    std::atomic<std::int64_t> _bytesTotal{0};
    mutable std::mutex _mutex;
    std::vector<RippedFile> _results;
    std::string _error;
    std::jthread _thread;
};
} // namespace Dup::Core
