// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Core/ExternalTools.h"
#include "Core/TypeIdentifier.h"
#include "Core/ListTemplate.h"
#include "Ui/ListDialog.h"
#include "Core/AppSettings.h"
#include "Core/ArchiveIndex.h"
#include "Core/ArchiveSource.h"
#include "Core/DriverHost.h"
#include "Core/PreviewWorker.h"
#include "Core/ThumbnailWorker.h"
#include "Dudi7/Driver.h"

#include <QMainWindow>
#include <QStringList>

#include <filesystem>
#include <memory>

class QLabel;
class QListView;
class QSlider;
class QStackedWidget;
class QAction;
class QCheckBox;
class QComboBox;
class QTemporaryDir;
class QEvent;
class QLineEdit;
class QMenu;
class QPlainTextEdit;
class QTableView;
class QTreeView;

namespace Dup::Ui
{
class ArchiveTableModel;
class FolderTreeModel;
class ImageViewer;
class JobsPanel;
class PreviewPanel;

/// Prototype of the new main window: Hierarchy (folders), Project (entries), Viewer, Inspector and Console panels,
/// all dockable, over the real driver.
class MainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    /// Both are owned by the composition root (Boost.DI singletons) and outlive the window.
    MainWindow(Core::DriverHost &driverHost, Core::AppSettings &settings);
    ~MainWindow() override;

    /// Opens an archive; returns false (and shows the error) when no format recognizes it.
    auto OpenArchive(const QString &path) -> bool;
    /// Scans a file with the HyperRipper (all types, no dialog) and opens the finds; for the command line.
    auto OpenRipped(const QString &path) -> bool;
    /// A HyperRipper file (.hrf): the finds of an earlier scan, opened on the file that was scanned.
    auto OpenHrf(const QString &path) -> bool;
    /// The same for any source (a driver archive, the result of the HyperRipper).
    auto OpenSource(std::shared_ptr<const Core::ArchiveSource> source) -> bool;
    /// Selects the first entry with this file name (used by the screenshot hooks).
    // in the thumbnail view the thumbnail is clicked with this button
    auto SelectEntryByName(const QString &fileName, Qt::MouseButton button = Qt::LeftButton) -> bool;
    /// The previews of colour mapped pictures use the palette of the entry with this file name (a .pal file of the archive).
    auto UsePaletteEntryByName(const QString &fileName) -> bool;
    /// Without dialogs (the command line hooks): errors go to the log and the status bar instead of a message box.
    void SetInteractive(bool interactive);
    /// Details (table) or thumbnails (icon grid) for the entries.
    void SetThumbnailView(bool thumbnails);
    /// "default", "browse" (folders and entries only) or "preview" (entries, viewer, inspector): which panels are shown.
    void SetWorkspace(const QString &name);
    /// Applies the settings that can change while running (theme, thumbnail size, log level, preview limits, default view).
    void ApplySettings();
    /// Test hook: what a drag of the selected entries gives (the URLs of the extracted files); 0 when nothing was made.
    auto TestDragOut() -> int;
    void SelectAllRows();
    /// Test hook: the name filter, in every folder or only the shown one.
    void SetNameFilter(const QString &text, bool wholeArchive);
    /// Test hook: show the models animated at this position (0 to 1).
    void SetModelPose(double u);
    /// The skinned models start in this animation (test hook).
    void SetModelAnimation(int index);
    /// Test hook: the type filter by extension ("" = entries without one); returns false when the archive has no such type.
    auto SetTypeFilter(const QString &extension) -> bool;
    /// Test hooks: include subfolders / sort by a column (0 name, 1 type, 2 size, 3 offset).
    void SetRecursive(bool recursive);
    void SortBy(int column, Qt::SortOrder order);

    /// Extraction to a directory chosen by the caller (the command line hooks); the job runs on its own thread.
    void ExtractAllTo(const QString &directory, bool convertImages = false,
                         std::shared_ptr<const Core::ExternalTool> tool = nullptr);
    auto ExtractSelectedTo(const QString &directory) -> bool; // false: nothing is selected
    /// Clicks the thumbnail of a row with the mouse (tests: the selection takes the path of a real click)
    void ClickThumbnail(int row, Qt::MouseButton button = Qt::LeftButton);
    /// The right-click menu of the selection, one action per line ("+ " enabled, "- " disabled) (tests)
    auto TestEntryMenu() -> std::string;
    /// Drags with the mouse in the 3D view of the preview (tests): press, a move by delta, release; false if no 3D view is shown
    auto TestViewDrag(Qt::MouseButton button, Qt::KeyboardModifiers modifiers, QPoint delta) -> bool;
    /// Presses a key in the 3D view of the preview (tests); false if no 3D view is shown
    auto TestViewKey(int key) -> bool;

