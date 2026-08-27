# SwiftMatter

Swift wrapper for the ESP-Matter SDK. Swift module name: **`Matter`**.

Depends on: `SwiftPlatform`, `SwiftNVS`, `SwiftSupport`, `espressif/esp_matter` (registry, pinned to 1.6.0).

## Files

Sources are organized into per-endpoint directories under `src/`; each directory contains the Swift file (named after the public type) and its C facade (`.h`/`.cpp`).

| Directory | Role |
|---|---|
| `src/MatterDevice/` | `MatterDevice.swift` (root node, `Endpoint` nested struct, `run()`); `matter_core.h/cpp` (shared types, attribute helpers, node/stack API); `matter.h` (umbrella header) |
| `src/FactoryData/` | `MatterFactoryData.swift` — writes `serial-num` into the `chip-factory` NVS namespace that esp_matter's device-instance-info provider reads. Pure Swift, no C facade. |
| `src/RootClusters/TimeSynchronization/` | `TimeSynchronization.swift`; `matter_time_synchronization.h/cpp` (Time Synchronization cluster, 0x0038, on the *root* endpoint — not a new device-type endpoint) |
| `src/Endpoints/TemperatureSensorEndpoint/` | `TemperatureSensorEndpoint.swift`; `matter_temperature_sensor.h/cpp` (device type 0x0302) |
| `src/Endpoints/HumiditySensorEndpoint/` | `HumiditySensorEndpoint.swift`; `matter_humidity_sensor.h/cpp` (device type 0x0307) |
| `src/Endpoints/PressureSensorEndpoint/` | `PressureSensorEndpoint.swift`; `matter_pressure_sensor.h/cpp` (device type 0x0305) |
| `src/Endpoints/WaterValveEndpoint/` | `WaterValveEndpoint.swift`; `matter_water_valve.h/cpp` (device type 0x0042) |
| `src/Endpoints/ClosureEndpoint/` | `ClosureEndpoint.swift`; `matter_closure.h/cpp` (Matter 1.5, device type 0x0230) |
| `src/Endpoints/ModeSelectEndpoint/` | `ModeSelectEndpoint.swift`; `matter_mode_select.h/cpp` (cluster 0x0050, device type 0x0050) |
| `src/Endpoints/PowerSourceEndpoint/` | `PowerSourceEndpoint.swift`; `matter_power_source.h/cpp` (device type 0x0011, PowerSource cluster with Battery feature) |
| `module.modulemap` | Clang module `ESP_Matter` — umbrella over `src/MatterDevice/matter.h` |
| `project_include.cmake` | Applies `esp_matter.patch` to the managed esp_matter component at project configure |
| `esp_matter.patch` | Compile-flag gating (`COMPILE_LANGUAGE:C,CXX`) + ClosureControl accessor functions — see the patch's own header comment |

## Public API

```swift
import Matter

let matter = MatterDevice()
let temp = TemperatureSensorEndpoint(matter, min: -55, max: 125)
let hum  = HumiditySensorEndpoint(matter, min: 0, max: 100)
let pres = PressureSensorEndpoint(matter, min: 300, max: 1100)
let valve = WaterValveEndpoint(matter,
    onOpen:  { _ in /* open hardware */  valve.setCurrentState(.open)   },
    onClose: { _ in /* close hardware */ valve.setCurrentState(.closed) })
matter.enableTimeSynchronization()
matter.run { event in
    if case .commissioningComplete = event { /* commissioned! */ }
}
temp.set(23.5)
hum.set(65.0)
pres.set(1013.0)
```

`TemperatureSensorEndpoint`, `HumiditySensorEndpoint`, `PressureSensorEndpoint` are top-level structs.
Each maps to a separate Matter endpoint (device types 0x0302 / 0x0307 / 0x0305).
Scaling: temperature ×100 (0.01 °C, int16), humidity ×100 (0.01 %RH, uint16),
pressure direct-cast hPa→int16 (0.1 kPa units; 1 hPa = 0.1 kPa).
`set(_:)` is non-mutating — works on `let` bindings since it only writes to the C/Matter side.

`WaterValveEndpoint` is a top-level struct mapping to device type 0x0042 (ValveConfigurationAndControl cluster).
`onOpen` / `onClose` callbacks fire from the CHIP event loop (not the attribute-update path).
Call `setCurrentState(.open/.closed)` after hardware actuation to update the Matter attribute.

