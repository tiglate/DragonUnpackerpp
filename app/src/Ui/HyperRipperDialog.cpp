// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Ui/HyperRipperDialog.h"

#include "Core/HrfFile.h"
#include "Core/RipNaming.h"
#include "Ui/ArchiveTableModel.h"
#include "Ui/MpegOptionsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <filesystem>
#include <fstream>

namespace Dup::Ui
{
namespace
{
constexpr const char *KEY = "hr.";

auto Get(const Core::AppSettings *settings, const std::string &name, const std::string &fallback) -> std::string
{
    if (settings != nullptr)
    {
        const auto found = settings->Layout.find(KEY + name);
        if (found != settings->Layout.end())
        {
            return found->second;
        }
    }
    return fallback;
}

auto GetInt(const Core::AppSettings *settings, const std::string &name, std::int64_t fallback) -> std::int64_t
{
    try
    {
        return std::stoll(Get(settings, name, std::to_string(fallback)));
    }
    catch (const std::exception &)
    {
        return fallback;
    }
}

auto FormatDuration(std::int64_t seconds) -> QString
{
    return QString("%1:%2:%3").arg(seconds / 3600).arg(seconds / 60 % 60, 2, 10, QChar('0')).arg(seconds % 60, 2, 10, QChar('0'));
}
} // namespace

HyperRipperDialog::HyperRipperDialog(const QString &initialFile, Core::AppSettings *settings, QWidget *parent)
    : QDialog(parent), _settings(settings), _signatures(Core::HyperRipper::Signatures()), _file(new QLineEdit(initialFile, this)), _types(new QListWidget(this)),
      _excludeNoisy(new QCheckBox(tr("Leave out the formats that give many false positives"), this)), _configure(new QPushButton(tr("MP3 / AAC options..."), this)),
      _minSize(new QSpinBox(this)), _bufferKiB(new QSpinBox(this)), _autoStart(new QCheckBox(tr("Start the scan as soon as this window opens on a file"), this)),
      _openWhenDone(new QCheckBox(tr("Open the files found as soon as the scan ends"), this)),
      _template(new QLineEdit(Core::NamingOptions::DEFAULT_RIP_TEMPLATE, this)), _customNames(new QCheckBox(tr("Name the files with this template:"), this)),
      _makeDirs(new QCheckBox(tr("Put the files in the folders Audio, Video, Image and Unknown"), this)),
      _hrfGroup(new QGroupBox(tr("Save an HRF file (to open the finds again without scanning)"), this)), _hrfFile(new QLineEdit(this)), _hrfVersion(new QComboBox(this)),
      _hrfInfo(new QCheckBox(tr("Add information:"), this)), _hrfTitle(new QLineEdit(this)), _hrfAuthor(new QLineEdit(this)), _hrfUrl(new QLineEdit(this)),
      _hrfNoProgram(new QCheckBox(tr("Do not store the program and its version (version 3)"), this)), _progress(new QProgressBar(this)), _status(new QLabel(this)),
      _stats(new QLabel(this)), _hexDump(new QLabel(this)), _start(new QPushButton(tr("Scan"), this)), _open(new QPushButton(tr("Open the files found"), this)),
      _timer(new QTimer(this))
{
    setWindowTitle(tr("HyperRipper"));
    resize(620, 640);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Looks for files embedded in any file by their signatures (images, sounds, videos, archives...)."), this));

    auto *fileRow = new QHBoxLayout;
    auto *browse = new QPushButton(tr("Browse..."), this);
    fileRow->addWidget(_file, 1);
    fileRow->addWidget(browse);
    layout->addLayout(fileRow);

    auto *tabs = new QTabWidget(this);
    layout->addWidget(tabs, 1);

    // formats
    auto *formats = new QWidget(this);
    auto *formatsLayout = new QVBoxLayout(formats);
    for (const auto &signature : _signatures)
    {
        auto *item = new QListWidgetItem(QString("%1: %2").arg(QString::fromStdString(signature.Type), QString::fromStdString(signature.Description)), _types);
        item->setData(Qt::UserRole, QString::fromStdString(signature.Type));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
    }
    formatsLayout->addWidget(_types, 1);
    auto *selectRow = new QHBoxLayout;
    const std::pair<QString, unsigned> kinds[] = {{tr("All"), 0}, {tr("Images"), Core::RIPPER_IMAGE}, {tr("Videos"), Core::RIPPER_VIDEO}, {tr("Audio"), Core::RIPPER_AUDIO}};
    for (const auto &[text, kind] : kinds)
    {
        auto *button = new QPushButton(text, this);
        selectRow->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, kind = kind] { SelectKinds(kind); });
    }
    selectRow->addStretch(1);
    selectRow->addWidget(_configure);
    formatsLayout->addLayout(selectRow);
    formatsLayout->addWidget(_excludeNoisy);
    tabs->addTab(formats, tr("Formats"));

    // names
    auto *names = new QWidget(this);
    auto *namesLayout = new QVBoxLayout(names);
    auto *templateRow = new QHBoxLayout;
    templateRow->addWidget(_customNames);
    templateRow->addWidget(_template, 1);
    namesLayout->addLayout(templateRow);
    auto *help = new QLabel(tr("%f name of the scanned file, %x its extension, %o offset, %h offset in hexadecimal, %s size, %l size in hexadecimal, %n number of the find. "
                               "The default is %f_%x-%n."),
                            this);
    help->setWordWrap(true);
    namesLayout->addWidget(help);
    namesLayout->addWidget(_makeDirs);
    namesLayout->addStretch(1);
    tabs->addTab(names, tr("Names"));

    // HRF file
    _hrfGroup->setCheckable(true);
    _hrfGroup->setChecked(false);
    auto *hrfLayout = new QFormLayout(_hrfGroup);
    auto *hrfFileRow = new QHBoxLayout;
    auto *hrfBrowse = new QPushButton(tr("Browse..."), this);
    hrfFileRow->addWidget(_hrfFile, 1);
    hrfFileRow->addWidget(hrfBrowse);
    hrfLayout->addRow(tr("File"), hrfFileRow);
    _hrfVersion->addItem(tr("Version 3.0 (64 bits offsets, long names)"), 3);
    _hrfVersion->addItem(tr("Version 2 (names of 63 characters)"), 2);
    _hrfVersion->addItem(tr("Version 1 (names of 31 characters)"), 1);
    hrfLayout->addRow(tr("Format"), _hrfVersion);
    hrfLayout->addRow(_hrfInfo);
    hrfLayout->addRow(tr("Title"), _hrfTitle);
    hrfLayout->addRow(tr("Author"), _hrfAuthor);
    hrfLayout->addRow(tr("URL"), _hrfUrl);
    hrfLayout->addRow(_hrfNoProgram);
    _hrfTitle->setMaxLength(63);
    _hrfAuthor->setMaxLength(63);
    _hrfUrl->setMaxLength(127);
    auto *hrfPage = new QWidget(this);
    auto *hrfPageLayout = new QVBoxLayout(hrfPage);
    hrfPageLayout->addWidget(_hrfGroup);
    hrfPageLayout->addStretch(1);
    tabs->addTab(hrfPage, tr("HRF file"));

    // scan options
    auto *scan = new QWidget(this);
    auto *scanLayout = new QFormLayout(scan);
    _minSize->setRange(1, 1 << 30);
    _minSize->setValue(64);
    _minSize->setSuffix(tr(" bytes"));
    scanLayout->addRow(tr("Ignore finds smaller than"), _minSize);
    _bufferKiB->setRange(4, 262144);
    _bufferKiB->setValue(4096);
    _bufferKiB->setSuffix(tr(" KiB"));
    _bufferKiB->setToolTip(tr("What is read from the file at once: only the speed changes, not the finds"));
    scanLayout->addRow(tr("Read buffer"), _bufferKiB);
    scanLayout->addRow(_autoStart);
    scanLayout->addRow(_openWhenDone);
    tabs->addTab(scan, tr("Options"));

    _progress->setRange(0, 1000);
    layout->addWidget(_progress);
    layout->addWidget(_status);
    layout->addWidget(_stats);
    _hexDump->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    layout->addWidget(_hexDump);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch(1);
    buttons->addWidget(_open);
    buttons->addWidget(_start);
    auto *close = new QPushButton(tr("Close"), this);
    buttons->addWidget(close);
    layout->addLayout(buttons);
    _open->setEnabled(false);

    connect(browse, &QPushButton::clicked, this, &HyperRipperDialog::Browse);
    connect(_start, &QPushButton::clicked, this, &HyperRipperDialog::StartOrCancel);
    connect(close, &QPushButton::clicked, this, &QDialog::close);
    connect(_open, &QPushButton::clicked, this, [this] {
        if (_result != nullptr)
        {
            emit OpenResults(_result);
            accept();
        }
    });
    connect(_timer, &QTimer::timeout, this, &HyperRipperDialog::Poll);
    connect(hrfBrowse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getSaveFileName(this, tr("HRF file"), _hrfFile->text(), tr("HyperRipper files (*.hrf)"));
        if (!path.isEmpty())
        {
            _hrfFile->setText(path);
            _hrfFile->setProperty("automatic", false);
        }
    });
    connect(_hrfInfo, &QCheckBox::toggled, this, &HyperRipperDialog::UpdateHrfControls);
    connect(_hrfVersion, &QComboBox::currentIndexChanged, this, &HyperRipperDialog::UpdateHrfControls);
    connect(_customNames, &QCheckBox::toggled, _template, &QWidget::setEnabled);
    connect(_excludeNoisy, &QCheckBox::toggled, this, [this](bool on) {
        if (on)
        {
            ApplyFalsePositiveExclusion();
        }
    });
    connect(_configure, &QPushButton::clicked, this, &HyperRipperDialog::ConfigureMpeg);
    connect(_types, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
        const QString type = item != nullptr ? item->data(Qt::UserRole).toString() : QString();
        _configure->setEnabled(type == "MP3" || type == "AAC");
    });
    _configure->setEnabled(false);

    LoadSettings();
    _template->setEnabled(_customNames->isChecked());
    UpdateHrfControls();
    if (_excludeNoisy->isChecked())
    {
        ApplyFalsePositiveExclusion();
    }
    if (_hrfFile->text().isEmpty() && std::filesystem::is_regular_file(initialFile.toStdString()))
    {
        _hrfFile->setText(QString::fromStdString(std::filesystem::path(initialFile.toStdString()).replace_extension(".hrf").string()));
        _hrfFile->setProperty("automatic", true);
    }
    if (_autoStart->isChecked() && std::filesystem::is_regular_file(initialFile.toStdString()))
    {
        QTimer::singleShot(0, this, &HyperRipperDialog::StartOrCancel);
    }
}

