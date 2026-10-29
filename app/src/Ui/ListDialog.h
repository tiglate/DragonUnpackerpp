// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/AppSettings.h"
#include "Core/ListTemplate.h"
#include "Ui/ListTemplates.h"

#include <QDialog>

class QCheckBox;
class QLabel;
class QListWidget;
class QRadioButton;

namespace Dup::Ui
{
enum class ListScope
{
    WHOLE_ARCHIVE,
    CURRENT_FOLDER,
    SELECTION
};

/// File > Create a list from a template: the template, which entries and in which order (the "Create list" window of v5).
class ListDialog : public QDialog
{
    Q_OBJECT

  public:
    ListDialog(const std::vector<LoadedTemplate> &templates, int selectedCount, Core::AppSettings *settings, QWidget *parent = nullptr);
    ~ListDialog() override;

    [[nodiscard]] auto GetTemplateIndex() const -> int;
    [[nodiscard]] auto GetScope() const -> ListScope;
    [[nodiscard]] auto IncludesSubfolders() const -> bool;
    [[nodiscard]] auto GetSort() const -> Core::ListSort;
    [[nodiscard]] auto IsSortInverted() const -> bool;

  private:
    void ShowTemplate(int row);
    void UpdateSortControls();

    const std::vector<LoadedTemplate> &_templates;
    Core::AppSettings *_settings;
    QListWidget *_list;
    QLabel *_preview;
    QLabel *_details;
    QRadioButton *_whole;
    QRadioButton *_folder;
    QRadioButton *_selection;
    QCheckBox *_subfolders;
    QCheckBox *_sort;
    QRadioButton *_byName;
    QRadioButton *_bySize;
    QRadioButton *_byOffset;
    QCheckBox *_invert;
};
} // namespace Dup::Ui
