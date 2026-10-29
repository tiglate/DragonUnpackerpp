// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <QDialog>

namespace Dup::Ui
{
/// Help > About: the name and version, the credits (the original author and the port), the components and the license.
class AboutDialog : public QDialog
{
    Q_OBJECT

  public:
    /// driverText: the loaded driver, one line of rich text per fact (empty when none is loaded).
    AboutDialog(const QString &driverText, QWidget *parent = nullptr);
};
} // namespace Dup::Ui
