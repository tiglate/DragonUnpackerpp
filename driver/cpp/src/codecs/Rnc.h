// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Decompression of Rob Northen Compression (RNC ProPack) methods 1 and 2.
// C++ port of drv_rnc.pas, itself a port of the unpack logic of the decompiled "RNC ProPackED" tool
// (https://github.com/lab313ru/rnc_propack_source, free to use).
//
// An RNC file starts with an 18 bytes big-endian header:
//   "RNC" + method (1 or 2), unpacked size, packed size,
//   CRC-16 of the unpacked data, CRC-16 of the packed data,
//   leeway byte, number of chunks byte.
// Protected (encrypted with a key) files are not supported.
#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace Dud
{
constexpr size_t RncHeaderSize = 18;
// Larger unpacked sizes are refused (the whole result is built in memory)
constexpr uint32_t RncMaxUnpackedSize = 512u * 1024 * 1024;

class RncError : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

struct RncHeader
{
    uint8_t Method = 0;
    uint32_t UnpackedSize = 0;
    uint32_t PackedSize = 0;
    uint16_t UnpackedCrc = 0;
    uint16_t PackedCrc = 0;
};

// Parse the header of an RNC file (data holds at least RncHeaderSize bytes).
// False if there is no valid RNC signature.
auto RncReadHeader(const uint8_t *data, size_t size, RncHeader &header) -> bool;

// Unpack a whole RNC file held in memory (header included, at least header.PackedSize +
// RncHeaderSize bytes). Throws RncError if the data is corrupted or protected.
auto RncUnpack(const std::vector<uint8_t> &packed, const RncHeader &header) -> std::vector<uint8_t>;
} // namespace Dud
