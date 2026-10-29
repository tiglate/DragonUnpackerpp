// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// What the objects of the Unity serialized files become when they are extracted: textures are
// given as TGA (uncompressed formats, the rows of Unity start at the bottom like TGA's) or DDS
// (block compressed formats; only the first mip level, the rows are stored bottom first as in
// Unity so a DDS viewer shows the image upside down), other texture formats are the raw data.
// Also the guess of the extension of sounds, texts and fonts from their first bytes.
// C++ port of drv_unity_export.pas.
#pragma once

#include <cstdint>
#include <string>

namespace Dud
{
// UxCrunch: a DXT1/DXT5 texture compressed with Unity's crunch, extracted as DDS
// UxDecode: ETC/EAC/ASTC blocks decoded to a 32 bits TGA; UxSprite: a rectangle of a texture as a 32 bits TGA;
// UxMesh: a Mesh as OBJ
enum class UxKind
{
    Raw,
    Tga,
    Dds,
    Crunch,
    Decode,
    Sprite,
    Mesh
};

struct UxPlan
{
    UxKind Kind = UxKind::Raw;
    std::string Ext; // extension with the dot
    int Fmt = 0;     // Unity texture format
    int W = 0, H = 0;
    int HeaderLen = 0;
    int64_t PayloadLen = 0; // bytes of the texture data used
    int64_t OutSize = 0;    // size of what is extracted
};

// How a texture is extracted. avail is the number of bytes of texture data there are.
auto PlanTexture(int fmt, int w, int h, int64_t avail) -> UxPlan;
// A crunched texture (formats 28 and 29) whose crunch header says fmt/w/h (crunchFormat: 0 = DXT1, else
// DXT5): the plan to extract it as DDS, or a raw one when the header does not fit the texture
auto PlanCrunch(int fmt, int w, int h, int crunchFormat, int crunchW, int crunchH, int64_t avail) -> UxPlan;
// The header written before the pixels (HeaderLen bytes of hdr)
void BuildHeader(const UxPlan &p, uint8_t *hdr);
// Convert pixels pixels of the texture data to the pixels of the TGA (same size, may be in place)
void ConvertPixels(int fmt, const uint8_t *src, uint8_t *dst, int pixels);
// Bytes per pixel of the data of the TGA formats (0 for the others)
auto SourceBytesPerPixel(int fmt) -> int;
// Bytes per pixel of the TGA that is written (more than the source for the 16 bits formats)
auto OutBytesPerPixel(int fmt) -> int;

auto AudioExt(const std::string &head) -> std::string;
auto TextExt(const std::string &head, int64_t len) -> std::string;
auto FontExt(const std::string &head) -> std::string;
} // namespace Dud
