// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Pcm.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>

namespace Dud
{
namespace
{
constexpr size_t Window = 256;
constexpr size_t Step = 128;
constexpr int64_t MinimumLength = 1024; // shorter runs are noise
constexpr int MaxFlatPercent = 30;
constexpr int EdgeStep = 16;            // the extension of a run stops at a jump bigger than this

// Does the window look like a piece of recorded sound?
auto SoundLike(const uint8_t *w) -> bool
{
    double sum = 0;
    for (size_t i = 0; i < Window; ++i)
        sum += w[i];
    const double mean = sum / Window;
    if (mean < 96 || mean > 160)
        return false;
    double variance = 0;
    double steps = 0;
    int turns = 0;
    int flat = 0;
    std::set<unsigned> distinct;
    for (size_t i = 0; i < Window; ++i)
    {
        variance += (w[i] - mean) * (w[i] - mean);
        distinct.insert(w[i]);
        if (i + 1 < Window)
        {
            steps += std::abs(static_cast<int>(w[i + 1]) - static_cast<int>(w[i]));
            flat += w[i + 1] == w[i] ? 1 : 0;
        }
        if (i + 2 < Window && (static_cast<int>(w[i + 1]) - static_cast<int>(w[i])) * (static_cast<int>(w[i + 2]) - static_cast<int>(w[i + 1])) < 0)
            ++turns;
    }
    const double deviation = std::sqrt(variance / Window);
    // a swing, steps that are small next to it, and a signal that goes up and down (a ramp does not)
    // and not in stairs: a recording does not hold a value for long (a table or a picture does)
    return deviation > 10 && steps / (Window - 1) < deviation * 0.45 && turns >= 8 && distinct.size() >= 16 && flat * 100 < static_cast<int>(Window - 1) * MaxFlatPercent;
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

auto ScanPcm8(const uint8_t *rom, size_t size) -> std::vector<PcmSample>
{
    std::vector<PcmSample> found;
    if (size < Window)
        return found;
    // the windows that look like sound, merged when they touch or overlap
    int64_t start = -1;
    int64_t end = -1;
    auto close = [&]() {
        if (start < 0)
            return;
        // the run grows byte by byte while the signal stays smooth: the windows are coarser than the sample
        int64_t from = start;
        int64_t to = end;
        for (int k = 0; k < static_cast<int>(Window) && from > 0 && std::abs(rom[from - 1] - rom[from]) <= EdgeStep; ++k)
            --from;
        for (int k = 0; k < static_cast<int>(Window) && static_cast<size_t>(to) < size && std::abs(rom[to] - rom[to - 1]) <= EdgeStep; ++k)
            ++to;
        if (to - from >= MinimumLength)
            found.push_back({from, to - from});
        start = end = -1;
    };
    for (size_t at = 0; at + Window <= size; at += Step)
    {
        if (SoundLike(rom + at))
        {
            if (start >= 0 && static_cast<int64_t>(at) <= end)
                end = static_cast<int64_t>(at + Window);
            else
            {
                close();
                start = static_cast<int64_t>(at);
                end = static_cast<int64_t>(at + Window);
            }
        }
    }
    close();
    // extended runs can touch the next one
    std::vector<PcmSample> merged;
    for (const auto &s : found)
    {
        if (!merged.empty() && s.Offset <= merged.back().Offset + merged.back().Length)
            merged.back().Length = std::max(merged.back().Length, s.Offset + s.Length - merged.back().Offset);
        else
            merged.push_back(s);
    }
    return merged;
}

auto PcmWavSize(int64_t length) -> int64_t
{
    return 44 + length;
}

auto MakePcmWav(const uint8_t *data, size_t length, int sampleRate) -> std::vector<uint8_t>
{
    std::vector<uint8_t> wav;
    wav.reserve(44 + length);
    wav.insert(wav.end(), {'R', 'I', 'F', 'F'});
    Put32(wav, static_cast<uint32_t>(36 + length));
    wav.insert(wav.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    Put32(wav, 16);
    Put16(wav, 1); // PCM
    Put16(wav, 1); // mono
    Put32(wav, static_cast<uint32_t>(sampleRate));
    Put32(wav, static_cast<uint32_t>(sampleRate));
    Put16(wav, 1);
    Put16(wav, 8);
    wav.insert(wav.end(), {'d', 'a', 't', 'a'});
    Put32(wav, static_cast<uint32_t>(length));
    wav.insert(wav.end(), data, data + length);
    return wav;
}
} // namespace Dud
