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
    src/spellchecker.cpp)

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

# --- Optional spell check via KDE Frameworks Sonnet -------------------------
# When Sonnet is not installed the editor builds and runs normally with
# spell check disabled; no other functionality is affected.
find_package(KF6Sonnet QUIET COMPONENTS SonnetUi SonnetCore)
if(KF6Sonnet_FOUND)
    target_link_libraries(markdowneditor PRIVATE KF6::SonnetUi KF6::SonnetCore)
    target_compile_definitions(markdowneditor PRIVATE FREEBUFF_HAVE_SONNET)
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
        src/spellchecker.h)
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
    endif()
    add_test(NAME test_core COMMAND test_core)
endif()
