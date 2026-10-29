// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/DdsDecoder.h"
#include "Core/PreviewWorker.h"
#include "TestSupport.h"

#include <condition_variable>
#include <cstring>

using namespace Dup::Core;

namespace
{
auto Bytes(const std::vector<std::uint8_t> &values) -> std::vector<std::byte>
{
    std::vector<std::byte> bytes;
    for (const auto v : values)
    {
        bytes.push_back(std::byte{v});
    }
    return bytes;
}

auto Put32(std::vector<std::uint8_t> &out, std::size_t offset, std::uint32_t value) -> void
{
    for (int i = 0; i < 4; ++i)
    {
        out[offset + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(value >> (8 * i));
    }
}

/// A DDS file with the given FourCC, size and payload.
auto Dds(const char *fourCc, std::uint32_t width, std::uint32_t height, const std::vector<std::uint8_t> &payload) -> std::vector<std::byte>
{
    std::vector<std::uint8_t> file(128, 0);
    std::memcpy(file.data(), "DDS ", 4);
    Put32(file, 4, 124);
    Put32(file, 12, height);
    Put32(file, 16, width);
    Put32(file, 76, 32);
    Put32(file, 80, 4);
    std::memcpy(file.data() + 84, fourCc, 4);
    file.insert(file.end(), payload.begin(), payload.end());
    return Bytes(file);
}
} // namespace

TEST_CASE("DDS: BC1 block with two colours")
{
    // red (565 0xF800) and blue (0x001F), index bits: pixel 0 = colour 0, pixel 1 = colour 1, others colour 0
    std::vector<std::uint8_t> block{0x00, 0xF8, 0x1F, 0x00, 0x04, 0x00, 0x00, 0x00};
    const auto result = DecodeDds(Dds("DXT1", 4, 4, block));
    REQUIRE(result.Image.has_value());
    const auto &rgba = result.Image->Rgba;
    CHECK(rgba[0] == 255);
    CHECK(rgba[1] == 0);
    CHECK(rgba[2] == 0);
    CHECK(rgba[3] == 255);
    CHECK(rgba[4] == 0);   // pixel 1 is blue
    CHECK(rgba[6] == 255);
}

TEST_CASE("DDS: BC3 alpha and flipping the rows")
{
    // alpha endpoints 255 / 0, all indices 0 -> alpha 255; colour block: red everywhere
    std::vector<std::uint8_t> block{255, 0, 0, 0, 0, 0, 0, 0, 0x00, 0xF8, 0x00, 0xF8, 0, 0, 0, 0};
    auto result = DecodeDds(Dds("DXT5", 4, 4, block));
    REQUIRE(result.Image.has_value());
    CHECK(result.Image->Rgba[3] == 255);
    result.Image->Rgba[0] = 7; // mark the first pixel of the top row
    FlipRows(*result.Image);
    CHECK(result.Image->Rgba[(3 * 4) * 4] == 7);
    CHECK(result.Image->Rgba[0] == 255);
}

TEST_CASE("DDS: BC7 blocks decode like the reference decoder (modes 1, 4 and 6)")
{
    struct Vector
    {
        std::vector<std::uint8_t> Block;
        std::vector<std::uint8_t> Rgba;
    };
    // blocks and expected pixels from texture2ddecoder.decode_bc7
    const std::vector<Vector> vectors{
        {{42, 107, 176, 107, 162, 72, 122, 215, 55, 253, 202, 121, 206, 246, 131, 46},
         {125, 137, 101, 255, 75, 137, 111, 255, 102, 137, 106, 255, 28, 137, 120, 255, 102, 137, 106, 255, 28, 137, 120, 255, 149, 137, 97, 255, 183, 136, 153, 255,
          102, 137, 106, 255, 106, 122, 255, 255, 106, 122, 255, 255, 239, 147, 78, 255, 162, 133, 180, 255, 125, 126, 230, 255, 143, 129, 205, 255, 239, 147, 78, 255}},
        {{144, 135, 60, 201, 0, 3, 94, 238, 149, 109, 17, 37, 203, 192, 59, 235},
         {57, 123, 99, 75, 50, 130, 71, 130, 43, 137, 42, 103, 50, 130, 71, 48, 50, 130, 71, 130, 36, 144, 14, 75, 50, 130, 71, 130, 36, 144, 14, 130,
          57, 123, 99, 103, 57, 123, 99, 103, 33, 148, 0, 48, 40, 141, 28, 130, 47, 134, 57, 103, 36, 144, 14, 75, 50, 130, 71, 130, 33, 148, 0, 103}},
        {{64, 177, 10, 111, 255, 254, 189, 197, 224, 104, 157, 113, 8, 244, 84, 138},
         {197, 241, 191, 189, 91, 236, 250, 141, 137, 238, 224, 162, 151, 239, 217, 168, 100, 237, 245, 145, 130, 238, 228, 159, 190, 241, 195, 186, 144, 239, 221, 165,
          137, 238, 224, 162, 197, 241, 191, 189, 167, 240, 208, 175, 84, 236, 254, 138, 167, 240, 208, 175, 160, 239, 212, 172, 121, 238, 233, 155, 137, 238, 224, 162}}};
    for (const auto &vector : vectors)
    {
        std::vector<std::uint8_t> file(148, 0);
        std::memcpy(file.data(), "DDS ", 4);
        Put32(file, 4, 124);
        Put32(file, 12, 4);
        Put32(file, 16, 4);
        Put32(file, 80, 4);
        std::memcpy(file.data() + 84, "DX10", 4);
        Put32(file, 128, 98);
        file.insert(file.end(), vector.Block.begin(), vector.Block.end());
        const auto result = DecodeDds(Bytes(file));
        REQUIRE(result.Image.has_value());
        CHECK(result.Image->Rgba == vector.Rgba);
    }
}

TEST_CASE("DDS: BC6H blocks decode like the reference decoder (modes 3, 10 and 0)")
{
    struct Vector
    {
        std::vector<std::uint8_t> Block;
        std::vector<std::uint8_t> Rgba;
    };
    // blocks and expected pixels (clipped to 8 bits) from texture2ddecoder.decode_bc6
    const std::vector<Vector> vectors{
        {{195, 64, 83, 50, 61, 49, 210, 240, 29, 74, 121, 243, 142, 255, 156, 60},
         {255, 17, 255, 255, 255, 0, 255, 255, 255, 255, 255, 255, 255, 4, 255, 255, 255, 119, 255, 255, 255, 32, 255, 255, 255, 2, 255, 255, 255, 255, 255, 255,
          255, 255, 255, 255, 255, 62, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 119, 255, 255, 255, 255, 255, 255, 255, 2, 255, 255}},
        {{42, 142, 161, 169, 128, 0, 217, 71, 7, 213, 245, 127, 44, 202, 208, 48},
         {255, 52, 0, 255, 255, 49, 0, 255, 255, 49, 0, 255, 255, 49, 0, 255, 255, 51, 0, 255, 255, 51, 0, 255, 255, 51, 0, 255, 255, 52, 0, 255,
          255, 51, 0, 255, 255, 51, 0, 255, 255, 52, 0, 255, 255, 51, 0, 255, 255, 50, 0, 255, 255, 52, 0, 255, 255, 50, 0, 255, 255, 52, 0, 255}},
        {{128, 255, 213, 99, 46, 84, 82, 30, 22, 141, 199, 66, 136, 120, 52, 57},
         {255, 255, 255, 255, 68, 255, 255, 255, 3, 255, 255, 255, 0, 255, 255, 255, 255, 255, 255, 255, 0, 255, 255, 255, 0, 255, 255, 255, 4, 255, 255, 255,
          0, 255, 255, 255, 255, 255, 255, 255, 0, 255, 255, 255, 255, 255, 255, 255, 69, 255, 255, 255, 255, 255, 255, 255, 0, 255, 255, 255, 255, 255, 255, 255}}};
    for (const auto &vector : vectors)
    {
        std::vector<std::uint8_t> file(148, 0);
        std::memcpy(file.data(), "DDS ", 4);
        Put32(file, 4, 124);
        Put32(file, 12, 4);
        Put32(file, 16, 4);
        Put32(file, 80, 4);
        std::memcpy(file.data() + 84, "DX10", 4);
        Put32(file, 128, 95); // BC6H_UF16
        file.insert(file.end(), vector.Block.begin(), vector.Block.end());
        const auto result = DecodeDds(Bytes(file));
        REQUIRE(result.Image.has_value());
        CHECK(result.Image->Rgba == vector.Rgba);
    }
}

TEST_CASE("DDS: unsupported and damaged files give a message, not a crash")
{
    std::vector<std::uint8_t> file(148 + 16, 0);
    std::memcpy(file.data(), "DDS ", 4);
    Put32(file, 4, 124);
    Put32(file, 12, 4);
    Put32(file, 16, 4);
    Put32(file, 80, 4);
    std::memcpy(file.data() + 84, "DX10", 4);
    Put32(file, 128, 61); // R8_UNORM: not one of the decoded formats
    const auto unsupported = DecodeDds(Bytes(file));
    CHECK_FALSE(unsupported.Image.has_value());
    CHECK_FALSE(unsupported.Message.empty());
    CHECK_FALSE(DecodeDds(Bytes({'D', 'D', 'S', ' '})).Image.has_value());
    CHECK_FALSE(DecodeDds(Dds("DXT1", 4, 4, {1, 2, 3})).Image.has_value()); // truncated
    CHECK_FALSE(DecodeDds(Dds("DXT1", 0, 4, std::vector<std::uint8_t>(8, 0))).Image.has_value());
}

TEST_CASE("Prepare chooses what to show")
{
    const auto text = Bytes({'{', '"', 'a', '"', ':', '1', '}'});
    auto result = PreviewWorker::Prepare("config.json", "PACK", text, 7);
    CHECK(result.Kind == PreviewKind::TEXT);
    CHECK(result.Text == "{\"a\":1}");

    // an unknown extension with text inside is text too
    result = PreviewWorker::Prepare("README", "PACK", text, 7);
    CHECK(result.Kind == PreviewKind::TEXT);

    const auto binary = Bytes({0, 1, 2, 3, 0xff, 0xfe, 0x80, 0x00});
    result = PreviewWorker::Prepare("data.bin", "PACK", binary, 8);
    CHECK(result.Kind == PreviewKind::HEX);
    CHECK(result.Text.find("00000000  00 01 02 03 ff fe 80 00") == 0);

    result = PreviewWorker::Prepare("clip.wav", "PACK", binary, 8);
    CHECK(result.Kind == PreviewKind::AUDIO);
    CHECK(result.Bytes.size() == 8);

    result = PreviewWorker::Prepare("pic.png", "PACK", binary, 8);
    CHECK(result.Kind == PreviewKind::IMAGE_FILE);

    result = PreviewWorker::Prepare("empty.dat", "PACK", {}, 0);
    CHECK(result.Kind == PreviewKind::NONE);

    // only the beginning of a big text was read
    result = PreviewWorker::Prepare("big.txt", "PACK", text, 100);
    CHECK(result.Kind == PreviewKind::TEXT);
    CHECK_FALSE(result.Message.empty());
}

TEST_CASE("Prepare flips the DDS of Unity textures only")
{
    std::vector<std::uint8_t> block{0x00, 0xF8, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x55}; // top rows red, last row mixed
    const auto dds = Dds("DXT1", 4, 4, block);
    const auto plain = PreviewWorker::Prepare("a.dds", "XNB", dds, static_cast<std::int64_t>(dds.size()));
    const auto unity = PreviewWorker::Prepare("a.dds", "UnityFS", dds, static_cast<std::int64_t>(dds.size()));
    REQUIRE(plain.Kind == PreviewKind::IMAGE);
    REQUIRE(unity.Kind == PreviewKind::IMAGE);
    CHECK(plain.Image.Rgba != unity.Image.Rgba);
    auto flipped = plain.Image;
    FlipRows(flipped);
    CHECK(flipped.Rgba == unity.Image.Rgba);
}

TEST_CASE("The preview worker reads an entry through its own archive handle; the newest request wins")
{
    const auto driver = Dup::Dudi7::Driver::Load(Dup::Tests::PluginPath());
    std::mutex mutex;
    std::condition_variable done;
    std::vector<PreviewResult> results;
    {
        PreviewWorker worker([&](PreviewResult result) {
            const std::scoped_lock lock(mutex);
            results.push_back(std::move(result));
            done.notify_all();
        });
        PreviewRequest request;
        request.Source = std::make_shared<DriverArchiveSource>(*driver, Dup::Tests::Fixture("test.pak"));
        request.FormatId = "PACK";
        request.Generation = 1;
        request.Entry = 0;
        request.FileName = "hello.txt";
        worker.Submit(request);
        std::unique_lock lock(mutex);
        REQUIRE(done.wait_for(lock, std::chrono::seconds(10), [&] { return !results.empty(); }));
        lock.unlock();
        CHECK(results[0].Generation == 1);
        CHECK(results[0].Kind == PreviewKind::TEXT);
        CHECK(results[0].Text == "Hello, Dragon UnPACKer!\n");

        // the big binary entry: a hex dump of the beginning only
        request.Generation = 2;
        request.Entry = 1;
        request.FileName = "data.bin";
        worker.Submit(request);
        lock.lock();
        REQUIRE(done.wait_for(lock, std::chrono::seconds(10), [&] { return results.size() >= 2; }));
        CHECK(results.back().Generation == 2);
        CHECK(results.back().Kind == PreviewKind::HEX);
        CHECK(results.back().EntrySize == 76800);
        lock.unlock();

        // an entry that does not exist is an error message, not a crash
        request.Generation = 3;
        request.Entry = 99;
        worker.Submit(request);
        lock.lock();
        REQUIRE(done.wait_for(lock, std::chrono::seconds(10), [&] { return results.size() >= 3; }));
        CHECK(results.back().Kind == PreviewKind::ERROR_MESSAGE);
    }
}

#include "Core/Converters.h"
#include "Core/PngEncoder.h"
#include "Core/ArchiveIndex.h"
#include "Core/Extractor.h"

TEST_CASE("EncodePng writes a PNG the HyperRipper recognizes and the TGA decoder round trip keeps the pixels")
{
    DecodedImage image;
    image.Width = 3;
    image.Height = 2;
    image.Rgba = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120};
    const auto png = EncodePng(image);
    REQUIRE(png.has_value());
    CHECK(std::memcmp(png->data(), "\x89PNG\r\n\x1a\n", 8) == 0);
    CHECK_FALSE(EncodePng(DecodedImage{}).has_value());

