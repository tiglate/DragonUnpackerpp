// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/AppSettings.h"
#include "Core/DriverHost.h"

#include <QDialog>

class QComboBox;
class QCheckBox;
class QLineEdit;
class QSpinBox;

namespace Dup::Ui
{
/// Tools > Options: appearance and language, preview limits, log level, and the list of what the driver reads.
/// Changes the settings object when accepted; the window applies them (MainWindow::ApplySettings).
class OptionsDialog : public QDialog
{
    Q_OBJECT

  public:
    OptionsDialog(Core::AppSettings &settings, const Core::DriverHost &driverHost, QWidget *parent = nullptr);

  private:
    void Accept();

    Core::AppSettings &_settings;
    QComboBox *_theme;
    QComboBox *_language;
    QComboBox *_view;
    QSpinBox *_thumbnailSize;
    QSpinBox *_imageMegabytes;
    QLineEdit *_palette;
    QLineEdit *_toolsDirectory;
    QCheckBox *_detectedExtension;
    QLineEdit *_templatesDirectory;
    QSpinBox *_textKilobytes;
    QComboBox *_logLevel;
};
} // namespace Dup::Ui
