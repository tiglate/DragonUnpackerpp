// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Ui/MainWindow.h"

#include "Core/PreviewWorker.h"
#include "Ui/AboutDialog.h"
#include "Ui/ArchiveTableModel.h"
#include "Ui/FolderTreeModel.h"
#include "Core/EntryListExport.h"
#include "Core/Extractor.h"
#include "Core/HrfFile.h"
#include "Core/ListTemplate.h"
#include "Core/TextureConverters.h"
#include "Core/HyperRipper.h"
#include "Ui/HyperRipperDialog.h"
#include "Ui/ListDialog.h"
#include "Ui/ListTemplates.h"
#include "Ui/ImageViewer.h"
#include "Ui/Icons.h"
#include "Ui/OptionsDialog.h"
#include "Ui/PreviewPanel.h"
#include "Ui/JobsPanel.h"
#include "Ui/QtLogSink.h"
#include "Ui/ThemeManager.h"

#include <spdlog/spdlog.h>

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QApplication>
#include <QCheckBox>
#include <QDragEnterEvent>
#include <QTemporaryDir>
#include <QToolBar>
#include <QUrl>
#include <QDropEvent>
#include <QLineEdit>
#include <QListView>
#include <QSlider>
#include <QStackedWidget>
#include <QMimeData>
#include <QVBoxLayout>
#include <QShortcut>
#include <QDockWidget>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QFileDialog>
#include <QTimer>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QAbstractButton>
#include <QMenu>
#include <QMimeData>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QStatusBar>
#include <QTableView>
#include <QTreeView>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <numeric>
#include <set>
#include <string_view>

namespace Dup::Ui
{
namespace
{
} // namespace

MainWindow::MainWindow(Core::DriverHost &driverHost, Core::AppSettings &settings)
    : _driverHost(driverHost), _settings(settings), _tableModel(new ArchiveTableModel(this)), _treeModel(new FolderTreeModel(this)), _table(new QTableView(this)),
      _tree(new QTreeView(this)), _preview(new PreviewPanel(this)), _inspector(new QPlainTextEdit(this)),
      _console(new QPlainTextEdit(this)), _statusEntries(new QLabel(this)), _statusZoom(new QLabel(this)), _jobs(new JobsPanel(this))
{
    setWindowTitle("Dragon UnPACKer++");
    resize(1500, 900);

    // the Console panel is fed by the logger
    _console->setReadOnly(true);
    _console->setMaximumBlockCount(5000);
    spdlog::default_logger()->sinks().push_back(std::make_shared<QtLogSink>(_console));

    BuildMenus();
    BuildDocks();

    _table->setModel(_tableModel);
    _table->setColumnHidden(ArchiveTableModel::DETECTED, true);
    _table->setSelectionBehavior(QAbstractItemView::SelectRows);
    _table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    _table->setAlternatingRowColors(true);
    _table->setShowGrid(false);
    _table->setSortingEnabled(true);
    _table->verticalHeader()->hide();
    _table->verticalHeader()->setDefaultSectionSize(22);
    _table->horizontalHeader()->setStretchLastSection(false);
    _table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    _table->setColumnWidth(ArchiveTableModel::NAME, 380);
    _table->setColumnWidth(ArchiveTableModel::TYPE, 70);
    _table->setColumnWidth(ArchiveTableModel::SIZE, 90);
    _table->setColumnWidth(ArchiveTableModel::OFFSET, 110);
    _table->setColumnWidth(ArchiveTableModel::FOLDER, 260);
    _filter = new QLineEdit(this);
    _filter->setPlaceholderText(tr("Filter by name (Ctrl+F)"));
    _filter->setClearButtonEnabled(true);
    auto *central = new QWidget(this);
    auto *centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(2);
    _searchAll = new QCheckBox(tr("Whole archive"), this);
    _searchAll->setToolTip(tr("Search in every folder (Ctrl+Shift+F)"));
    auto *filterRow = new QHBoxLayout;
    filterRow->setContentsMargins(0, 0, 0, 0);
    filterRow->addWidget(_filter, 1);
    filterRow->addWidget(_searchAll);
    centralLayout->addLayout(filterRow);
    connect(_searchAll, &QCheckBox::toggled, this, [this] { OnFolderChanged(); });
    _grid = new QListView(this);
    _grid->setViewMode(QListView::IconMode);
    _grid->setResizeMode(QListView::Adjust);
    _grid->setMovement(QListView::Static);
    _grid->setUniformItemSizes(true); // all cells have the size the model gives (SizeHintRole)
    _grid->setWordWrap(true);
    _grid->setSelectionMode(QAbstractItemView::ExtendedSelection);
    _grid->setSpacing(4);
    _grid->setModel(_tableModel);
    _grid->setModelColumn(ArchiveTableModel::NAME);
    _grid->setSelectionModel(_table->selectionModel()); // one selection for both views
    _views = new QStackedWidget(this);
    _views->addWidget(_table);
    _views->addWidget(_grid);
    centralLayout->addWidget(_views, 1);
    setCentralWidget(central);
    connect(_filter, &QLineEdit::textChanged, this, [this](const QString &text) {
        _tableModel->SetFilter(text);
        _statusEntries->setText(tr("%1 entries").arg(_tableModel->rowCount()));
    });
    auto *focusFilter = new QShortcut(QKeySequence::Find, this);
    connect(focusFilter, &QShortcut::activated, this, [this] {
        _filter->setFocus();
        _filter->selectAll();
    });
    auto *searchEverywhere = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F), this);
    connect(searchEverywhere, &QShortcut::activated, this, [this] {
        _searchAll->setChecked(true);
        _filter->setFocus();
        _filter->selectAll();
    });
    auto *clearFilter = new QShortcut(QKeySequence(Qt::Key_Escape), _filter);
    clearFilter->setContext(Qt::WidgetShortcut);
    connect(clearFilter, &QShortcut::activated, this, [this] {
        _filter->clear();
        _table->setFocus();
    });
    setAcceptDrops(true);
    // drag the entries out to the file manager: they are extracted to a temporary folder first (small selections only)
    _tableModel->SetDragProvider([this](const std::vector<std::uint32_t> &entries) { return ExtractForDrag(entries); });
    _table->setDragEnabled(true);
    _table->setDragDropMode(QAbstractItemView::DragOnly);
    _grid->setDragEnabled(true);
    _grid->setDragDropMode(QAbstractItemView::DragOnly);

    _tree->setModel(_treeModel);
    _tree->setHeaderHidden(true);

    statusBar()->addPermanentWidget(_statusEntries, 1);
    _thumbnailSize = new QSlider(Qt::Horizontal, this);
    _thumbnailSize->setRange(48, 256);
    _thumbnailSize->setValue(96);
    _thumbnailSize->setFixedWidth(120);
    _thumbnailSize->setToolTip(tr("Thumbnail size"));
    statusBar()->addPermanentWidget(_thumbnailSize);
    statusBar()->addPermanentWidget(_statusZoom);
    connect(_thumbnailSize, &QSlider::valueChanged, this, [this](int size) {
        _grid->setIconSize(QSize(size, size));
        _grid->setGridSize(QSize(size + 28, size + 46));
        _tableModel->SetItemSize(QSize(size + 24, size + 42));
        _tableModel->ClearThumbnails(); // they were made for the previous size
        if (_thumbnailWorker != nullptr)
        {
            _thumbnailWorker->ClearPending();
        }
        _grid->viewport()->update();
    });
    _grid->setIconSize(QSize(96, 96));
    _grid->setGridSize(QSize(124, 142));
    _thumbnailSize->setVisible(false);
    connect(_grid->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { OnVisibleChanged(); });
    _statusZoom->setText("100%");
    connect(_preview->GetImageViewer(), &ImageViewer::ZoomChanged, this,
            [this](double zoom) { _statusZoom->setText(QString::number(static_cast<int>(zoom * 100)) + "%"); });
    connect(_tree->selectionModel(), &QItemSelectionModel::currentChanged, this, [this] { OnFolderChanged(); });
    connect(_table->selectionModel(), &QItemSelectionModel::currentChanged, this, [this] { OnEntryChanged(); });
    connect(_jobs, &JobsPanel::JobEnded, this, [this](const QString &summary, bool success) {
        if (success)
        {
            spdlog::info("{}", summary.toStdString());
        }
        else
        {
            spdlog::warn("{}", summary.toStdString());
        }
        statusBar()->showMessage(summary, 8000);
    });
    _table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(_table, &QWidget::customContextMenuRequested, this, [this](const QPoint &position) {
        QMenu menu(this);
        menu.addAction(tr("Extract selected..."), this, &MainWindow::OnExtractSelected)->setEnabled(!SelectedEntries().empty());
        menu.addAction(tr("Extract all..."), this, &MainWindow::OnExtractAll);
        menu.addSeparator();
        menu.addAction(tr("Extract selected, images as PNG..."), this, &MainWindow::OnExtractSelectedAsPng)->setEnabled(!SelectedEntries().empty());
        menu.exec(_table->viewport()->mapToGlobal(position));
    });
    _tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(_tree, &QWidget::customContextMenuRequested, this, [this](const QPoint &position) {
        QMenu menu(this);
        menu.addAction(tr("Extract this folder..."), this, &MainWindow::OnExtractFolder);
        menu.exec(_tree->viewport()->mapToGlobal(position));
    });

    if (const auto geometry = _settings.Layout.find("geometry"); geometry != _settings.Layout.end())
    {
        restoreGeometry(QByteArray::fromBase64(QByteArray::fromStdString(geometry->second)));
    }
    if (const auto state = _settings.Layout.find("state"); state != _settings.Layout.end())
    {
        restoreState(QByteArray::fromBase64(QByteArray::fromStdString(state->second)));
    }

    if (_driverHost.IsLoaded())
    {
        const auto &driver = _driverHost.GetDriver();
        // previews are prepared on a worker thread; the answer comes back to the GUI thread and stale ones are dropped
        _previewWorker = std::make_unique<Core::PreviewWorker>([this](Core::PreviewResult result) {
            auto shared = std::make_shared<Core::PreviewResult>(std::move(result));
            QMetaObject::invokeMethod(
                this,
                [this, shared] {
                    if (shared->Generation == _previewGeneration)
                    {
                        _preview->ShowResult(*shared);
                        if (!_preview->GetDescription().isEmpty())
                        {
                            _inspector->appendPlainText(_preview->GetDescription());
                        }
                        if (shared->Detected && shared->Detected->Category != Core::FileCategory::UNKNOWN)
                        {
                            _inspector->appendPlainText(tr("Detected: %1 (%2%)").arg(QString::fromStdString(shared->Detected->Description)).arg(shared->Detected->Confidence));
                        }
                        else if (shared->Detected && shared->Detected->Id == "random")
                        {
                            _inspector->appendPlainText(tr("Detected: %1").arg(QString::fromStdString(shared->Detected->Description)));
                        }
                    }
                },
                Qt::QueuedConnection);
        });
        _previewWorker->SetFallbackPalette(Core::LoadPaletteFile(_settings.PalettePath));
        statusBar()->showMessage(tr("Driver: %1 (%2 formats)").arg(QString::fromStdString(driver.GetName())).arg(driver.GetFormats().size()), 5000);
    }
    else if (_interactive)
    {
        QMessageBox::critical(this, tr("Driver"),
                              tr("Cannot load the driver:\n%1\n%2").arg(QString::fromStdString(_driverHost.GetPath().string()),
                                                                          QString::fromStdString(_driverHost.GetError())));
    }
}

