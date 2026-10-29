// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

namespace Dup::Ui
{
enum class Theme
{
    DARK,
    LIGHT
};

/// The two built-in themes: Fusion style, a palette and a small style sheet for the details the palette cannot reach.
class ThemeManager
{
  public:
    static void Apply(Theme theme);
    [[nodiscard]] static auto Current() -> Theme;
};
} // namespace Dup::Ui
