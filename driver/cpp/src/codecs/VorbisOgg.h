// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Ogg Vorbis files from the raw Vorbis packets of FMOD (FSB5): FMOD keeps the audio packets only, each with
// a 16 bits size in front, and the setup header is replaced by the CRC32 of a known one (FsbCodebooks).
// C++ port of drv_vorbis_ogg.pas. The identification header is made here (the blocksizes are those of
// FMOD: 256 and 2048), then the comment and the setup, and the Ogg pages with the granule positions (the
// last one is the number of samples of the sound, so that the decoders cut the padding). The layout of the
// pages is the same for the size that is announced when the archive is opened (a dry run that only reads
// the sizes of the packets) and for the extraction.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Reader.h"

namespace Dud
{
struct VorbisModes
{
    int Count = 0;
    bool Long[64] = {}; // blockflag of each mode
};

// The modes of a setup header for a stream of channels channels (false: not a valid setup header)
auto ParseVorbisSetup(const std::string &setup, int channels, VorbisModes &modes) -> bool;

// Makes (dry: only counts) the Ogg file of one sound whose packets are the length bytes at start of the
// reader (16 bits size, packet, ...). Returns the size of the file, or -1 when the data is not valid.
// out may be null when dry.
auto VorbisToOgg(Reader &source, int64_t start, int64_t length, int channels, int rate, int64_t samples,
                 const std::string &setup, const VorbisModes &modes, bool dry, std::vector<uint8_t> *out)
    -> int64_t;
} // namespace Dud
