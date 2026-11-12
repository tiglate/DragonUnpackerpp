// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Synthetic files built from the layouts of the Descent 3 format specification.

#include "Core/OutrageGraphics.h"
#include "Core/OutrageModel.h"

#include "ThirdParty/doctest.h"

#include <cstring>
#include <string>
#include <vector>

using namespace Dup::Core;

namespace
{
using Bytes = std::vector<std::byte>;

void Put(Bytes &b, std::initializer_list<int> values)
{
    for (const int v : values)
    {
        b.push_back(std::byte{static_cast<std::uint8_t>(v)});
    }
}
void Put16(Bytes &b, unsigned v)
{
    Put(b, {static_cast<int>(v & 255), static_cast<int>(v >> 8)});
}
void Put32(Bytes &b, std::uint32_t v)
{
    Put16(b, v & 0xFFFF);
    Put16(b, v >> 16);
}
void PutF(Bytes &b, float f)
{
    std::uint32_t v;
    std::memcpy(&v, &f, 4);
    Put32(b, v);
}
void PutText(Bytes &b, const std::string &s)
{
    for (const char c : s)
    {
        Put(b, {c});
    }
}

/// A type 122 OGF named "t", w x h, one level (or more) of runs.
auto Ogf(unsigned type, unsigned w, unsigned h, unsigned mips, std::initializer_list<int> packets) -> Bytes
{
    Bytes b;
    Put(b, {0, 0, static_cast<int>(type)});
    PutText(b, "t");
    Put(b, {0});
    Put(b, {static_cast<int>(mips)});
    Put(b, {0, 0, 0, 0, 0, 0, 0, 0, 0});
    Put16(b, w);
    Put16(b, h);
    Put(b, {32, 0x28});
    Put(b, packets);
    return b;
}
} // namespace

TEST_CASE("OGF type 122: runs of ARGB1555")
{
    const auto data = Ogf(122, 2, 2, 1, {3, 0x00, 0xFC, 0, 0x00, 0x00});
    const auto image = DecodeOgf(data);
    REQUIRE(image.has_value());
    CHECK(image->Name == "t");
    CHECK(image->Consumed == data.size());
    REQUIRE(image->Levels.size() == 1);
    const auto &px = image->Levels[0].Rgba;
    CHECK(px[0] == 255);
    CHECK(px[1] == 0);
    CHECK(px[3] == 255);
    CHECK(px[8] == 255); // third red pixel
    CHECK(px[15] == 0);  // the fourth is transparent
}

TEST_CASE("OGF type 121 is ARGB4444, and the 0x80 command means 128 repeats")
{
    const auto data = Ogf(121, 16, 8, 1, {0x80, 0x00, 0xF0});
    const auto image = DecodeOgf(data);
    REQUIRE(image.has_value());
    CHECK(image->Levels[0].Rgba[0] == 0);
    CHECK(image->Levels[0].Rgba[3] == 255);
}

TEST_CASE("OGF stored mip levels and the number of bytes used")
{
    auto data = Ogf(122, 2, 2, 2, {4, 0xFF, 0x7F, 0, 0xFF, 0x7F});
    Put(data, {9, 9, 9}); // belongs to whatever follows
    const auto image = DecodeOgf(data);
    REQUIRE(image.has_value());
    REQUIRE(image->Levels.size() == 2);
    CHECK(image->Levels[1].Width == 1);
    CHECK(image->Consumed == data.size() - 3);
}

TEST_CASE("Legacy RGB565 green is the transparent colour, black is opaque")
{
    const auto data = Ogf(123, 2, 1, 1, {0, 0xE0, 0x07, 0, 0x00, 0x00});
    const auto image = DecodeOgf(data);
    REQUIRE(image.has_value());
    CHECK(image->Levels[0].Rgba[3] == 0);
    CHECK(image->Levels[0].Rgba[7] == 255);
}

TEST_CASE("OGF rejects reserved commands, overruns, truncation and unsupported types")
{
    CHECK_FALSE(DecodeOgf(Ogf(122, 2, 2, 1, {1, 0, 0, 0, 0, 0})).has_value());
    CHECK_FALSE(DecodeOgf(Ogf(122, 2, 2, 1, {5, 0, 0})).has_value());
    CHECK_FALSE(DecodeOgf(Ogf(122, 2, 2, 1, {3, 0, 0})).has_value());
    CHECK_FALSE(DecodeOgf(Ogf(122, 2, 2, 0, {})).has_value());
    CHECK_FALSE(DecodeOgf(Ogf(122, 2, 2, 3, {4, 0, 0, 1, 0, 0})).has_value()); // more levels than dimensions
    CHECK_FALSE(DecodeOgf(Ogf(125, 2, 2, 1, {4, 0, 0})).has_value());
    CHECK_FALSE(DecodeOgf(Ogf(126, 2, 2, 1, {4, 0, 0})).has_value());
}