    /// Waits for the jobs; the number of entries that could not be extracted so far.
    auto WaitForJobs() -> int;
    /// Shows a folder of the archive by its path ("a/b"); false when it does not exist.
    auto ShowFolderByPath(const QString &path) -> bool;

  protected:
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

  private slots:
    void OnOpenTriggered();
    void OnFolderChanged();
    void OnEntryChanged();
    void OnExtractSelected();
    void OnExtractSelectedAsPng();
    void OnExtractAllAsPng();
    void OnCreateList();
    void OnIdentifyTypes();
    void StopIdentifying();
    [[nodiscard]] auto MakeList(const Core::ListTemplate &chosen, ListScope scope, bool subfolders, Core::ListSort sort, bool invert, std::size_t *count = nullptr) -> std::string;
    void FillToolMenu(QMenu *menu, bool all);
    [[nodiscard]] auto GetToolsDirectory() const -> std::filesystem::path;

  public:
    /// The command line hook --identify: the type found for every entry, one line each (name, id, confidence, description).
    [[nodiscard]] auto IdentifyAll() -> std::string;
    /// The command line hook --create-list=FILE --list-template=NAME: the whole archive with the template (a bundled one by its folder name, or by its name).
    auto CreateListFile(const QString &path, const std::string &templateName) -> bool;
    /// The usable tool with this INI file name or this name (the command line hook --tool=), or nullptr.
    [[nodiscard]] auto FindTool(const std::string &iniOrName) const -> std::shared_ptr<const Core::ExternalTool>;

  private:
    void OnExportList();
    void OnExtractAll();
    void OnExtractFolder();
    void OnGoUp();
    void ApplyWorkspace(const QString &name);
    void RefreshIcons();
    void OnHyperRipper();
    void OnOptions();
    void OnVisibleChanged();
    void OnGoBack();
    void OnGoForward();

  private:
    void BuildMenus();
    void BuildToolBar(const std::vector<std::pair<QAction *, QString>> &items);
    void RebuildRecentMenu();
    void BuildDocks();
    void ShowFolder(std::uint32_t folder);
    void PreviewEntry(std::uint32_t entry);
    /// -1 to show the pictures with their own colours again.
    void SetPreviewPalette(std::int64_t entry);
    void CloseArchive();
    auto ExtractForDrag(const std::vector<std::uint32_t> &entries) -> QList<QUrl>;
    void SelectFolder(std::uint32_t folder, bool recordHistory);
    [[nodiscard]] auto OpenFilter() const -> QString;
    void StartExtraction(std::vector<std::uint32_t> entries, const QString &title, const QString &directory, bool convertImages = false,
                         std::shared_ptr<const Core::ExternalTool> tool = nullptr);
    auto ChooseDirectory(const QString &title) -> QString;
    auto SelectedEntries() const -> std::vector<std::uint32_t>;
    void FillEntryMenu(QMenu *menu);

    Core::DriverHost &_driverHost;
    Core::AppSettings &_settings;
    std::shared_ptr<const Core::ArchiveSource> _source; // how to open more handles on the shown archive
    std::unique_ptr<Dudi7::IArchive> _archive;
    std::shared_ptr<const Core::ArchiveIndex> _index; // shared with the extraction jobs
    std::string _archiveTitle;

    ArchiveTableModel *_tableModel;
    FolderTreeModel *_treeModel;
    QTableView *_table;
    QTreeView *_tree;
    PreviewPanel *_preview;
    QPlainTextEdit *_inspector;
    QPlainTextEdit *_console;
    QLabel *_statusEntries;
    QLabel *_statusZoom;
    JobsPanel *_jobs;
    QMenu *_recentMenu{nullptr};
    std::unique_ptr<QTemporaryDir> _dragDirectory; // the files of the last drag out of the window
    std::vector<std::pair<QAction *, QString>> _iconActions; // actions with the name of their icon, redrawn when the theme changes
    QLineEdit *_filter{nullptr};
    QComboBox *_typeFilter{nullptr};
    void FillTypeFilter();
    QCheckBox *_searchAll{nullptr};
    QListView *_grid{nullptr};
    QStackedWidget *_views{nullptr};
    QSlider *_thumbnailSize{nullptr};
    std::unique_ptr<Core::ThumbnailWorker> _thumbnailWorker;
    std::unique_ptr<Core::TypeIdentifier> _identifier;
    QTimer *_identifyTimer{nullptr};
    bool _thumbnailView{false};
    std::vector<std::uint32_t> _history;
    int _historyPosition{-1};
    bool _recursive{false};
    bool _interactive{true};
    std::uint64_t _previewGeneration{0};
    std::int64_t _previewPaletteEntry{-1}; // an entry of the shown archive, or -1
    std::unique_ptr<Core::PreviewWorker> _previewWorker; // after the members it does not need: destroyed first
};
} // namespace Dup::Ui
