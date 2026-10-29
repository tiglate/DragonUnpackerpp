// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <spdlog/sinks/base_sink.h>

#include <QPlainTextEdit>
#include <QPointer>

#include <mutex>

namespace Dup::Ui
{
/// A spdlog sink that appends to a text widget (from any thread: the text is queued to the GUI thread).
class QtLogSink : public spdlog::sinks::base_sink<std::mutex>
{
  public:
    explicit QtLogSink(QPlainTextEdit *target);

  protected:
    void sink_it_(const spdlog::details::log_msg &message) override;
    void flush_() override
    {
    }

  private:
    QPointer<QPlainTextEdit> _target;
};
} // namespace Dup::Ui