`ModeSelectEndpoint` is a top-level struct using the Mode Select cluster (0x0050, device type 0x0050).
The caller supplies a `description`, a `modes: [ModeSelectEndpoint.Mode]` array (each with a `UInt8`
value and a string `label`), and an `initialMode`. In Home Assistant it appears as a `select` entity.
`onChange` fires from the CHIP event loop via the `didUpdateAttribute` path when a controller sends
`ChangeToMode`, delivering the raw `UInt8` mode value. The caller maps it to a domain type (enum,
integer, etc.). Call `setCurrentMode(_:)` after `matter.run()` to restore a previously stored value
from NVS.

`ClosureEndpoint` is a top-level struct mapping to device type 0x0230 (ClosureControl cluster, Matter 1.5).
Feature flags select capabilities (positioning, protection, calibration, etc.); use `ClosureEndpoint.features(...)`.
`onMoveTo` / `onStop` / `onCalibrate` fire from the CHIP event loop.
`isReadyToMove` is polled in `WaitingForMotion` state — return `false` to hold (e.g. obstacle sensor).
Call `setMainState(_:)` and `setCurrentPosition(_:)` after hardware state changes.
Pass `initialPosition` at init if boot position is known; otherwise call `setCurrentPosition` after `run()` before the first MoveTo.

`PowerSourceEndpoint` is a top-level struct mapping to device type 0x0011 (PowerSource cluster with the
Battery feature). `set(percent:voltageMv:)` writes BatPercentRemaining (scaled ×2 into Matter's 0–200
half-percent range) and BatVoltage (mV); pass `nil` for either to set the Matter null sentinel.
`set(_:)` is non-mutating, so the endpoint can be stored as `let`.

`MatterDevice.factoryReset()` wraps `esp_matter::factory_reset()` (`esp_matter_factory_reset()` in
`matter_core.h/cpp`) — erases Matter/Thread NVS state and reboots. Only call after `run()` returns:
`chip::Server::ScheduleFactoryReset()` requires the CHIP event loop to already be running, so
calling it earlier is undefined behavior. Does not return on success.

`MatterDevice.enableTimeSynchronization()` adds the Time Synchronization cluster (0x0038) to the
*root* endpoint (not a new device-type endpoint — Matter defines this cluster as living on EP0).
Call it after `MatterDevice()` init and before `run()`. It's a fire-and-forget call, not a
struct — the whole sync flow happens inside CHIP with no Swift-side state or attribute writes.
Passes connectedhomeip's stock `DefaultTimeSyncDelegate`, unmodified — this is the standard
esp_matter/connectedhomeip pattern (no example app in either repo subclasses this delegate).
It inherits the Trusted-Time-Source client feature, compiled in by default
(`TIME_SYNC_ENABLE_TSC_FEATURE` is only forced off when `CONFIG_DISABLE_READ_CLIENT` is set). Once
commissioned, if the controller (or another fabric node) registers itself as a Trusted Time
Source, the device reads UTC time over a CASE session and calls `SetClock_RealTime()`, which
unconditionally does `settimeofday()` regardless of sdkconfig — no SNTP client needed for that
path. Set `CONFIG_ENABLE_SNTP_TIME_SYNC=y` in the host project's sdkconfig anyway: it doesn't gate
the trusted-time-source sync itself, but gates `GetClock_RealTime()` (used for cert-validity checks
and by `DefaultTimeSyncDelegate::UpdateTimeFromPlatformSource`), which otherwise always reports
`CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE` even after the clock has been set once.

`enableTimeSynchronization()` does **not** advertise the cluster's TimeZone (`TZ`) feature bit and
there is no `localDate()`-style API — deliberately. `TZ` is a controller-supplied UTC *display*
offset (derived from phone locale / hub server tz), unrelated to a device's actual geographic
location; it existed in an earlier version of this component but nothing here needs it. Callers
wanting real local solar time (e.g. sunrise/sunset) should use plain UTC (`time()`/
`gettimeofday()`, synced independently — e.g. via the standalone `esp-swift-sntp` component)
together with the device's own known latitude/longitude — Matter has no cluster that provides
the latter.

