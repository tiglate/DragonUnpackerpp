// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Ui/JobsPanel.h"

#include "Ui/ArchiveTableModel.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace Dup::Ui
{
JobsPanel::JobsPanel(QWidget *parent) : QWidget(parent), _layout(new QVBoxLayout), _timer(new QTimer(this))
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *area = new QScrollArea(this);
    area->setWidgetResizable(true);
    auto *content = new QWidget(area);
    _layout->setContentsMargins(6, 6, 6, 6);
    _layout->setSpacing(6);
    _layout->addStretch(1);
    content->setLayout(_layout);
    area->setWidget(content);
    outer->addWidget(area);
    connect(_timer, &QTimer::timeout, this, &JobsPanel::Refresh);
    _timer->start(100);
}

JobsPanel::~JobsPanel()
{
    CancelAll();
    _rows.clear(); // the jobs join their threads
}

void JobsPanel::AddJob(std::unique_ptr<Core::ExtractionJob> job)
{
    auto row = std::make_unique<Row>();
    row->Widget = new QWidget(this);
    auto *layout = new QHBoxLayout(row->Widget);
    layout->setContentsMargins(0, 0, 0, 0);
    row->Title = new QLabel(QString::fromStdString(job->GetRequest().Title), row->Widget);
    row->Title->setMinimumWidth(160);
    row->Bar = new QProgressBar(row->Widget);
    row->Bar->setRange(0, 1000);
    row->Bar->setTextVisible(true);
    row->Status = new QLabel(row->Widget);
    row->Status->setMinimumWidth(200);
    row->Cancel = new QPushButton(tr("Cancel"), row->Widget);
    layout->addWidget(row->Title);
    layout->addWidget(row->Bar, 1);
    layout->addWidget(row->Status, 2);
    layout->addWidget(row->Cancel);
    _layout->insertWidget(0, row->Widget);

    row->Job = std::move(job);
    Row *raw = row.get();
    connect(row->Cancel, &QPushButton::clicked, this, [this, raw] {
        if (raw->Job->IsDone())
        {
            // Dismiss: forget the row
            raw->Widget->deleteLater();
            _rows.erase(std::remove_if(_rows.begin(), _rows.end(), [raw](const auto &r) { return r.get() == raw; }), _rows.end());
        }
        else
        {
            raw->Job->Cancel();
        }
    });
    row->Job->Start();
    _rows.push_back(std::move(row));
}

auto JobsPanel::HasRunningJobs() const -> bool
{
    return std::any_of(_rows.begin(), _rows.end(), [](const auto &row) { return !row->Job->IsDone(); });
}

void JobsPanel::CancelAll()
{
    for (auto &row : _rows)
    {
        row->Job->Cancel();
    }
}

void JobsPanel::WaitForAll()
{
    while (HasRunningJobs())
    {
        QApplication::processEvents(QEventLoop::AllEvents, 50);
        Refresh();
    }
    Refresh();
}

void JobsPanel::Report(Row &row)
{
    row.Reported = true;
    const auto stats = row.Job->GetStats();
    const QString title = QString::fromStdString(row.Job->GetRequest().Title);
    QString summary;
    bool success = false;
    switch (row.Job->GetState())
    {
    case Core::JobState::FINISHED:
        success = stats.Failed == 0;
        summary = tr("%1: %2 extracted, %3 skipped, %4 failed").arg(title).arg(stats.Extracted).arg(stats.Skipped).arg(stats.Failed);
        break;
    case Core::JobState::CANCELLED:
        summary = tr("%1: cancelled (%2 extracted)").arg(title).arg(stats.Extracted);
        break;
    default:
        summary = tr("%1: failed: %2").arg(title, QString::fromStdString(row.Job->GetError()));
        break;
    }
    _failedEntries += stats.Failed + (row.Job->GetState() == Core::JobState::FAILED ? 1 : 0);
    for (const auto &error : stats.Errors)
    {
        emit JobEnded(QString::fromStdString(error), false);
    }
    emit JobEnded(summary, success);
}

void JobsPanel::Refresh()
{
    for (auto &row : _rows)
    {
        const auto total = row->Job->GetBytesTotal();
        const auto done = row->Job->GetBytesDone();
        row->Bar->setValue(total > 0 ? static_cast<int>(done * 1000 / total) : (row->Job->IsDone() ? 1000 : 0));
        if (!row->Job->IsDone())
        {
            const auto current = QString::fromStdString(row->Job->GetCurrentEntry());
            row->Status->setText(tr("%1 of %2  %3").arg(ArchiveTableModel::FormatSize(done), ArchiveTableModel::FormatSize(total), current));
            row->Status->setToolTip(current);
            continue;
        }
        if (!row->Reported)
        {
            Report(*row);
            row->Cancel->setText(tr("Dismiss"));
        }
        const auto stats = row->Job->GetStats();
        switch (row->Job->GetState())
        {
        case Core::JobState::FINISHED:
            row->Status->setText(tr("Done: %1 extracted, %2 skipped, %3 failed").arg(stats.Extracted).arg(stats.Skipped).arg(stats.Failed));
            break;
        case Core::JobState::CANCELLED:
            row->Status->setText(tr("Cancelled"));
            break;
        default:
            row->Status->setText(tr("Failed: %1").arg(QString::fromStdString(row->Job->GetError())));
            break;
        }
    }
}
} // namespace Dup::Ui
