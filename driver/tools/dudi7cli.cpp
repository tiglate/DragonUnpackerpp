// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Lists a file through a DUDI 7 driver, with the MD5 of every entry.
// Usage: dudi7cli plugin.so archive [list | FIRST-LAST] [outdir]
// "list" only lists (no extraction, no MD5); FIRST-LAST only extracts those entries;
// outdir: also saves the extracted entries there (as <index>_<last part of the name>)
// Output: one line per entry: index <TAB> offset <TAB> size <TAB> md5 <TAB> name
// A failure is reported on the standard error as "<call>: <message> (code <status>)".
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#define dlsym(lib, name) reinterpret_cast<void *>(GetProcAddress(static_cast<HMODULE>(lib), name))
#define dlclose(lib) FreeLibrary(static_cast<HMODULE>(lib))
#else
#include <dlfcn.h>
#endif

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

#include "dudi7.h"

namespace
{
// MD5 (RFC 1321)
class Md5
{
  public:
    Md5()
    {
        _state[0] = 0x67452301;
        _state[1] = 0xefcdab89;
        _state[2] = 0x98badcfe;
        _state[3] = 0x10325476;
    }

    void Update(const uint8_t *data, size_t length)
    {
        _total += length;
        while (length > 0)
        {
            const size_t n = std::min(length, 64 - _fill);
            std::memcpy(_buffer + _fill, data, n);
            _fill += n;
            data += n;
            length -= n;
            if (_fill == 64)
            {
                Block(_buffer);
                _fill = 0;
            }
        }
    }

    auto Done() -> std::string
    {
        const uint64_t bits = _total * 8;
        const uint8_t pad = 0x80;
        Update(&pad, 1);
        const uint8_t zero = 0;
        while (_fill != 56)
            Update(&zero, 1);
        uint8_t tail[8];
        for (int i = 0; i < 8; ++i)
            tail[i] = static_cast<uint8_t>(bits >> (8 * i));
        Update(tail, 8);
        char out[33];
        for (int i = 0; i < 16; ++i)
            std::snprintf(out + i * 2, 3, "%02x", static_cast<uint8_t>(_state[i / 4] >> (8 * (i % 4))));
        return out;
    }

  private:
    static auto Rotl(uint32_t x, int c) -> uint32_t
    {
        return (x << c) | (x >> (32 - c));
    }

    void Block(const uint8_t *p)
    {
        static const uint32_t k[64] = {
            0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
            0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
            0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
            0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
            0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
            0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
            0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
            0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
        static const int s[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9,  14, 20, 5, 9,
                                  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                                  4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
        uint32_t m[16];
        for (int i = 0; i < 16; ++i)
            m[i] = static_cast<uint32_t>(p[i * 4]) | (static_cast<uint32_t>(p[i * 4 + 1]) << 8) |
                   (static_cast<uint32_t>(p[i * 4 + 2]) << 16) | (static_cast<uint32_t>(p[i * 4 + 3]) << 24);
        uint32_t a = _state[0], b = _state[1], c = _state[2], d = _state[3];
        for (int i = 0; i < 64; ++i)
        {
            uint32_t f;
            int g;
            if (i < 16)
            {
                f = (b & c) | (~b & d);
                g = i;
            }
            else if (i < 32)
            {
                f = (d & b) | (~d & c);
                g = (5 * i + 1) % 16;
            }
            else if (i < 48)
            {
                f = b ^ c ^ d;
                g = (3 * i + 5) % 16;
            }
            else
            {
                f = c ^ (b | ~d);
                g = (7 * i) % 16;
            }
            f += a + k[i] + m[g];
            a = d;
            d = c;
            c = b;
            b += Rotl(f, s[i]);
        }
        _state[0] += a;
        _state[1] += b;
        _state[2] += c;
        _state[3] += d;
    }

    uint32_t _state[4];
    uint8_t _buffer[64];
    size_t _fill = 0;
    uint64_t _total = 0;
};

// Hashes what the driver writes without keeping it in memory
struct Sink
{
    Md5 Hash;
    int64_t Size = 0;
    std::ofstream *Copy = nullptr;
};

auto Write(void *context, const void *buffer, int64_t length) -> int64_t
{
    auto *sink = static_cast<Sink *>(context);
    sink->Hash.Update(static_cast<const uint8_t *>(buffer), static_cast<size_t>(length));
    if (sink->Copy != nullptr)
        sink->Copy->write(static_cast<const char *>(buffer), length);
    sink->Size += length;
    return length;
}

struct Failure : std::runtime_error
{
    using std::runtime_error::runtime_error;
};

template <typename T> auto Bind(void *lib, const char *name) -> T
{
    void *symbol = dlsym(lib, name);
    if (symbol == nullptr)
        throw Failure(std::string("is not a DUDI 7 driver: export ") + name + " is missing");
    return reinterpret_cast<T>(symbol);
}
} // namespace

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::fprintf(stderr, "usage: dudi7cli plugin.so archive [list | FIRST-LAST] [outdir]\n");
        return 2;
    }
    const std::string mode = argc > 3 ? argv[3] : "";
    const std::string outDir = argc > 4 ? argv[4] : "";
    int64_t first = 0, last = std::numeric_limits<int64_t>::max();
    const size_t dash = mode.find('-');
    if (dash != std::string::npos)
    {
        first = std::atoll(mode.substr(0, dash).c_str());
        last = std::atoll(mode.substr(dash + 1).c_str());
    }
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY); // one LF per line, as on Linux
    void *lib = LoadLibraryA(argv[1]);
