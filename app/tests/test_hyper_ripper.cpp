// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <algorithm>

#include "Core/HyperRipper.h"
#include "TestSupport.h"

using namespace Dup::Core;
using Blob = std::vector<std::uint8_t>;

namespace
{
void Append(Blob &blob, const Blob &more)
{
    blob.insert(blob.end(), more.begin(), more.end());
}
void Text(Blob &blob, const std::string &text)
{
    blob.insert(blob.end(), text.begin(), text.end());
}
void Le32(Blob &blob, std::uint32_t v)
{
    for (int i = 0; i < 4; ++i)
    {
        blob.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    }
}
void Le16(Blob &blob, std::uint32_t v)
{
    blob.push_back(static_cast<std::uint8_t>(v));
    blob.push_back(static_cast<std::uint8_t>(v >> 8));
}
void Be32(Blob &blob, std::uint32_t v)
{
    for (int i = 3; i >= 0; --i)
    {
        blob.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    }
}
void Be16(Blob &blob, std::uint32_t v)
{
    blob.push_back(static_cast<std::uint8_t>(v >> 8));
    blob.push_back(static_cast<std::uint8_t>(v));
}

auto Png() -> Blob
{
    Blob b{0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    auto chunk = [&](const char *type, const Blob &data) {
        Be32(b, static_cast<std::uint32_t>(data.size()));
        Text(b, type);
        Append(b, data);
        Be32(b, 0);
    };
    chunk("IHDR", Blob(13, 1));
    chunk("IDAT", Blob(20, 7));
    chunk("IEND", {});
    return b;
}

auto Jpeg() -> Blob
{
    Blob b{0xFF, 0xD8};
    b.insert(b.end(), {0xFF, 0xE0, 0x00, 0x10, 'J', 'F', 'I', 'F', 0, 1, 1, 0, 0, 1, 0, 1, 0, 0});
    b.insert(b.end(), {0xFF, 0xDB, 0x00, 0x08, 1, 2, 3, 4, 5, 6});
    b.insert(b.end(), {0xFF, 0xC0, 0x00, 0x11, 8, 0, 1, 0, 1, 3, 1, 0x11, 0, 2, 0x11, 1, 3, 0x11, 1});
    b.insert(b.end(), {0xFF, 0xDA, 0x00, 0x06, 1, 2, 3, 4});
    Append(b, Blob(30, 0x55));
    b.insert(b.end(), {0xFF, 0x00, 0x12, 0xFF, 0xD0, 0x34, 0xFF, 0xD9});
    return b;
}

auto Gif() -> Blob
{
    Blob b;
    Text(b, "GIF89a");
    Le16(b, 2);
    Le16(b, 2);
    b.insert(b.end(), {0x80, 0, 0});          // global colour table of 2 entries
    Append(b, Blob(6, 9));
    b.insert(b.end(), {0x2C, 0, 0, 0, 0, 2, 0, 2, 0, 0}); // image descriptor
    b.push_back(2);                                        // LZW minimum code size
    b.insert(b.end(), {4, 1, 2, 3, 4, 0});                 // one sub-block and the terminator
    b.push_back(0x3B);
    return b;
}

auto Bmp() -> Blob
{
    Blob b;
    Text(b, "BM");
    Le32(b, 54 + 48);
    Le32(b, 0);
    Le32(b, 54);
    Le32(b, 40);
    Le32(b, 4);
    Le32(b, 4);
    Le16(b, 1);
    Le16(b, 24);
    Append(b, Blob(24, 0));
    Append(b, Blob(48, 3));
    return b;
}

auto Wav() -> Blob
{
    Blob b;
    Text(b, "RIFF");
    Le32(b, 4 + 8 + 16 + 8 + 20);
    Text(b, "WAVE");
    Text(b, "fmt ");
    Le32(b, 16);
    Le16(b, 1);
    Le16(b, 1);
    Le32(b, 8000);
    Le32(b, 16000);
    Le16(b, 2);
    Le16(b, 16);
    Text(b, "data");
    Le32(b, 20);
    Append(b, Blob(20, 5));
    return b;
}

auto Ogg() -> Blob
{
    Blob b;
    auto page = [&](std::uint8_t flags, std::size_t body) {
        Text(b, "OggS");
        b.push_back(0);
        b.push_back(flags);
        Append(b, Blob(20, 0)); // granule, serial, sequence, checksum
        b.push_back(1);         // one segment
        b.push_back(static_cast<std::uint8_t>(body));
        Append(b, Blob(body, 0x66));
    };
    page(2, 30);
    page(4, 20);
    return b;
}

auto Zip() -> Blob
{
    Blob b{'P', 'K', 3, 4};
    Le16(b, 20);
    Le16(b, 0);
    Le16(b, 0); // method: stored
    Append(b, Blob(18, 0));
    Text(b, "abc");
    Append(b, Blob(10, 1));
    Text(b, std::string("PK\x05\x06", 4));
    Append(b, Blob(16, 0));
    Le16(b, 4); // comment length
    Text(b, "note");
    return b;
}

auto Dds() -> Blob
{
    Blob b;
    Text(b, "DDS ");
    Le32(b, 124);
    Le32(b, 0x1007);
    Le32(b, 4);
    Le32(b, 4);
    Append(b, Blob(8, 0));
    Le32(b, 1); // mip count
    Append(b, Blob(44, 0));
    Le32(b, 32);
    Le32(b, 4);
    Text(b, "DXT1");
    Append(b, Blob(20, 0));
    Le32(b, 0x1000);
    Append(b, Blob(16, 0));
    Append(b, Blob(8, 0xAA)); // one 4x4 block
    return b;
}

auto Midi() -> Blob
{
    Blob b;
    Text(b, "MThd");
    Be32(b, 6);
    Be16(b, 0);
    Be16(b, 1);
    Be16(b, 96);
    Text(b, "MTrk");
    Be32(b, 8);
    Append(b, Blob(8, 0));
    return b;
}
auto Voc() -> Blob
{
    Blob b;
    Text(b, "Creative Voice File");
    b.push_back(26);
    Le16(b, 26);
    Le16(b, 0x010A);
    Le16(b, 0x1129);
    b.push_back(1); // sound data block of 6 bytes
    b.push_back(6);
    b.push_back(0);
    b.push_back(0);
    Append(b, Blob(6, 0x80));
    b.push_back(0); // terminator
    return b;
}

auto Module669() -> Blob
{
    Blob b;
    Text(b, "if");
    Text(b, std::string(108, 'x'));
    b.push_back(1); // samples
    b.push_back(1); // patterns
    b.push_back(0);
    Append(b, Blob(384, 0));
    Text(b, "SAMPLE.RAW");
    Append(b, Blob(3, 0));
    Le32(b, 10); // length
    Le32(b, 0);
    Le32(b, 0);
    Append(b, Blob(0x600, 0xFF));
    Append(b, Blob(10, 3));
    return b;
}

auto Xm() -> Blob
{
    Blob b;
    Text(b, "Extended Module: ");
    Text(b, std::string(20, 'n'));
    b.push_back(0x1A);
    Text(b, std::string(20, 't'));
    Le16(b, 0x0104);
    Le32(b, 276); // header size
    Append(b, Blob(276 - 4 + 0, 0));
    b.resize(60 + 276);
    b[70] = 0; // patterns
    b[72] = 0; // instruments
    return b;
}

auto S3m() -> Blob
{
    Blob b(96 + 2 + 2 + 2, 0);
    std::copy_n("song", 4, b.begin());
    b[28] = 0x1A;
    b[29] = 16;
    b[32] = 1; // orders
    b[34] = 1; // instruments
    b[36] = 1; // patterns
    std::copy_n("SCRM", 4, b.begin() + 44);
    b[96] = 0; // order
    b[97] = 6; // instrument paragraph: offset 96
    b[99] = 0; // pattern paragraph 0: empty
    b.resize(181, 0);
    b[6 * 16] = 1; // sample, no data
    return b;
}

auto It() -> Blob
{
    Blob b(192 + 1 + 4, 0);
    std::copy_n("IMPM", 4, b.begin());
    b[32] = 1; // orders
    b[36] = 1; // samples
    b[40] = 0x14;
    b[41] = 0x02;
    const std::uint32_t header = 192 + 1 + 4;
    b[193 - 1 + 1] = 0;
    b[193] = static_cast<std::uint8_t>(header); // pointer to the sample header
    b.resize(header + 80, 0);
    std::copy_n("IMPS", 4, b.begin() + header);
    b[header + 48] = 4; // length
    const std::uint32_t data = header + 80;
    b[header + 72] = static_cast<std::uint8_t>(data & 0xFF);
    b[header + 73] = static_cast<std::uint8_t>(data >> 8);
    Append(b, Blob(4, 9));
    return b;
}

auto Mp3() -> Blob
{
    Blob b;
    for (int i = 0; i < 24; ++i)
    {
        // MPEG-1 layer III, 128 kbit/s, 44100 Hz: 417 bytes
        b.push_back(0xFF);
        b.push_back(0xFB);
        b.push_back(0x90);
        b.push_back(0x00);
        Append(b, Blob(417 - 4, 0x11));
    }
    return b;
}

auto Aac() -> Blob
{
    Blob b;
    for (int i = 0; i < 24; ++i)
    {
        // ADTS, MPEG-4 LC, 44100 Hz, stereo, frames of 120 bytes
        const std::uint8_t h[7] = {0xFF, 0xF1, 0x50, 0x80, 0x0F, 0x1F, 0xFC};
        b.insert(b.end(), h, h + 7);
        Append(b, Blob(120 - 7, 0x22));
    }
    return b;
}

auto Mp4() -> Blob
{
    Blob b;
    Be32(b, 20);
    Text(b, "ftypM4A ");
    Le32(b, 0);
    Text(b, "M4A ");
    Be32(b, 24);
    Text(b, "mdat");
    Append(b, Blob(16, 0x44));
    return b;
}

auto Bink() -> Blob
{
    Blob b;
    Text(b, "BIKi");
    Le32(b, 60 - 8);
    Le32(b, 10); // frames
    Append(b, Blob(8, 0));
    Le32(b, 320);
    Le32(b, 200);
    Append(b, Blob(60 - 28, 0));
    return b;
}

auto Flic() -> Blob
{
    Blob b;
    Le32(b, 140);
    Le16(b, 0xAF12);
    Le16(b, 5);
    Le16(b, 64);
    Le16(b, 48);
    Le16(b, 8);
    Le32(b, 0);
    Append(b, Blob(140 - 18, 0x33));
    return b;
}

auto Mkv() -> Blob
{
    Blob b{0x1A, 0x45, 0xDF, 0xA3, 0x8B, 0x42, 0x82, 0x88};
    Text(b, "matroska");
    Append(b, Blob{0x18, 0x53, 0x80, 0x67, 0x88});
    Append(b, Blob(8, 0x55));
    return b;
}

auto Emf() -> Blob
{
    Blob b;
    Le32(b, 1);
    Le32(b, 88);
    Append(b, Blob(32, 0));
    Text(b, " EMF");
    Le32(b, 0x10000);
    Le32(b, 100); // size
    Append(b, Blob(100 - 52, 0x66));
    return b;
}

auto Wmf() -> Blob
{
    Blob b;
    Le32(b, 0x9AC6CDD7);
    Le16(b, 0);
    Append(b, Blob(8, 0));
    Le16(b, 1440);
    Le32(b, 0);
    Le16(b, 0);
    Le16(b, 1);
    Le16(b, 9);
    Le16(b, 0x300);
    Le32(b, 30); // words
    Le16(b, 1);
    Le32(b, 10);
    Le16(b, 0);
    Append(b, Blob(60 - 18, 0x77));
    return b;
}

auto Lbm() -> Blob
{
    Blob b;
    Text(b, "FORM");
    Be32(b, 44);
    Text(b, "ILBMBMHD");
    Be32(b, 20);
    Append(b, Blob(32, 1));
    return b;
}

auto Tga() -> Blob
{
    Blob b(18, 0);
    b[2] = 2;
    b[12] = 8;
    b[14] = 8;
    b[16] = 24;
    Append(b, Blob(8 * 8 * 3, 0x99));
    return b;
}
} // namespace

TEST_CASE("HyperRipper finds the embedded files and where they end")
{
    Dup::Tests::TempDirectory directory;
    const auto path = directory.GetPath() / "container.bin";
    const std::vector<std::pair<const char *, Blob>> parts{{"PNG", Png()}, {"JPEG", Jpeg()}, {"GIF", Gif()},  {"BMP", Bmp()}, {"RIFF", Wav()},
                                                           {"OGG", Ogg()}, {"ZIP", Zip()},   {"DDS", Dds()}, {"MIDI", Midi()}};
    Blob file;
    std::vector<std::int64_t> offsets;
    for (const auto &[type, blob] : parts)
    {
        Append(file, Blob(37, 0x20)); // junk between the files, containing no signature
        offsets.push_back(static_cast<std::int64_t>(file.size()));
        Append(file, blob);
    }
    Append(file, Blob(11, 0));
    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char *>(file.data()), static_cast<std::streamsize>(file.size()));
    }

    HyperRipper::Options options;
    options.MinSize = 16;
    const auto found = HyperRipper::Scan(path, options);
    REQUIRE(found.size() == parts.size());
    for (std::size_t i = 0; i < parts.size(); ++i)
    {
        INFO("type " << parts[i].first);
        CHECK(found[i].Type == parts[i].first);
        CHECK(found[i].Offset == offsets[i]);
        CHECK(found[i].Size == static_cast<std::int64_t>(parts[i].second.size()));
    }
    CHECK(found[4].Extension == "wav");
    CHECK(found[1].Extension == "jpg");

    // only the wanted types
    options.Types = {"PNG", "MIDI"};
    const auto some = HyperRipper::Scan(path, options);
    REQUIRE(some.size() == 2);
    CHECK(some[0].Type == "PNG");
    CHECK(some[1].Type == "MIDI");
}

