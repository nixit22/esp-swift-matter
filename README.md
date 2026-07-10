# SwiftMatter

Swift bindings for the ESP-Matter SDK — root node, sensor/valve/mode-select/power-source endpoints, and the Time Synchronization cluster. Swift module name: **`Matter`**.

Depends on: `SwiftPlatform`, `SwiftNVS`, `SwiftSupport`, `espressif/esp_matter` (registry, pinned 1.5.0).

## Usage

```swift
import Matter

let matter = MatterDevice()
let temp = TemperatureSensorEndpoint(matter, min: -55, max: 125)
let hum  = HumiditySensorEndpoint(matter, min: 0, max: 100)
let pres = PressureSensorEndpoint(matter, min: 300, max: 1100)
matter.enableTimeSynchronization()
matter.run { event in
    if event == .commissioningComplete { /* commissioned! */ }
}
temp.set(23.5)
hum.set(65.0)
pres.set(1013.0)
```

Declaring `SwiftMatter` in an app's `REQUIRES` triggers the IDF Component Manager to fetch `espressif/esp_matter` and its transitive dependencies into `managed_components/`.

See [`CLAUDE.md`](CLAUDE.md) for the full endpoint list, factory-data setup, and non-obvious patterns.

## License

MIT — see [LICENSE](LICENSE).
