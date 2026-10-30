// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// BRR, the sample format of the Super Nintendo sound chip (S-DSP): blocks of 9 bytes (a header
// with range, filter and the loop and end flags, then 16 samples of 4 bits). The scanner finds
// the samples stored as is in a ROM (games upload them to the sound CPU at start up).
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Dud
{
constexpr int BrrBlockBytes = 9;
constexpr int BrrBlockSamples = 16;
// Played at its natural pitch the S-DSP outputs 32 kHz
constexpr int BrrSampleRate = 32000;

struct BrrSample
{
    int64_t Offset = 0; // in the ROM
    int64_t Blocks = 0;
    bool Loops = false; // the last block has the loop flag
};

// 16 bits samples of blocks BRR blocks, as the DSP computes them
auto DecodeBrr(const uint8_t *data, size_t blocks) -> std::vector<int16_t>;
// A mono 16 bits PCM WAV file
auto MakeWav(const std::vector<int16_t> &samples, int sampleRate) -> std::vector<uint8_t>;
// The size of that file for a sample of blocks BRR blocks
auto BrrWavSize(int64_t blocks) -> int64_t;

// The BRR samples of a ROM, not overlapping, in order of offset
auto ScanBrr(const uint8_t *rom, size_t size) -> std::vector<BrrSample>;
} // namespace Dud