    // a TGA through ConvertForExtraction gives a PNG
    std::vector<std::uint8_t> tga(18, 0);
    tga[2] = 2;
    tga[12] = 1;
    tga[14] = 1;
    tga[16] = 32;
    tga[17] = 0x28;
    tga.insert(tga.end(), {1, 2, 3, 4});
    const auto converted = ConvertForExtraction("a.tga", "PACK", Bytes(tga));
    REQUIRE(converted.has_value());
    CHECK(converted->Extension == "png");
    CHECK_FALSE(ConvertForExtraction("a.txt", "PACK", Bytes({1, 2, 3})).has_value());
    CHECK_FALSE(ConvertForExtraction("broken.tga", "PACK", Bytes({1, 2, 3})).has_value());
    CHECK(IsConvertible("X.DDS"));
    CHECK_FALSE(IsConvertible("x.png"));
}

TEST_CASE("Extraction with conversion writes images as PNG and the rest unchanged")
{
    const auto driver = Dup::Dudi7::Driver::Load(Dup::Tests::PluginPath());
    const auto archive = driver->Open(Dup::Tests::Fixture("obj22.bundle"));
    const auto index = ArchiveIndex::Build(*archive);
    std::vector<std::uint32_t> all(index.GetEntryCount());
    for (std::uint32_t i = 0; i < all.size(); ++i)
    {
        all[i] = i;
    }
    Dup::Tests::TempDirectory out;
    ExtractOptions options;
    options.ConvertImages = true;
    const Extractor extractor(*archive, index);
    const auto stats = extractor.Run(all, out.GetPath(), {}, options);
    CHECK(stats.Failed == 0);
    CHECK(stats.Extracted == static_cast<int>(all.size()));
    CHECK(std::filesystem::exists(out.GetPath() / "CAB-obj.objects" / "Texture2D" / "rgba.png"));
    CHECK_FALSE(std::filesystem::exists(out.GetPath() / "CAB-obj.objects" / "Texture2D" / "rgba.tga"));
    CHECK(std::filesystem::exists(out.GetPath() / "CAB-obj.objects" / "Texture2D" / "dxt1.png"));
    CHECK(std::filesystem::exists(out.GetPath() / "CAB-obj.objects" / "Texture2D" / "bc7.png"));
    CHECK(std::filesystem::exists(out.GetPath() / "CAB-obj.objects" / "TextAsset" / "json.json"));
    const auto png = Dup::Tests::ReadFile(out.GetPath() / "CAB-obj.objects" / "Texture2D" / "rgba.png");
    CHECK(png.compare(0, 4, "\x89PNG") == 0);
}

