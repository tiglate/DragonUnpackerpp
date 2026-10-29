// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Ui/QtLogSink.h"

#include <QMetaObject>

namespace Dup::Ui
{
QtLogSink::QtLogSink(QPlainTextEdit *target) : _target(target)
{
}

void QtLogSink::sink_it_(const spdlog::details::log_msg &message)
{
    spdlog::memory_buf_t formatted;
    formatter_->format(message, formatted);
    if (_target == nullptr)
    {
        return;
    }
    const QString text = QString::fromUtf8(formatted.data(), static_cast<qsizetype>(formatted.size())).trimmed();
    QMetaObject::invokeMethod(
        _target.data(), [target = _target, text] {
            if (target != nullptr)
            {
                target->appendPlainText(text);
            }
        },
        Qt::QueuedConnection);
}
} // namespace Dup::Ui
