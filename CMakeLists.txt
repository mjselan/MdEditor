cmake_minimum_required(VERSION 3.21)

project(MarkdownEditor
    VERSION 1.0.0
    DESCRIPTION "Markdown Editor"
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

# A macOS build must be a real application bundle, rather than a bare
# executable.  Qt's deployment tools (macdeployqt and the Installer Framework)
# expect the Contents/MacOS layout and will otherwise either reject the input
# or produce an installer that macOS cannot launch normally.
if(APPLE)
    set_target_properties(markdowneditor PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME "MarkdownEditor"
        MACOSX_BUNDLE_BUNDLE_NAME "Markdown Editor"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.mdeditor.markdowneditor"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "Copyright (C) MdEditor")
    # Keep Qt's normal build-tree runtime path so the freshly built app can be
    # launched from the build directory.  Installer/MacOS/macdeploy.sh lets
    # macdeployqt rewrite the staged copy to use its bundled Frameworks.
endif()

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

    set(MARKDOWNEDITOR_APP_RC "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
    set(MARKDOWNEDITOR_APP_ICO "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.ico")
    add_custom_command(
        OUTPUT "${MARKDOWNEDITOR_APP_ICO}"
        COMMAND ${CMAKE_COMMAND} -E env
            "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}"
            $<TARGET_FILE:tool_makeico> "${MARKDOWNEDITOR_APP_ICO}"
        DEPENDS tool_makeico tools/makeico/main.cpp src/appicons.cpp src/appicons.h
        COMMENT "Generating resources/app.ico")
    add_custom_target(app_icon DEPENDS "${MARKDOWNEDITOR_APP_ICO}")
    add_dependencies(markdowneditor app_icon)

    target_sources(markdowneditor PRIVATE "${MARKDOWNEDITOR_APP_RC}")
    set_source_files_properties("${MARKDOWNEDITOR_APP_RC}" PROPERTIES
        OBJECT_DEPENDS "${MARKDOWNEDITOR_APP_ICO}")
endif()

# --- Application icon (macOS bundle metadata) --------------------------------
# The artwork is generated from the same C++ painter used by the runtime
# window icon.  A .icns is needed in the bundle for Finder/Dock/Launchpad;
# setting only QApplication::windowIcon() does not provide that metadata.
if(APPLE)
    add_executable(tool_makeicns tools/makeicns/main.cpp src/appicons.cpp)
    target_include_directories(tool_makeicns PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(tool_makeicns PRIVATE Qt6::Gui)
    set_target_properties(tool_makeicns PROPERTIES
        MACOSX_BUNDLE OFF
        WIN32_EXECUTABLE OFF)

    set(MARKDOWNEDITOR_APP_ICNS
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/MarkdownEditor.icns")
    add_custom_command(
        OUTPUT "${MARKDOWNEDITOR_APP_ICNS}"
        COMMAND ${CMAKE_COMMAND} -E env
            "QT_QPA_PLATFORM=offscreen"
            $<TARGET_FILE:tool_makeicns> "${MARKDOWNEDITOR_APP_ICNS}"
        DEPENDS tool_makeicns tools/makeicns/main.cpp src/appicons.cpp src/appicons.h
        COMMENT "Generating resources/MarkdownEditor.icns")
    add_custom_target(app_icon DEPENDS "${MARKDOWNEDITOR_APP_ICNS}")
    add_dependencies(markdowneditor app_icon)
    set_source_files_properties("${MARKDOWNEDITOR_APP_ICNS}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(markdowneditor PRIVATE "${MARKDOWNEDITOR_APP_ICNS}")
    set_target_properties(markdowneditor PROPERTIES
        MACOSX_BUNDLE_ICON_FILE "MarkdownEditor.icns")
endif()

# --- Optional spell check via KDE Frameworks Sonnet -------------------------
# When Sonnet is not installed the editor builds and runs normally with
# spell check disabled; no other functionality is affected.
#
# A full KF6 Sonnet + static hunspell stack lives in build/prefix (built by
# build\hunspell-cl.bat + build\deps-sonnet.bat) for Windows packaging. When
# that prefix exists it is searched first; otherwise a system KF6Sonnet is
# used if present. A non-Windows prefix must provide a matching native layout;
# the Windows DLL copy steps below are deliberately limited to WIN32.
if(WIN32 AND NOT MARKDOWNEDITOR_SONNET_PREFIX
        AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/build/prefix/lib/cmake/KF6Sonnet")
    set(MARKDOWNEDITOR_SONNET_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/build/prefix")
endif()

set(MARKDOWNEDITOR_SONNET_SEARCH "")
if(MARKDOWNEDITOR_SONNET_PREFIX)
    list(APPEND MARKDOWNEDITOR_SONNET_SEARCH "${MARKDOWNEDITOR_SONNET_PREFIX}/lib/cmake/KF6Sonnet")
endif()

find_package(KF6Sonnet QUIET COMPONENTS SonnetUi SonnetCore
    PATHS ${MARKDOWNEDITOR_SONNET_SEARCH}
    NO_DEFAULT_PATH)
if(NOT KF6Sonnet_FOUND)
    find_package(KF6Sonnet QUIET COMPONENTS SonnetUi SonnetCore)
endif()

if(KF6Sonnet_FOUND)
    target_link_libraries(markdowneditor PRIVATE KF6::SonnetUi KF6::SonnetCore)
    target_compile_definitions(markdowneditor PRIVATE MARKDOWNEDITOR_HAVE_SONNET)
    if(MARKDOWNEDITOR_SONNET_PREFIX)
        # Runtime layout for the self-contained Windows prefix: Loader
        # searches QCoreApplication::libraryPaths() + "/kf6/sonnet" (the exe
        # dir), and dictionary data is found via the baked
        # SONNET_INSTALL_PREFIX (build/prefix/bin/data/hunspell). The macOS
        # bundle has a different dylib/plugin layout and is validated by
        # Installer/MacOS/macdeploy.sh rather than copying Windows DLLs here.
        if(WIN32)
            foreach(_sonnet_dll KF6SonnetCore.dll KF6SonnetUi.dll)
                add_custom_command(TARGET markdowneditor POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${MARKDOWNEDITOR_SONNET_PREFIX}/bin/${_sonnet_dll}"
                        $<TARGET_FILE_DIR:markdowneditor>)
            endforeach()
            add_custom_command(TARGET markdowneditor POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${MARKDOWNEDITOR_SONNET_PREFIX}/lib/plugins/kf6/sonnet/sonnet_hunspell.dll"
                    "$<TARGET_FILE_DIR:markdowneditor>/kf6/sonnet/sonnet_hunspell.dll")
        endif()
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
        target_compile_definitions(test_core PRIVATE MARKDOWNEDITOR_HAVE_SONNET)
        if(MARKDOWNEDITOR_SONNET_PREFIX AND WIN32)
            foreach(_sonnet_dll KF6SonnetCore.dll KF6SonnetUi.dll)
                add_custom_command(TARGET test_core POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${MARKDOWNEDITOR_SONNET_PREFIX}/bin/${_sonnet_dll}"
                        $<TARGET_FILE_DIR:test_core>)
            endforeach()
            add_custom_command(TARGET test_core POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${MARKDOWNEDITOR_SONNET_PREFIX}/lib/plugins/kf6/sonnet/sonnet_hunspell.dll"
                    "$<TARGET_FILE_DIR:test_core>/kf6/sonnet/sonnet_hunspell.dll")
        endif()
    endif()
    add_test(NAME test_core COMMAND test_core)
endif()