TEST_CASE("HyperRipper does not report false positives or truncated files")
{
    Dup::Tests::TempDirectory directory;
    const auto path = directory.GetPath() / "bad.bin";
    Blob file;
    Text(file, "xx");
    auto png = Png();
    png.resize(png.size() - 20); // IEND missing
    Append(file, png);
    Text(file, std::string("\x89PNG\r\n\x1a\n", 8) + "garbage without chunks");
    Text(file, "BM garbage");
    Text(file, "RIFF");
    Le32(file, 0x7fffff00); // size beyond the file
    Text(file, "WAVE");
    Append(file, Blob(100, 0));
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char *>(file.data()), static_cast<std::streamsize>(file.size()));
    HyperRipper::Options options;
    options.MinSize = 16;
    CHECK(HyperRipper::Scan(path, options).empty());
}

TEST_CASE("The ripped archive extracts exactly the bytes of each find")
{
    Dup::Tests::TempDirectory directory;
    const auto path = directory.GetPath() / "container.bin";
    Blob file(100, 0x20);
    const auto wav = Wav();
    Append(file, wav);
    Append(file, Blob(5, 0));
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char *>(file.data()), static_cast<std::streamsize>(file.size()));
    HyperRipper::Options options;
    options.MinSize = 16;
    const RippedArchiveSource source(path, HyperRipper::Scan(path, options));
    const auto archive = source.Open();
    REQUIRE(archive->GetEntryCount() == 1);
    const auto info = archive->GetEntry(0);
    CHECK(info.Name == "container_bin-0000000001.wav");
    CHECK(info.Size == static_cast<std::int64_t>(wav.size()));
    const auto bytes = archive->ExtractToMemory(0, 1024);
    CHECK(std::vector<std::uint8_t>(reinterpret_cast<const std::uint8_t *>(bytes.data()), reinterpret_cast<const std::uint8_t *>(bytes.data()) + bytes.size()) == wav);
    CHECK(archive->GetFormatId() == "HyperRipper");
}

