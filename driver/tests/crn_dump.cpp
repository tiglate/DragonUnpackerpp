// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Test tool (used by verify_texdecode.sh): unpacks one level of a Unity crunch file (DXT1/DXT5) to a file of
// blocks. Usage: crn_dump in.crn out.blocks [level]; prints "format width height levels"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>

#include "../cpp/src/codecs/Crunch.h"

int main(int argc, char **argv)
{
    if (argc < 3)
        return 1;
    std::ifstream in(argv[1], std::ios::binary);
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const auto size = static_cast<int64_t>(buf.size());
    buf.resize(buf.size() + 16);
    Dud::CrunchHeaderInfo info;
    if (!Dud::CrunchInfo(buf.data(), size, size, info))
    {
        std::puts("not a supported crunch file");
        return 2;
    }
    const int level = argc >= 4 ? std::atoi(argv[3]) : 0;
    std::vector<uint8_t> out(static_cast<size_t>(Dud::CrunchLevelSize(info, level)) + 1);
    out.resize(out.size() - 1);
    if (!Dud::CrunchUnpack(buf.data(), size, level, out.data(), static_cast<int64_t>(out.size())))
    {
        std::puts("unpack failed");
        return 3;
    }
    std::ofstream f(argv[2], std::ios::binary);
    f.write(reinterpret_cast<const char *>(out.data()), static_cast<std::streamsize>(out.size()));
    std::printf("%d %d %d %d\n", info.Format, info.Width, info.Height, info.Levels);
    return 0;
}
