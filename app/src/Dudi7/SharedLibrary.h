// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <filesystem>
#include <string>

namespace Dup::Dudi7
{
/// A dynamically loaded library (the only place that knows dlopen / LoadLibrary).
class SharedLibrary
{
  public:
    /// Throws std::runtime_error with the loader's message when the library cannot be loaded.
    explicit SharedLibrary(const std::filesystem::path &path);
    ~SharedLibrary();

    SharedLibrary(const SharedLibrary &) = delete;
    auto operator=(const SharedLibrary &) -> SharedLibrary & = delete;
    SharedLibrary(SharedLibrary &&other) noexcept;
    auto operator=(SharedLibrary &&other) noexcept -> SharedLibrary &;

    /// Address of an exported symbol, nullptr when it does not exist.
    [[nodiscard]] auto Symbol(const char *name) const -> void *;

    template <typename Signature> [[nodiscard]] auto Function(const char *name) const -> Signature *
    {
        return reinterpret_cast<Signature *>(Symbol(name));
    }

  private:
    void Close();

    void *_handle{nullptr};
};
} // namespace Dup::Dudi7
