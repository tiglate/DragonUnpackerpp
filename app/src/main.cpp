// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Command line (test hooks, for the tests):
//   dragonunpacker [--plugin=FILE] [--settings=FILE] [--theme=dark|light] [--open=FILE | --rip=FILE] [--folder=a/b] [--select=NAME] [--palette=NAME]
//                  [--filter=TEXT [--search-all]] [--type=EXTENSION] [--model-pose=U] [--workspace=default|browse|preview] [--recursive] [--thumbnails] [--sort=COLUMN|-COLUMN] [--identify] [--create-list=FILE --list-template=text-list|text-db|xml-db|homepage|NAME] [--extract-all=DIR [--convert-images | --tool=NAME] | --extract-selected=DIR] [--screenshot=FILE.png] [--quit]
//   dragonunpacker --benchmark=FILE [--plugin=FILE]     (no window: prints the timings and exits)

#include "Core/AppSettings.h"
#include "Core/ArchiveIndex.h"
#include "Core/DriverHost.h"
#include "Core/HyperRipper.h"
#include "Dudi7/Driver.h"
#include "Ui/AboutDialog.h"
#include "Ui/HyperRipperDialog.h"
#include "Ui/Languages.h"
#include "Ui/ListDialog.h"
#include "Ui/MainWindow.h"
#include "Ui/OptionsDialog.h"
#include "Ui/ThemeManager.h"

// vendored upstream header: its own -Wshadow noise is not ours to fix
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#include "ThirdParty/di.hpp"
#pragma GCC diagnostic pop

#include <spdlog/spdlog.h>

#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>
#include <QTimer>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>

namespace di = boost::di;

namespace
{
auto ArgumentValue(int argc, char **argv, const std::string &name) -> std::string
{
    const std::string prefix = "--" + name + "=";
    for (int i = 1; i < argc; ++i)
    {
        const std::string argument = argv[i];
        if (argument.rfind(prefix, 0) == 0)
        {
            return argument.substr(prefix.size());
        }
    }
    return {};
}

auto HasFlag(int argc, char **argv, const std::string &name) -> bool
{
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--" + name)
        {
            return true;
        }
    }
    return false;
}

/// The driver: --plugin, else $DUP_PLUGIN, else next to the executable (plugins/), else the build tree.
auto PluginFromArguments(int argc, char **argv, const std::filesystem::path &startDirectory) -> Dup::Core::PluginPath
{
    if (const auto requested = ArgumentValue(argc, argv, "plugin"); !requested.empty())
    {
        return Dup::Core::PluginPath{requested};
    }
    return Dup::Core::PluginPath::Find(startDirectory);
}

auto Benchmark(const Dup::Core::PluginPath &plugin, const std::string &file) -> int
{
    using Clock = std::chrono::steady_clock;
    using Ms = std::chrono::milliseconds;
    try
    {
        const auto driver = Dup::Dudi7::Driver::Load(plugin.Path);
        const auto start = Clock::now();
        const auto archive = driver->Open(file);
        const auto opened = Clock::now();
        const auto index = Dup::Core::ArchiveIndex::Build(*archive);
        const auto built = Clock::now();
        std::printf("entries=%u folders=%zu open_ms=%lld index_ms=%lld\n", index.GetEntryCount(), index.GetFolders().size(),
                    static_cast<long long>(std::chrono::duration_cast<Ms>(opened - start).count()),
                    static_cast<long long>(std::chrono::duration_cast<Ms>(built - opened).count()));
        return 0;
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "error: %s\n", error.what());
        return 2;
    }
}

/// --hyperripper=FILE: lists what the HyperRipper finds (type, offset, size), no window.
auto RipList(const std::string &file) -> int
{
    try
    {
        for (const auto &found : Dup::Core::HyperRipper::Scan(file))
        {
            std::printf("%s\t%lld\t%lld\n", found.Type.c_str(), static_cast<long long>(found.Offset), static_cast<long long>(found.Size));
        }
        return 0;
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "error: %s\n", error.what());
        return 2;
    }
}
} // namespace

