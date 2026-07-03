# SwiftMatter

Swift bindings for the ESP-Matter SDK. Provides a Swift module `Matter` that wraps the registry component `espressif/esp_matter` (1.5.0).

Depends on: `SwiftPlatform`, `SwiftNVS`, `SwiftSupport`, `espressif/esp_matter`.

Consumed via the IDF Component Manager — declaring `SwiftMatter` in your app's `REQUIRES` triggers the manager to fetch `espressif/esp_matter` and its transitive dependencies into `managed_components/`.

## License

MIT — see [LICENSE](LICENSE).