#else
    void *lib = dlopen(argv[1], RTLD_NOW);
#endif
    if (lib == nullptr)
    {
        std::fprintf(stderr, "Cannot load %s\n", argv[1]);
        return 1;
    }
    void *handle = nullptr;
    int (*shutdown)() = nullptr;
    int (*closeArchive)(void *) = nullptr;
    try
    {
        const auto version = Bind<int32_t (*)(int32_t, int32_t *)>(lib, "Dudi7_Version");
        const auto init = Bind<int32_t (*)(const dudi7_host *)>(lib, "Dudi7_Init");
        shutdown = Bind<int32_t (*)()>(lib, "Dudi7_Shutdown");
        const auto open = Bind<int32_t (*)(const char *, int32_t, void **)>(lib, "Dudi7_Open");
        const auto archiveInfo = Bind<int32_t (*)(void *, dudi7_archive_info *)>(lib, "Dudi7_GetArchiveInfo");
        const auto getEntry = Bind<int32_t (*)(void *, int64_t, dudi7_entry *)>(lib, "Dudi7_GetEntry");
        const auto extract = Bind<int32_t (*)(void *, int64_t, dudi7_write_cb, void *)>(lib, "Dudi7_Extract");
        closeArchive = Bind<int32_t (*)(void *)>(lib, "Dudi7_Close");
        const auto lastError = Bind<int32_t (*)(void *, char *, int32_t)>(lib, "Dudi7_GetLastError");
        const auto check = [&](int32_t code, void *archive, const char *what) {
            if (code == DUDI7_OK)
                return;
            char message[1024] = {};
            lastError(archive, message, sizeof message);
            throw Failure(std::string(what) + ": " + message + " (code " + std::to_string(code) + ")");
        };
        int32_t pluginVersion = 0;
        check(version(DUDI7_CONTRACT_VERSION, &pluginVersion), nullptr, "Dudi7_Version");
        dudi7_host host{};
        host.struct_size = sizeof host;
        host.host_version = DUDI7_CONTRACT_VERSION;
        check(init(&host), nullptr, "Dudi7_Init");

        check(open(argv[2], 0, &handle), nullptr, "Dudi7_Open");
        dudi7_archive_info info{};
        info.struct_size = sizeof info;
        check(archiveInfo(handle, &info), handle, "Dudi7_GetArchiveInfo");
        std::printf("#%s\t%lld\n", info.format_id, static_cast<long long>(info.entry_count));
        for (int64_t i = 0; i < info.entry_count; ++i)
        {
            dudi7_entry entry{};
            entry.struct_size = sizeof entry;
            check(getEntry(handle, i, &entry), handle, "Dudi7_GetEntry");
            if (mode == "list" || i < first || i > last)
            {
                std::printf("%lld\t%lld\t%lld\t-\t%s\n", static_cast<long long>(i), static_cast<long long>(entry.offset),
                            static_cast<long long>(entry.size), entry.name);
                continue;
            }
            Sink sink;
            std::ofstream copy;
            if (!outDir.empty())
            {
                std::string name = entry.name;
                for (char &c : name)
                    if (c == '\\')
                        c = '/';
                const size_t slash = name.find_last_of('/');
                if (slash != std::string::npos)
                    name = name.substr(slash + 1);
                std::string dir = outDir;
                if (dir.back() != '/')
                    dir += '/';
                copy.open(dir + std::to_string(i) + "_" + name, std::ios::binary);
                sink.Copy = &copy;
            }
            try
            {
                check(extract(handle, i, &Write, &sink), handle, "Dudi7_Extract");
            }
            catch (const Failure &e)
            {
                std::fprintf(stderr, "cannot extract %s: %s\n", entry.name, e.what());
            }
            if (sink.Size != entry.size)
                std::fprintf(stderr, "size mismatch for %s\n", entry.name);
            std::printf("%lld\t%lld\t%lld\t%s\t%s\n", static_cast<long long>(i), static_cast<long long>(entry.offset),
                        static_cast<long long>(entry.size), sink.Hash.Done().c_str(), entry.name);
        }
        closeArchive(handle);
        shutdown();
        dlclose(lib);
        return 0;
    }
    catch (const Failure &e)
    {
        std::fprintf(stderr, "%s\n", e.what());
        if (handle != nullptr && closeArchive != nullptr)
            closeArchive(handle);
        if (shutdown != nullptr)
            shutdown();
        return 1;
    }
}
