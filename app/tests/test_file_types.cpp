// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Core/FileTypes.h"
#include "TestSupport.h"

#include <cstring>
#include <random>

using namespace Dup::Core;
using Blob = std::vector<std::uint8_t>;

namespace
{
auto Span(const Blob &b) -> std::span<const std::byte>
{
    return std::as_bytes(std::span(b));
}

auto Type(const Blob &b, std::string_view name = {}) -> DetectedType
{
    return IdentifyFile(Span(b), static_cast<std::int64_t>(b.size()), name);
}

auto Bytes(const std::string &text, std::size_t padTo = 0) -> Blob
{
    Blob b(text.begin(), text.end());
    if (b.size() < padTo)
    {
        b.resize(padTo, 0);
    }
    return b;
}

void Put32(Blob &b, std::size_t at, std::uint32_t v, bool big = false)
{
    if (b.size() < at + 4)
    {
        b.resize(at + 4);
    }
    for (int i = 0; i < 4; ++i)
    {
        b[at + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(v >> (big ? 8 * (3 - i) : 8 * i));
    }
}
} // namespace

TEST_CASE("Files with a signature are recognized")
{
    struct Case
    {
        Blob Data;
        const char *Id;
    };
    Blob png = Bytes(std::string("\x89PNG\r\n\x1a\n", 8), 64);
    Blob jpg = Bytes(std::string("\xff\xd8\xff\xe0", 4), 64);
    Blob gif = Bytes("GIF89a", 64);
    Blob dds = Bytes("DDS ", 128);
    Blob wav = Bytes("RIFF", 44);
    std::memcpy(&wav[8], "WAVEfmt ", 8);
    Blob avi = Bytes("RIFF", 44);
    std::memcpy(&avi[8], "AVI LIST", 8);
    Blob ogg = Bytes("OggS", 64);
    std::memcpy(&ogg[28], "\x01vorbis", 7);
    Blob opus = Bytes("OggS", 64);
    std::memcpy(&opus[28], "OpusHead", 8);
    Blob mp4 = Bytes("....ftypisom", 64);
    Blob m4a = Bytes("....ftypM4A ", 64);
    Blob zip = Bytes(std::string("PK\3\4", 4), 64);
    Blob wad2 = Bytes("WAD2", 32);
    Blob pack = Bytes("PACK", 12);
    Put32(pack, 4, 12);
    Put32(pack, 8, 0);
    Blob elf = Bytes(std::string("\x7f" "ELF", 4), 64);
    Blob pe = Bytes("MZ", 128);
    Put32(pe, 0x3C, 0x40);
    std::memcpy(&pe[0x40], "PE\0\0", 4);
    Blob dll = pe;
    Put32(dll, 0x40 + 22, 0x2000);
    dll[0x40 + 22] = 0;
    dll[0x40 + 23] = 0x20;
    Blob xm = Bytes("Extended Module: ", 80);
    Blob it = Bytes("IMPM", 64);
    Blob mkv = Bytes(std::string("\x1a\x45\xdf\xa3", 4), 64);
    Blob webm = Bytes(std::string("\x1a\x45\xdf\xa3", 4), 64);
    std::memcpy(&webm[20], "webm", 4);
    Blob unreal = Bytes(std::string("\xc1\x83\x2a\x9e", 4), 64);
    Blob lua = Bytes("\x1bLua", 32);
    Blob pdf = Bytes("%PDF-1.4", 32);
    Blob midi = Bytes(std::string("MThd\0\0\0\6", 8), 32);
    const std::vector<Case> cases{{png, "png"}, {jpg, "jpg"}, {gif, "gif"}, {dds, "dds"}, {wav, "wav"}, {avi, "avi"}, {ogg, "ogg"}, {opus, "opus"}, {mp4, "mp4"},
                                  {m4a, "m4a"}, {zip, "zip"}, {wad2, "wad2"}, {pack, "pak"}, {elf, "elf"}, {pe, "exe"}, {dll, "dll"}, {xm, "xm"}, {it, "it"}, {mkv, "mkv"},
                                  {webm, "webm"}, {unreal, "upk"}, {lua, "luac"}, {pdf, "pdf"}, {midi, "midi"}};
    for (const auto &c : cases)
    {
        const auto type = Type(c.Data);
        INFO("expected " << c.Id << ", found " << type.Id);
        CHECK(type.Id == c.Id);
        CHECK(type.Confidence >= 60);
        CHECK(!type.Description.empty());
    }
    CHECK(Type(png).Category == FileCategory::IMAGE);
    CHECK(Type(wav).Category == FileCategory::AUDIO);
    CHECK(Type(avi).Category == FileCategory::VIDEO);
    CHECK(Type(zip).Category == FileCategory::ARCHIVE);
    CHECK(SuggestExtension(Type(png)) == "png");
    CHECK(Type(wad2).Extension == "wad");
}

TEST_CASE("Signatures that are not enough on their own are checked")
{
    // "BM" and "PACK" at the start of something else
    CHECK(Type(Bytes("BM and then plain text, not a bitmap at all", 64)).Id != "bmp");
    Blob bmp = Bytes("BM", 64);
    Put32(bmp, 14, 40);
    CHECK(Type(bmp).Id == "bmp");
    Blob notPack = Bytes("PACK of cards, a text", 40);
    CHECK(Type(notPack).Id != "pak");
    // a Java class and the universal binary of Mach-O share their first bytes
    Blob java = Bytes(std::string("\xca\xfe\xba\xbe", 4), 16);
    java[6] = 0;
    java[7] = 52;
    CHECK(Type(java).Id == "class");
    Blob fat = Bytes(std::string("\xca\xfe\xba\xbe", 4), 16);
    fat[7] = 2;
    CHECK(Type(fat).Id == "macho");
    // TGA has no signature: the size of the file has to agree
    Blob tga(18, 0);
    tga[2] = 2;
    tga[12] = 4;
    tga[14] = 4;
    tga[16] = 32;
    tga.resize(18 + 4 * 4 * 4, 0x55);
    CHECK(Type(tga).Id == "tga");
    tga.resize(tga.size() + 3);
    CHECK(Type(tga).Id != "tga");
    // an MP3 frame header (MPEG-1 layer III, 128 kbit/s, 44100 Hz: 417 bytes), with a second frame behind
    Blob mp3(1000, 0x11);
    for (std::size_t at : {std::size_t{0}, std::size_t{417}})
    {
        mp3[at] = 0xFF;
        mp3[at + 1] = 0xFB;
        mp3[at + 2] = 0x90;
        mp3[at + 3] = 0;
    }
    CHECK(Type(mp3).Id == "mp3");
    mp3[417] = 0;
    CHECK(Type(mp3).Id != "mp3"); // the second frame is missing
}

TEST_CASE("Text is told from binary data and from a content type")
{
    CHECK(Type(Bytes("Hello, this is just a plain text file.\nWith two lines.\n")).Id == "text");
    CHECK(Type(Bytes("<?xml version=\"1.0\"?><a/>")).Id == "xml");
    CHECK(Type(Bytes("  <!DOCTYPE html><html></html>")).Id == "html");
    CHECK(Type(Bytes("{\"name\": \"x\", \"value\": 2}")).Id == "json");
    CHECK(Type(Bytes("#!/usr/bin/env python3\nprint(1)\n")).Id == "py");
    CHECK(Type(Bytes("[section]\nkey=value\n")).Id == "ini");
    CHECK(Type(Bytes("# Wavefront\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n")).Id == "obj");
    const auto accented = Type(Bytes("caf\xc3\xa9 au lait\n"));
    CHECK(accented.Id == "text");
    CHECK(accented.Description == "Text (UTF-8)");
    Blob utf16{0xFF, 0xFE, 'H', 0, 'i', 0, '!', 0};
    CHECK(Type(utf16).Id == "text");
    Blob binary(200);
    for (std::size_t i = 0; i < binary.size(); ++i)
    {
        binary[i] = static_cast<std::uint8_t>(i % 7);
    }
    CHECK(Type(binary).Id == "data");
    CHECK(Type({}).Id == "empty");
}

TEST_CASE("Data with nothing known is a guess from the name, or random data")
{
    Blob odd(300);
    for (std::size_t i = 0; i < odd.size(); ++i)
    {
        odd[i] = static_cast<std::uint8_t>((i * 7 + 3) % 5);
    }
    const auto named = Type(odd, "data/level3.xyz");
    CHECK(named.Id == "ext-xyz");
    CHECK(named.Confidence <= 30);
    CHECK(!SuggestExtension(named));
    CHECK(Type(odd, "x.dll").Description == "Dynamic Link Library");
    std::mt19937 random(7);
    Blob noise(4096);
    for (auto &b : noise)
    {
        b = static_cast<std::uint8_t>(random());
    }
    CHECK(Type(noise).Id == "random");
}

TEST_CASE("The detector never reads outside of what it was given")
{
    std::mt19937 random(42);
    // every length from 0 to 80 bytes of what the signatures look at, and random starts
    for (const auto &known : GetKnownTypes())
    {
        CHECK(!known.Id.empty());
    }
    for (int round = 0; round < 3000; ++round)
    {
        Blob b(random() % 130);
        for (auto &x : b)
        {
            x = static_cast<std::uint8_t>(random() % 4 == 0 ? random() : "RIFFPKMZ\x1f\x8b\xff\xd8\x89PNG"[random() % 16]);
        }
        const auto type = IdentifyFile(Span(b), static_cast<std::int64_t>(b.size() + random() % 100000), "f.bin");
        CHECK(!type.Id.empty());
    }
    // cut copies of real signatures
    for (const auto &text : {"RIFF", "PK\3\4", "\x89PNG\r\n\x1a\n", "ftyp", "OggS", "MZ", "IWAD", "BM"})
    {
        const std::string whole = std::string(text) + std::string(40, 'x');
        for (std::size_t n = 0; n <= whole.size(); ++n)
        {
            const Blob b(whole.begin(), whole.begin() + static_cast<std::ptrdiff_t>(n));
            CHECK(!Type(b).Id.empty());
        }
    }
}
