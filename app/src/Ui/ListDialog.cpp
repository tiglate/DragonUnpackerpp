// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Ui/ListDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

namespace Dup::Ui
{
namespace
{
constexpr const char *KEY = "list.";

auto Get(const Core::AppSettings *settings, const std::string &name, int fallback) -> int
{
    if (settings != nullptr)
    {
        const auto found = settings->Layout.find(KEY + name);
        if (found != settings->Layout.end())
        {
            try
            {
                return std::stoi(found->second);
            }
            catch (const std::exception &)
            {
            }
        }
    }
    return fallback;
}
} // namespace

ListDialog::ListDialog(const std::vector<LoadedTemplate> &templates, int selectedCount, Core::AppSettings *settings, QWidget *parent)
    : QDialog(parent), _templates(templates), _settings(settings), _list(new QListWidget(this)), _preview(new QLabel(this)), _details(new QLabel(this)),
      _whole(new QRadioButton(tr("Every entry"), this)), _folder(new QRadioButton(tr("The current folder"), this)),
      _selection(new QRadioButton(tr("The selected entries (%1)").arg(selectedCount), this)), _subfolders(new QCheckBox(tr("Include the subfolders"), this)),
      _sort(new QCheckBox(tr("Sort the list"), this)), _byName(new QRadioButton(tr("by name"), this)), _bySize(new QRadioButton(tr("by size"), this)),
      _byOffset(new QRadioButton(tr("by offset"), this)), _invert(new QCheckBox(tr("Invert the order"), this))
{
    setWindowTitle(tr("Create a list from a template"));
    resize(640, 520);
    auto *layout = new QVBoxLayout(this);

    auto *top = new QHBoxLayout;
    _list->setMaximumWidth(220);
    top->addWidget(_list);
    auto *right = new QVBoxLayout;
    _preview->setMinimumSize(320, 200);
    _preview->setAlignment(Qt::AlignCenter);
    _preview->setFrameShape(QFrame::StyledPanel);
    _details->setWordWrap(true);
    _details->setTextInteractionFlags(Qt::TextSelectableByMouse);
    right->addWidget(_preview, 1);
    right->addWidget(_details);
    top->addLayout(right, 1);
    layout->addLayout(top, 1);

    for (const auto &item : _templates)
    {
        _list->addItem(QString::fromStdString(item.Template.Info.Name));
    }

    auto *scope = new QGroupBox(tr("Entries"), this);
    auto *scopeLayout = new QVBoxLayout(scope);
    scopeLayout->addWidget(_whole);
    scopeLayout->addWidget(_folder);
    auto *folderRow = new QHBoxLayout;
    folderRow->addSpacing(24);
    folderRow->addWidget(_subfolders);
    scopeLayout->addLayout(folderRow);
    scopeLayout->addWidget(_selection);
    _selection->setEnabled(selectedCount > 0);
    layout->addWidget(scope);

    auto *order = new QGroupBox(tr("Order"), this);
    auto *orderLayout = new QHBoxLayout(order);
    orderLayout->addWidget(_sort);
    orderLayout->addWidget(_byName);
    orderLayout->addWidget(_bySize);
    orderLayout->addWidget(_byOffset);
    orderLayout->addWidget(_invert);
    layout->addWidget(order);

    auto *buttons = new QDialogButtonBox(this);
    auto *create = buttons->addButton(tr("Create..."), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    create->setEnabled(!_templates.empty());

    connect(_list, &QListWidget::currentRowChanged, this, &ListDialog::ShowTemplate);
    connect(_sort, &QCheckBox::toggled, this, &ListDialog::UpdateSortControls);
    connect(_folder, &QRadioButton::toggled, _subfolders, &QWidget::setEnabled);

    const int scopeValue = Get(settings, "scope", 0);
    (scopeValue == 1 ? _folder : scopeValue == 2 && selectedCount > 0 ? _selection : _whole)->setChecked(true);
    _subfolders->setChecked(Get(settings, "subfolders", 1) != 0);
    _subfolders->setEnabled(_folder->isChecked());
    _sort->setChecked(Get(settings, "sort", 0) != 0);
    const int by = Get(settings, "sortBy", 0);
    (by == 1 ? _bySize : by == 2 ? _byOffset : _byName)->setChecked(true);
    _invert->setChecked(Get(settings, "invert", 0) != 0);
    UpdateSortControls();
    if (_list->count() > 0)
    {
        _list->setCurrentRow(std::clamp(Get(settings, "template", 0), 0, _list->count() - 1));
    }
}

ListDialog::~ListDialog()
{
    if (_settings == nullptr)
    {
        return;
    }
    auto put = [this](const std::string &name, int value) { _settings->Layout[KEY + name] = std::to_string(value); };
    put("scope", _folder->isChecked() ? 1 : _selection->isChecked() ? 2 : 0);
    put("subfolders", _subfolders->isChecked() ? 1 : 0);
    put("sort", _sort->isChecked() ? 1 : 0);
    put("sortBy", _bySize->isChecked() ? 1 : _byOffset->isChecked() ? 2 : 0);
    put("invert", _invert->isChecked() ? 1 : 0);
    put("template", std::max(0, _list->currentRow()));
}

void ListDialog::UpdateSortControls()
{
    for (QWidget *widget : std::initializer_list<QWidget *>{_byName, _bySize, _byOffset, _invert})
    {
        widget->setEnabled(_sort->isChecked());
    }
}

void ListDialog::ShowTemplate(int row)
{
    if (row < 0 || static_cast<std::size_t>(row) >= _templates.size())
    {
        return;
    }
    const auto &info = _templates[static_cast<std::size_t>(row)].Template.Info;
    QPixmap picture;
    const auto &preview = _templates[static_cast<std::size_t>(row)].Template.Preview;
    if (!preview.empty() && picture.loadFromData(reinterpret_cast<const uchar *>(preview.data()), static_cast<uint>(preview.size())))
    {
        _preview->setPixmap(picture.scaled(_preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    else
    {
        _preview->clear();
    }
    QString text = tr("Version: %1\nAuthor: %2").arg(QString::fromStdString(info.Version), QString::fromStdString(info.Author));
    if (!info.Email.empty())
    {
        text += "\n" + tr("Email: %1").arg(QString::fromStdString(info.Email));
    }
    if (!info.Url.empty())
    {
        text += "\n" + tr("URL: %1").arg(QString::fromStdString(info.Url));
    }
    text += "\n" + QString::fromStdString(info.ExtensionInfo);
    _details->setText(text);
}

auto ListDialog::GetTemplateIndex() const -> int
{
    return _list->currentRow();
}

auto ListDialog::GetScope() const -> ListScope
{
    return _folder->isChecked() ? ListScope::CURRENT_FOLDER : _selection->isChecked() ? ListScope::SELECTION : ListScope::WHOLE_ARCHIVE;
}

auto ListDialog::IncludesSubfolders() const -> bool
{
    return _subfolders->isChecked();
}

auto ListDialog::GetSort() const -> Core::ListSort
{
    if (!_sort->isChecked())
    {
        return Core::ListSort::NONE;
    }
    return _bySize->isChecked() ? Core::ListSort::SIZE : _byOffset->isChecked() ? Core::ListSort::OFFSET : Core::ListSort::NAME;
}

auto ListDialog::IsSortInverted() const -> bool
{
    return _invert->isChecked();
}
} // namespace Dup::Ui
