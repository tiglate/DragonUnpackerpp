// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <QList>
#include <QString>

namespace Dup::Ui
{
struct LanguageInfo
{
    const char *Code;       // the name of the .ts file: "pt_BR", "es"...
    const char *NativeName; // as its speakers write it
    bool RightToLeft;
};

/// The languages of the interface (English is the language of the source texts), in the order of the options.
[[nodiscard]] auto GetLanguages() -> const QList<LanguageInfo> &;

/// The language of the interface for a language or a locale name ("es_MX", "zh-TW", "pt_PT", "he_IL"...): the nearest of
/// GetLanguages(), "en" when there is none.
[[nodiscard]] auto ResolveLanguage(const QString &name) -> QString;

/// Is it written from right to left (Arabic, Hebrew)?
[[nodiscard]] auto IsRightToLeft(const QString &code) -> bool;
} // namespace Dup::Ui
