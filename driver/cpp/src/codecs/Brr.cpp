// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Brr.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace Dud
{
namespace
{
constexpr int MaxRange = 12;           // the encoders never use more
constexpr int64_t MinimumBlocks = 12;  // 192 samples: shorter chains are chance
constexpr int64_t MaximumBlocks = 4096; // 65,536 samples
constexpr int MinimumPeak = 512;
// Encoded sound changes its range slowly from a block to the next one; noise jumps by about 4
constexpr int MaxMeanRangeStep = 2;
constexpr double MinimumSmoothness = 0.3; // correlation of a sample with the next one
// Zero filled data that happens to end like a sample is a flat line: at most a quarter of silent blocks
constexpr int64_t MaxSilentFraction = 4;
constexpr double MinimumRmsToPeak = 0.1;
// ... and a sample that is flat for most of its length (a block that moves by under 1/64 of the
// peak is flat) is a line with a blip at the end: at most 40% of flat blocks
constexpr int FlatDivisor = 64;
// Samples are stored together: one is kept when another starts within this gap, or when it is long
constexpr int64_t NeighbourGap = 512;
constexpr int64_t LongSampleBlocks = 100;

auto Clamp16(int value) -> int
{
    return std::clamp(value, -32768, 32767);
}

// Decode one block; previous1 and previous2 are the two samples before it (as the filters use
// them); returns the number of samples that had to be clipped
auto DecodeBlock(const uint8_t *block, int &previous1, int &previous2, int16_t *out) -> int
{
    const int range = block[0] >> 4;
    const int filter = (block[0] >> 2) & 3;
    int clipped = 0;
    for (int i = 0; i < BrrBlockSamples; ++i)
    {
        int nibble = (i & 1) ? (block[1 + i / 2] & 15) : (block[1 + i / 2] >> 4);
        if (nibble >= 8)
            nibble -= 16;
        int sample = range <= 12 ? (nibble << range) >> 1 : (nibble < 0 ? -2048 : 0);
        switch (filter)
        {
        case 1:
            sample += previous1 + ((-previous1) >> 4);
            break;
        case 2:
            sample += (previous1 << 1) + ((-(previous1 * 3)) >> 5) - previous2 + (previous2 >> 4);
            break;
        case 3:
            sample += (previous1 << 1) + ((-(previous1 * 13)) >> 6) - previous2 + ((previous2 * 3) >> 4);
            break;
        default:
            break;
        }
        const int limited = Clamp16(sample);
        clipped += limited != sample;
        // the DSP keeps 15 bits: the output is the doubled value, wrapped as a signed 16 bits
        const auto doubled = static_cast<int16_t>(limited * 2);
        out[i] = doubled;
        previous2 = previous1;
        previous1 = doubled >> 1;
    }
    return clipped;
}

void Put16(std::vector<uint8_t> &out, uint16_t value)
{
    out.push_back(static_cast<uint8_t>(value & 255));
    out.push_back(static_cast<uint8_t>(value >> 8));
}

void Put32(std::vector<uint8_t> &out, uint32_t value)
{
    Put16(out, static_cast<uint16_t>(value & 0xFFFF));
    Put16(out, static_cast<uint16_t>(value >> 16));
}
} // namespace

auto DecodeBrr(const uint8_t *data, size_t blocks) -> std::vector<int16_t>
{
    std::vector<int16_t> samples(blocks * BrrBlockSamples);
    int previous1 = 0;
    int previous2 = 0;
    for (size_t b = 0; b < blocks; ++b)
        DecodeBlock(data + b * BrrBlockBytes, previous1, previous2, samples.data() + b * BrrBlockSamples);
    return samples;
}

auto BrrWavSize(int64_t blocks) -> int64_t
{
    return 44 + blocks * BrrBlockSamples * 2;
}

auto MakeWav(const std::vector<int16_t> &samples, int sampleRate) -> std::vector<uint8_t>
{
    const auto dataBytes = static_cast<uint32_t>(samples.size() * 2);
    std::vector<uint8_t> wav;
    wav.reserve(44 + dataBytes);
    wav.insert(wav.end(), {'R', 'I', 'F', 'F'});
    Put32(wav, 36 + dataBytes);
    wav.insert(wav.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    Put32(wav, 16);
    Put16(wav, 1); // PCM
    Put16(wav, 1); // mono
    Put32(wav, static_cast<uint32_t>(sampleRate));
    Put32(wav, static_cast<uint32_t>(sampleRate) * 2);
    Put16(wav, 2);
    Put16(wav, 16);
    wav.insert(wav.end(), {'d', 'a', 't', 'a'});
    Put32(wav, dataBytes);
    for (const int16_t s : samples)
        Put16(wav, static_cast<uint16_t>(s));
    return wav;
}

auto ScanBrr(const uint8_t *rom, size_t size) -> std::vector<BrrSample>
{
    std::vector<BrrSample> found;
    std::vector<int16_t> decoded(BrrBlockSamples);

    // Length in blocks of the chain that starts at each offset (a chain is blocks without flags
    // up to one with the end flag): -1 is not a chain, -2 not known yet. The chain of an offset is
    // its block plus the chain 9 bytes further, so every offset is walked once.
    std::vector<int32_t> chain(size, -2);
    auto chainLength = [&](size_t start) -> int32_t {
        std::vector<size_t> path;
        int32_t length = -1;
        size_t pos = start;
        for (;;)
        {
            if (chain[pos] != -2)
            {
                length = chain[pos];
                break;
            }
            if (pos + BrrBlockBytes > size || (rom[pos] >> 4) > MaxRange)
            {
                chain[pos] = -1;
                length = -1;
                break;
            }
            const uint8_t header = rom[pos];
            if (header & 1)
            {
                chain[pos] = 1;
                length = 1;
                break;
            }
            if (header & 2) // loop without end: not a sample
            {
                chain[pos] = -1;
                length = -1;
                break;
            }
            path.push_back(pos);
            pos += BrrBlockBytes;
        }
        for (auto it = path.rbegin(); it != path.rend(); ++it)
        {
            length = (length < 0 || length >= MaximumBlocks) ? -1 : length + 1;
            chain[*it] = length;
        }
        return chain[start];
    };

    for (size_t at = 0; at + BrrBlockBytes * MinimumBlocks <= size;)
    {
        // the first block of a sample has filter 0 and no flags
        const uint8_t first = rom[at];
        // Samples may start with silent blocks, but the middle of a zero filled area is not a start
        // (every byte of it would walk a long chain)
        const bool insideZeros =
            at >= BrrBlockBytes && std::all_of(rom + at - BrrBlockBytes, rom + at + BrrBlockBytes, [](uint8_t v) { return v == 0; });
        if ((first & 0x0F) != 0 || (first >> 4) > MaxRange || insideZeros)
        {
            ++at;
            continue;
        }
        const int32_t length = chainLength(at);
        const int64_t blocks = length;
        // the range of a block moves little from one to the next in a sample
        int64_t rangeSteps = 0;
        bool ok = length >= MinimumBlocks;
        for (int64_t b = 1; ok && b < blocks && rangeSteps <= MaxMeanRangeStep * blocks; ++b)
            rangeSteps += std::abs((rom[at + static_cast<size_t>(b) * BrrBlockBytes] >> 4) -
                                   (rom[at + static_cast<size_t>(b - 1) * BrrBlockBytes] >> 4));
        const bool loops = ok && (rom[at + static_cast<size_t>(blocks - 1) * BrrBlockBytes] & 2) != 0;
        if (!ok || rangeSteps > MaxMeanRangeStep * blocks)
        {
            ++at;
            continue;
        }
        // silence, constant data and saturated noise are not samples
        int previous1 = 0;
        int previous2 = 0;
        int peak = 0;
        int64_t clipped = 0;
        int64_t silent = 0;
        std::vector<int> blockSpan;
        double sumProducts = 0;
        double sumSquares = 0;
        int last = 0;
        for (int64_t b = 0; b < blocks; ++b)
        {
            const uint8_t *block = rom + at + static_cast<size_t>(b) * BrrBlockBytes;
            silent += std::all_of(block + 1, block + BrrBlockBytes, [](uint8_t v) { return v == 0; }) ? 1 : 0;
            clipped += DecodeBlock(block, previous1, previous2, decoded.data());
            if (clipped != 0) // sound never clips: stop at once (noise does within a few blocks)
                break;
            const auto [low, high] = std::minmax_element(decoded.begin(), decoded.end());
            blockSpan.push_back(*high - *low);
            for (const int16_t s : decoded)
            {
                peak = std::max(peak, std::abs(static_cast<int>(s)));
                sumProducts += static_cast<double>(last) * s;
                sumSquares += static_cast<double>(s) * s;
                last = s;
            }
        }
        const auto flat = std::count_if(blockSpan.begin(), blockSpan.end(), [&](int span) { return span * FlatDivisor < peak; });
        const double rms = std::sqrt(sumSquares / static_cast<double>(blocks * BrrBlockSamples));
        if (peak < MinimumPeak || clipped != 0 || sumProducts < MinimumSmoothness * sumSquares ||
            silent * MaxSilentFraction > blocks || rms < MinimumRmsToPeak * peak || flat * 5 > blocks * 2)
        {
            ++at;
            continue;
        }
        found.push_back({static_cast<int64_t>(at), blocks, loops});
        at += static_cast<size_t>(blocks) * BrrBlockBytes;
    }
    std::vector<BrrSample> kept;
    for (size_t i = 0; i < found.size(); ++i)
    {
        const bool before = i > 0 && found[i].Offset - (found[i - 1].Offset + found[i - 1].Blocks * BrrBlockBytes) <= NeighbourGap;
        const bool after = i + 1 < found.size() &&
                           found[i + 1].Offset - (found[i].Offset + found[i].Blocks * BrrBlockBytes) <= NeighbourGap;
        if (before || after || found[i].Blocks >= LongSampleBlocks)
            kept.push_back(found[i]);
    }
    return kept;
}
} // namespace Dud
