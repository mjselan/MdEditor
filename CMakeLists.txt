cmake_minimum_required(VERSION 3.21)

project(MarkdownEditor
    VERSION 1.0.1
    DESCRIPTION "Markdown Editor"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# qt_standard_project_setup() below enables AUTOMOC/AUTOUIC/AUTORCC, so the
# explicit set() calls are intentionally omitted here.

if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets PrintSupport)

qt_standard_project_setup()

set(CORE_SOURCES
    src/mainwindow.cpp
    src/markdowneditor.cpp
    src/markdownhighlighter.cpp
    src/markdownpreview.cpp
    src/outlinepanel.cpp
    src/findreplacebar.cpp
    src/theme.cpp
    src/spellchecker.cpp
    src/appicons.cpp)

set(CORE_HEADERS
    src/mainwindow.h
    src/markdowneditor.h
    src/markdownhighlighter.h
    src/markdownpreview.h
    src/outlinepanel.h
    src/findreplacebar.h
    src/theme.h
    src/spellchecker.h)

# Everything except main() lives in a static library so the application
# and the test suite share one compilation instead of building every
# source file twice.
add_library(mdeditor_core STATIC ${CORE_SOURCES} ${CORE_HEADERS})
target_include_directories(mdeditor_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(mdeditor_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::PrintSupport)

qt_add_executable(markdowneditor WIN32 src/main.cpp)
target_link_libraries(markdowneditor PRIVATE mdeditor_core)
# Single source of truth for the release version: packagers, the About
# dialog (via QCoreApplication::applicationVersion), and the installer
# scripts all derive it from project(VERSION).
target_compile_definitions(markdowneditor PRIVATE
    "MARKDOWNEDITOR_VERSION=\"${PROJECT_VERSION}\"")

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
        VERBATIM
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
        VERBATIM
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
# MARKDOWNEDITOR_SPELLCHECK controls the backend: AUTO (default) uses Sonnet
# when found, ON makes a missing Sonnet a configure error (for packagers),
# OFF skips detection entirely.
#
# A full KF6 Sonnet + static hunspell stack lives in build/prefix (built by
# tools\windows\hunspell-cl.bat + tools\windows\deps-sonnet.bat on Windows,
# or the matching native cmake invocations on Linux). When that prefix
# exists it is searched first -- either the plain lib/ layout or the
# multi-arch lib/<triplet>/ layout -- otherwise a system KF6Sonnet is used
# if present. The Windows DLL copy steps below are deliberately limited to
# WIN32; on other platforms the build-tree RPATH covers the prefix
# libraries.
set(MARKDOWNEDITOR_SPELLCHECK "AUTO" CACHE STRING
    "Sonnet spell check backend: AUTO, ON or OFF")
set_property(CACHE MARKDOWNEDITOR_SPELLCHECK PROPERTY STRINGS AUTO ON OFF)
if(NOT MARKDOWNEDITOR_SPELLCHECK MATCHES "^(AUTO|ON|OFF)$")
    message(FATAL_ERROR "MARKDOWNEDITOR_SPELLCHECK must be AUTO, ON or OFF")
endif()

if(NOT MARKDOWNEDITOR_SONNET_PREFIX)
    foreach(_sonnet_cmake_dir
            "${CMAKE_CURRENT_SOURCE_DIR}/build/prefix/lib/cmake/KF6Sonnet"
            "${CMAKE_CURRENT_SOURCE_DIR}/build/prefix/lib/x86_64-linux-gnu/cmake/KF6Sonnet")
        if(EXISTS "${_sonnet_cmake_dir}")
            set(MARKDOWNEDITOR_SONNET_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/build/prefix")
            break()
        endif()
    endforeach()
endif()
if(MARKDOWNEDITOR_SONNET_PREFIX)
    list(PREPEND CMAKE_PREFIX_PATH "${MARKDOWNEDITOR_SONNET_PREFIX}")
endif()

if(MARKDOWNEDITOR_SPELLCHECK STREQUAL "OFF")
    set(KF6Sonnet_FOUND FALSE)
else()
    find_package(KF6Sonnet QUIET COMPONENTS SonnetUi SonnetCore)
    if(MARKDOWNEDITOR_SPELLCHECK STREQUAL "ON" AND NOT KF6Sonnet_FOUND)
        message(FATAL_ERROR "MARKDOWNEDITOR_SPELLCHECK=ON but KF6Sonnet was not found")
    endif()
endif()

# Copies the Windows Sonnet runtime next to a consuming executable. Layout
# matches Sonnet's search paths: client plugins load from
# <exeDir>/kf6/sonnet and dictionaries resolve via the baked
# SONNET_INSTALL_PREFIX (build/prefix/bin/data/hunspell). The macOS bundle
# has a different dylib/plugin layout and is validated by
# Installer/MacOS/macdeploy.sh rather than copying DLLs here.
function(mdeditor_copy_sonnet_runtime target)
    if(NOT (KF6Sonnet_FOUND AND MARKDOWNEDITOR_SONNET_PREFIX AND WIN32))
        return()
    endif()
    foreach(_sonnet_dll KF6SonnetCore.dll KF6SonnetUi.dll)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MARKDOWNEDITOR_SONNET_PREFIX}/bin/${_sonnet_dll}"
                $<TARGET_FILE_DIR:${target}>)
    endforeach()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MARKDOWNEDITOR_SONNET_PREFIX}/lib/plugins/kf6/sonnet/sonnet_hunspell.dll"
            "$<TARGET_FILE_DIR:${target}>/kf6/sonnet/sonnet_hunspell.dll")
endfunction()

if(KF6Sonnet_FOUND)
    target_link_libraries(mdeditor_core PUBLIC KF6::SonnetUi KF6::SonnetCore)
    target_compile_definitions(mdeditor_core PUBLIC MARKDOWNEDITOR_HAVE_SONNET)
    message(STATUS "Sonnet found - spell check enabled")
else()
    message(STATUS "Sonnet not found - building without spell check")
endif()
mdeditor_copy_sonnet_runtime(markdowneditor)

# --- Tests -------------------------------------------------------------------
# Only when built standalone: include(CTest) defines BUILD_TESTING defaulting
# to ON, which would otherwise leak tests into parent projects and Release
# builds that did not ask for them.
if(PROJECT_IS_TOP_LEVEL)
    include(CTest)
endif()
if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    qt_add_executable(test_core tests/test_core.cpp)
    target_link_libraries(test_core PRIVATE mdeditor_core Qt6::Test)
    # The Sonnet backend (link + define) comes transitively from
    # mdeditor_core; only the Windows runtime DLLs need a per-executable
    # copy step.
    mdeditor_copy_sonnet_runtime(test_core)
    add_test(NAME test_core COMMAND test_core)
    # The suite instantiates widgets; force a headless platform so it also
    # passes on CI runners and servers without a display.
    set_tests_properties(test_core PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# --- Install rules (distro packagers, Flatpak, AppImage) --------------------
include(GNUInstallDirs)
install(TARGETS markdowneditor
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .)
if(UNIX AND NOT APPLE)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/Installer/Linux/markdowneditor.desktop"
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/Installer/Linux/markdowneditor.png"
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps
        RENAME markdowneditor.png)
endif()
