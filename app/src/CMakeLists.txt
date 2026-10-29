add_library(Dudi7 STATIC
    Dudi7/SharedLibrary.cpp
    Dudi7/Driver.cpp)
target_include_directories(Dudi7 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${DUDI7_INCLUDE_DIR})
target_link_libraries(Dudi7 PUBLIC Microsoft.GSL::GSL ${CMAKE_DL_LIBS})

add_library(Core STATIC
    Core/AppSettings.cpp
    Core/Converters.cpp
    Core/PngEncoder.cpp
    Core/ArchiveIndex.cpp
    Core/DriverHost.cpp
    Core/EntryNames.cpp
    Core/ExtractionJob.cpp
    Core/DdsDecoder.cpp
    Core/EntryListExport.cpp
    Core/Extractor.cpp
    Core/ObjParser.cpp
    Core/HyperRipper.cpp
    Core/TypeIdentifier.cpp
    Core/FileTypes.cpp
    Core/ListTemplate.cpp
    Core/ExternalTools.cpp
    Core/TextureConverters.cpp
    Core/HrfFile.cpp
    Core/RipNaming.cpp
    Core/PreviewWorker.cpp
    Core/TgaDecoder.cpp
    Core/ThumbnailWorker.cpp)
target_include_directories(Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
target_link_libraries(Core PUBLIC Dudi7 spdlog::spdlog Microsoft.GSL::GSL Threads::Threads ZLIB::ZLIB)

qt_add_resources(RESOURCE_FILES ${PROJECT_SOURCE_DIR}/resources/resources.qrc)
add_executable(dragonunpacker
    main.cpp
    ${RESOURCE_FILES}
    Ui/AboutDialog.cpp
    Ui/ArchiveTableModel.cpp
    Ui/FolderTreeModel.cpp
    Ui/HyperRipperDialog.cpp
    Ui/MpegOptionsDialog.cpp
    Ui/Icons.cpp
    Ui/Languages.cpp
    Ui/ListDialog.cpp
    Ui/ListTemplates.cpp
    Ui/ImageViewer.cpp
    Ui/JobsPanel.cpp
    Ui/MainWindow.cpp
    Ui/MeshViewer.cpp
    Ui/OptionsDialog.cpp
    Ui/PreviewPanel.cpp
    Ui/QtLogSink.cpp
    Ui/ThemeManager.cpp)
target_compile_definitions(dragonunpacker PRIVATE DUP_VERSION="${PROJECT_VERSION}")
target_link_libraries(dragonunpacker PRIVATE Core Qt6::Widgets Qt6::Multimedia Qt6::Svg)

# Translations: lupdate writes translations/*.ts (target update_translations), lrelease makes the .qm files that are
# embedded in the executable and loaded by main.cpp according to the language of the settings or of the system.
file(GLOB TRANSLATED_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Ui/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/Ui/*.h)
qt_add_translations(dragonunpacker
    TS_FILES ${PROJECT_SOURCE_DIR}/translations/dragonunpacker_pt_BR.ts
             ${PROJECT_SOURCE_DIR}/translations/dragonunpacker_es.ts
             ${PROJECT_SOURCE_DIR}/translations/dragonunpacker_it.ts
             ${PROJECT_SOURCE_DIR}/translations/dragonunpacker_fr.ts
             ${PROJECT_SOURCE_DIR}/translations/dragonunpacker_de.ts
             ${PROJECT_SOURCE_DIR}/translations/dragonunpacker_pl.ts
             ${PROJECT_SOURCE_DIR}/translations/dragonunpacker_ru.ts
             ${PROJECT_SOURCE_DIR}/translations/dragonunpacker_zh_CN.ts
             ${PROJECT_SOURCE_DIR}/translations/dragonunpacker_ja.ts
             ${PROJECT_SOURCE_DIR}/translations/dragonunpacker_ar.ts
             ${PROJECT_SOURCE_DIR}/translations/dragonunpacker_he.ts
    SOURCES ${TRANSLATED_SOURCES}
    RESOURCE_PREFIX /i18n)

# Installation: the executable in bin/, the Pascal driver in lib/dragonunpacker/plugins/ (found from the executable by
# DriverHost: PluginPath::Find), desktop file and icon in share/. The driver is built by ./make.sh build driver.
install(TARGETS dragonunpacker RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
set(DRIVER_BUILT ${PROJECT_SOURCE_DIR}/../build/plugins/libdrv_default.d6d.so)
if (EXISTS ${DRIVER_BUILT})
    install(FILES ${DRIVER_BUILT} DESTINATION ${CMAKE_INSTALL_LIBDIR}/dragonunpacker/plugins)
else ()
    message(WARNING "the driver ${DRIVER_BUILT} does not exist yet: build it (./make.sh build driver) before installing")
endif ()
install(FILES ${PROJECT_SOURCE_DIR}/resources/dragonunpacker.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ${PROJECT_SOURCE_DIR}/resources/dragonunpacker.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
foreach (size 16 24 32 48 64 128 256 512)
    install(FILES ${PROJECT_SOURCE_DIR}/resources/icons/dragonunpacker-${size}.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps RENAME dragonunpacker.png)
endforeach ()