MainWindow::~MainWindow()
{
    _thumbnailWorker.reset();
    _previewWorker.reset(); // its thread has its own archive handle
    CloseArchive(); // the archive must go before the driver
}

void MainWindow::BuildMenus()
{
    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    auto *open = fileMenu->addAction(tr("&Open..."), this, &MainWindow::OnOpenTriggered);
    open->setShortcut(QKeySequence::Open);
    _recentMenu = fileMenu->addMenu(tr("Open &recent"));
    RebuildRecentMenu();
    fileMenu->addSeparator();
    auto *extractSelected = fileMenu->addAction(tr("&Extract selected..."), this, &MainWindow::OnExtractSelected);
    extractSelected->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    auto *extractAll = fileMenu->addAction(tr("Extract &all..."), this, &MainWindow::OnExtractAll);
    extractAll->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
    auto *extractSelectedPng = fileMenu->addAction(tr("Extract selected, images as &PNG..."), this, &MainWindow::OnExtractSelectedAsPng);
    auto *extractAllPng = fileMenu->addAction(tr("Extract all, images as P&NG..."), this, &MainWindow::OnExtractAllAsPng);
    auto *toolSelected = fileMenu->addMenu(tr("Extract selected, converted with..."));
    auto *toolAll = fileMenu->addMenu(tr("Extract all, converted with..."));
    connect(toolSelected, &QMenu::aboutToShow, this, [this, toolSelected] { FillToolMenu(toolSelected, false); });
    connect(toolAll, &QMenu::aboutToShow, this, [this, toolAll] { FillToolMenu(toolAll, true); });
    auto *createList = fileMenu->addAction(tr("Create a li&st from a template..."), this, &MainWindow::OnCreateList);
    createList->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    auto *exportList = fileMenu->addAction(tr("Export the &list of entries..."), this, &MainWindow::OnExportList);
    Q_UNUSED(exportList);
    Q_UNUSED(extractSelectedPng);
    Q_UNUSED(extractAllPng);
    fileMenu->addSeparator();
    auto *quit = fileMenu->addAction(tr("&Quit"), this, &QWidget::close);
    quit->setShortcut(QKeySequence::Quit);

    auto *viewMenu = menuBar()->addMenu(tr("&View"));
    auto *recursive = viewMenu->addAction(tr("Include &subfolders"));
    recursive->setCheckable(true);
    recursive->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    connect(recursive, &QAction::toggled, this, [this](bool on) {
        _recursive = on;
        OnFolderChanged();
    });
    viewMenu->addSeparator();
    auto *details = viewMenu->addAction(tr("&Details"));
    auto *thumbnails = viewMenu->addAction(tr("&Thumbnails"));
    auto *viewGroup = new QActionGroup(this);
    for (auto *action : {details, thumbnails})
    {
        action->setCheckable(true);
        viewGroup->addAction(action);
    }
    details->setChecked(true);
    details->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));
    thumbnails->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_2));
    connect(details, &QAction::triggered, this, [this] { SetThumbnailView(false); });
    connect(thumbnails, &QAction::triggered, this, [this] { SetThumbnailView(true); });
    viewMenu->addSeparator();
    auto *back = viewMenu->addAction(tr("&Back"), this, &MainWindow::OnGoBack);
    back->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Left));
    auto *forward = viewMenu->addAction(tr("&Forward"), this, &MainWindow::OnGoForward);
    forward->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Right));
    auto *up = viewMenu->addAction(tr("&Up one folder"), this, &MainWindow::OnGoUp);
    up->setShortcut(QKeySequence(Qt::Key_Backspace));
    up->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    viewMenu->addSeparator();
    auto *themeMenu = viewMenu->addMenu(tr("&Theme"));
    auto *group = new QActionGroup(this);
    auto *dark = themeMenu->addAction(tr("&Dark"));
    auto *light = themeMenu->addAction(tr("&Light"));
    for (auto *action : {dark, light})
    {
        action->setCheckable(true);
        group->addAction(action);
    }
    (ThemeManager::Current() == Theme::DARK ? dark : light)->setChecked(true);
    connect(dark, &QAction::triggered, this, [] { ThemeManager::Apply(Theme::DARK); });
    connect(light, &QAction::triggered, this, [] { ThemeManager::Apply(Theme::LIGHT); });
    viewMenu->addSeparator(); // the panels' toggles are added by BuildDocks

    auto *toolsMenu = menuBar()->addMenu(tr("&Tools"));
    auto *ripper = toolsMenu->addAction(tr("&HyperRipper..."), this, &MainWindow::OnHyperRipper);
    auto *identify = toolsMenu->addAction(tr("&Identify the file types"), this, &MainWindow::OnIdentifyTypes);
    identify->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    Q_UNUSED(identify);
    toolsMenu->addSeparator();
    auto *optionsAction = toolsMenu->addAction(tr("&Options..."), this, &MainWindow::OnOptions);
    optionsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));

    BuildToolBar({{open, "open"}, {extractSelected, "extract"}, {extractAll, "extract-all"}, {nullptr, ""}, {details, "details"}, {thumbnails, "thumbnails"},
                  {nullptr, ""}, {back, "back"}, {forward, "forward"}, {up, "up"}, {nullptr, ""}, {ripper, "hyperripper"}, {optionsAction, "options"}});

    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About"), this, [this] {
        QString driverText;
        if (_driverHost.IsLoaded())
        {
            const auto &driver = _driverHost.GetDriver();
            driverText = tr("<p>%1: %2 formats<br><small>%3</small></p>")
                             .arg(QString::fromStdString(driver.GetName()).toHtmlEscaped())
                             .arg(driver.GetFormats().size())
                             .arg(QString::fromStdString(_driverHost.GetPath().string()).toHtmlEscaped());
        }
        AboutDialog(driverText, this).exec();
    });
}