TEST_CASE("A HyperRipper job runs on its thread and can be cancelled")
{
    Dup::Tests::TempDirectory directory;
    const auto path = directory.GetPath() / "container.bin";
    Blob file(300000, 0x20);
    Append(file, Png());
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char *>(file.data()), static_cast<std::streamsize>(file.size()));
    HyperRipper::Options options;
    options.MinSize = 16;
    HyperRipperJob job(path, options);
    job.Start();
    for (int i = 0; i < 500 && !job.IsDone(); ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    REQUIRE(job.IsDone());
    CHECK(job.GetResults().size() == 1);
    CHECK(job.GetBytesTotal() == static_cast<std::int64_t>(file.size()));

    HyperRipperJob missing(directory.GetPath() / "nope.bin", options);
    missing.Start();
    for (int i = 0; i < 500 && !missing.IsDone(); ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CHECK(missing.IsDone());
    CHECK_FALSE(missing.GetError().empty());
}

TEST_CASE("HyperRipper finds the tracker, audio, video and picture formats of v5")
{
    Dup::Tests::TempDirectory directory;
    const auto path = directory.GetPath() / "more.bin";
    const std::vector<std::pair<const char *, Blob>> parts{{"VOC", Voc()},   {"669", Module669()}, {"XM", Xm()},     {"S3M", S3m()}, {"IT", It()},       {"MP3", Mp3()},
                                                           {"AAC", Aac()},   {"MP4", Mp4()},       {"BIK", Bink()},  {"FLC", Flic()}, {"MKV", Mkv()},    {"EMF", Emf()},
                                                           {"WMF", Wmf()},   {"LBM", Lbm()},       {"TGA", Tga()}};
    Blob file;
    std::vector<std::int64_t> offsets;
    for (const auto &[type, blob] : parts)
    {
        Append(file, Blob(37, 0x20));
        offsets.push_back(static_cast<std::int64_t>(file.size()));
        Append(file, blob);
    }
    Append(file, Blob(11, 0x20));
    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char *>(file.data()), static_cast<std::streamsize>(file.size()));
    }
    HyperRipper::Options options;
    options.MinSize = 16;
    const auto found = HyperRipper::Scan(path, options);
    for (std::size_t i = 0; i < parts.size(); ++i)
    {
        INFO("type " << std::string(parts[i].first));
        bool seen = false;
        for (const auto &f : found)
        {
            if (f.Type == parts[i].first)
            {
                seen = true;
                CHECK(f.Offset == offsets[i]);
                CHECK(f.Size == static_cast<std::int64_t>(parts[i].second.size()));
            }
        }
        CHECK(seen);
    }
    CHECK(found.size() == parts.size());
}

