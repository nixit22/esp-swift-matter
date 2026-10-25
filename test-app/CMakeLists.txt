# Copyright (c) 2026 Nicolas Christe
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

cmake_minimum_required(VERSION 3.19)

# Resolve the Swift packages before ESP-IDF scans the components: esp-swift-support
# (swift_package()) and any package whose root CMakeLists.txt calls
# idf_component_register() (for example esp-swift-matter's IDF side) are IDF
# components inside SwiftPM checkouts. Any `swift` can resolve (run in this folder,
# so that swiftly's `swift` uses its .swift-version); swift_package() picks the
# Embedded Swift toolchain later.
# SwiftPM works in <build>/swiftpm, the folder swift_package() builds in.
find_program(SWIFT_PROGRAM swift REQUIRED)
set(SWIFTPM_SCRATCH "${CMAKE_BINARY_DIR}/swiftpm")
# ESP-IDF's PATH has its GCC/binutils toolchains (riscv32-esp-elf, esp32ulp-elf...,
# with a plain `ld`), which SwiftPM would use to link the package manifest for the
# host: take them out of PATH for SwiftPM.
string(REPLACE ":" ";" SWIFTPM_PATH "$ENV{PATH}")
list(FILTER SWIFTPM_PATH EXCLUDE REGEX "-elf")
list(JOIN SWIFTPM_PATH ":" SWIFTPM_PATH)
set(SWIFTPM_ENV "${CMAKE_COMMAND}" -E env "PATH=${SWIFTPM_PATH}")
execute_process(
    COMMAND ${SWIFTPM_ENV} "${SWIFT_PROGRAM}" package
        --package-path "${CMAKE_CURRENT_LIST_DIR}" --scratch-path "${SWIFTPM_SCRATCH}" resolve
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    COMMAND_ERROR_IS_FATAL ANY)
execute_process(
    COMMAND ${SWIFTPM_ENV} "${SWIFT_PROGRAM}" package
        --package-path "${CMAKE_CURRENT_LIST_DIR}" --scratch-path "${SWIFTPM_SCRATCH}"
        show-dependencies --format json
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    OUTPUT_VARIABLE SWIFTPM_DEPENDENCIES
    COMMAND_ERROR_IS_FATAL ANY)
file(REAL_PATH "${CMAKE_CURRENT_LIST_DIR}" SWIFTPM_ROOT)
string(REGEX MATCHALL "\"path\" *: *\"[^\"]+\"" SWIFTPM_PATHS "${SWIFTPM_DEPENDENCIES}")
list(REMOVE_DUPLICATES SWIFTPM_PATHS)
foreach(entry IN LISTS SWIFTPM_PATHS)
    string(REGEX REPLACE "^\"path\" *: *\"(.*)\"$" "\\1" package_path "${entry}")
    file(REAL_PATH "${package_path}" package_path)
    # Not every CMakeLists.txt is an IDF component (swift-collections has its own).
    if(NOT package_path STREQUAL SWIFTPM_ROOT AND EXISTS "${package_path}/CMakeLists.txt")
        file(STRINGS "${package_path}/CMakeLists.txt" registers REGEX "^[ \t]*idf_component_register")
        if(registers)
            list(APPEND EXTRA_COMPONENT_DIRS "${package_path}")
        endif()
    endif()
endforeach()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_swift_matter_test_app)
