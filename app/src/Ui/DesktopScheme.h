// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <functional>
#include <optional>

namespace Dup::Ui
{
/// The light/dark preference of the desktop from the freedesktop settings portal over D-Bus (GNOME, KDE...); only
/// built when Qt DBus is there (DUP_HAVE_DBUS). Returns the current preference (true: dark, nullopt: none or no
/// portal) and calls onChange whenever the desktop changes it (a desktop that switches on a schedule).
auto WatchDesktopScheme(std::function<void(std::optional<bool>)> onChange) -> std::optional<bool>;
} // namespace Dup::Ui