Clock sync running independently of the Time Synchronization cluster (e.g. via `esp-swift-sntp`)
needs no interaction with the Matter cluster at all, and none is needed:
`DefaultTimeSyncDelegate::UpdateTimeFromPlatformSource()` just checks whether the system clock is
already set (`System::SystemClock().GetClock_RealTime()`) and succeeds if so, so *if and when*
`AttemptToGetTime()` next runs it picks up whatever already set the clock. This component used to
ship its own SNTP entry point (`MatterDevice.startNTPSync(host:)`, backed by
`esp_matter_time_synchronization_start_sntp()`) for exactly this — it was removed once
`esp-swift-sntp` existed as a standalone component so this one wouldn't carry a second, redundant
SNTP implementation.

**No proactive retry of the cluster's own time fetch, on purpose.** connectedhomeip only calls
its internal `AttemptToGetTime()` once, on boot (`kServerReady`) — plus again whenever a
controller resends `SetTrustedTimeSource`. If a reboot happens before the mesh/network settles,
the cluster's own `UTCTime`/`Granularity` attributes can stay stuck at "unsynced" until the next
controller interaction. This is a widely-known, widely-discussed ecosystem gap (see e.g.
IKEA's ALPSTUGA losing its clock on every power cycle) — but the fix has moved to *controllers*,
not devices: matter.js's server added a `TimeSyncManager` that proactively resyncs on
reconnect/failure/a 12h timer, and a Home Assistant custom component pushes time the same way.
Earlier revisions of this component patched a private `SetTrustedTimeSource`/`GetTrustedTimeSource`
friend into `TimeSynchronizationCluster.h` to nudge this from the device side — removed once that
research surfaced; it's not something devices are expected to work around, and our own use of
this cluster doesn't depend on its attributes being timely anyway (see `matter-time-test/TIME-SYNC.md`
for the investigation that originally motivated it).

## Public API — factory data

```swift
import Matter

// Once at boot, after nvs_flash_init() and before MatterDevice()/matter.run():
MatterFactoryData.initialize(serialNumber: MatterFactoryData.macSerial(prefix: "AC"))
```

