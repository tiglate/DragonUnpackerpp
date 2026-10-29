// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Lzo.h"

#include <cstring>

namespace Dud
{
namespace
{
class Lzo1x
{
  public:
    Lzo1x(const uint8_t *src, int srcLength, uint8_t *dst, int dstLength)
        : _src(src), _srcLength(srcLength), _dst(dst), _dstLength(dstLength)
    {
    }

    auto Run() -> int
    {
        if (_srcLength < 1)
            return LzoErrInput;
        State state;
        if (_src[0] > 17)
        {
            _t = Rd() - 17;
            if (_t < 4)
            {
                state = State::MatchNext;
            }
            else
            {
                Lit(_t);
                state = State::FirstLit;
            }
        }
        else
        {
            state = State::Loop;
        }

        while (_err == 0)
        {
            switch (state)
            {
            case State::Loop:
                _t = Rd();
                if (_t >= 16)
                {
                    state = State::Match;
                }
                else
                {
                    if (_t == 0)
                        _t = Ext(15);
                    Lit(_t + 3);
                    state = State::FirstLit;
                }
                break;

            case State::FirstLit:
                _t = Rd();
                if (_t >= 16)
                {
                    state = State::Match;
                }
                else
                {
                    // 3 bytes match at a distance above 2048
                    _mp = _op - (1 + 0x800) - (_t >> 2) - (Rd() << 2);
                    Mat(3);
                    state = State::MatchNext;
                    _t = _src[_ip - 2] & 3;
                    if (_err == 0 && _t == 0)
                        state = State::Loop;
                    if (_err == 0 && _t != 0)
                    {
                        Lit(_t);
                        state = State::Match;
                        _t = Rd();
                    }
                }
                break;

            case State::Match:
                if (_t >= 64)
                {
                    _mp = _op - 1 - ((_t >> 2) & 7) - (Rd() << 3);
                    Mat((_t >> 5) - 1 + 2);
                }
                else if (_t >= 32)
                {
                    _t &= 31;
                    if (_t == 0)
                        _t = Ext(31);
                    int v = Rd();
                    v |= Rd() << 8;
                    _mp = _op - 1 - (v >> 2);
                    Mat(_t + 2);
                }
                else if (_t >= 16)
                {
                    _mp = _op - ((_t & 8) << 11);
                    _t &= 7;
                    if (_t == 0)
                        _t = Ext(7);
                    int v = Rd();
                    v |= Rd() << 8;
                    _mp -= v >> 2;
                    if (_err != 0)
                        break;
                    if (_mp == _op)
                    {
                        // end marker
                        return _op == _dstLength ? LzoOk : LzoErrSize;
                    }
                    _mp -= 0x4000;
                    Mat(_t + 2);
                }
                else
                {
                    // 2 bytes match right after a match
                    _mp = _op - 1 - (_t >> 2) - (Rd() << 2);
                    Mat(2);
                }
                if (_err != 0)
                    break;
                _t = _src[_ip - 2] & 3;
                state = _t == 0 ? State::Loop : State::MatchNext;
                break;

            case State::MatchNext:
                Lit(_t);
                _t = Rd();
                state = State::Match;
                break;
            }
        }
        return _err;
    }

  private:
    enum class State
    {
        Loop,
        FirstLit,
        Match,
        MatchNext
    };

    auto Rd() -> int
    {
        if (_ip >= _srcLength)
        {
            _err = LzoErrInput;
            return 0;
        }
        return _src[_ip++];
    }

    // copy n literals
    void Lit(int n)
    {
        if (n < 0 || static_cast<int64_t>(_ip) + n > _srcLength)
        {
            _err = LzoErrInput;
            return;
        }
        if (static_cast<int64_t>(_op) + n > _dstLength)
        {
            _err = LzoErrOutput;
            return;
        }
        std::memcpy(_dst + _op, _src + _ip, static_cast<size_t>(n));
        _ip += n;
        _op += n;
    }

    // copy n bytes from the output at _mp (may overlap)
    void Mat(int n)
    {
        if (_mp < 0)
        {
            _err = LzoErrLookBehind;
            return;
        }
        if (static_cast<int64_t>(_op) + n > _dstLength)
        {
            _err = LzoErrOutput;
            return;
        }
        while (n > 0)
        {
            _dst[_op++] = _dst[_mp++];
            --n;
        }
    }

    // length extension: zero bytes add 255 each (the sum wraps like a 32 bits integer)
    auto Ext(int base) -> int
    {
        uint32_t result = 0;
        while (_err == 0 && _ip < _srcLength && _src[_ip] == 0)
        {
            result += 255;
            ++_ip;
        }
        result += static_cast<uint32_t>(base) + static_cast<uint32_t>(Rd());
        return static_cast<int>(result);
    }

    const uint8_t *_src;
    int _srcLength;
    uint8_t *_dst;
    int _dstLength;
    int _ip = 0;
    int _op = 0;
    int _mp = 0;
    int _t = 0;
    int _err = 0;
};
} // namespace

auto Lzo1xDecompress(const uint8_t *src, int srcLength, uint8_t *dst, int dstLength) -> int
{
    return Lzo1x(src, srcLength, dst, dstLength).Run();
}
} // namespace Dud
