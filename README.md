# SwiftMatter

Swift bindings for the ESP-Matter SDK — root node, sensor/valve/mode-select/power-source endpoints, and the Time Synchronization cluster. Swift module name: **`Matter`**.

Depends on: `SwiftPlatform`, `SwiftNVS`, `SwiftSupport`, `espressif/esp_matter` (registry, pinned 1.6.1).

## Adding it to a project

This is a [SwiftPM](https://www.swift.org/package-manager/) package, built for ESP-IDF by
`swift_package()` from [esp-swift-support](https://github.com/nixit22/esp-swift-support). In your
app's `Package.swift`:

```swift
dependencies: [
    .package(url: "https://github.com/nixit22/esp-swift-matter.git", from: "0.1.0"),
],
targets: [
    .target(name: "App", dependencies: [.product(name: "Matter", package: "esp-swift-matter")]),
]
```

See esp-swift-support's README for the project setup (top-level `CMakeLists.txt`, `main`
component, Swift toolchain).

## Usage

```swift
import Matter

let matter = MatterDevice()
let temp = TemperatureSensorEndpoint(matter, min: -55, max: 125)
let hum  = HumiditySensorEndpoint(matter, min: 0, max: 100)
let pres = PressureSensorEndpoint(matter, min: 300, max: 1100)
matter.enableTimeSynchronization()
matter.run { event in
    if case .commissioningComplete = event { /* commissioned! */ }
}
temp.set(23.5)
hum.set(65.0)
pres.set(1013.0)
```

The package folder is also an IDF component (`CMakeLists.txt`, `idf_component.yml`, `project_include.cmake`): the app's bootstrap adds it to `EXTRA_COMPONENT_DIRS`, which makes the IDF Component Manager fetch `espressif/esp_matter` and its transitive dependencies into `managed_components/`, and applies `esp_matter.patch`.

See [`CLAUDE.md`](CLAUDE.md) for the full endpoint list, factory-data setup, and non-obvious patterns.

## License

MIT — see [LICENSE](LICENSE).
