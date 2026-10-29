// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/AppSettings.h"
#include "Core/HyperRipper.h"

#include <QDialog>

#include <chrono>
#include <memory>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTimer;

namespace Dup::Ui
{
/// Tools > HyperRipper: choose a file and the types to look for, scan on a thread, then open the finds as an archive.
/// The choices are kept in the settings between two runs.
class HyperRipperDialog : public QDialog
{
    Q_OBJECT

  public:
    HyperRipperDialog(const QString &initialFile, Core::AppSettings *settings, QWidget *parent = nullptr);
    ~HyperRipperDialog() override;

  signals:
    /// The scan found files: the window opens them.
    void OpenResults(std::shared_ptr<const Core::ArchiveSource> source);

  private:
    void Browse();
    void StartOrCancel();
    void Poll();
    void SetRunning(bool running);
    void SaveHrf(const std::vector<Core::RippedFile> &files, const std::vector<std::string> &names);
    void UpdateHrfControls();
    void SelectKinds(unsigned kinds);
    void ApplyFalsePositiveExclusion();
    void ConfigureMpeg();
    void ShowProgress(std::int64_t done, std::int64_t total);
    void LoadSettings();
    void StoreSettings() const;
    [[nodiscard]] auto GetOptions() const -> Core::HyperRipper::Options;
    [[nodiscard]] auto FindSignature(const QString &type) const -> const Core::SignatureInfo *;

    Core::AppSettings *_settings;
    std::vector<Core::SignatureInfo> _signatures;
    Core::MpegOptions _mpeg;

    QLineEdit *_file;
    QListWidget *_types;
    QCheckBox *_excludeNoisy;
    QPushButton *_configure;
    QSpinBox *_minSize;
    QSpinBox *_bufferKiB;
    QCheckBox *_autoStart;
    QCheckBox *_openWhenDone;
    QLineEdit *_template;
    QCheckBox *_customNames;
    QCheckBox *_makeDirs;
    QGroupBox *_hrfGroup;
    QLineEdit *_hrfFile;
    QComboBox *_hrfVersion;
    QCheckBox *_hrfInfo;
    QLineEdit *_hrfTitle;
    QLineEdit *_hrfAuthor;
    QLineEdit *_hrfUrl;
    QCheckBox *_hrfNoProgram;
    QProgressBar *_progress;
    QLabel *_status;
    QLabel *_stats;
    QLabel *_hexDump;
    QPushButton *_start;
    QPushButton *_open;
    QTimer *_timer;
    std::chrono::steady_clock::time_point _startedAt;
    std::unique_ptr<Core::HyperRipperJob> _job;
    std::shared_ptr<const Core::ArchiveSource> _result;
};
} // namespace Dup::Ui