HyperRipperDialog::~HyperRipperDialog()
{
    StoreSettings();
    if (_job != nullptr)
    {
        _job->Cancel();
    }
}

auto HyperRipperDialog::FindSignature(const QString &type) const -> const Core::SignatureInfo *
{
    for (const auto &signature : _signatures)
    {
        if (QString::fromStdString(signature.Type) == type)
        {
            return &signature;
        }
    }
    return nullptr;
}

void HyperRipperDialog::SelectKinds(unsigned kinds)
{
    for (int i = 0; i < _types->count(); ++i)
    {
        auto *item = _types->item(i);
        const auto *signature = FindSignature(item->data(Qt::UserRole).toString());
        const bool wanted = kinds == 0 || (signature != nullptr && (signature->Kinds & kinds) != 0);
        item->setCheckState(wanted && !(_excludeNoisy->isChecked() && signature != nullptr && signature->FalsePositives) ? Qt::Checked : Qt::Unchecked);
    }
}

void HyperRipperDialog::ApplyFalsePositiveExclusion()
{
    for (int i = 0; i < _types->count(); ++i)
    {
        const auto *signature = FindSignature(_types->item(i)->data(Qt::UserRole).toString());
        if (signature != nullptr && signature->FalsePositives)
        {
            _types->item(i)->setCheckState(Qt::Unchecked);
        }
    }
}

