# Unit tests of the portable layers (Dudi7, Core): no Qt, no display. The tests that open archives need the Pascal driver
# (built by ./make.sh build driver) and the synthetic fixtures of the driver tests, which are generated here.

add_executable(dragonunpacker_tests
    test_main.cpp
    TestSupport.h
    test_entry_names.cpp
    test_file_masks.cpp
    test_archive_index.cpp
    test_app_settings.cpp
    test_tga_decoder.cpp
    test_outrage.cpp
    test_oof_pose.cpp
    test_oof_render.cpp
    test_rig.cpp
    test_driver.cpp
    test_extractor.cpp
    test_extraction_job.cpp
    test_preview.cpp
    test_thumbnails.cpp
    test_hyper_ripper.cpp
    test_hrf.cpp
    test_file_types.cpp
    test_list_template.cpp
    test_external_tools.cpp
    test_texture_converters.cpp
    test_entry_list.cpp)
target_include_directories(dragonunpacker_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src)
find_package(Threads REQUIRED)
target_link_libraries(dragonunpacker_tests PRIVATE Core Threads::Threads)

set(PLUGIN_DEFAULT ${PROJECT_SOURCE_DIR}/../build/plugins/libdrv_default.d6d.so)
set(FIXTURES_DIR ${CMAKE_CURRENT_BINARY_DIR}/fixtures)

include(CTest)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_test(NAME fixtures_generate
         COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/../driver/tests/make_fixtures.py ${FIXTURES_DIR})
set_tests_properties(fixtures_generate PROPERTIES FIXTURES_SETUP fixtures)

add_test(NAME unit COMMAND dragonunpacker_tests)
set_tests_properties(unit PROPERTIES
    FIXTURES_REQUIRED fixtures
    ENVIRONMENT "DUP_PLUGIN=${PLUGIN_DEFAULT};DUP_FIXTURES=${FIXTURES_DIR}")

# Smoke tests of the application itself, without a display (Qt's offscreen platform).
set(APP_ENV "DUP_PLUGIN=${PLUGIN_DEFAULT};QT_QPA_PLATFORM=offscreen")
add_test(NAME ui_extract_all
         COMMAND dragonunpacker --settings=${CMAKE_CURRENT_BINARY_DIR}/ui_settings.ini --open=${FIXTURES_DIR}/dups.wad
                 --extract-all=${CMAKE_CURRENT_BINARY_DIR}/ui_out --quit)
add_test(NAME ui_screenshot
         COMMAND dragonunpacker --settings=${CMAKE_CURRENT_BINARY_DIR}/ui_settings.ini --open=${FIXTURES_DIR}/test.pak
                 --folder=sub/dir --screenshot=${CMAKE_CURRENT_BINARY_DIR}/ui_shot.png --quit)
add_test(NAME ui_open_missing_file_does_not_block
         COMMAND dragonunpacker --settings=${CMAKE_CURRENT_BINARY_DIR}/ui_settings.ini --open=/nonexistent/file.pak --quit)
set_tests_properties(ui_extract_all ui_screenshot ui_open_missing_file_does_not_block PROPERTIES
    FIXTURES_REQUIRED fixtures ENVIRONMENT "${APP_ENV}" TIMEOUT 60)

# the HyperRipper on a file of this build tree (any binary has something or nothing to find: the run must end cleanly)
add_test(NAME ui_hyperripper_open
         COMMAND dragonunpacker --settings=${CMAKE_CURRENT_BINARY_DIR}/ui_settings.ini --rip=${FIXTURES_DIR}/obj22.bundle --thumbnails
                 --screenshot=${CMAKE_CURRENT_BINARY_DIR}/ui_rip.png --quit)
set_tests_properties(ui_hyperripper_open PROPERTIES FIXTURES_REQUIRED fixtures ENVIRONMENT "${APP_ENV}" TIMEOUT 60)

foreach(DIALOG options hyperripper)
    add_test(NAME ui_dialog_${DIALOG}
             COMMAND dragonunpacker --settings=${CMAKE_CURRENT_BINARY_DIR}/ui_settings.ini --show-dialog=${DIALOG}
                     --screenshot=${CMAKE_CURRENT_BINARY_DIR}/ui_dialog_${DIALOG}.png --quit)
    set_tests_properties(ui_dialog_${DIALOG} PROPERTIES ENVIRONMENT "${APP_ENV}" TIMEOUT 60)
endforeach()

