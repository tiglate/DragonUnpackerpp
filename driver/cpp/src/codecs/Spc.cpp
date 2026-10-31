// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "codecs/Spc.h"

#include <algorithm>

namespace Dud
{
namespace
{
void PutText(std::vector<uint8_t> &file, size_t at, const std::string &text, size_t size)
{
    for (size_t i = 0; i < size && i < text.size(); ++i)
        file[at + i] = static_cast<uint8_t>(text[i]);
}
} // namespace

auto MakeSpc(const KnownMusic &music, size_t bank, size_t song, const RomSource &rom, const std::string &title, const std::string &game)
    -> std::vector<uint8_t>
{
    std::vector<uint8_t> file(static_cast<size_t>(SpcFileBytes), 0);
    constexpr std::string_view Signature = "SNES-SPC700 Sound File Data v0.30";
    std::copy(Signature.begin(), Signature.end(), file.begin());
    file[0x21] = file[0x22] = 26;
    file[0x23] = 26; // there is a tag
    file[0x24] = 30;
    file[0x25] = static_cast<uint8_t>(music.Pc & 255);
    file[0x26] = static_cast<uint8_t>(music.Pc >> 8);
    file[0x27] = static_cast<uint8_t>(music.A);
    file[0x28] = static_cast<uint8_t>(music.X);
    file[0x29] = static_cast<uint8_t>(music.Y);
    file[0x2A] = static_cast<uint8_t>(music.Psw);
    file[0x2B] = static_cast<uint8_t>(music.Sp);
    PutText(file, 0x2E, title, 32);
    PutText(file, 0x4E, game, 32);
    PutText(file, 0x7E, "Rebuilt from the ROM by Dragon UnPACKer", 32);
    PutText(file, 0xA9, "180", 3);   // seconds to play
    PutText(file, 0xAC, "10000", 5); // fade out, ms
    PutText(file, 0xD2, "0", 1);     // dumped with an unknown emulator

    uint8_t *ram = file.data() + 0x100;
    const auto put = [&](const KnownBlock &block) {
        const auto length = static_cast<size_t>(std::min<int64_t>(block.Length, 0x10000 - block.Dest));
        rom.Read(block.Offset, ram + block.Dest, static_cast<int64_t>(length));
    };
    for (const KnownBlock &block : music.Engine)
        put(block);
    if (bank < music.Banks.size())
        for (const KnownBlock &block : music.Banks[bank].Blocks)
            put(block);
    for (const auto &patch : music.Patches)
        std::copy_n(patch.second.begin(), std::min<size_t>(patch.second.size(), 0x10000 - static_cast<size_t>(patch.first)), ram + patch.first);
    std::copy_n(music.Io.begin(), std::min<size_t>(music.Io.size(), 16), ram + 0xF0);
    if (bank < music.Banks.size() && song < music.Banks[bank].Songs.size())
        for (size_t i = 0; i < 4; ++i)
            ram[0xF4 + i] = music.Banks[bank].Songs[song][i]; // what the CPU wrote to the ports
    std::copy_n(music.Dsp.begin(), std::min<size_t>(music.Dsp.size(), 128), file.begin() + 0x10100);
    return file;
}
} // namespace Dud
