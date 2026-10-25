// Copyright (c) 2026 Nicolas Christe
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// swift-tools-version: 6.0

// Swift wrapper for the ESP-Matter SDK, built by swift_package() from esp-swift-support.
//
// SwiftPM side: CMatter is a C++ target (the extern "C" facade over esp_matter), Matter
// the Swift API. It uses ESP-IDF's and esp_matter's headers; -fno-exceptions and
// -fno-rtti come from the toolset swift_build.py writes, so no unsafeFlags are needed.
//
// IDF side (same folder): CMakeLists.txt, idf_component.yml and project_include.cmake
// make the IDF component manager fetch esp_matter and patch it (esp_matter.patch).

import PackageDescription

let package = Package(
    name: "esp-swift-matter",
    products: [
        .library(name: "Matter", targets: ["Matter"]),
    ],
    dependencies: [
        .package(url: "https://github.com/nixit22/esp-swift-support.git", branch: "SPM"),
        .package(url: "https://github.com/nixit22/esp-swift-platform.git", branch: "SPM"),
        .package(url: "https://github.com/nixit22/esp-swift-nvs.git", branch: "SPM"),
    ],
    targets: [
        // include/CMatter.h is the umbrella header: SwiftPM generates the module map.
        .target(
            name: "CMatter",
            dependencies: [.product(name: "SwiftSupport", package: "esp-swift-support")]
        ),
        .target(
            name: "Matter",
            dependencies: [
                "CMatter",
                .product(name: "Platform", package: "esp-swift-platform"),
                .product(name: "NVS", package: "esp-swift-nvs"),
            ]
        ),
    ],
    swiftLanguageModes: [.v5],
    cxxLanguageStandard: .gnucxx17
)