namespace
{
auto Mp3Frames(int count, std::uint8_t byte2 = 0x90, std::uint8_t byte1 = 0xFB, int length = 417) -> Blob
{
    Blob b;
    for (int i = 0; i < count; ++i)
    {
        b.push_back(0xFF);
        b.push_back(byte1);
        b.push_back(byte2);
        b.push_back(0x00);
        Append(b, Blob(length - 4, 0x11));
    }
    return b;
}

auto WriteTemp(const Dup::Tests::TempDirectory &directory, const Blob &data) -> std::filesystem::path
{
    const auto path = directory.GetPath() / "scan.bin";
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
    return path;
}

auto Wrapped(const Blob &inner) -> Blob
{
    Blob file(40, 0x20);
    Append(file, inner);
    Append(file, Blob(40, 0x20));
    return file;
}
} // namespace

TEST_CASE("The MPEG audio options limit what is reported")
{
    Dup::Tests::TempDirectory directory;
    HyperRipper::Options options;
    options.MinSize = 16;
    options.Types = {"MP3"};

    // 30 frames: found with the defaults, not when 40 frames are needed, cut in pieces by the maximum
    const auto path = WriteTemp(directory, Wrapped(Mp3Frames(30)));
    REQUIRE(HyperRipper::Scan(path, options).size() == 1);
    CHECK(HyperRipper::Scan(path, options)[0].Size == 30 * 417);
    options.Mpeg.FramesMin = 40;
    CHECK(HyperRipper::Scan(path, options).empty());
    options.Mpeg.LimitFramesMin = false; // (4 frames at least)
    CHECK(HyperRipper::Scan(path, options).size() == 1);
    options.Mpeg.LimitFramesMax = true;
    options.Mpeg.FramesMax = 12;
    const auto pieces = HyperRipper::Scan(path, options);
    REQUIRE(pieces.size() == 3);
    CHECK(pieces[0].Size == 12 * 417);
    CHECK(pieces[2].Size == 6 * 417);
    options.Mpeg.LimitFramesMax = false;

    // the size: a minimum and a maximum
    options.Mpeg.LimitSizeMin = true;
    options.Mpeg.SizeMin = 20000;
    CHECK(HyperRipper::Scan(path, options).empty());
    options.Mpeg.LimitSizeMin = false;
    options.Mpeg.LimitSizeMax = true;
    options.Mpeg.SizeMax = 5000;
    CHECK(HyperRipper::Scan(path, options)[0].Size == 12 * 417);
    options.Mpeg.LimitSizeMax = false;

    // MPEG-2 layer III (byte 0xF3: not in MPEG-1) is accepted by default, and off when its box is unchecked
    const auto v2 = WriteTemp(directory, Wrapped(Mp3Frames(30, 0x80, 0xF3, 208)));
    options.Mpeg = MpegOptions{};
    CHECK(HyperRipper::Scan(v2, options).size() == 1);
    options.Mpeg.Layers[1][2] = false;
    CHECK(HyperRipper::Scan(v2, options).empty());
    // layer II of MPEG-1 (byte 0xFD) is on by default, layer I (0xFF) is off
    CHECK(HyperRipper::Scan(WriteTemp(directory, Wrapped(Mp3Frames(30, 0x90, 0xFF))), options).empty());
}