void HyperRipperDialog::ConfigureMpeg()
{
    MpegOptionsDialog dialog(_mpeg, this);
    if (dialog.exec() == QDialog::Accepted)
    {
        _mpeg = dialog.GetOptions();
    }
}

void HyperRipperDialog::Browse()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("File to scan"), _file->text());
    if (!path.isEmpty())
    {
        _file->setText(path);
        if (_hrfFile->text().isEmpty() || _hrfFile->property("automatic").toBool())
        {
            _hrfFile->setText(QString::fromStdString(std::filesystem::path(path.toStdString()).replace_extension(".hrf").string()));
            _hrfFile->setProperty("automatic", true);
        }
    }
}

void HyperRipperDialog::UpdateHrfControls()
{
    const int version = _hrfVersion->currentData().toInt();
    _hrfInfo->setEnabled(version >= 2);
    const bool info = version >= 2 && _hrfInfo->isChecked();
    _hrfTitle->setEnabled(info);
    _hrfAuthor->setEnabled(info);
    _hrfUrl->setEnabled(info);
    _hrfNoProgram->setEnabled(version == 3);
}

void HyperRipperDialog::SaveHrf(const std::vector<Core::RippedFile> &files, const std::vector<std::string> &names)
{
    Core::HrfFile hrf;
    hrf.Version = _hrfVersion->currentData().toInt();
    const std::filesystem::path scanned = _file->text().toStdString();
    hrf.SourceName = scanned.filename().string();
    hrf.SourceSize = static_cast<std::int64_t>(std::filesystem::file_size(scanned));
    if (hrf.Version == 3 && _hrfNoProgram->isChecked())
    {
        hrf.ProgramId = 0;
        hrf.ProgramVersion = 0;
    }
    else
    {
        hrf.ProgramId = Core::HRF_PROGRAM_ID;
        hrf.ProgramVersion = Core::HRF_PROGRAM_VERSION;
    }
    if (hrf.Version >= 2 && _hrfInfo->isChecked())
    {
        hrf.Info = Core::HrfInfo{_hrfTitle->text().toStdString(), _hrfAuthor->text().toStdString(), _hrfUrl->text().toStdString()};
    }
    for (std::size_t i = 0; i < files.size(); ++i)
    {
        hrf.Entries.push_back({names[i], files[i].Offset, files[i].Size});
    }
    Core::WriteHrf(_hrfFile->text().toStdString(), hrf);
}