void MainWindow::BuildToolBar(const std::vector<std::pair<QAction *, QString>> &items)
{
    auto *bar = addToolBar(tr("Main"));
    bar->setObjectName("toolBarMain");
    bar->setMovable(false);
    bar->setIconSize(QSize(20, 20));
    bar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    for (const auto &[action, icon] : items)
    {
        if (action == nullptr)
        {
            bar->addSeparator();
            continue;
        }
        _iconActions.emplace_back(action, icon);
        const QString shortcut = action->shortcut().toString(QKeySequence::NativeText);
        action->setToolTip(shortcut.isEmpty() ? action->text().remove('&').remove("...") : action->text().remove('&').remove("...") + " (" + shortcut + ")");
        bar->addAction(action);
    }
    RefreshIcons();
}

void MainWindow::RefreshIcons()
{
    const QColor colour = palette().color(QPalette::WindowText);
    for (const auto &[action, name] : _iconActions)
    {
        action->setIcon(Icons::Make(name, colour));
    }
    _tableModel->SetIconColour(colour);
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange)
    {
        RefreshIcons(); // the theme changed
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::BuildDocks()
{
    auto addDock = [this](const QString &title, const QString &name, QWidget *widget, Qt::DockWidgetArea area) {
        auto *dock = new QDockWidget(title, this);
        dock->setObjectName(name);
        dock->setWidget(widget);
        addDockWidget(area, dock);
        return dock;
    };
    auto *hierarchy = addDock(tr("Hierarchy"), "dockHierarchy", _tree, Qt::LeftDockWidgetArea);
    auto *viewer = addDock(tr("Viewer"), "dockViewer", _preview, Qt::RightDockWidgetArea);
    auto *inspector = addDock(tr("Inspector"), "dockInspector", _inspector, Qt::RightDockWidgetArea);
    auto *console = addDock(tr("Console"), "dockConsole", _console, Qt::BottomDockWidgetArea);
    auto *jobs = addDock(tr("Jobs"), "dockJobs", _jobs, Qt::BottomDockWidgetArea);
    tabifyDockWidget(console, jobs);
    console->raise();
    _inspector->setReadOnly(true);
    hierarchy->setMinimumWidth(220);
    viewer->setMinimumWidth(380);
    resizeDocks({hierarchy, viewer}, {260, 520}, Qt::Horizontal);
    resizeDocks({viewer, inspector}, {560, 200}, Qt::Vertical);
    resizeDocks({console}, {150}, Qt::Vertical);

    auto *viewMenu = menuBar()->findChildren<QMenu *>().at(1);
    // workspaces: named sets of visible panels, like the layouts of the Unity editor
    auto *workspaces = viewMenu->addMenu(tr("&Workspace"));
    workspaces->addAction(tr("&Default"), this, [this] { ApplyWorkspace("default"); });
    workspaces->addAction(tr("&Browse (folders and entries)"), this, [this] { ApplyWorkspace("browse"); });
    workspaces->addAction(tr("&Preview (entries, viewer, inspector)"), this, [this] { ApplyWorkspace("preview"); });
    viewMenu->addSeparator();
    for (auto *dock : {hierarchy, viewer, inspector, console, jobs})
    {
        viewMenu->addAction(dock->toggleViewAction());
    }
}

/// "All supported formats", then one entry per format of the driver, then all files.
auto MainWindow::OpenFilter() const -> QString
{
    QStringList filters;
    QStringList everything;
    if (_driverHost.IsLoaded())
    {
        for (const auto &format : _driverHost.GetDriver().GetFormats())
        {
            QStringList masks = QString::fromStdString(format.Extensions).split(';', Qt::SkipEmptyParts);
            const QString joined = masks.join(' ');
            everything << masks;
            filters << QString("%1 (%2)").arg(QString::fromStdString(format.Name).replace('(', '[').replace(')', ']'), joined);
        }
    }
    everything << "*.hrf";
    everything.removeDuplicates();
    filters.removeDuplicates();
    QStringList result{tr("All supported formats (%1)").arg(everything.join(' '))};
    result << filters << tr("HyperRipper files (*.hrf)") << tr("All files (*)");
    return result.join(";;");
}

void MainWindow::OnOptions()
{
    OptionsDialog dialog(_settings, _driverHost, this);
    if (dialog.exec() == QDialog::Accepted)
    {
        ApplySettings();
        _settings.Save();
    }
}

void MainWindow::ApplySettings()
{
    ThemeManager::Apply(_settings.Theme == "light" ? Theme::LIGHT : Theme::DARK);
    spdlog::set_level(spdlog::level::from_str(_settings.LogLevel));
    _thumbnailSize->setValue(_settings.ThumbnailSize);
    if (_previewWorker != nullptr)
    {
        _previewWorker->SetLimits(static_cast<std::size_t>(_settings.PreviewImageMegabytes) * 1024 * 1024,
                                  static_cast<std::size_t>(_settings.PreviewTextKilobytes) * 1024);
        _previewWorker->SetFallbackPalette(Core::LoadPaletteFile(_settings.PalettePath));
    }
}

void MainWindow::OnHyperRipper()
{
    HyperRipperDialog dialog(QString::fromStdString(_settings.LastOpenDirectory), &_settings, this);
    connect(&dialog, &HyperRipperDialog::OpenResults, this, [this](std::shared_ptr<const Core::ArchiveSource> source) { OpenSource(std::move(source)); });
    dialog.exec();
}

auto MainWindow::OpenRipped(const QString &path) -> bool
{
    try
    {
        auto files = Core::HyperRipper::Scan(path.toStdString());
        return OpenSource(std::make_shared<Core::RippedArchiveSource>(path.toStdString(), std::move(files)));
    }
    catch (const std::exception &error)
    {
        statusBar()->showMessage(tr("Cannot scan %1: %2").arg(path, error.what()));
        return false;
    }
}

auto MainWindow::OpenHrf(const QString &path) -> bool
{
    try
    {
        const Core::HrfFile hrf = Core::ReadHrf(path.toStdString());
        std::filesystem::path companion = std::filesystem::path(path.toStdString()).parent_path() / hrf.SourceName;
        if (!std::filesystem::exists(companion))
        {
            const QString chosen = QFileDialog::getOpenFileName(this, tr("Where is %1?").arg(QString::fromStdString(hrf.SourceName)), QFileInfo(path).absolutePath());
            if (chosen.isEmpty())
            {
                return false;
            }
            companion = chosen.toStdString();
        }
        const auto size = static_cast<std::int64_t>(std::filesystem::file_size(companion));
        if (size != hrf.SourceSize)
        {
            statusBar()->showMessage(tr("%1 is not the file the HRF file was made for (%2 bytes instead of %3)").arg(QString::fromStdString(companion.string())).arg(size).arg(hrf.SourceSize));
            return false;
        }
        std::vector<Core::RippedFile> files;
        std::vector<std::string> names;
        for (const auto &entry : hrf.Entries)
        {
            if (entry.Offset + entry.Size > size)
            {
                statusBar()->showMessage(tr("The HRF file is damaged: %1 is outside of the file").arg(QString::fromStdString(entry.Name)));
                return false;
            }
            std::string extension = std::filesystem::path(entry.Name).extension().string();
            if (!extension.empty())
            {
                extension.erase(0, 1);
            }
            std::string type = extension;
            for (char &c : type)
            {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            files.push_back({entry.Offset, entry.Size, type, extension});
            names.push_back(entry.Name);
        }
        return OpenSource(std::make_shared<Core::RippedArchiveSource>(companion, std::move(files), std::move(names)));
    }
    catch (const std::exception &error)
    {
        statusBar()->showMessage(tr("Cannot open %1: %2").arg(path, error.what()));
        return false;
    }
}

void MainWindow::RebuildRecentMenu()
{
    _recentMenu->clear();
    for (const auto &file : _settings.GetRecent())
    {
        const auto path = QString::fromStdString(file);
        _recentMenu->addAction(path, this, [this, path] { OpenArchive(path); });
    }
    _recentMenu->setEnabled(!_settings.GetRecent().empty());
    if (!_settings.GetRecent().empty())
    {
        _recentMenu->addSeparator();
        _recentMenu->addAction(tr("&Clear list"), this, [this] {
            _settings.ClearRecent();
            RebuildRecentMenu();
        });
    }
}

void MainWindow::OnOpenTriggered()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Open archive"), QString::fromStdString(_settings.LastOpenDirectory), OpenFilter());
    if (!path.isEmpty())
    {
        OpenArchive(path);
    }
}

void MainWindow::CloseArchive()
{
    StopIdentifying();
    _tableModel->SetIndex(nullptr);
    _treeModel->SetIndex(nullptr, QString());
    _thumbnailWorker.reset();
    _index.reset();
    _archive.reset();
    _source.reset();
    ++_previewGeneration; // a preview still in flight belongs to the old archive
    _preview->ShowMessage(tr("Select an entry to preview it"));
}

auto MainWindow::OpenArchive(const QString &path) -> bool
{
    if (path.endsWith(".hrf", Qt::CaseInsensitive))
    {
        return OpenHrf(path);
    }
    if (!_driverHost.IsLoaded())
    {
        return false;
    }
    return OpenSource(std::make_shared<Core::DriverArchiveSource>(_driverHost.GetDriver(), path.toStdString()));
}

auto MainWindow::OpenSource(std::shared_ptr<const Core::ArchiveSource> source) -> bool
{
    using Clock = std::chrono::steady_clock;
    CloseArchive();
    const QString title = QString::fromStdString(source->GetTitle());
    const QString path = QString::fromStdString(source->GetPath().string());
    try
    {
        const auto start = Clock::now();
        _source = std::move(source);
        _archive = _source->Open();
        const auto opened = Clock::now();
        _index = std::make_shared<const Core::ArchiveIndex>(Core::ArchiveIndex::Build(*_archive));
        _archiveTitle = title.toStdString();
        const auto built = Clock::now();
        using Ms = std::chrono::milliseconds;
        spdlog::info("{}: {} entries, {} folders (open {} ms, index {} ms)", title.toStdString(), _index->GetEntryCount(),
                     _index->GetFolders().size(), std::chrono::duration_cast<Ms>(opened - start).count(),
                     std::chrono::duration_cast<Ms>(built - opened).count());
    }
    catch (const std::exception &error)
    {
        spdlog::error("cannot open {}: {}", title.toStdString(), error.what());
        CloseArchive();
        if (_interactive)
        {
            QMessageBox::warning(this, tr("Open"), tr("Cannot open %1:\n%2").arg(title, error.what()));
        }
        else
        {
            statusBar()->showMessage(tr("Cannot open %1: %2").arg(title, error.what()));
        }
        return false;
    }
    _history.clear();
    _historyPosition = -1;
    _filter->clear();
    _thumbnailWorker = std::make_unique<Core::ThumbnailWorker>(
        _source, _index, _archive->GetFormatId(), 256U, [this](Core::ThumbnailResult result) {
            auto shared = std::make_shared<Core::ThumbnailResult>(std::move(result));
            QMetaObject::invokeMethod(
                this,
                [this, shared] {
                    QImage image;
                    if (shared->Image.Width > 0)
                    {
                        image = QImage(shared->Image.Rgba.data(), static_cast<int>(shared->Image.Width), static_cast<int>(shared->Image.Height),
                                       static_cast<qsizetype>(shared->Image.Width) * 4, QImage::Format_RGBA8888)
                                    .copy();
                    }
                    else if (!shared->EncodedBytes.empty())
                    {
                        image.loadFromData(reinterpret_cast<const uchar *>(shared->EncodedBytes.data()), static_cast<int>(shared->EncodedBytes.size()),
                                           shared->Extension.c_str());
                    }
                    const int size = _thumbnailSize->value();
                    QPixmap canvas;
                    if (!image.isNull())
                    {
                        // a square cell with the image centered, on a quiet checkerboard so that transparency shows
                        canvas = QPixmap(size, size);
                        QPainter painter(&canvas);
                        constexpr int CELL = 8;
                        for (int y = 0; y < size; y += CELL)
                        {
                            for (int x = 0; x < size; x += CELL)
                            {
                                painter.fillRect(x, y, CELL, CELL, ((x / CELL + y / CELL) % 2 == 0) ? QColor(0x3a, 0x3a, 0x3a) : QColor(0x30, 0x30, 0x30));
                            }
                        }
                        const QImage fitted = image.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                        painter.drawImage((size - fitted.width()) / 2, (size - fitted.height()) / 2, fitted);
                    }
                    _tableModel->SetThumbnail(shared->Entry, canvas);
                },
                Qt::QueuedConnection);
        });
    _thumbnailWorker->SetFallbackPalette(Core::LoadPaletteFile(_settings.PalettePath));
    _tableModel->SetThumbnailRequester([this](std::uint32_t entry) {
        if (_thumbnailWorker != nullptr)
        {
            _thumbnailWorker->Request(entry);
        }
    });
    if (!path.isEmpty())
    {
        _settings.AddRecent(path.toStdString());
        _settings.LastOpenDirectory = QFileInfo(path).absolutePath().toStdString();
        RebuildRecentMenu();
    }
    _tableModel->SetIndex(_index.get());
    _treeModel->SetIndex(_index.get(), title);
    setWindowTitle(title + " - Dragon UnPACKer++");
    const auto root = _treeModel->index(0, 0);
    _tree->setCurrentIndex(root);
    _tree->expand(root);
    return true;
}

void MainWindow::SetNameFilter(const QString &text, bool wholeArchive)
{
    _searchAll->setChecked(wholeArchive);
    _filter->setText(text);
}

void MainWindow::SetThumbnailView(bool thumbnails)
{
    _thumbnailView = thumbnails;
    _tableModel->SetThumbnailsEnabled(thumbnails);
    _views->setCurrentWidget(thumbnails ? static_cast<QWidget *>(_grid) : static_cast<QWidget *>(_table));
    _thumbnailSize->setVisible(thumbnails);
    if (thumbnails)
    {
        OnVisibleChanged();
    }
}

void MainWindow::OnVisibleChanged()
{
    // what was asked and not made is not needed any more once it scrolled out of view
    if (_thumbnailWorker != nullptr)
    {
        _thumbnailWorker->ClearPending();
    }
    _tableModel->ForgetPendingThumbnails();
    _grid->viewport()->update();
}

void MainWindow::SetInteractive(bool interactive)
{
    _interactive = interactive;
}

void MainWindow::SetRecursive(bool recursive)
{
    _recursive = recursive;
    OnFolderChanged();
}

void MainWindow::SortBy(int column, Qt::SortOrder order)
{
    const auto start = std::chrono::steady_clock::now();
    _table->sortByColumn(column, order);
    spdlog::debug("sorted {} rows in {} ms", _tableModel->rowCount(),
                  std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
}

void MainWindow::ShowFolder(std::uint32_t folder)
{
    const bool everywhere = _searchAll != nullptr && _searchAll->isChecked();
    const auto start = std::chrono::steady_clock::now();
    _tableModel->ShowFolder(folder, _recursive || everywhere);
    spdlog::debug("folder shown: {} rows in {} ms", _tableModel->rowCount(),
                  std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
    _statusEntries->setText(tr("%1 entries").arg(_tableModel->rowCount()));
}

void MainWindow::OnFolderChanged()
{
    if (_index == nullptr)
    {
        return;
    }
    const auto folder = FolderTreeModel::FolderOf(_tree->currentIndex());
    ShowFolder(_searchAll->isChecked() ? Core::ArchiveIndex::ROOT : folder);
    // history: a new folder drops what was "forward"
    if (_historyPosition < 0 || _history[static_cast<std::size_t>(_historyPosition)] != folder)
    {
        _history.resize(static_cast<std::size_t>(_historyPosition + 1));
        _history.push_back(folder);
        _historyPosition = static_cast<int>(_history.size()) - 1;
    }
}

/// The tree index of a folder (the root is the only top level row).
static auto TreeIndexOf(const Core::ArchiveIndex &index, const FolderTreeModel &model, std::uint32_t folder) -> QModelIndex
{
    std::vector<std::uint32_t> chain;
    for (auto k = folder; k != Core::ArchiveIndex::ROOT; k = index.GetFolders()[k].Parent)
    {
        chain.push_back(k);
    }
    QModelIndex node = model.index(0, 0);
    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
    {
        const auto &children = index.GetFolders()[index.GetFolders()[*it].Parent].Children;
        const auto row = std::find(children.begin(), children.end(), *it) - children.begin();
        node = model.index(static_cast<int>(row), 0, node);
    }
    return node;
}

void MainWindow::SelectFolder(std::uint32_t folder, bool recordHistory)
{
    if (_index == nullptr)
    {
        return;
    }
    const auto node = TreeIndexOf(*_index, *_treeModel, folder);
    const int saved = _historyPosition;
    _tree->setCurrentIndex(node);
    if (!recordHistory)
    {
        _historyPosition = saved; // moving in the history must not add to it
    }
}

void MainWindow::OnGoUp()
{
    if (_index == nullptr)
    {
        return;
    }
    const auto folder = FolderTreeModel::FolderOf(_tree->currentIndex());
    if (folder != Core::ArchiveIndex::ROOT)
    {
        SelectFolder(_index->GetFolders()[folder].Parent, true);
    }
}

void MainWindow::OnGoBack()
{
    if (_historyPosition > 0)
    {
        --_historyPosition;
        SelectFolder(_history[static_cast<std::size_t>(_historyPosition)], false);
    }
}

void MainWindow::OnGoForward()
{
    if (_historyPosition + 1 < static_cast<int>(_history.size()))
    {
        ++_historyPosition;
        SelectFolder(_history[static_cast<std::size_t>(_historyPosition)], false);
    }
}

auto MainWindow::ShowFolderByPath(const QString &path) -> bool
{
    if (_index == nullptr)
    {
        return false;
    }
    std::uint32_t folder = Core::ArchiveIndex::ROOT;
    for (const auto &part : path.split('/', Qt::SkipEmptyParts))
    {
        bool found = false;
        for (const auto child : _index->GetFolders()[folder].Children)
        {
            if (QString::fromStdString(_index->GetFolders()[child].Name) == part)
            {
                folder = child;
                found = true;
                break;
            }
        }
        if (!found)
        {
            return false;
        }
    }
    SelectFolder(folder, true);
    return true;
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const auto urls = event->mimeData()->urls();
    if (!urls.isEmpty() && urls.front().isLocalFile())
    {
        OpenArchive(urls.front().toLocalFile());
    }
}

void MainWindow::OnEntryChanged()
{
    const auto current = _table->currentIndex();
    if (_index == nullptr || !current.isValid())
    {
        return;
    }
    PreviewEntry(_tableModel->EntryAt(current.row()));
}

auto MainWindow::SelectEntryByName(const QString &fileName) -> bool
{
    if (_index == nullptr)
    {
        return false;
    }
    const auto wanted = fileName.toStdString();
    for (std::uint32_t i = 0; i < _index->GetEntryCount(); ++i)
    {
        if (_index->GetFileName(i) == wanted)
        {
            // find its folder: walk the folders (prototype: linear)
            const auto &folders = _index->GetFolders();
            for (std::uint32_t f = 0; f < folders.size(); ++f)
            {
                const auto &entries = folders[f].Entries;
                if (std::find(entries.begin(), entries.end(), i) != entries.end())
                {
                    std::vector<std::uint32_t> chain;
                    for (auto k = f; k != Core::ArchiveIndex::ROOT; k = folders[k].Parent)
                    {
                        chain.push_back(k);
                    }
                    QModelIndex node = _treeModel->index(0, 0);
                    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
                    {
                        const auto &children = folders[folders[*it].Parent].Children;
                        const auto row = std::find(children.begin(), children.end(), *it) - children.begin();
                        node = _treeModel->index(static_cast<int>(row), 0, node);
                    }
                    _tree->setCurrentIndex(node);
                    for (int row = 0; row < _tableModel->rowCount(); ++row)
                    {
                        if (_tableModel->EntryAt(row) == i)
                        {
                            _table->setCurrentIndex(_tableModel->index(row, 0));
                            _table->selectRow(row);
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

void MainWindow::PreviewEntry(std::uint32_t entry)
{
    const auto name = _index->GetFileName(entry);
    const auto size = _index->GetSize(entry);
    QString text;
    text += tr("Name:    %1\n").arg(QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size())));
    const auto path = _index->GetPath(entry);
    text += tr("Path:    %1\n").arg(QString::fromUtf8(path.data(), static_cast<qsizetype>(path.size())));
    text += tr("Size:    %1 (%2 bytes)\n").arg(ArchiveTableModel::FormatSize(size)).arg(size);
    text += tr("Offset:  %1\n").arg(_index->GetOffset(entry));
    text += tr("Format:  %1\n").arg(QString::fromStdString(_archive->GetFormatId()));
    _inspector->setPlainText(text);

    if (_previewWorker == nullptr)
    {
        return;
    }
    Core::PreviewRequest request;
    request.Generation = ++_previewGeneration;
    request.Source = _source;
    request.Entry = entry;
    request.FileName = std::string(name);
    request.FormatId = _archive->GetFormatId();
    _previewWorker->Submit(std::move(request));
}

auto MainWindow::SelectedEntries() const -> std::vector<std::uint32_t>
{
    std::vector<std::uint32_t> entries;
    const auto rows = _table->selectionModel()->selectedRows();
    entries.reserve(static_cast<std::size_t>(rows.size()));
    for (const auto &row : rows)
    {
        entries.push_back(_tableModel->EntryAt(row.row()));
    }
    std::ranges::sort(entries);
    return entries;
}

auto MainWindow::ChooseDirectory(const QString &title) -> QString
{
    const QString start = QString::fromStdString(_settings.LastExtractDirectory.empty() ? _settings.LastOpenDirectory : _settings.LastExtractDirectory);
    return QFileDialog::getExistingDirectory(this, title, start);
}

void MainWindow::StartExtraction(std::vector<std::uint32_t> entries, const QString &title, const QString &directory, bool convertImages,
                                 std::shared_ptr<const Core::ExternalTool> tool)
{
    if (_index == nullptr || entries.empty() || directory.isEmpty())
    {
        return;
    }
    _settings.LastExtractDirectory = directory.toStdString();
    Core::ExtractionJob::Request request;
    request.Title = title.toStdString();
    request.Source = _source;
    request.Index = _index;
    request.Entries = std::move(entries);
    request.Destination = directory.toStdString();
    request.Options.ConvertImages = convertImages;
    request.Options.Tool = std::move(tool);
    request.Options.AddDetectedExtension = _settings.AddDetectedExtension;
    request.Options.FallbackPalette = Core::LoadPaletteFile(_settings.PalettePath);
    // the job asks on its own thread: the question is shown by the GUI thread and the job waits for the answer
    auto onConflict = [this](const std::filesystem::path &path) {
        if (!_interactive)
        {
            return Core::ConflictAction::OVERWRITE; // automated runs: nobody to ask
        }
        Core::ConflictAction answer = Core::ConflictAction::SKIP;
        QMetaObject::invokeMethod(
            this,
            [this, &answer, &path] {
                QMessageBox box(this);
                box.setIcon(QMessageBox::Question);
                box.setWindowTitle(tr("File exists"));
                box.setText(tr("The file already exists:\n%1").arg(QString::fromStdString(path.string())));
                auto *overwrite = box.addButton(tr("Overwrite"), QMessageBox::AcceptRole);
                auto *overwriteAll = box.addButton(tr("Overwrite all"), QMessageBox::AcceptRole);
                auto *skip = box.addButton(tr("Skip"), QMessageBox::RejectRole);
                auto *skipAll = box.addButton(tr("Skip all"), QMessageBox::RejectRole);
                auto *cancel = box.addButton(tr("Cancel"), QMessageBox::DestructiveRole);
                box.exec();
                const QAbstractButton *clicked = box.clickedButton();
                answer = clicked == overwrite      ? Core::ConflictAction::OVERWRITE
                         : clicked == overwriteAll ? Core::ConflictAction::OVERWRITE_ALL
                         : clicked == skip         ? Core::ConflictAction::SKIP
                         : clicked == skipAll      ? Core::ConflictAction::SKIP_ALL
                         : clicked == cancel       ? Core::ConflictAction::CANCEL
                                                   : Core::ConflictAction::CANCEL;
            },
            Qt::BlockingQueuedConnection);
        return answer;
    };
    spdlog::info("extracting {} entries of {} to {}", request.Entries.size(), title.toStdString(), directory.toStdString());
    _jobs->AddJob(std::make_unique<Core::ExtractionJob>(std::move(request), onConflict));
    if (auto *dock = _jobs->parentWidget(); dock != nullptr && dock->parentWidget() != nullptr)
    {
        dock->raise();
    }
}

void MainWindow::OnExtractSelected()
{
    const auto entries = SelectedEntries();
    if (entries.empty())
    {
        statusBar()->showMessage(tr("Nothing is selected"), 3000);
        return;
    }
    StartExtraction(entries, QFileInfo(QString::fromStdString(_archiveTitle)).fileName(), ChooseDirectory(tr("Extract the selection to")));
}

void MainWindow::OnExtractSelectedAsPng()
{
    const auto entries = SelectedEntries();
    if (entries.empty())
    {
        statusBar()->showMessage(tr("Nothing is selected"), 3000);
        return;
    }
    StartExtraction(entries, QString::fromStdString(_archiveTitle), ChooseDirectory(tr("Extract the selection (images as PNG) to")), true);
}

auto MainWindow::FindTool(const std::string &iniOrName) const -> std::shared_ptr<const Core::ExternalTool>
{
    for (const auto &tool : Core::LoadExternalTools(GetToolsDirectory()))
    {
        if (tool.IsUsable() && (tool.IniName == iniOrName || tool.Name == iniOrName))
        {
            return std::make_shared<const Core::ExternalTool>(tool);
        }
    }
    return nullptr;
}

auto MainWindow::GetToolsDirectory() const -> std::filesystem::path
{
    return _settings.ToolsDirectory.empty() ? Core::DefaultToolsDirectory(_settings.GetFile()) : std::filesystem::path(_settings.ToolsDirectory);
}

// The tools described by the INI files that can be used, one entry each
void MainWindow::FillToolMenu(QMenu *menu, bool all)
{
    menu->clear();
    const auto tools = Core::LoadExternalTools(GetToolsDirectory());
    bool any = false;
    for (const auto &tool : tools)
    {
        if (!tool.IsUsable())
        {
            continue;
        }
        any = true;
        auto shared = std::make_shared<const Core::ExternalTool>(tool);
        QString extensions = QString::fromStdString(tool.Extensions.empty() ? std::string() : tool.Extensions.front());
        auto *action = menu->addAction(tr("%1 (to .%2)").arg(QString::fromStdString(tool.Name), QString::fromStdString(tool.ResultExtension)));
        action->setToolTip(QString::fromStdString(tool.Comment));
        connect(action, &QAction::triggered, this, [this, shared, all] {
            if (_index == nullptr)
            {
                return;
            }
            std::vector<std::uint32_t> entries;
            if (all)
            {
                entries.resize(_index->GetEntryCount());
                std::iota(entries.begin(), entries.end(), 0U);
            }
            else
            {
                entries = SelectedEntries();
            }
            if (entries.empty())
            {
                statusBar()->showMessage(tr("Nothing is selected"), 3000);
                return;
            }
            StartExtraction(std::move(entries), QString::fromStdString(_archiveTitle), ChooseDirectory(tr("Extract (converted with %1) to").arg(QString::fromStdString(shared->Name))), false, shared);
        });
    }
    if (!any)
    {
        auto *none = menu->addAction(tr("No tool: put .ini files in %1").arg(QString::fromStdString(GetToolsDirectory().string())));
        none->setEnabled(false);
    }
}

void MainWindow::OnExtractAllAsPng()
{
    if (_index == nullptr)
    {
        return;
    }
    std::vector<std::uint32_t> all(_index->GetEntryCount());
    std::iota(all.begin(), all.end(), 0U);
    StartExtraction(std::move(all), QString::fromStdString(_archiveTitle), ChooseDirectory(tr("Extract everything (images as PNG) to")), true);
}

void MainWindow::SelectAllRows()
{
    _table->selectAll();
}

auto MainWindow::TestDragOut() -> int
{
    const auto selected = _table->selectionModel()->selectedRows();
    std::unique_ptr<QMimeData> data(_tableModel->mimeData(selected));
    if (data == nullptr)
    {
        return 0;
    }
    int existing = 0;
    for (const auto &url : data->urls())
    {
        existing += QFileInfo::exists(url.toLocalFile()) ? 1 : 0;
    }
    return existing;
}

auto MainWindow::ExtractForDrag(const std::vector<std::uint32_t> &entries) -> QList<QUrl>
{
    constexpr std::int64_t DRAG_LIMIT = 256LL * 1024 * 1024;
    constexpr std::size_t DRAG_FILES = 2000;
    if (_index == nullptr || _archive == nullptr || entries.empty())
    {
        return {};
    }
    std::int64_t total = 0;
    for (const auto entry : entries)
    {
        total += _index->GetSize(entry);
    }
    if (total > DRAG_LIMIT || entries.size() > DRAG_FILES)
    {
        statusBar()->showMessage(tr("Too much to drag out (%1 files): use Extract").arg(entries.size()), 6000);
        return {};
    }
    _dragDirectory = std::make_unique<QTemporaryDir>();
    if (!_dragDirectory->isValid())
    {
        return {};
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    Core::Extractor extractor(*_archive, *_index);
    const auto stats = extractor.Run(entries, _dragDirectory->path().toStdString());
    QApplication::restoreOverrideCursor();
    // the files are the top level items of the folder: a folder per top level directory is dropped, as a file manager does
    QList<QUrl> urls;
    for (const auto &item : std::filesystem::directory_iterator(_dragDirectory->path().toStdString()))
    {
        urls.push_back(QUrl::fromLocalFile(QString::fromStdString(item.path().string())));
    }
    if (stats.Failed > 0)
    {
        spdlog::warn("drag out: {} entries could not be extracted", stats.Failed);
    }
    return urls;
}

void MainWindow::SetWorkspace(const QString &name)
{
    ApplyWorkspace(name);
}

void MainWindow::ApplyWorkspace(const QString &name)
{
    const bool browse = name == "browse";
    const bool preview = name == "preview";
    auto show = [this](const char *objectName, bool visible) {
        if (auto *dock = findChild<QDockWidget *>(objectName))
        {
            dock->setVisible(visible);
        }
    };
    show("dockHierarchy", !preview);
    show("dockViewer", !browse);
    show("dockInspector", !browse);
    show("dockConsole", !browse && !preview);
    show("dockJobs", !browse && !preview);
    if (auto *hierarchy = findChild<QDockWidget *>("dockHierarchy"); hierarchy != nullptr && hierarchy->isVisible())
    {
        resizeDocks({hierarchy}, {260}, Qt::Horizontal);
    }
    if (auto *viewer = findChild<QDockWidget *>("dockViewer"); viewer != nullptr && viewer->isVisible())
    {
        resizeDocks({viewer}, {preview ? 760 : 520}, Qt::Horizontal);
    }
}

auto MainWindow::MakeList(const Core::ListTemplate &chosen, ListScope scope, bool subfolders, Core::ListSort sort, bool invert, std::size_t *count) -> std::string
{
    const auto selected = SelectedEntries();
    // which entries, and the folder that is the "directory" of the list
    const std::string &separator = _index->GetDirSeparator();
    std::vector<std::uint32_t> numbers;
    std::string directory;
    switch (scope)
    {
    case ListScope::WHOLE_ARCHIVE:
        numbers.resize(_index->GetEntryCount());
        std::iota(numbers.begin(), numbers.end(), 0U);
        break;
    case ListScope::CURRENT_FOLDER:
    {
        const auto folder = FolderTreeModel::FolderOf(_tree->currentIndex());
        numbers = subfolders ? _index->GetEntriesRecursive(folder) : _index->GetFolders()[folder].Entries;
        for (auto at = folder; at != Core::ArchiveIndex::ROOT; at = _index->GetFolders()[at].Parent)
        {
            directory = _index->GetFolders()[at].Name + separator + directory;
        }
        break;
    }
    case ListScope::SELECTION:
        numbers = selected;
        break;
    }
    std::vector<Core::ListEntry> entries;
    entries.reserve(numbers.size());
    for (const auto number : numbers)
    {
        const auto info = _archive->GetEntry(number);
        entries.push_back({std::string(_index->GetPath(number)), _index->GetOffset(number), _index->GetSize(number), info.DataX, info.DataY});
    }
    Core::SortEntries(entries, sort, invert);

    Core::ListContext context;
    context.ArchiveName = QFileInfo(QString::fromStdString(_archiveTitle)).fileName().toStdString();
    context.Format = _archive->GetFormatId();
    context.Directory = directory;
    context.Separator = separator;
    context.AppVersion = QCoreApplication::applicationVersion().isEmpty() ? "6.0.0" : QCoreApplication::applicationVersion().toStdString();
    context.AppUrl = "https://www.elberethzone.net/dragon-unpacker.html";
    context.ThousandsSeparator = QLocale().groupSeparator().toStdString();
    context.Language = [](const std::string &keyword) { return ListText(keyword); };
    context.Describe = [](const std::string &extension) { return Core::DescribeExtension(extension, [](const std::string &keyword) { return ListText(keyword); }); };
    const std::string text = Core::RenderList(chosen, context, entries);
    if (count != nullptr)
    {
        *count = entries.size();
    }
    return text;
}

void MainWindow::StopIdentifying()
{
    if (_identifyTimer != nullptr)
    {
        _identifyTimer->stop();
    }
    _identifier.reset();
    _tableModel->SetTypes(nullptr);
    _table->setColumnHidden(ArchiveTableModel::DETECTED, true);
}

// Tools > Identify the file types: reads the start of every entry on a thread and shows what the content says in a column
void MainWindow::OnIdentifyTypes()
{
    if (_index == nullptr || _source == nullptr)
    {
        return;
    }
    if (_identifier != nullptr && !_identifier->IsDone())
    {
        _identifier->Cancel();
        return;
    }
    std::vector<std::uint32_t> all(_index->GetEntryCount());
    std::iota(all.begin(), all.end(), 0U);
    _identifier = std::make_unique<Core::TypeIdentifier>(_source, _index, std::move(all));
    _tableModel->SetTypes(_identifier->GetTypes());
    _table->setColumnHidden(ArchiveTableModel::DETECTED, false);
    _identifier->Start();
    if (_identifyTimer == nullptr)
    {
        _identifyTimer = new QTimer(this);
        connect(_identifyTimer, &QTimer::timeout, this, [this] {
            if (_identifier == nullptr)
            {
                _identifyTimer->stop();
                return;
            }
            _tableModel->RefreshTypes();
            const auto types = _identifier->GetTypes();
            if (!_identifier->IsDone())
            {
                statusBar()->showMessage(tr("Identifying the file types: %1 of %2").arg(types->GetDone()).arg(_identifier->GetTotal()));
                return;
            }
            _identifyTimer->stop();
            QStringList parts;
            for (const auto &[type, count] : types->GetSummary())
            {
                if (parts.size() < 5)
                {
                    parts << QString("%1: %2").arg(QString::fromStdString(type.Description)).arg(count);
                }
                spdlog::info("identified: {} x {} ({}%)", count, type.Description, type.Confidence);
            }
            statusBar()->showMessage(tr("%1 file types identified: %2").arg(types->GetDone()).arg(parts.join(", ")), 15000);
        });
    }
    _identifyTimer->start(250);
}

auto MainWindow::IdentifyAll() -> std::string
{
    std::string out;
    if (_index == nullptr || _source == nullptr)
    {
        return out;
    }
    std::vector<std::uint32_t> all(_index->GetEntryCount());
    std::iota(all.begin(), all.end(), 0U);
    Core::TypeIdentifier identifier(_source, _index, std::move(all));
    identifier.Start();
    while (!identifier.IsDone())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    const auto types = identifier.GetTypes();
    for (std::uint32_t entry = 0; entry < _index->GetEntryCount(); ++entry)
    {
        if (const auto type = types->Get(entry))
        {
            out += std::string(_index->GetPath(entry)) + "\t" + type->Id + "\t" + std::to_string(type->Confidence) + "\t" + type->Description + "\n";
        }
    }
    return out;
}

void MainWindow::OnCreateList()
{
    if (_index == nullptr || _archive == nullptr)
    {
        return;
    }
    const std::filesystem::path userDirectory = _settings.TemplatesDirectory.empty() ? DefaultTemplatesDirectory(_settings.GetFile()) : std::filesystem::path(_settings.TemplatesDirectory);
    const auto templates = LoadListTemplates(userDirectory);
    const auto selected = SelectedEntries();
    ListDialog dialog(templates, static_cast<int>(selected.size()), &_settings, this);
    if (dialog.exec() != QDialog::Accepted || dialog.GetTemplateIndex() < 0)
    {
        return;
    }
    const Core::ListTemplate &chosen = templates[static_cast<std::size_t>(dialog.GetTemplateIndex())].Template;

    std::size_t entryCount = 0;
    const std::string text = MakeList(chosen, dialog.GetScope(), dialog.IncludesSubfolders(), dialog.GetSort(), dialog.IsSortInverted(), &entryCount);

    const QString extension = QString::fromStdString(chosen.Info.DefaultExtension).toLower();
    const QString filters = QString("%1 (*.%2);;%3").arg(QString::fromStdString(chosen.Info.ExtensionInfo), extension, tr("All files (*)"));
    QString path = QFileDialog::getSaveFileName(this, tr("Save the list to..."), QString::fromStdString(_settings.LastExtractDirectory), filters);
    if (path.isEmpty())
    {
        return;
    }
    if (QFileInfo(path).suffix().isEmpty() && !extension.isEmpty())
    {
        path += "." + extension;
    }
    {
        std::ofstream out(path.toStdString(), std::ios::binary | std::ios::trunc);
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!out)
        {
            QMessageBox::warning(this, tr("Export"), tr("Cannot write %1").arg(path));
            return;
        }
    }
    // the pictures and files that the list uses go next to it
    const QDir folder = QFileInfo(path).absoluteDir();
    for (const auto &image : chosen.Images)
    {
        QFile file(folder.filePath(QString::fromStdString(image.Name)));
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            file.write(reinterpret_cast<const char *>(image.Data.data()), static_cast<qint64>(image.Data.size()));
        }
    }
    statusBar()->showMessage(tr("%1 entries written to %2").arg(entryCount).arg(path), 6000);
}

auto MainWindow::CreateListFile(const QString &path, const std::string &templateName) -> bool
{
    if (_index == nullptr || _archive == nullptr)
    {
        return false;
    }
    const std::filesystem::path userDirectory = _settings.TemplatesDirectory.empty() ? DefaultTemplatesDirectory(_settings.GetFile()) : std::filesystem::path(_settings.TemplatesDirectory);
    for (const auto &item : LoadListTemplates(userDirectory))
    {
        if (item.Source == "bundled:" + templateName || item.Template.Info.Name == templateName)
        {
            const std::string text = MakeList(item.Template, ListScope::WHOLE_ARCHIVE, true, Core::ListSort::NONE, false);
            std::ofstream out(path.toStdString(), std::ios::binary | std::ios::trunc);
            out.write(text.data(), static_cast<std::streamsize>(text.size()));
            const QDir folder = QFileInfo(path).absoluteDir();
            for (const auto &image : item.Template.Images)
            {
                QFile file(folder.filePath(QString::fromStdString(image.Name)));
                if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
                {
                    file.write(reinterpret_cast<const char *>(image.Data.data()), static_cast<qint64>(image.Data.size()));
                }
            }
            return static_cast<bool>(out);
        }
    }
    return false;
}

void MainWindow::OnExportList()
{
    if (_index == nullptr)
    {
        return;
    }
    const QString filters = tr("CSV (*.csv);;JSON (*.json);;Text (*.txt)");
    const QString path = QFileDialog::getSaveFileName(this, tr("Export the list of entries"), QString::fromStdString(_settings.LastExtractDirectory), filters);
    if (path.isEmpty())
    {
        return;
    }
    // the selected entries when there are some, else every entry
    auto entries = SelectedEntries();
    if (entries.size() <= 1)
    {
        entries.resize(_index->GetEntryCount());
        std::iota(entries.begin(), entries.end(), 0U);
    }
    std::ofstream out(path.toStdString(), std::ios::trunc);
    Core::WriteEntryList(*_index, entries, Core::ListFormatForFile(path.toStdString()), out);
    if (!out)
    {
        QMessageBox::warning(this, tr("Export"), tr("Cannot write %1").arg(path));
        return;
    }
    statusBar()->showMessage(tr("%1 entries written to %2").arg(entries.size()).arg(path), 6000);
}

void MainWindow::OnExtractAll()
{
    if (_index == nullptr)
    {
        return;
    }
    std::vector<std::uint32_t> all(_index->GetEntryCount());
    std::iota(all.begin(), all.end(), 0U);
    StartExtraction(std::move(all), QFileInfo(QString::fromStdString(_archiveTitle)).fileName(), ChooseDirectory(tr("Extract everything to")));
}

void MainWindow::OnExtractFolder()
{
    if (_index == nullptr)
    {
        return;
    }
    const auto folder = FolderTreeModel::FolderOf(_tree->currentIndex());
    StartExtraction(_index->GetEntriesRecursive(folder), QString::fromStdString(_index->GetFolders()[folder].Name.empty() ? _archiveTitle : _index->GetFolders()[folder].Name),
                    ChooseDirectory(tr("Extract the folder to")));
}

void MainWindow::ExtractAllTo(const QString &directory, bool convertImages, std::shared_ptr<const Core::ExternalTool> tool)
{
    if (_index == nullptr)
    {
        return;
    }
    std::vector<std::uint32_t> all(_index->GetEntryCount());
    std::iota(all.begin(), all.end(), 0U);
    StartExtraction(std::move(all), QString::fromStdString(_archiveTitle), directory, convertImages, std::move(tool));
}

void MainWindow::ExtractSelectedTo(const QString &directory)
{
    StartExtraction(SelectedEntries(), QString::fromStdString(_archiveTitle), directory);
}

auto MainWindow::WaitForJobs() -> int
{
    _jobs->WaitForAll();
    return _jobs->GetFailedEntries();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    _jobs->CancelAll();
    _jobs->WaitForAll();
    _settings.Layout["geometry"] = saveGeometry().toBase64().toStdString();
    _settings.Layout["state"] = saveState().toBase64().toStdString();
    _settings.Theme = ThemeManager::Current() == Theme::DARK ? "dark" : "light";
    _settings.Save();
    QMainWindow::closeEvent(event);
}
} // namespace Dup::Ui