int main(int argc, char **argv)
{
    if (const auto ripFile = ArgumentValue(argc, argv, "hyperripper"); !ripFile.empty())
    {
        return RipList(ripFile);
    }
    const auto benchmarkFile = ArgumentValue(argc, argv, "benchmark");
    if (!benchmarkFile.empty())
    {
        return Benchmark(PluginFromArguments(argc, argv, std::filesystem::current_path()), benchmarkFile);
    }

    // Qt Multimedia (GStreamer) loads the libcamera plugin, which prints a warning about a missing IPA module and an info line
    // at every start; they say nothing about this program, so only errors are kept (unless the user set the variable)
    if (!qEnvironmentVariableIsSet("LIBCAMERA_LOG_LEVELS"))
    {
        qputenv("LIBCAMERA_LOG_LEVELS", "*:ERROR");
    }
    QApplication application(argc, argv);
    QApplication::setOrganizationName("DragonUnPACKer");
    QApplication::setApplicationName("dragonunpacker6");
    QApplication::setWindowIcon(QIcon(":/icons/dragonunpacker.svg"));

    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern("%H:%M:%S %l %v");

    // Composition root: the two services are singletons shared by everything that asks for them.
    Dup::Core::SettingsFile settingsFile = Dup::Core::SettingsFile::Default();
    if (const auto requested = ArgumentValue(argc, argv, "settings"); !requested.empty())
    {
        settingsFile.Path = requested;
    }
    const auto injector =
        di::make_injector(di::bind<Dup::Core::PluginPath>().to(PluginFromArguments(argc, argv, QApplication::applicationDirPath().toStdString())),
                          di::bind<Dup::Core::SettingsFile>().to(settingsFile), di::bind<Dup::Core::DriverHost>().in(di::singleton),
                          di::bind<Dup::Core::AppSettings>().in(di::singleton));
    auto &settings = injector.create<Dup::Core::AppSettings &>();
    settings.Load();

    // language: --language=pt_BR, else the setting, else the system's; English is the language of the source texts
    QString language = QString::fromStdString(ArgumentValue(argc, argv, "language"));
    if (language.isEmpty())
    {
        language = QString::fromStdString(settings.Language);
    }
    if (language.isEmpty())
    {
        language = QLocale::system().name();
    }
    QTranslator qtTranslator;
    QTranslator appTranslator;
    language = Dup::Ui::ResolveLanguage(language);
    if (qtTranslator.load("qtbase_" + language, QLibraryInfo::path(QLibraryInfo::TranslationsPath)) ||
        qtTranslator.load("qtbase_" + language.section('_', 0, 0), QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
    {
        application.installTranslator(&qtTranslator); // the standard buttons of the dialogs
    }
    if (appTranslator.load(":/i18n/dragonunpacker_" + language + ".qm"))
    {
        application.installTranslator(&appTranslator);
    }
    // Arabic and Hebrew are written from right to left: the whole interface is mirrored
    QApplication::setLayoutDirection(Dup::Ui::IsRightToLeft(language) ? Qt::RightToLeft : Qt::LeftToRight);

    if (const auto themeName = ArgumentValue(argc, argv, "theme"); !themeName.empty())
    {
        settings.Theme = themeName == "light" ? "light" : "dark"; // the command line wins for this run
    }
    Dup::Ui::ThemeManager::Apply(settings.Theme == "light" ? Dup::Ui::Theme::LIGHT : Dup::Ui::Theme::DARK);

    auto window = injector.create<std::unique_ptr<Dup::Ui::MainWindow>>();
    // with --quit the run is automated: no modal dialog may wait for a user
    window->ApplySettings();
    if (settings.DefaultView == "thumbnails")
    {
        window->SetThumbnailView(true);
    }
    window->SetInteractive(!HasFlag(argc, argv, "quit"));
    window->show();

    const auto openFile = ArgumentValue(argc, argv, "open");
    if (!openFile.empty())
    {
        window->OpenArchive(QString::fromStdString(openFile));
    }
    if (const auto folder = ArgumentValue(argc, argv, "folder"); !folder.empty())
    {
        window->ShowFolderByPath(QString::fromStdString(folder));
    }
    if (const auto ripped = ArgumentValue(argc, argv, "rip"); !ripped.empty())
    {
        window->OpenRipped(QString::fromStdString(ripped)); // the finds of the HyperRipper as the shown archive
    }
    if (const auto filter = ArgumentValue(argc, argv, "filter"); !filter.empty())
    {
        window->SetNameFilter(QString::fromStdString(filter), HasFlag(argc, argv, "search-all"));
    }
    if (const auto pose = ArgumentValue(argc, argv, "model-pose"); !pose.empty())
    {
        window->SetModelPose(QString::fromStdString(pose).toDouble()); // "0.5" whatever the locale
    }
    if (const auto type = ArgumentValue(argc, argv, "type"); !type.empty())
    {
        if (!window->SetTypeFilter(QString::fromStdString(type)))
        {
            spdlog::warn("no file of type {} in the archive", type);
        }
    }
    if (const auto workspace = ArgumentValue(argc, argv, "workspace"); !workspace.empty())
    {
        window->SetWorkspace(QString::fromStdString(workspace));
    }
    if (HasFlag(argc, argv, "test-drag-out"))
    {
        // select every row, ask for what a drag would carry, and report: exit 0 when the files exist
        window->SelectAllRows();
        const int made = window->TestDragOut();
        std::printf("drag out: %d files\n", made);
        window.reset();
        return made > 0 ? 0 : 1;
    }
    if (HasFlag(argc, argv, "thumbnails"))
    {
        window->SetThumbnailView(true);
    }
    if (HasFlag(argc, argv, "recursive"))
    {
        window->SetRecursive(true);
    }
    const auto sort = ArgumentValue(argc, argv, "sort"); // column number, "-" before it for descending
    if (!sort.empty())
    {
        window->SortBy(std::abs(std::atoi(sort.c_str())), sort[0] == '-' ? Qt::DescendingOrder : Qt::AscendingOrder);
    }
    const auto palette = ArgumentValue(argc, argv, "palette"); // a .pal entry of the archive for the colour mapped pictures
    if (!palette.empty() && !window->UsePaletteEntryByName(QString::fromStdString(palette)))
    {
        spdlog::error("no palette entry named {}", palette);
    }
    const auto select = ArgumentValue(argc, argv, "select");
    if (!select.empty())
    {
        window->SelectEntryByName(QString::fromStdString(select));
    }

    if (HasFlag(argc, argv, "identify"))
    {
        std::printf("%s", window->IdentifyAll().c_str());
        window.reset();
        return 0;
    }
    if (const auto listFile = ArgumentValue(argc, argv, "create-list"); !listFile.empty())
    {
        if (!window->CreateListFile(QString::fromStdString(listFile), ArgumentValue(argc, argv, "list-template")))
        {
            spdlog::error("cannot create the list {}", listFile);
            return 2;
        }
    }
    // extraction hooks: the exit code is 0 when every entry was extracted, 1 otherwise
    int exitCode = 0;
    if (const auto directory = ArgumentValue(argc, argv, "extract-all"); !directory.empty())
    {
        std::shared_ptr<const Dup::Core::ExternalTool> tool;
        if (const auto toolName = ArgumentValue(argc, argv, "tool"); !toolName.empty())
        {
            tool = window->FindTool(toolName);
            if (tool == nullptr)
            {
                spdlog::error("no usable tool named {}", toolName);
                return 2;
            }
        }
        window->ExtractAllTo(QString::fromStdString(directory), HasFlag(argc, argv, "convert-images"), std::move(tool));
        exitCode = window->WaitForJobs() == 0 ? 0 : 1;
    }
    else if (const auto selected = ArgumentValue(argc, argv, "extract-selected"); !selected.empty())
    {
        window->ExtractSelectedTo(QString::fromStdString(selected));
        exitCode = window->WaitForJobs() == 0 ? 0 : 1;
    }

    // --show-dialog=options|hyperripper|list|about with --screenshot: a picture of the dialog instead of the window
    if (const auto dialogName = ArgumentValue(argc, argv, "show-dialog"); !dialogName.empty() && !ArgumentValue(argc, argv, "screenshot").empty())
    {
        auto &host = injector.create<Dup::Core::DriverHost &>();
        std::unique_ptr<QDialog> dialog;
        if (dialogName == "options")
        {
            dialog = std::make_unique<Dup::Ui::OptionsDialog>(settings, host, window.get());
        }
        else if (dialogName == "about")
        {
            dialog = std::make_unique<Dup::Ui::AboutDialog>(QString("<p>Default driver: 134 formats</p>"), window.get());
        }
        else if (dialogName == "list")
        {
            static const auto templates = Dup::Ui::LoadListTemplates({});
            dialog = std::make_unique<Dup::Ui::ListDialog>(templates, 2, nullptr, window.get());
        }
        else
        {
            dialog = std::make_unique<Dup::Ui::HyperRipperDialog>(QString(), nullptr, window.get());
        }
        dialog->show();
        QApplication::processEvents();
        dialog->grab().save(QString::fromStdString(ArgumentValue(argc, argv, "screenshot")));
        dialog.reset(); // a child of the window: it goes first
        window.reset();
        return 0;
    }

    const auto screenshot = ArgumentValue(argc, argv, "screenshot");
    const bool quit = HasFlag(argc, argv, "quit");
    if (!screenshot.empty())
    {
        QTimer::singleShot(400, window.get(), [&window, screenshot, quit] {
            window->grab().save(QString::fromStdString(screenshot));
            if (quit)
            {
                QApplication::quit();
            }
        });
    }
    else if (quit)
    {
        QTimer::singleShot(0, &application, &QApplication::quit);
    }
    const int result = QApplication::exec();
    window.reset(); // before the services it refers to
    return result != 0 ? result : exitCode;
}