add_test(NAME ui_language_pt_BR
         COMMAND dragonunpacker --settings=${CMAKE_CURRENT_BINARY_DIR}/ui_settings.ini --language=pt_BR --open=${FIXTURES_DIR}/test.pak
                 --screenshot=${CMAKE_CURRENT_BINARY_DIR}/ui_pt.png --quit)
set_tests_properties(ui_language_pt_BR PROPERTIES FIXTURES_REQUIRED fixtures ENVIRONMENT "${APP_ENV}" TIMEOUT 60)

# a thumbnail clicked in the thumbnail view is what "Extract selected" extracts (the click selects one cell, not a row)
add_test(NAME ui_extract_selected_thumbnail
         COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_CURRENT_BINARY_DIR}/ui_thumb_out)
add_test(NAME ui_extract_selected_thumbnail_run
         COMMAND dragonunpacker --settings=${CMAKE_CURRENT_BINARY_DIR}/ui_settings.ini --open=${FIXTURES_DIR}/test.pak --thumbnails
                 --select=hello.txt --extract-selected=${CMAKE_CURRENT_BINARY_DIR}/ui_thumb_out --quit)
add_test(NAME ui_extract_selected_thumbnail_check
         COMMAND ${CMAKE_COMMAND} -E cat ${CMAKE_CURRENT_BINARY_DIR}/ui_thumb_out/hello.txt)
set_tests_properties(ui_extract_selected_thumbnail PROPERTIES FIXTURES_SETUP thumb_out)
set_tests_properties(ui_extract_selected_thumbnail_run PROPERTIES FIXTURES_REQUIRED "fixtures;thumb_out" FIXTURES_SETUP thumb_done
                     ENVIRONMENT "${APP_ENV}" TIMEOUT 60)
set_tests_properties(ui_extract_selected_thumbnail_check PROPERTIES FIXTURES_REQUIRED thumb_done)

add_test(NAME ui_drag_out
         COMMAND dragonunpacker --settings=${CMAKE_CURRENT_BINARY_DIR}/ui_settings.ini --open=${FIXTURES_DIR}/dups.wad --test-drag-out --quit)
set_tests_properties(ui_drag_out PROPERTIES FIXTURES_REQUIRED fixtures ENVIRONMENT "${APP_ENV}" TIMEOUT 60)

# every text of the interface is translated in every language, with the same %1 and line breaks (tools/translations_data.py)
add_test(NAME translations_complete
         COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/apply_translations.py --check)

# the interface starts in a language written from right to left, and in one with another script
foreach(LANGUAGE ar he ja)
    add_test(NAME ui_language_${LANGUAGE}
             COMMAND dragonunpacker --settings=${CMAKE_CURRENT_BINARY_DIR}/ui_settings_${LANGUAGE}.ini --language=${LANGUAGE}
                     --open=${FIXTURES_DIR}/test.pak --screenshot=${CMAKE_CURRENT_BINARY_DIR}/ui_${LANGUAGE}.png --quit)
    set_tests_properties(ui_language_${LANGUAGE} PROPERTIES FIXTURES_REQUIRED fixtures ENVIRONMENT "${APP_ENV}" TIMEOUT 60)
endforeach()

# the lists made from the templates of v5 (File > Create a list from a template), by the command line hook
foreach(TEMPLATE text-list text-db xml-db homepage)
    add_test(NAME ui_create_list_${TEMPLATE}
             COMMAND dragonunpacker --settings=${CMAKE_CURRENT_BINARY_DIR}/ui_settings.ini --open=${FIXTURES_DIR}/test.pak
                     --create-list=${CMAKE_CURRENT_BINARY_DIR}/ui_list_${TEMPLATE}.txt --list-template=${TEMPLATE} --quit)
    set_tests_properties(ui_create_list_${TEMPLATE} PROPERTIES FIXTURES_REQUIRED fixtures ENVIRONMENT "${APP_ENV}" TIMEOUT 60)
endforeach()

# the identification of the file types from the content, by the command line hook
add_test(NAME ui_identify
         COMMAND dragonunpacker --settings=${CMAKE_CURRENT_BINARY_DIR}/ui_settings.ini --open=${FIXTURES_DIR}/test.pak --identify --quit)
set_tests_properties(ui_identify PROPERTIES FIXTURES_REQUIRED fixtures ENVIRONMENT "${APP_ENV}" TIMEOUT 60 PASS_REGULAR_EXPRESSION "hello.txt\ttext\t")
