cmake_minimum_required(VERSION 3.21)

project(FreebuffMarkdownEditor
    VERSION 1.0.0
    DESCRIPTION "Freebuff Markdown Editor"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets PrintSupport)

qt_standard_project_setup()

set(EDITOR_SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/markdowneditor.cpp
    src/markdownhighlighter.cpp
    src/markdownpreview.cpp
    src/outlinepanel.cpp
    src/findreplacebar.cpp
    src/theme.cpp
    src/spellchecker.cpp
    src/appicons.cpp)

set(EDITOR_HEADERS
    src/mainwindow.h
    src/markdowneditor.h
    src/markdownhighlighter.h
    src/markdownpreview.h
    src/outlinepanel.h
    src/findreplacebar.h
    src/theme.h
    src/spellchecker.h)

qt_add_executable(markdowneditor WIN32
    ${EDITOR_SOURCES}
    ${EDITOR_HEADERS})

target_include_directories(markdowneditor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(markdowneditor PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::PrintSupport)

# --- Application icon (Windows shell embedding) ------------------------------
# The icon artwork is painted in code (src/appicons.cpp). The makeico tool
# renders it into resources/app.ico (classic uncompressed BMP entries), and the
# .rc script embeds it into the exe. Regenerated at configure time when stale.
if(WIN32)
    # Target is named tool_makeico (not makeico) so it sorts *after*
    # markdowneditor in the CMake file API / Qt Creator run-config list.
    # Qt Creator defaults to run configuration index 0; with "makeico"
    # alphabetically first it would launch the icon tool with no args
    # ("usage: makeico <output.ico>") instead of the editor.
    add_executable(tool_makeico tools/makeico/main.cpp src/appicons.cpp)
    target_include_directories(tool_makeico PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tool_makeico PRIVATE Qt6::Gui)
    set_target_properties(tool_makeico PROPERTIES WIN32_EXECUTABLE OFF)

    set(FREEBUFF_APP_RC "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
    set(FREEBUFF_APP_ICO "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.ico")
    add_custom_command(
        OUTPUT "${FREEBUFF_APP_ICO}"
        COMMAND ${CMAKE_COMMAND} -E env
            "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}"
            $<TARGET_FILE:tool_makeico> "${FREEBUFF_APP_ICO}"
        DEPENDS tool_makeico tools/makeico/main.cpp src/appicons.cpp src/appicons.h
        COMMENT "Generating resources/app.ico")
    add_custom_target(app_icon DEPENDS "${FREEBUFF_APP_ICO}")
    add_dependencies(markdowneditor app_icon)

    target_sources(markdowneditor PRIVATE "${FREEBUFF_APP_RC}")
    set_source_files_properties("${FREEBUFF_APP_RC}" PROPERTIES
        OBJECT_DEPENDS "${FREEBUFF_APP_ICO}")
endif()

# --- Optional spell check via KDE Frameworks Sonnet -------------------------
# When Sonnet is not installed the editor builds and runs normally with
# spell check disabled; no other functionality is affected.
#
# A full KF6 Sonnet + static hunspell stack lives in build/prefix (built by
# build\hunspell-cl.bat + build\deps-sonnet.bat). When that prefix exists it
# is searched first; otherwise a system KF6Sonnet is used if present.
if(NOT FREEBUFF_SONNET_PREFIX AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/build/prefix/lib/cmake/KF6Sonnet")
    set(FREEBUFF_SONNET_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/build/prefix")
endif()

set(FREEBUFF_SONNET_SEARCH "")
if(FREEBUFF_SONNET_PREFIX)
    list(APPEND FREEBUFF_SONNET_SEARCH "${FREEBUFF_SONNET_PREFIX}/lib/cmake/KF6Sonnet")
endif()

find_package(KF6Sonnet QUIET COMPONENTS SonnetUi SonnetCore
    PATHS ${FREEBUFF_SONNET_SEARCH}
    NO_DEFAULT_PATH)
if(NOT KF6Sonnet_FOUND)
    find_package(KF6Sonnet QUIET COMPONENTS SonnetUi SonnetCore)
endif()

if(KF6Sonnet_FOUND)
    target_link_libraries(markdowneditor PRIVATE KF6::SonnetUi KF6::SonnetCore)
    target_compile_definitions(markdowneditor PRIVATE FREEBUFF_HAVE_SONNET)
    if(FREEBUFF_SONNET_PREFIX)
        # Runtime layout for a prefix-based Sonnet: Loader searches
        # QCoreApplication::libraryPaths() + "/kf6/sonnet" (the exe dir), and
        # dictionary data is found via the baked SONNET_INSTALL_PREFIX
        # (build/prefix/bin/data/hunspell). No extra environment needed.
        foreach(_sonnet_dll KF6SonnetCore.dll KF6SonnetUi.dll)
            add_custom_command(TARGET markdowneditor POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${FREEBUFF_SONNET_PREFIX}/bin/${_sonnet_dll}"
                    $<TARGET_FILE_DIR:markdowneditor>)
        endforeach()
        add_custom_command(TARGET markdowneditor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${FREEBUFF_SONNET_PREFIX}/lib/plugins/kf6/sonnet/sonnet_hunspell.dll"
                "$<TARGET_FILE_DIR:markdowneditor>/kf6/sonnet/sonnet_hunspell.dll")
    endif()
    message(STATUS "Sonnet found - spell check enabled")
else()
    message(STATUS "Sonnet not found - building without spell check")
endif()

# --- Tests -------------------------------------------------------------------
include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    qt_add_executable(test_core tests/test_core.cpp
        src/markdowneditor.cpp
        src/markdowneditor.h
        src/markdownhighlighter.cpp
        src/markdownhighlighter.h
        src/markdownpreview.cpp
        src/markdownpreview.h
        src/mainwindow.cpp
        src/mainwindow.h
        src/outlinepanel.cpp
        src/outlinepanel.h
        src/findreplacebar.cpp
        src/findreplacebar.h
        src/theme.cpp
        src/theme.h
        src/spellchecker.cpp
        src/spellchecker.h
        src/appicons.cpp
        src/appicons.h)
    target_include_directories(test_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(test_core PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::PrintSupport
        Qt6::Test)
    # Mirror the optional Sonnet backend so the enabled code path is covered
    # by CI wherever Sonnet is installed.
    if(KF6Sonnet_FOUND)
        target_link_libraries(test_core PRIVATE KF6::SonnetUi KF6::SonnetCore)
        target_compile_definitions(test_core PRIVATE FREEBUFF_HAVE_SONNET)
        if(FREEBUFF_SONNET_PREFIX)
            foreach(_sonnet_dll KF6SonnetCore.dll KF6SonnetUi.dll)
                add_custom_command(TARGET test_core POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${FREEBUFF_SONNET_PREFIX}/bin/${_sonnet_dll}"
                        $<TARGET_FILE_DIR:test_core>)
            endforeach()
            add_custom_command(TARGET test_core POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${FREEBUFF_SONNET_PREFIX}/lib/plugins/kf6/sonnet/sonnet_hunspell.dll"
                    "$<TARGET_FILE_DIR:test_core>/kf6/sonnet/sonnet_hunspell.dll")
        endif()
    endif()
    add_test(NAME test_core COMMAND test_core)
endif()