void HyperRipperDialog::SetRunning(bool running)
{
    _start->setText(running ? tr("Cancel") : tr("Scan"));
    for (QWidget *widget : std::initializer_list<QWidget *>{_file, _types, _minSize, _bufferKiB, _hrfGroup, _customNames, _makeDirs, _excludeNoisy, _configure, _autoStart, _openWhenDone})
    {
        widget->setEnabled(!running);
    }
    _template->setEnabled(!running && _customNames->isChecked());
    if (!running)
    {
        _configure->setEnabled(_types->currentItem() != nullptr && (_types->currentItem()->data(Qt::UserRole).toString() == "MP3" || _types->currentItem()->data(Qt::UserRole).toString() == "AAC"));
    }
}

auto HyperRipperDialog::GetOptions() const -> Core::HyperRipper::Options
{
    Core::HyperRipper::Options options;
    for (int i = 0; i < _types->count(); ++i)
    {
        if (_types->item(i)->checkState() == Qt::Checked)
        {
            options.Types.insert(_types->item(i)->data(Qt::UserRole).toString().toStdString());
        }
    }
    options.MinSize = _minSize->value();
    options.BlockSize = static_cast<std::size_t>(_bufferKiB->value()) * 1024;
    options.Mpeg = _mpeg;
    return options;
}

void HyperRipperDialog::StartOrCancel()
{
    if (_job != nullptr && !_job->IsDone())
    {
        _job->Cancel();
        return;
    }
    Core::HyperRipper::Options options = GetOptions();
    if (options.Types.empty())
    {
        _status->setText(tr("Select at least one type"));
        return;
    }
    StoreSettings();
    _result.reset();
    _open->setEnabled(false);
    _startedAt = std::chrono::steady_clock::now();
    _job = std::make_unique<Core::HyperRipperJob>(_file->text().toStdString(), options);
    _job->Start();
    SetRunning(true);
    _timer->start(100);
}

void HyperRipperDialog::ShowProgress(std::int64_t done, std::int64_t total)
{
    using namespace std::chrono;
    const double seconds = duration<double>(steady_clock::now() - _startedAt).count();
    const double speed = seconds > 0 ? static_cast<double>(done) / seconds : 0;
    const auto elapsed = static_cast<std::int64_t>(seconds);
    const QString remaining = speed > 0 ? FormatDuration(static_cast<std::int64_t>(static_cast<double>(total - done) / speed)) : QStringLiteral("...");
    _stats->setText(tr("Speed %1/s    Scanned %2    Left %3    Elapsed %4    Remaining %5")
                        .arg(ArchiveTableModel::FormatSize(static_cast<std::int64_t>(speed)), ArchiveTableModel::FormatSize(done), ArchiveTableModel::FormatSize(total - done),
                             FormatDuration(elapsed), remaining));
    // the first bytes at the position being scanned
    std::ifstream stream(_file->text().toStdString(), std::ios::binary);
    std::array<unsigned char, 18> bytes{};
    stream.seekg(done);
    stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    QString dump;
    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        dump += QString("%1 ").arg(i < static_cast<std::size_t>(stream.gcount()) ? bytes[i] : 0, 2, 16, QChar('0')).toUpper();
    }
    _hexDump->setText(dump);
}

