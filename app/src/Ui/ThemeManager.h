// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <optional>
#include <string>

namespace Dup::Ui
{
enum class Theme
{
    SYSTEM, // dark or light, as the desktop is
    DARK,
    LIGHT
};

/// The built-in themes: Fusion style, a complete palette (no colour is taken from the system's, or a dark desktop
/// would put its white text on the light theme) and a small style sheet for the details the palette cannot reach.
class ThemeManager
{
  public:
    /// The user's choice; SYSTEM follows the desktop (and its changes, with Qt 6.5 or later).
    static void Apply(Theme theme);
    /// A theme for this run only (the --theme switch): it wins over Apply until it is cleared with std::nullopt.
    static void Force(std::optional<Theme> theme);
    /// The user's choice (what is saved), not the forced theme
    [[nodiscard]] static auto Current() -> Theme;
    /// Is the theme on screen the dark one?
    [[nodiscard]] static auto IsDark() -> bool;

    [[nodiscard]] static auto FromName(const std::string &name) -> Theme;
    [[nodiscard]] static auto Name(Theme theme) -> std::string;
};
} // namespace Dup::Ui