TEST_CASE("OAF: modern and legacy headers")
{
    Bytes modern;
    Put(modern, {127, 1, 2});
    PutF(modern, 0.5f);
    for (int i = 0; i < 2; ++i)
    {
        const auto frame = Ogf(122, 1, 1, 1, {0, 0x00, 0xFC});
        modern.insert(modern.end(), frame.begin(), frame.end());
    }
    const auto oaf = DecodeOaf(modern);
    REQUIRE(oaf.has_value());
    CHECK(oaf->Frames.size() == 2);
    CHECK(oaf->FrameTime == 0.5f);
    CHECK(MakeFrameSheet(*oaf).Width == 2);

    Bytes legacy;
    Put(legacy, {1});
    PutF(legacy, 1.0f);
    PutF(legacy, 0.25f);
    Put32(legacy, 0);
    PutF(legacy, 0.0f);
    const auto frame = Ogf(122, 1, 1, 1, {0, 0x00, 0xFC});
    legacy.insert(legacy.end(), frame.begin(), frame.end());
    const auto old = DecodeOaf(legacy);
    REQUIRE(old.has_value());
    CHECK(old->Legacy);
    CHECK(old->FrameTime == 0.25f);

    modern[1] = std::byte{2};
    CHECK_FALSE(DecodeOaf(modern).has_value());
}

TEST_CASE("FNT: monochrome proportional font with kerning")
{
    Bytes b;
    Put32(b, 0xFEEDBABA);
    Put16(b, 8);
    Put16(b, 2);
    Put16(b, 0x06); // widths and kerning
    Put16(b, 0);
    Put(b, {65, 66}); // A..B
    Bytes name(32);
    name[0] = std::byte{'f'};
    b.insert(b.end(), name.begin(), name.end());
    Put16(b, 3); // width of A
    Put16(b, 9); // width of B
    Put16(b, 1); // one kerning triple
    Put(b, {65, 66, 0xFF});
    Put32(b, 2 + 4);
    Put(b, {0x80, 0x40});       // A: two rows of one byte
    Put(b, {0xFF, 0x80, 0, 0}); // B: two rows of two bytes
    const auto font = DecodeFnt(b);
    REQUIRE(font.has_value());
    CHECK(font->Glyphs.size() == 2);
    CHECK(font->Glyphs[0].Rgba[3] == 255);      // leftmost pixel of the first row
    CHECK(font->Glyphs[0].Rgba[4 * 4 + 3] == 255); // second row, second pixel
    CHECK(font->Glyphs[0].Rgba[4 * 3 + 3] == 0);
    CHECK(font->Glyphs[1].Width == 9);
    CHECK(font->Glyphs[1].Rgba[8 * 4 + 3] == 255); // ninth pixel of the first row
    CHECK(font->Kerns.size() == 1);
    CHECK(font->Kerns[0].Adjust == -1);
    CHECK(MakeGlyphSheet(*font).Width > 0);
    b.resize(b.size() - 1);
    CHECK_FALSE(DecodeFnt(b).has_value());
}

namespace
{
void Chunk(Bytes &model, const char *id, const Bytes &payload)
{
    PutText(model, id);
    Put32(model, static_cast<std::uint32_t>(payload.size()));
    model.insert(model.end(), payload.begin(), payload.end());
}
void PutString(Bytes &b, const std::string &s)
{
    Put32(b, static_cast<std::uint32_t>(s.size() + 1));
    PutText(b, s);
    Put(b, {0});
}
void PutVec(Bytes &b, float x, float y, float z)
{
    PutF(b, x);
    PutF(b, y);
    PutF(b, z);
}

/// One subobject: a textured triangle and a flat coloured one sharing three vertices.
auto Model(unsigned version) -> Bytes
{
    Bytes m;
    PutText(m, "PSPO");
    Put32(m, version);
    Bytes header;
    Put32(header, 1);
    PutF(header, 2.0f);
    PutVec(header, -1, -1, -1);
    PutVec(header, 1, 1, 1);
    Put32(header, 1);
    Put32(header, 0);
    Chunk(m, "OHDR", header);
    Bytes so;
    Put32(so, 0);
    Put32(so, static_cast<std::uint32_t>(-1));
    PutVec(so, 0, 1, 0);
    PutF(so, 0);
    PutVec(so, 0, 0, 0);
    PutVec(so, 10, 0, 0); // offset
    PutF(so, 1);
    Put32(so, 0);
    Put32(so, 0);
    PutVec(so, 0, 0, 0);
    PutString(so, "body");
    PutString(so, "$rotate=4");
    Put32(so, 0);
    Put32(so, 0);
    Put32(so, 0);
    Put32(so, 3);
    PutVec(so, 0, 0, 0);
    PutVec(so, 1, 0, 0);
    PutVec(so, 0, 1, 0);
    for (int i = 0; i < 3; ++i)
    {
        PutVec(so, 0, 0, 1);
    }
    if (version / 100 >= 23)
    {
        for (int i = 0; i < 3; ++i)
        {
            PutF(so, 1);
        }
    }
    Put32(so, 2);
    for (int f = 0; f < 2; ++f)
    {
        PutVec(so, 0, 0, 1);
        Put32(so, 3);
        Put32(so, f == 0 ? 1 : 0);
        if (f == 0)
        {
            Put32(so, 0);
        }
        else
        {
            Put(so, {10, 20, 30});
        }
        for (int c = 0; c < 3; ++c)
        {
            Put32(so, static_cast<std::uint32_t>(c));
            PutF(so, 0.5f);
            PutF(so, 0.25f);
        }
        if (version / 100 >= 21)
        {
            PutF(so, 1);
            PutF(so, 1);
        }
    }
    Chunk(m, "SOBJ", so);
    Bytes tx;
    Put32(tx, 1);
    PutString(tx, "wall");
    Chunk(m, "TXTR", tx);
    Chunk(m, "IDTA", Bytes(5)); // skipped
    return m;
}
} // namespace

