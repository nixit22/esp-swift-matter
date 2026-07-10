# SwiftMatter — implementation TODO

State: the component **compiles and links**. Node creation, all three sensor
endpoints (temperature, humidity, pressure), attribute writes, attribute
callbacks, and chip-factory NVS provisioning (`MatterFactoryData`) are working.
Remaining gaps are commissioning, additional device types, and a missing
example app.

Work the items roughly in the order below. 1 → 2 is the shortest path to
a genuinely working, end-to-end component. 3 / 4 are breadth and polish.

---

## 1. Commissioning / onboarding — partially done

`run(onEvent:)` now accepts an optional `MatterDevice.Event` callback and prints
the commissioning QR code + manual pairing code via `ChipLogProgress` immediately
after the CHIP stack initializes.

Remaining:

- **Commissioning-window control** — on first boot the window opens automatically
  (BLE advertising). After commissioning, re-opening the window (e.g. via a button)
  requires calling `esp_matter::commissioning::open_basic_commissioning_window()`.
  No Swift wrapper yet.

## 2. Docs reference a non-existent example

`CLAUDE.md` references `MatterTemperatureSensor/sdkconfig.defaults` (two places)
as the "working baseline" — **that file/app does not exist** in the repo.
Either add the example app + `sdkconfig.defaults`, or fix the docs to point
at the real baseline. There is currently no end-to-end runnable Matter example
and `Matter.run()` is never exercised in the test-app.

## 3. Device types — partially done

`TemperatureSensor`, `HumiditySensor`, and `PressureSensor` endpoints exist
(covering both BME280 outputs and AHT20 humidity). Time Synchronization
(root-endpoint cluster 0x0038, `MatterDevice.enableTimeSynchronization()`) is
also done — exercised end-to-end by the `matter-time-test` project. Remaining:

- Common types: on/off light, contact/occupancy sensor, etc.
- A generic `Endpoint` builder so new device types don't each need a bespoke C
  façade function.

## 4. Minor cleanups

- `MatterDevice.Endpoint` is `internal`, not `public` — fine only while everything
  goes through the typed sensor endpoints; a generic API would need it exposed.
