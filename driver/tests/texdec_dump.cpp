// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Test tool (used by verify_texdecode.sh): decodes a file of blocks of a Unity texture format into 32 bits
// BGRA pixels. Usage: texdec_dump format width height blocks.bin out.bgra [validity.bin]
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>

#include "../cpp/src/codecs/TexDecode.h"

int main(int argc, char **argv)
{
    if (argc < 6)
        return 1;
    const int fmt = std::atoi(argv[1]), w = std::atoi(argv[2]), h = std::atoi(argv[3]);
    int bw, bh, bb;
    if (!Dud::DecodableFormatInfo(fmt, bw, bh, bb))
    {
        std::puts("format not handled");
        return 2;
    }
    std::ifstream in(argv[4], std::ios::binary);
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const size_t size = buf.size();
    buf.resize(size + 64);
    const int nx = (w + bw - 1) / bw, ny = (h + bh - 1) / bh;
    if (static_cast<int64_t>(size) < static_cast<int64_t>(nx) * ny * bb)
    {
        std::puts("too short");
        return 3;
    }
    std::vector<uint8_t> row(static_cast<size_t>(w) * bh * 4);
    if (argc >= 7)
    {
        std::ofstream v(argv[6], std::ios::binary);
        Dud::BlockPixels px;
        for (int i = 0; i < nx * ny; ++i)
            v.put(static_cast<char>(Dud::DecodeBlockOk(fmt, &buf[static_cast<size_t>(i) * bb], px) ? 1 : 0));
    }
    std::ofstream out(argv[5], std::ios::binary);
    for (int by = 0; by < ny; ++by)
    {
        Dud::DecodeBlockRow(fmt, &buf[static_cast<size_t>(by) * nx * bb], w, h, by, row.data());
        const int lines = h - by * bh < bh ? h - by * bh : bh;
        out.write(reinterpret_cast<const char *>(row.data()), static_cast<std::streamsize>(lines) * w * 4);
    }
    return 0;
}