void HyperRipperDialog::Poll()
{
    const auto total = _job->GetBytesTotal();
    const auto done = _job->GetBytesDone();
    _progress->setValue(total > 0 ? static_cast<int>(done * 1000 / total) : 0);
    if (total > 0)
    {
        ShowProgress(done, total);
    }
    if (!_job->IsDone())
    {
        _status->setText(tr("Scanning: %1 of %2").arg(ArchiveTableModel::FormatSize(done), ArchiveTableModel::FormatSize(total)));
        return;
    }
    _timer->stop();
    SetRunning(false);
    if (!_job->GetError().empty())
    {
        _status->setText(tr("Cannot scan: %1").arg(QString::fromStdString(_job->GetError())));
        return;
    }
    if (_job->WasCancelled())
    {
        _status->setText(tr("Cancelled"));
        return;
    }
    auto results = _job->GetResults();
    _progress->setValue(1000);
    if (!results.empty())
    {
        Core::NamingOptions naming;
        if (_customNames->isChecked())
        {
            naming.Template = _template->text().toStdString();
        }
        naming.MakeDirs = _makeDirs->isChecked();
        auto names = Core::MakeRipNames(_job->GetFile(), results, naming);
        QString hrfMessage;
        if (_hrfGroup->isChecked())
        {
            try
            {
                SaveHrf(results, names);
                hrfMessage = tr(", saved in %1").arg(_hrfFile->text());
            }
            catch (const std::exception &error)
            {
                hrfMessage = tr(", but the HRF file was not saved: %1").arg(QString::fromStdString(error.what()));
            }
        }
        _status->setText(tr("%1 files found").arg(results.size()) + hrfMessage);
        _result = std::make_shared<Core::RippedArchiveSource>(_job->GetFile(), std::move(results), std::move(names));
        _open->setEnabled(true);
        if (_openWhenDone->isChecked())
        {
            emit OpenResults(_result);
            accept();
        }
    }
    else
    {
        _status->setText(tr("Nothing found"));
    }
}

void HyperRipperDialog::LoadSettings()
{
    if (_settings == nullptr)
    {
        return;
    }
    const std::string unchecked = Get(_settings, "unchecked", "\n");
    if (unchecked != "\n")
    {
        for (int i = 0; i < _types->count(); ++i)
        {
            const std::string type = _types->item(i)->data(Qt::UserRole).toString().toStdString();
            const bool off = ("|" + unchecked + "|").find("|" + type + "|") != std::string::npos;
            _types->item(i)->setCheckState(off ? Qt::Unchecked : Qt::Checked);
        }
    }
    _excludeNoisy->setChecked(GetInt(_settings, "excludeNoisy", 0) != 0);
    _minSize->setValue(static_cast<int>(GetInt(_settings, "minSize", 64)));
    _bufferKiB->setValue(static_cast<int>(GetInt(_settings, "bufferKiB", 4096)));
    _autoStart->setChecked(GetInt(_settings, "autoStart", 0) != 0);
    _openWhenDone->setChecked(GetInt(_settings, "openWhenDone", 0) != 0);
    _customNames->setChecked(GetInt(_settings, "customNames", 0) != 0);
    _template->setText(QString::fromStdString(Get(_settings, "template", Core::NamingOptions::DEFAULT_RIP_TEMPLATE)));
    _makeDirs->setChecked(GetInt(_settings, "makeDirs", 0) != 0);
    _hrfGroup->setChecked(GetInt(_settings, "hrf", 0) != 0);
    const int version = static_cast<int>(GetInt(_settings, "hrfVersion", 3));
    _hrfVersion->setCurrentIndex(std::max(0, _hrfVersion->findData(version)));
    _hrfInfo->setChecked(GetInt(_settings, "hrfInfo", 0) != 0);
    _hrfTitle->setText(QString::fromStdString(Get(_settings, "hrfTitle", "")));
    _hrfAuthor->setText(QString::fromStdString(Get(_settings, "hrfAuthor", "")));
    _hrfUrl->setText(QString::fromStdString(Get(_settings, "hrfUrl", "")));
    _hrfNoProgram->setChecked(GetInt(_settings, "hrfNoProgram", 0) != 0);
    for (int v = 0; v < 3; ++v)
    {
        for (int l = 0; l < 3; ++l)
        {
            _mpeg.Layers[v][l] = GetInt(_settings, "mpeg.layer" + std::to_string(v) + std::to_string(l), _mpeg.Layers[v][l] ? 1 : 0) != 0;
        }
    }
    _mpeg.LimitFramesMin = GetInt(_settings, "mpeg.limitFramesMin", _mpeg.LimitFramesMin) != 0;
    _mpeg.FramesMin = static_cast<std::uint32_t>(std::max<std::int64_t>(1, GetInt(_settings, "mpeg.framesMin", _mpeg.FramesMin)));
    _mpeg.LimitFramesMax = GetInt(_settings, "mpeg.limitFramesMax", _mpeg.LimitFramesMax) != 0;
    _mpeg.FramesMax = static_cast<std::uint32_t>(std::max<std::int64_t>(_mpeg.FramesMin, GetInt(_settings, "mpeg.framesMax", _mpeg.FramesMax)));
    _mpeg.LimitSizeMin = GetInt(_settings, "mpeg.limitSizeMin", _mpeg.LimitSizeMin) != 0;
    _mpeg.SizeMin = std::max<std::int64_t>(1, GetInt(_settings, "mpeg.sizeMin", _mpeg.SizeMin));
    _mpeg.LimitSizeMax = GetInt(_settings, "mpeg.limitSizeMax", _mpeg.LimitSizeMax) != 0;
    _mpeg.SizeMax = std::max<std::int64_t>(_mpeg.SizeMin, GetInt(_settings, "mpeg.sizeMax", _mpeg.SizeMax));
    _mpeg.XingVbr = GetInt(_settings, "mpeg.xing", _mpeg.XingVbr) != 0;
    _mpeg.Id3Tag = GetInt(_settings, "mpeg.id3", _mpeg.Id3Tag) != 0;
}