`initialize` writes `serial-num` into the `chip-factory` NVS namespace only if it's
missing — safe to call on every boot. Never throws (a factory-data write failure on an
already-provisioned device must not block boot); failures are logged via `NVS`'s own
logging and skipped.
`macSerial(prefix:)` is an optional convenience that derives a serial number from the
last 4 bytes of the device's 802.15.4 MAC address; callers with a different serial
scheme (e.g. a manufacturing-programmed serial) just pass their own `serialNumber`
string instead. The `"chip-factory"` namespace and exact key string are hardcoded
internally — they're fixed by connectedhomeip's `ESP32Config.cpp` and are not
caller-configurable. Always writes to the default NVS partition (matching
`CONFIG_CHIP_FACTORY_NAMESPACE_PARTITION_LABEL`'s default); no consumer in this
mono-repo uses a dedicated `fctry` partition, so partition-label support was dropped
from `NVS`/`MatterFactoryData` (see `SwiftNVS/CLAUDE.md`).

`vendor-name`/`product-name`/`hw-ver-str` used to be written here too — removed because
`GenericDeviceInstanceInfoProvider` (the default `DeviceInstanceInfoProvider` unless a
project enables `CONFIG_ENABLE_ESP32_FACTORY_DATA_PROVIDER`) hardcodes
`GetVendorName()`/`GetProductName()`/`GetHardwareVersionString()` to compile-time
`CHIP_DEVICE_CONFIG_*` macros and never reads NVS for them — those writes were dead on
every consumer, not just one project. `GetSerialNumber()` and the numeric
`GetHardwareVersion()` *do* read NVS, which is why `serial-num` stays. Vendor/product
name now need a `CHIP_PROJECT_CONFIG` override header at the app level instead (see
`matter-time-test/chip_project_config.h` for a working example and why the include
wiring is non-obvious on ESP-IDF).

## Non-obvious patterns

**`MatterFactoryData.macSerial` vs. `MatterDevice`'s internal node-label ID** — both
derive an identifier from `esp_read_mac(&mac, ESP_MAC_IEEE802154)`, but are deliberately
not shared: `MatterDevice`'s node label uses the last 2 bytes and aborts on MAC-read
failure (fatal to node creation anyway); `macSerial` uses the last 4 bytes and degrades
to `"PREFIX-00000000"` on failure (a factory-data write must never block boot). Different
byte counts, different failure semantics, different lifetimes (cosmetic label recomputed
every boot vs. serial written once to NVS) — not worth forcing into one abstraction.

**C façade, not C++ interop** — esp_matter is C++, but Swift only sees `extern "C"` signatures in `matter.h` decorated with `SWIFT_NAME(...)` (from `esp-swift-support`'s `swift_support.h`, not the raw `__attribute__((swift_name(...)))` form). The bridging into `esp_matter::node::create`, `esp_matter::endpoint::temperature_sensor::create`, etc. happens in `matter.cpp`. This avoids dragging C++ headers (and `-cxx-interoperability-mode`) into the Swift build.

**Endpoint registry via `privData`** — C attribute callbacks are `@convention(c)` and cannot capture Swift context. `MatterDevice.init` passes `Unmanaged.passRetained(self).toOpaque()` as `privData` to `esp_matter_create_node`; `matter_core.cpp` forwards this through `callback_context.user_priv_data` to every attribute callback. Callbacks recover the `MatterDevice` instance with `Unmanaged<MatterDevice>.fromOpaque(priv_data).takeUnretainedValue()` and look up the endpoint by ID in `matter.endpoints`. No global/static state. The `passRetained` is intentional — the Matter node lives for the process lifetime. Sensor constructors receive the `MatterDevice` instance explicitly and call `matter.register(endpoint:)` directly. `MatterDevice.Endpoint` is a value type (struct); the registry owns the authoritative copy, sensor structs hold a second copy used only for `update()` (which only needs the endpoint ID). `esp_matter_endpoint_set_priv_data` is kept in the C façade but unused by Swift — the per-endpoint `priv_data` path in `matter_core.cpp` is a dead fallback (always null, falls through to `user_priv_data`).

**`_esp_matter_attr_val_t`** — the underscored type in `matter.h` mirrors esp_matter's `esp_matter_attr_val_t` byte-for-byte so Swift callbacks can receive it without seeing the C++ definition. `matter.cpp` reinterprets between the two via `reinterpret_cast`.

**`MatterDevice.run(onEvent:)` starts the full stack** — it sets the default OpenThread platform config, calls `esp_matter::start` (blocks until CHIP init completes), then calls `esp_matter_print_onboarding_codes()` to print the commissioning QR code and manual pairing code via `ChipLogProgress`. The optional `onEvent` closure fires from the CHIP event loop thread for commissioning events (`MatterDevice.Event`). Callers must enable in sdkconfig: BT (NimBLE), OpenThread, custom partition table (Matter needs the NVS layout esp_matter expects), MBEDTLS_HKDF_C, and friends.

**Event callback uses `passUnretained`** — the event callback arg is `Unmanaged.passUnretained(self)`, not `passRetained`, because the `init()` `passRetained` already keeps the object alive for the process lifetime. A second retain would leak.

**Patched at configure time** — `project_include.cmake` applies `esp_matter.patch` to the registry-managed `managed_components/espressif__esp_matter` tree: gates esp_matter's PUBLIC compile options behind `$<$<COMPILE_LANGUAGE:C,CXX>:...>` (without that, the Swift driver chokes on `-Wno-error=...` and `-std=gnu++17`), and adds two small accessor functions (`ClosureControl::GetClusterInstance`/`SetInitialOverallCurrentState`) that esp_matter 1.6.0 doesn't expose publicly but our C facade needs — see the patch file's own header comment for the current list. The patch is idempotent (sentinel `PATCH_APPLIED`) and inert when esp_matter hasn't been downloaded.

If you edit `esp_matter.patch` itself, run `idf.py reconfigure` afterwards — CMake doesn't watch the patch file as a configure dependency, so a plain `idf.py build` won't re-evaluate `project_include.cmake` and the patch won't be re-applied.

**Delegate-pattern clusters use a generic C++ trampoline + shared Swift box** — Some clusters (e.g. ValveConfigurationAndControl) handle commands via a `chip::app::Clusters::Foo::Delegate` virtual interface rather than through `esp_matter::attribute::update()`. For each such cluster, its `matter_*.cpp` file defines a `Swift*Delegate` class (inheriting the cluster's specific base) that stores a single `matter_command_cb_t` function pointer and dispatches from virtual methods using cluster-specific `ESP_MATTER_*_CMD_*` constants as `command_id`. On the Swift side, a single shared `CommandCallbacks` class (in `MatterDevice.swift`) holds a `(UInt16, UInt8) -> Void` dispatch closure — all delegate-pattern endpoints reuse it instead of per-cluster callback box classes. The `void *delegate` field in each cluster's `config_t` is how esp_matter forwards the pointer to `Cluster::SetDefaultDelegate()` during `esp_matter::start()`. Adding a new delegate-pattern cluster requires: (1) a new `Swift*Delegate` C++ class in that cluster's `.cpp` file, (2) a factory function + `ESP_MATTER_*_CMD_*` defines in its `.h` file, (3) a new `*Endpoint.swift` that reuses `CommandCallbacks`.

**ClosureControl cluster delegate wiring (esp_matter 1.6.0)** — `ClosureEndpoint` (`ClosureEndpoint.swift` / `matter_closure.cpp`) implements `chip::app::Clusters::ClosureControl::ClosureControlClusterDelegate` directly and passes it through the normal `cfg.closure_control.delegate` path — no custom init callback needed any more. Earlier (esp_matter 1.5.0), a hand-rolled `ClusterLogic`/`Interface`/`MatterContext` construction plus a custom init CB (`SwiftClosureControlDelegateInitCB`) and an endpoint→delegate lookup table (`gClosureTable[4]`) were needed to work around a bug where the stock `ClosureControlDelegateInitCB` never called `ClusterLogic::Init()`, aborting on the first Stop or MoveTo. 1.6.0 rewrote this cluster's integration (`components/esp_matter/data_model_provider/clusters/closure_control/integration.cpp`, not connectedhomeip's) to construct it correctly itself — that whole workaround was deleted. Two gaps remain, closed via `esp_matter.patch`: `ClosureControlCluster::SetMainState`/`SetOverallCurrentState` are reachable from our facade only through a patch-added `ClosureControl::GetClusterInstance(endpointId)` (esp_matter's own glue keeps its per-endpoint cluster map file-local), and `initial_position` is applied via a patch-added `ClosureControl::MatterClosureControlSetInitialOverallCurrentState(endpointId, state)` called before `esp_matter_start()` (esp_matter's `Config` builder supports `WithInitialOverallCurrentState()` but its own integration code never calls it, always starting from defaults). This mirrors the `GetClusterInstance()`/`Set*()` accessor pattern esp_matter already exposes for `time_synchronization`, `resource_monitor` and `electrical_energy_measurement` — `closure_control` is their newest cluster integration and just hasn't caught up yet; reported upstream at https://github.com/espressif/esp-matter/issues/1824, drop the patch hunk once that lands. `ClosureEndpoint` still uses its own `ClosureCallbacks` box (in `ClosureEndpoint.swift`) rather than the shared `CommandCallbacks`, because the MoveTo command carries three optional parameters that don't fit the generic `(UInt16, UInt8) -> Void` signature. **Unverified**: the delegate/init-callback ordering (`MatterClosureControlSetDelegate` must run before `ESPMatterClosureControlClusterServerInitCallback`) and the whole Closure/TimeSync runtime path are compile-checked only — the test-app never calls `MatterDevice.run()`.

**Pulled via the IDF Component Manager** — `idf_component.yml` declares `espressif/esp_matter` 1.6.0, with `idf: ">=5.5,<6"` kept as an intentional upper bound: upstream's `release/v1.6` branch (what the 1.6.0 registry package is built from) itself recommends ESP-IDF v5.5.5, not v6.0.x — IDF-6 support exists only on esp_matter's unreleased `main` branch (their "v1.7, ongoing" track), not as a numbered component. Any consumer (including `esp-swift-matter/test-app`) that lists `SwiftMatter` in `REQUIRES` triggers the manager to fetch esp_matter and its tree into `managed_components/`. `MatterDevice.run()` only succeeds when the host project enables BT, OpenThread, custom partition table, MBEDTLS_HKDF_C, etc. The test-app intentionally does **not** call `MatterDevice.run()`; it lists `SwiftMatter` as a REQUIRES dependency only, to validate that the component links cleanly.
