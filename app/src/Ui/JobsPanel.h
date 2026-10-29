// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/ExtractionJob.h"

#include <QWidget>

#include <memory>
#include <vector>

class QProgressBar;
class QPushButton;
class QLabel;
class QTimer;
class QVBoxLayout;

namespace Dup::Ui
{
/// The "Jobs" panel: one row per extraction (title, progress, current entry, Cancel / Dismiss). The jobs run on their own
/// threads; the panel only polls their state every 100 ms.
class JobsPanel : public QWidget
{
    Q_OBJECT

  public:
    explicit JobsPanel(QWidget *parent = nullptr);
    ~JobsPanel() override;

    void AddJob(std::unique_ptr<Core::ExtractionJob> job);
    [[nodiscard]] auto HasRunningJobs() const -> bool;
    void CancelAll();
    /// Waits (processing events) until no job runs; for the command line hooks and the closing of the window.
    void WaitForAll();
    /// Total of the statistics of the finished jobs since the start.
    [[nodiscard]] auto GetFailedEntries() const -> int
    {
        return _failedEntries;
    }

  signals:
    /// A job ended: one line for the log, and whether everything went well.
    void JobEnded(const QString &summary, bool success);

  private:
    struct Row
    {
        std::unique_ptr<Core::ExtractionJob> Job;
        QWidget *Widget{nullptr};
        QLabel *Title{nullptr};
        QProgressBar *Bar{nullptr};
        QLabel *Status{nullptr};
        QPushButton *Cancel{nullptr};
        bool Reported{false};
    };

    void Refresh();
    void Report(Row &row);

    QVBoxLayout *_layout;
    QTimer *_timer;
    std::vector<std::unique_ptr<Row>> _rows;
    int _failedEntries{0};
};
} // namespace Dup::Ui
