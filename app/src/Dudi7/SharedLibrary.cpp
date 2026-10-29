// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Dudi7/SharedLibrary.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <stdexcept>
#include <utility>

namespace Dup::Dudi7
{
SharedLibrary::SharedLibrary(const std::filesystem::path &path)
{
#ifdef _WIN32
    _handle = reinterpret_cast<void *>(LoadLibraryW(path.c_str()));
    if (_handle == nullptr)
    {
        throw std::runtime_error("cannot load " + path.string() + " (error " + std::to_string(GetLastError()) + ")");
    }
#else
    _handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (_handle == nullptr)
    {
        const char *message = dlerror();
        throw std::runtime_error(message != nullptr ? message : "cannot load " + path.string());
    }
#endif
}

SharedLibrary::~SharedLibrary()
{
    Close();
}

void SharedLibrary::Close()
{
    if (_handle != nullptr)
    {
#ifdef _WIN32
        FreeLibrary(reinterpret_cast<HMODULE>(_handle));
#else
        dlclose(_handle);
#endif
        _handle = nullptr;
    }
}

SharedLibrary::SharedLibrary(SharedLibrary &&other) noexcept : _handle(std::exchange(other._handle, nullptr))
{
}

auto SharedLibrary::operator=(SharedLibrary &&other) noexcept -> SharedLibrary &
{
    if (this != &other)
    {
        Close();
        _handle = std::exchange(other._handle, nullptr);
    }
    return *this;
}

auto SharedLibrary::Symbol(const char *name) const -> void *
{
#ifdef _WIN32
    return reinterpret_cast<void *>(GetProcAddress(reinterpret_cast<HMODULE>(_handle), name));
#else
    return dlsym(_handle, name);
#endif
}
} // namespace Dup::Dudi7
