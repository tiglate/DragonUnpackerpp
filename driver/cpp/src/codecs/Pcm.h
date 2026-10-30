// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Raw sampled sound in a ROM: unsigned 8 bits samples, as the Mega Drive games feed them to the DAC of the sound
// chip (the drums and voices of Sonic, Streets of Rage...). Nothing marks them: they are found as long runs of
// bytes that move smoothly around the middle value 0x80 and swing back and forth, which tables, ramps and code do
// not. The rate at which a game plays one is in its sound program, not in the data.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Dud
{
// What a viewer is told when a sample is exported (the rates of the games go from about 6 to 14 kHz)
constexpr int PcmSampleRate = 10000;

struct PcmSample
{
    int64_t Offset = 0; // in the ROM
    int64_t Length = 0; // bytes (= samples)
};

auto ScanPcm8(const uint8_t *rom, size_t size) -> std::vector<PcmSample>;
// A mono unsigned 8 bits PCM WAV file
auto MakePcmWav(const uint8_t *data, size_t length, int sampleRate) -> std::vector<uint8_t>;
auto PcmWavSize(int64_t length) -> int64_t;
} // namespace Dud
