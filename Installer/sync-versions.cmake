# sync-versions.cmake - propagate project VERSION to installer metadata.
#
# The top-level CMakeLists.txt project(VERSION ...) is the single source of
# truth. This script copies it into the <Version> tags of the six Qt
# Installer Framework metadata files (config.xml + package.xml per platform)
# so a release bump only ever edits CMakeLists.txt.
#
# When the version actually changes, the <ReleaseDate> tags in the three
# package.xml files are stamped with the current UTC date as well; otherwise
# they are left untouched, keeping the script idempotent.
#
# Usage:  cmake -P Installer/sync-versions.cmake
# Runs automatically from each platform's make-installer script; CI fails
# the build if the tree drifts out of sync (see .github/workflows/ci.yml).
cmake_minimum_required(VERSION 3.21)

set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

file(READ "${REPO_ROOT}/CMakeLists.txt" ROOT_CMAKE)
if(NOT ROOT_CMAKE MATCHES "project\\([^\\)]*VERSION[ \t]+([0-9]+\\.[0-9]+\\.[0-9]+)")
    message(FATAL_ERROR "Could not parse project VERSION from CMakeLists.txt")
endif()
set(VERSION "${CMAKE_MATCH_1}")

set(FILES
    "Installer/Windows/config/config.xml"
    "Installer/Windows/packages/com.mdeditor.markdowneditor/meta/package.xml"
    "Installer/MacOS/config/config.xml"
    "Installer/MacOS/packages/com.mdeditor.markdowneditor/meta/package.xml"
    "Installer/Linux/config/config.xml"
    "Installer/Linux/packages/com.mdeditor.markdowneditor/meta/package.xml")

set(PACKAGE_FILES
    "Installer/Windows/packages/com.mdeditor.markdowneditor/meta/package.xml"
    "Installer/MacOS/packages/com.mdeditor.markdowneditor/meta/package.xml"
    "Installer/Linux/packages/com.mdeditor.markdowneditor/meta/package.xml")

set(CHANGED 0)
foreach(REL IN LISTS FILES)
    set(ABS "${REPO_ROOT}/${REL}")
    if(NOT EXISTS "${ABS}")
        message(FATAL_ERROR "Installer metadata file missing: ${REL}")
    endif()
    file(READ "${ABS}" CONTENT)
    string(REGEX REPLACE "<Version>[^<]*</Version>"
        "<Version>${VERSION}</Version>" UPDATED "${CONTENT}")
    if(CONTENT STREQUAL UPDATED)
        message(STATUS "Already in sync: ${REL}")
    else()
        file(WRITE "${ABS}" "${UPDATED}")
        message(STATUS "Updated ${REL} to ${VERSION}")
        math(EXPR CHANGED "${CHANGED} + 1")
    endif()
endforeach()

if(CHANGED GREATER 0)
    string(TIMESTAMP TODAY "%Y-%m-%d" UTC)
    foreach(REL IN LISTS PACKAGE_FILES)
        set(ABS "${REPO_ROOT}/${REL}")
        file(READ "${ABS}" CONTENT)
        string(REGEX REPLACE "<ReleaseDate>[^<]*</ReleaseDate>"
            "<ReleaseDate>${TODAY}</ReleaseDate>" UPDATED "${CONTENT}")
        if(NOT CONTENT STREQUAL UPDATED)
            file(WRITE "${ABS}" "${UPDATED}")
            message(STATUS "Stamped ${REL} release date ${TODAY}")
        endif()
    endforeach()
endif()
message(STATUS "Version ${VERSION}: ${CHANGED} file(s) updated.")
