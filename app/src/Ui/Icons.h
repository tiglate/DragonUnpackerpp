// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

namespace Dup::Ui
{
/// Monochrome line icons drawn from embedded SVG (24 x 24 grid), painted in the colour given so that they follow the theme.
class Icons
{
  public:
    /// An icon by name ("open", "extract", "extract-all", "details", "thumbnails", "back", "forward", "up", "search", "options",
    /// "hyperripper", "file", "image", "sound", "text", "folder"); a null icon for an unknown name.
    [[nodiscard]] static auto Make(const QString &name, const QColor &colour) -> QIcon;
    /// The icon of a kind of entry by its file extension (lower case, no dot).
    [[nodiscard]] static auto ForExtension(const QString &extension, const QColor &colour) -> QIcon;
};
} // namespace Dup::Ui
