// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Ui/OptionsDialog.h"

#include "Ui/Languages.h"
#include "Ui/ListTemplates.h"

#include "Core/ExternalTools.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QSpinBox>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace Dup::Ui
{
namespace
{
void SelectData(QComboBox *combo, const QString &data)
{
    const int index = combo->findData(data);
    combo->setCurrentIndex(index >= 0 ? index : 0);
}
} // namespace

OptionsDialog::OptionsDialog(Core::AppSettings &settings, const Core::DriverHost &driverHost, QWidget *parent)
    : QDialog(parent), _settings(settings), _theme(new QComboBox(this)), _language(new QComboBox(this)), _view(new QComboBox(this)),
      _thumbnailSize(new QSpinBox(this)), _imageMegabytes(new QSpinBox(this)), _textKilobytes(new QSpinBox(this)), _logLevel(new QComboBox(this))
{
    setWindowTitle(tr("Options"));
    resize(620, 480);
    auto *layout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    layout->addWidget(tabs, 1);

    // General
    auto *general = new QWidget(this);
    auto *generalForm = new QFormLayout(general);
    _theme->addItem(tr("As the system"), "system");
    _theme->addItem(tr("Dark"), "dark");
    _theme->addItem(tr("Light"), "light");
    SelectData(_theme, QString::fromStdString(settings.Theme));
    _language->addItem(tr("Language of the system"), "");
    for (const auto &language : GetLanguages())
    {
        _language->addItem(QString::fromUtf8(language.NativeName), language.Code);
    }
    SelectData(_language, QString::fromStdString(settings.Language));
    _view->addItem(tr("Details"), "details");
    _view->addItem(tr("Thumbnails"), "thumbnails");
    SelectData(_view, QString::fromStdString(settings.DefaultView));
    _thumbnailSize->setRange(48, 256);
    _thumbnailSize->setSuffix(" px");
    _thumbnailSize->setValue(settings.ThumbnailSize);
    generalForm->addRow(tr("Theme"), _theme);
    generalForm->addRow(tr("Language (applied at the next start)"), _language);
    generalForm->addRow(tr("Entries are shown as"), _view);
    generalForm->addRow(tr("Thumbnail size"), _thumbnailSize);
    _toolsDirectory = new QLineEdit(QString::fromStdString(settings.ToolsDirectory), this);
    _toolsDirectory->setPlaceholderText(QString::fromStdString(Core::DefaultToolsDirectory(settings.GetFile()).string()));
    _toolsDirectory->setToolTip(tr("The folder of the .ini files that describe the command line programs used by File > Extract, converted with..."));
    generalForm->addRow(tr("Folder of the conversion tools"), _toolsDirectory);
    _detectedExtension = new QCheckBox(tr("When extracting, give the files without an extension the one their content says"), this);
    _detectedExtension->setChecked(settings.AddDetectedExtension);
    _detectedExtension->setToolTip(tr("For the archives that name their files with numbers or hashes: a PNG image becomes name.png (only when the type is certain)"));
    generalForm->addRow(_detectedExtension);
    _templatesDirectory = new QLineEdit(QString::fromStdString(settings.TemplatesDirectory), this);
    _templatesDirectory->setPlaceholderText(QString::fromStdString(DefaultTemplatesDirectory(settings.GetFile()).string()));
    _templatesDirectory->setToolTip(tr("The templates of your own for File > Create a list from a template (.uht files, or folders with a .tsf source)"));
    generalForm->addRow(tr("Folder of the list templates"), _templatesDirectory);
    tabs->addTab(general, tr("General"));

    // Preview
    auto *preview = new QWidget(this);
    auto *previewForm = new QFormLayout(preview);
    _imageMegabytes->setRange(1, 1024);
    _imageMegabytes->setSuffix(" MB");
    _imageMegabytes->setValue(settings.PreviewImageMegabytes);
    _textKilobytes->setRange(1, 65536);
    _textKilobytes->setSuffix(" KB");
    _textKilobytes->setValue(settings.PreviewTextKilobytes);
    previewForm->addRow(tr("Largest image to preview"), _imageMegabytes);
    previewForm->addRow(tr("Largest text to preview"), _textKilobytes);
    auto *paletteRow = new QHBoxLayout;
    _palette = new QLineEdit(QString::fromStdString(settings.PalettePath), this);
    _palette->setToolTip(tr("Used for the Quake pictures and the Build tiles when the archive has no PALETTE.DAT or PALETTE.LMP (768 bytes, or a JASC .pal file)"));
    auto *paletteBrowse = new QPushButton(tr("Browse..."), this);
    paletteRow->addWidget(_palette, 1);
    paletteRow->addWidget(paletteBrowse);
    connect(paletteBrowse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, tr("Palette"), _palette->text(), tr("Palettes (*.pal *.lmp *.dat *.bin *.raw);;All files (*)"));
        if (!path.isEmpty())
        {
            _palette->setText(path);
        }
    });
    previewForm->addRow(tr("Palette of the 8 bits pictures"), paletteRow);
    tabs->addTab(preview, tr("Preview"));

    // Log
    auto *log = new QWidget(this);
    auto *logForm = new QFormLayout(log);
    for (const char *level : {"trace", "debug", "info", "warn", "error"})
    {
        _logLevel->addItem(level, level);
    }
    SelectData(_logLevel, QString::fromStdString(settings.LogLevel));
    logForm->addRow(tr("Messages shown in the Console"), _logLevel);
    tabs->addTab(log, tr("Log"));

    // Driver
    auto *plugins = new QWidget(this);
    auto *pluginLayout = new QVBoxLayout(plugins);
    if (driverHost.IsLoaded())
    {
        const auto &driver = driverHost.GetDriver();
        pluginLayout->addWidget(new QLabel(tr("%1 (%2 formats)\n%3").arg(QString::fromStdString(driver.GetName())).arg(driver.GetFormats().size()).arg(QString::fromStdString(driverHost.GetPath().string())), plugins));
        auto *formats = new QTreeWidget(plugins);
        formats->setHeaderLabels({tr("Format"), tr("Extensions")});
        formats->setRootIsDecorated(false);
        for (const auto &format : driver.GetFormats())
        {
            formats->addTopLevelItem(new QTreeWidgetItem({QString::fromStdString(format.Name), QString::fromStdString(format.Extensions).replace(';', ' ')}));
        }
        formats->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        pluginLayout->addWidget(formats, 1);
    }
    else
    {
        pluginLayout->addWidget(new QLabel(tr("The driver is not loaded: %1").arg(QString::fromStdString(driverHost.GetError())), plugins));
    }
    tabs->addTab(plugins, tr("Driver"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &OptionsDialog::Accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void OptionsDialog::Accept()
{
    _settings.Theme = _theme->currentData().toString().toStdString();
    _settings.Language = _language->currentData().toString().toStdString();
    _settings.DefaultView = _view->currentData().toString().toStdString();
    _settings.ThumbnailSize = _thumbnailSize->value();
    _settings.PreviewImageMegabytes = _imageMegabytes->value();
    _settings.PreviewTextKilobytes = _textKilobytes->value();
    _settings.PalettePath = _palette->text().toStdString();
    _settings.ToolsDirectory = _toolsDirectory->text().toStdString();
    _settings.AddDetectedExtension = _detectedExtension->isChecked();
    _settings.TemplatesDirectory = _templatesDirectory->text().toStdString();
    _settings.LogLevel = _logLevel->currentData().toString().toStdString();
    accept();
}
} // namespace Dup::Ui