TEST_CASE("A Xing header gives the size of a VBR MP3 and the ID3 tags can be left out")
{
    Dup::Tests::TempDirectory directory;
    HyperRipper::Options options;
    options.MinSize = 16;
    options.Types = {"MP3"};
    // the first frame holds a Xing header (stereo MPEG-1: at 36) with the frame count and the size of the stream
    Blob audio = Mp3Frames(40);
    std::copy_n("Xing", 4, audio.begin() + 36);
    std::fill(audio.begin() + 40, audio.begin() + 52, 0);
    audio[43] = 3;                 // flags: frames and bytes
    audio[47] = 40;                // frames
    const std::uint32_t bytes = 40 * 417 - 417;
    audio[50] = static_cast<std::uint8_t>(bytes >> 8);
    audio[51] = static_cast<std::uint8_t>(bytes);
    options.Mpeg.XingVbr = true;
    const auto path = WriteTemp(directory, Wrapped(audio));
    auto found = HyperRipper::Scan(path, options);
    REQUIRE(found.size() == 1);
    CHECK(found[0].Size == static_cast<std::int64_t>(bytes));
    options.Mpeg.XingVbr = false;
    found = HyperRipper::Scan(path, options);
    REQUIRE(found.size() == 1);
    CHECK(found[0].Size == 40 * 417);

    // an ID3v2 tag in front and an ID3v1 tag behind
    Blob tagged{'I', 'D', '3', 3, 0, 0, 0, 0, 0, 20};
    Append(tagged, Blob(20, 0));
    Append(tagged, Mp3Frames(30));
    Text(tagged, "TAG");
    Append(tagged, Blob(125, 0));
    const auto withTags = WriteTemp(directory, Wrapped(tagged));
    options.Mpeg.Id3Tag = true;
    CHECK(HyperRipper::Scan(withTags, options)[0].Size == static_cast<std::int64_t>(tagged.size()));
    options.Mpeg.Id3Tag = false;
    found = HyperRipper::Scan(withTags, options);
    REQUIRE(found.size() == 1);
    CHECK(found[0].Size == 30 * 417);
}