void HyperRipperDialog::StoreSettings() const
{
    if (_settings == nullptr)
    {
        return;
    }
    auto put = [this](const std::string &name, const std::string &value) { _settings->Layout[KEY + name] = value; };
    auto flag = [&](const std::string &name, bool value) { put(name, value ? "1" : "0"); };
    std::string unchecked;
    for (int i = 0; i < _types->count(); ++i)
    {
        if (_types->item(i)->checkState() != Qt::Checked)
        {
            unchecked += (unchecked.empty() ? "" : "|") + _types->item(i)->data(Qt::UserRole).toString().toStdString();
        }
    }
    put("unchecked", unchecked);
    flag("excludeNoisy", _excludeNoisy->isChecked());
    put("minSize", std::to_string(_minSize->value()));
    put("bufferKiB", std::to_string(_bufferKiB->value()));
    flag("autoStart", _autoStart->isChecked());
    flag("openWhenDone", _openWhenDone->isChecked());
    flag("customNames", _customNames->isChecked());
    put("template", _template->text().toStdString());
    flag("makeDirs", _makeDirs->isChecked());
    flag("hrf", _hrfGroup->isChecked());
    put("hrfVersion", std::to_string(_hrfVersion->currentData().toInt()));
    flag("hrfInfo", _hrfInfo->isChecked());
    put("hrfTitle", _hrfTitle->text().toStdString());
    put("hrfAuthor", _hrfAuthor->text().toStdString());
    put("hrfUrl", _hrfUrl->text().toStdString());
    flag("hrfNoProgram", _hrfNoProgram->isChecked());
    for (int v = 0; v < 3; ++v)
    {
        for (int l = 0; l < 3; ++l)
        {
            flag("mpeg.layer" + std::to_string(v) + std::to_string(l), _mpeg.Layers[v][l]);
        }
    }
    flag("mpeg.limitFramesMin", _mpeg.LimitFramesMin);
    put("mpeg.framesMin", std::to_string(_mpeg.FramesMin));
    flag("mpeg.limitFramesMax", _mpeg.LimitFramesMax);
    put("mpeg.framesMax", std::to_string(_mpeg.FramesMax));
    flag("mpeg.limitSizeMin", _mpeg.LimitSizeMin);
    put("mpeg.sizeMin", std::to_string(_mpeg.SizeMin));
    flag("mpeg.limitSizeMax", _mpeg.LimitSizeMax);
    put("mpeg.sizeMax", std::to_string(_mpeg.SizeMax));
    flag("mpeg.xing", _mpeg.XingVbr);
    flag("mpeg.id3", _mpeg.Id3Tag);
}
} // namespace Dup::Ui
