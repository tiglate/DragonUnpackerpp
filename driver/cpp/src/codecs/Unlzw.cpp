// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// See Unlzw.h for the origin and the license of this code.

#include "Unlzw.h"

#include <vector>

namespace Dud
{
namespace
{
constexpr int UnlzwBits = 9;
constexpr uint32_t UnlzwEnd = 256;

struct Entry
{
    int32_t Data = 0;  // data offset
    uint16_t Len = 0;  // data size, 16 bits
};

class Unlzw
{
  public:
    Unlzw(uint8_t *out, size_t outSize) : _out(out), _outSize(static_cast<uint32_t>(outSize))
    {
        _dictAlloc = 1u << (UnlzwBits + 3);
        _dict.assign(_dictAlloc, Entry());
        Init();
    }

    auto Run(const uint8_t *in, size_t inSize) -> size_t
    {
        uint32_t inlen = 0;
        const auto insize = static_cast<uint32_t>(inSize);
        int n = 0;
        while (inlen < insize)
        {
            // read at least 24 bits (16 + 7) and cut them
            uint32_t code = in[inlen];
            if (insize - inlen > 1)
                code |= static_cast<uint32_t>(in[inlen + 1]) << 8;
            if (insize - inlen > 2)
                code |= static_cast<uint32_t>(in[inlen + 2]) << 16;
            code = (code >> _ibits) & ((1u << _bits) - 1);

            inlen += (_bits + _ibits) >> 3; // increment the input length
            _ibits = static_cast<uint8_t>((_bits + _ibits) & 7);

            if (code == UnlzwEnd) // means that we need to reset
            {
                if (_ibits != 0)
                    ++inlen;
                Init();
                continue;
            }
            if (code == _dictSize) // I think this is used for repeated chars
            {
                Dictionary();
                n = Expand(code);
            }
            else
            {
                n = Expand(code);
                Dictionary();
            }
            if (n < 0)
                break;
            _dictOff = _outLen;
            _dictLen = static_cast<uint16_t>(n);
            _outLen += static_cast<uint32_t>(n);
        }
        return _outLen;
    }

  private:
    void Init()
    {
        _bits = UnlzwBits;
        _ibits = 0;
        _dictSize = UnlzwEnd + 1;
        _dictOff = 0;
        _dictLen = 0;
    }

    auto Expand(uint32_t code) -> int
    {
        if (code >= _dictSize) // invalid so return 0
            return 0;
        if (code >= UnlzwEnd) // put the data in the dictionary
        {
            const Entry &e = _dict[code];
            if (_outLen + e.Len > _outSize)
                return -1;
            for (int i = 0; i < e.Len; ++i)
                _out[_outLen + static_cast<uint32_t>(i)] = _out[static_cast<uint32_t>(e.Data) + static_cast<uint32_t>(i)];
            return e.Len;
        }
        if (_outLen + 1 > _outSize)
            return -1;
        _out[_outLen] = static_cast<uint8_t>(code); // put the byte
        return 1;
    }

    void Dictionary() // fill the dictionary
    {
        if (_dictLen != 0)
        {
            ++_dictLen;
            if (_dictOff + _dictLen > _outSize)
                _dictLen = static_cast<uint16_t>(_outSize - _dictOff);
            _dict[_dictSize].Data = static_cast<int32_t>(_dictOff);
            _dict[_dictSize].Len = _dictLen;
            ++_dictSize;
            if (((_dictSize + 1) >> _bits) != 0)
            {
                ++_bits; // dynamic dictionary
                const uint32_t tmp = 1u << _bits;
                if (tmp > _dictAlloc)
                {
                    _dict.resize(tmp);
                    _dictAlloc = tmp;
                }
            }
        }
        else
            ++_dictLen;
    }

    uint8_t *_out;
    uint32_t _outSize;
    std::vector<Entry> _dict;
    uint32_t _outLen = 0;
    uint32_t _dictSize = 0;
    uint32_t _dictOff = 0;
    uint32_t _dictAlloc = 0;
    uint16_t _dictLen = 0;
    uint32_t _bits = UnlzwBits;
    uint32_t _ibits = 0;
};
} // namespace

auto UnlzwDecode(const uint8_t *in, size_t inSize, uint8_t *out, size_t outSize) -> size_t
{
    if (inSize == 0)
        return 0;
    return Unlzw(out, outSize).Run(in, inSize);
}
} // namespace Dud
