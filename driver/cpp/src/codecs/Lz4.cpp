// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Lz4.h"

#include <cstring>

namespace Dud
{
auto Lz4DecompressBlock(const uint8_t *src, int srcLength, uint8_t *dst, int dstLength) -> int
{
    int ip = 0;
    int op = 0;
    while (ip < srcLength)
    {
        const int token = src[ip++];
        int literals = token >> 4;
        if (literals == 15)
        {
            int b;
            do
            {
                if (ip >= srcLength)
                    return Lz4ErrInput;
                b = src[ip++];
                literals += b;
                if (literals > dstLength + 255)
                    return Lz4ErrOutput;
            } while (b == 255);
        }
        if (ip + literals > srcLength)
            return Lz4ErrInput;
        if (op + literals > dstLength)
            return Lz4ErrOutput;
        if (literals > 0)
            std::memmove(dst + op, src + ip, static_cast<size_t>(literals));
        ip += literals;
        op += literals;
        // the last sequence has only literals
        if (ip >= srcLength)
            break;
        if (ip + 2 > srcLength)
            return Lz4ErrInput;
        const int offset = src[ip] | (src[ip + 1] << 8);
        ip += 2;
        if (offset == 0 || offset > op)
            return Lz4ErrOffset;
        int match = token & 15;
        if (match == 15)
        {
            int b;
            do
            {
                if (ip >= srcLength)
                    return Lz4ErrInput;
                b = src[ip++];
                match += b;
                if (match > dstLength + 255)
                    return Lz4ErrOutput;
            } while (b == 255);
        }
        match += 4;
        if (op + match > dstLength)
            return Lz4ErrOutput;
        for (int k = 1; k <= match; ++k)
        {
            dst[op] = dst[op - offset];
            ++op;
        }
    }
    return op;
}
} // namespace Dud