TEST_CASE("OOF: geometry, textures and OBJ export")
{
    for (const unsigned version : {1807u, 2100u, 2300u})
    {
        const auto data = Model(version);
        std::string why;
        const auto model = ParseOof(data, &why);
        INFO(version << ": " << why);
        REQUIRE(model.has_value());
        REQUIRE(model->Subobjects.size() == 1);
        const auto &so = model->Subobjects[0];
        CHECK(so.Name == "body");
        CHECK(so.Properties == "$rotate=4");
        CHECK(so.Positions.size() == 3);
        CHECK(so.Faces.size() == 2);
        CHECK(so.Faces[0].Texture == 0);
        CHECK(so.Faces[1].Texture == -1);
        CHECK(so.Faces[1].Color[2] == 30);
        CHECK(model->Textures == std::vector<std::string>{"wall"});
        const auto obj = ExportOofObj(*model);
        CHECK(obj.find("o body") != std::string::npos);
        CHECK(obj.find("v 10 0 0\n") != std::string::npos); // placed by its offset
        CHECK(obj.find("v 11 0 0\n") != std::string::npos);
        CHECK(obj.find("vt 0.5 0.75\n") != std::string::npos); // V counted from the bottom
        CHECK(obj.find("vn 0 0 -1\n") != std::string::npos);  // Z negated
        CHECK(obj.find("f 3/6/3 2/5/2 1/4/1\n") != std::string::npos); // winding reversed (the second face's corners)
        CHECK(obj.find("usemtl wall") != std::string::npos);
        CHECK(obj.find("usemtl rgb_0a141e") != std::string::npos);
        const auto mesh = MakeOofMesh(*model);
        REQUIRE(mesh.has_value());
        CHECK(mesh->Triangles.size() == 6);
        CHECK(mesh->Max[0] == 11);
    }
}

TEST_CASE("OOF: the preview leaves out effect and helper subobjects, and the detail list is not used")
{
    auto data = Model(2100);
    std::string why;
    auto model = ParseOof(data, &why);
    REQUIRE(model.has_value());
    model->Details = {5}; // nothing: every root is drawn
    CHECK(MakeOofMesh(*model).has_value());
    model->Subobjects[0].Properties = "$GLOW=1,1,1,2";
    CHECK_FALSE(MakeOofMesh(*model).has_value());
    CHECK(ExportOofObj(*model).find("o body") != std::string::npos); // the OBJ keeps every subobject
    model->Subobjects[0].Properties = "$shell";
    CHECK_FALSE(MakeOofMesh(*model).has_value());
}

TEST_CASE("OOF: the mesh is right-handed")
{
    const auto model = ParseOof(Model(2100));
    REQUIRE(model.has_value());
    const auto mesh = MakeOofMesh(*model);
    REQUIRE(mesh.has_value());
    // the first face is 0, 1, 2 in the file: after the Z flip its order is reversed (0, 2, 1) and the Z of the points is negated
    CHECK(mesh->Triangles[0] == 0);
    CHECK(mesh->Triangles[1] == 2);
    CHECK(mesh->Triangles[2] == 1);
}

TEST_CASE("OOF rejects what is malformed")
{
    auto data = Model(2100);
    CHECK_FALSE(ParseOof(Model(1700)).has_value());
    CHECK_FALSE(ParseOof(Model(2400)).has_value());
    data.resize(data.size() - 1); // the last chunk goes past the end
    CHECK_FALSE(ParseOof(data).has_value());
    auto bad = Model(2100);
    bad[0] = std::byte{'X'};
    CHECK_FALSE(IsOof(bad));
    // a face that refers to a texture the TXTR chunk does not have
    auto noTexture = Model(2100);
    const auto at = noTexture.size() - (8 + 4 + 4 + 5 + 8 + 5); // TXTR chunk and IDTA: cut both
    noTexture.resize(at);
    CHECK_FALSE(ParseOof(noTexture).has_value());
}