#include "Core/ObjParser.h"

#include <clocale>

TEST_CASE("ParseObj reads vertices and polygons, relative indexes and slashes")
{
    const std::string text =
        "# a quad and a triangle\n"
        "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv +0.5 2.5e0 -1\n"
        "vt 0 0\nvn 0 0 1\n"
        "f 1/1/1 2/2/1 3/3/1 4/4/1\n"
        "f -3 -2 -1\n";
    const auto mesh = ParseObj(text);
    REQUIRE(mesh.has_value());
    CHECK(mesh->Positions.size() == 15);
    CHECK(mesh->Triangles.size() == 9); // the quad makes two triangles
    CHECK(mesh->Triangles[6] == 2);     // -3 of 5 vertices = vertex 3
    CHECK(mesh->Max[1] == doctest::Approx(2.5));
    CHECK(mesh->Min[2] == doctest::Approx(-1));

    CHECK_FALSE(ParseObj("v 0 0 0\n").has_value());                      // no face
    CHECK_FALSE(ParseObj("v 0 0 0\nv 1 0 0\nf 1 2 9\n").has_value());    // index out of range
    CHECK_FALSE(ParseObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\nf 1 2 3\n", 1).has_value()); // too many triangles
}

TEST_CASE("ParseObj does not depend on the decimal separator of the locale")
{
    // under a locale with a decimal comma strtof reads "0.5" as 0: the viewer showed a flat, empty model
    const char *previous = std::setlocale(LC_NUMERIC, nullptr);
    const std::string saved = previous != nullptr ? previous : "C";
    if (std::setlocale(LC_NUMERIC, "pt_BR.UTF-8") == nullptr)
    {
        std::setlocale(LC_NUMERIC, saved.c_str());
        MESSAGE("pt_BR.UTF-8 is not installed: the check ran in the C locale only");
    }
    const auto mesh = ParseObj("v 0.5 0.25 0.125\nv 1.5 0 0\nv 0 1.5 0\nf 1 2 3\n");
    std::setlocale(LC_NUMERIC, saved.c_str());
    REQUIRE(mesh.has_value());
    CHECK(mesh->Positions[0] == doctest::Approx(0.5));
    CHECK(mesh->Positions[1] == doctest::Approx(0.25));
    CHECK(mesh->Positions[2] == doctest::Approx(0.125));
}

TEST_CASE("Prepare shows an OBJ as a mesh, or as text when it has no triangles")
{
    const std::string obj = "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    std::vector<std::byte> bytes;
    for (const char c : obj)
    {
        bytes.push_back(static_cast<std::byte>(c));
    }
    auto result = PreviewWorker::Prepare("tri.obj", "PACK", bytes, static_cast<std::int64_t>(bytes.size()));
    CHECK(result.Kind == PreviewKind::MESH);
    REQUIRE(result.Geometry != nullptr);
    CHECK(result.Geometry->Triangles.size() == 3);

    bytes.clear();
    for (const char c : std::string("# empty\n"))
    {
        bytes.push_back(static_cast<std::byte>(c));
    }
    result = PreviewWorker::Prepare("none.obj", "PACK", bytes, static_cast<std::int64_t>(bytes.size()));
    CHECK(result.Kind == PreviewKind::TEXT);
}
