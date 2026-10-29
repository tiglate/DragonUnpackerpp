// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Ui/Languages.h"

namespace Dup::Ui
{
auto GetLanguages() -> const QList<LanguageInfo> &
{
    static const QList<LanguageInfo> languages{
        {"en", "English", false},
        {"pt_BR", "Português (Brasil)", false},
        {"es", "Español", false},
        {"it", "Italiano", false},
        {"fr", "Français", false},
        {"de", "Deutsch", false},
        {"pl", "Polski", false},
        {"ru", "Русский", false},
        {"zh_CN", "中文（简体）", false},
        {"ja", "日本語", false},
        {"ar", "العربية", true},
        {"he", "עברית", true},
    };
    return languages;
}

auto ResolveLanguage(const QString &name) -> QString
{
    QString code = name;
    code.replace('-', '_');
    for (const auto &language : GetLanguages())
    {
        if (code.compare(QLatin1String(language.Code), Qt::CaseInsensitive) == 0)
        {
            return language.Code;
        }
    }
    const QString base = code.section('_', 0, 0).toLower();
    if (base == "pt")
    {
        return "pt_BR"; // the Portuguese of Portugal reads the Brazilian texts
    }
    if (base == "zh")
    {
        return "zh_CN"; // the traditional characters are not translated yet: the simplified ones are the nearest
    }
    if (base == "iw")
    {
        return "he"; // the old code of Hebrew
    }
    for (const auto &language : GetLanguages())
    {
        if (base == QLatin1String(language.Code))
        {
            return language.Code;
        }
    }
    return "en";
}

auto IsRightToLeft(const QString &code) -> bool
{
    for (const auto &language : GetLanguages())
    {
        if (code == QLatin1String(language.Code))
        {
            return language.RightToLeft;
        }
    }
    return false;
}
} // namespace Dup::Ui