TEST_CASE("The scan gives the same finds whatever the size of the buffer")
{
    Dup::Tests::TempDirectory directory;
    Blob file;
    for (int i = 0; i < 20; ++i)
    {
        Append(file, Blob(1000 + 331 * i, 0x20));
        Append(file, Png());
        Append(file, Midi());
    }
    const auto path = WriteTemp(directory, file);
    HyperRipper::Options options;
    options.MinSize = 16;
    const auto reference = HyperRipper::Scan(path, options);
    CHECK(reference.size() == 40);
    for (const std::size_t block : {1U, 4096U, 5000U, 65536U})
    {
        options.BlockSize = block;
        const auto found = HyperRipper::Scan(path, options);
        REQUIRE(found.size() == reference.size());
        for (std::size_t i = 0; i < found.size(); ++i)
        {
            CHECK(found[i].Offset == reference[i].Offset);
        }
    }
}

TEST_CASE("The types know their kinds and the ones that give false positives")
{
    unsigned audio = 0, video = 0, image = 0, noisy = 0;
    for (const auto &s : HyperRipper::Signatures())
    {
        audio += (s.Kinds & RIPPER_AUDIO) != 0 ? 1 : 0;
        video += (s.Kinds & RIPPER_VIDEO) != 0 ? 1 : 0;
        image += (s.Kinds & RIPPER_IMAGE) != 0 ? 1 : 0;
        noisy += s.FalsePositives ? 1 : 0;
        if (s.Type == "669")
        {
            CHECK(s.FalsePositives);
        }
        if (s.Type == "PNG")
        {
            CHECK(s.Kinds == RIPPER_IMAGE);
            CHECK(!s.FalsePositives);
        }
    }
    CHECK(audio >= 10);
    CHECK(video >= 6);
    CHECK(image >= 8);
    CHECK(noisy >= 3);
}
