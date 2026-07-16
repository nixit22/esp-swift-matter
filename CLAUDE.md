# SwiftMatter

Swift wrapper for the ESP-Matter SDK. Swift module name: **`Matter`**.

Depends on: `SwiftPlatform`, `SwiftNVS`, `SwiftSupport`, `espressif/esp_matter` (registry, pinned to 1.5.0).

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
| `project_include.cmake` | Patches esp_matter's `CMakeLists.txt` at project configure |
| `esp_matter.patch` | Gates esp_matter compile options on `COMPILE_LANGUAGE:C,CXX` |

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
    if event == .commissioningComplete { /* commissioned! */ }
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
Passes a custom `SwiftTimeSyncDelegate` (in `matter_time_synchronization.cpp`, subclassing
connectedhomeip's `DefaultTimeSyncDelegate`) rather than a null delegate — see "NTP fallback
delegate" below. It inherits the Trusted-Time-Source client feature, compiled in by default
(`TIME_SYNC_ENABLE_TSC_FEATURE` is only forced off when `CONFIG_DISABLE_READ_CLIENT` is set). Once
commissioned, if the controller (or another fabric node) registers itself as a Trusted Time
Source, the device reads UTC time over a CASE session and calls `SetClock_RealTime()`, which
unconditionally does `settimeofday()` regardless of sdkconfig — no SNTP client needed for that
path. Set `CONFIG_ENABLE_SNTP_TIME_SYNC=y` in the host project's sdkconfig anyway: it doesn't gate
the trusted-time-source sync itself, but gates `GetClock_RealTime()` (used for cert-validity checks
and by `DefaultTimeSyncDelegate::UpdateTimeFromPlatformSource`), which otherwise always reports
`CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE` even after the clock has been set once.

`enableTimeSynchronization()` also calls `esp_matter::cluster::time_synchronization::feature::time_zone::add()`
to advertise the cluster's TimeZone (`TZ`) feature bit. This is the Matter mechanism by which a
UI-less device learns local time: with the bit set, `AutoCommissioner` runs the
`kConfigureTimeZone` stage during commissioning and the controller (Apple Home, Google Home, Home
Assistant, ...) writes its own known local timezone — derived from phone locale or hub server
tz, no GPS or UI needed on the device — into the cluster's `TimeZone`/`DSTOffset` attributes.
Without this feature bit (esp_matter's own `time_synchronization::create()` hardcodes
`FeatureMap=0`), commissioners skip that stage entirely and the device only ever has UTC.
`MatterDevice.localDate() -> Date?` reads back the result: it calls
`TimeSynchronizationServer::Instance().GetLocalTime()` (connectedhomeip's own UTC+TimeZone+DSTOffset
math, in CHIP-epoch microseconds since 2000-01-01), converts to Unix epoch seconds via
`chip::ChipEpochToUnixEpochMicros()`, and wraps the result in an `esp-swift-foundation` `Date`
(hence esp-swift-matter's dependency on esp-swift-foundation). Returns `nil` until both the clock is synced *and* a
controller has written a timezone — callers should keep a fallback path (e.g. a hardcoded TZ) for
use before that happens. The returned value is UTC-plus-offset baked into a fake epoch, not a
real Unix timestamp — format it with `gmtime_r` (not `localtime_r`), since no actual `TZ` env var
is involved.

`MatterDevice.retryTimeSync()` re-invokes the Time Synchronization cluster's time-fetch attempt.
connectedhomeip only calls its internal `AttemptToGetTime()` once, on boot (plus once more if a
controller resends `SetTrustedTimeSource`) — no built-in periodic retry. If that single attempt's
CASE session to the trusted time source times out (e.g. Thread mesh not yet settled right after a
reboot), the device stays unsynced until the next reboot. `retryTimeSync()` re-enters the same
attempt chain via `SetTrustedTimeSource(GetTrustedTimeSource())` (the real `AttemptToGetTime()` is
private). Must be called under `esp_matter::lock::ScopedChipStackLock` when invoked from the app's
own task rather than the CHIP event-loop thread — `matter_time_synchronization.cpp` does this
internally. Callers should poll it periodically (e.g. a status-logging loop) while `localDate()`
stays `nil`. See `matter-time-test/TIME-SYNC.md` for the full investigation that led to this fix.

**NTP fallback delegate and `setDefaultNTP`** — connectedhomeip's `DefaultTimeSyncDelegate` leaves
`UpdateTimeUsingNTPFallback` unimplemented (`return CHIP_ERROR_NOT_IMPLEMENTED`, unchanged as of
this writing even on upstream `main`), so the NTP branch of `AttemptToGetTime()` was previously a
dead stub. Two independent fixes were needed, both in `matter_time_synchronization.cpp`:

1. `SwiftTimeSyncDelegate : public DefaultTimeSyncDelegate` overrides only
   `UpdateTimeUsingNTPFallback` (everything else — `IsNTPAddressValid`, `IsNTPAddressDomain`,
   `UpdateTimeFromPlatformSource` — is inherited as-is). The override spawns a dedicated FreeRTOS
   task to run ESP-IDF's `esp_netif_sntp` client (`esp_netif_sntp_init` +
   `esp_netif_sntp_sync_wait`, blocking) against the delegate-supplied NTP hostname —
   deliberately off the CHIP event-loop thread, since `UpdateTimeUsingNTPFallback` runs on it (or
   under `ScopedChipStackLock` via `retryTimeSync()`), and a blocking SNTP call there would stall
   the whole Matter stack. The result is marshaled back via
   `chip::DeviceLayer::PlatformMgr().ScheduleWork()` so the completion callback fires on the CHIP
   thread as required. `esp_netif_sntp`'s own lwIP client calls `settimeofday()` internally on
   sync, so the delegate does not call `SetClock_RealTime()` itself — doing so would just be a
   redundant second `settimeofday()`. A `std::atomic<bool>` in-flight guard makes a re-entrant
   call (e.g. from `retryTimeSync()`'s 30s loop, while a prior query is still waiting on the SNTP
   timeout) return `CHIP_ERROR_BUSY` instead of racing a second task. `esp_netif_sntp` is a
   singleton — the task calls `esp_netif_sntp_deinit()` before finishing either way, since a
   second `esp_netif_sntp_init()` without a prior deinit errors.
2. `esp_matter_enable_time_synchronization()` now passes a non-null `config.delegate` pointing at
   a static `SwiftTimeSyncDelegate` instance — `esp_matter_cluster.cpp`'s
   `time_synchronization::create()` already wires any non-null `config->delegate` via
   `set_delegate_and_init_callback(cluster, TimeSynchronizationDelegateInitCB, config->delegate)`
   → `TimeSynchronization::SetDefaultDelegate()`, so no esp_matter patch/fork was needed.
3. **This alone is not sufficient.** connectedhomeip's `AttemptToGetFallbackNTPTimeFromDelegate()`
   calls `TimeSynchronizationServer::GetDefaultNtp()` *first* and bails out to
   `emitTimeFailureEvent` *before ever calling the delegate* if no `DefaultNTP` value has been
   stored — and no controller observed in practice (Apple Home, Home Assistant — see
   `matter-time-test/TIME-SYNC.md` §5/§6) ever sends the `SetDefaultNTP` command. So
   `MatterDevice.setDefaultNTP(_ host: String)` (wrapping the new C function
   `esp_matter_time_synchronization_set_default_ntp`) seeds a local `DefaultNTP` value directly via
   `TimeSynchronizationServer::Instance().SetDefaultNTP()` — but only if `GetDefaultNtp()` shows
   nothing is stored yet, so it doesn't fight a value a controller already wrote. Call it after
   `run()`, not before: `TimeSynchronizationServer`'s persistent-storage pointer (which
   `GetDefaultNtp()`/`SetDefaultNTP()` dereference through `TimeSyncDataProvider::Load`) is only
   set by `MatterTimeSynchronizationPluginServerInitCallback`, fired synchronously from
   `esp_matter_start()` inside `run()` — calling `setDefaultNTP()` any earlier null-derefs.
   Host must be IPv6-reachable (e.g. `"time.google.com"`, `"2.pool.ntp.org"`) since Thread is an
   IPv6-only transport and many `pool.ntp.org` entries are IPv4-only.

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

**Patched at configure time** — `project_include.cmake` applies `esp_matter.patch` to `managed_components/espressif__esp_matter/CMakeLists.txt` so esp_matter's PUBLIC compile options are gated by `$<$<COMPILE_LANGUAGE:C,CXX>:...>`. Without that gating, the Swift driver chokes on `-Wno-error=...` and `-std=gnu++17`. The patch is idempotent (sentinel `PATCH_APPLIED`) and inert when esp_matter hasn't been downloaded.

If you edit `esp_matter.patch` itself, run `idf.py reconfigure` afterwards — CMake doesn't watch the patch file as a configure dependency, so a plain `idf.py build` won't re-evaluate `project_include.cmake` and the patch won't be re-applied.

**Delegate-pattern clusters use a generic C++ trampoline + shared Swift box** — Some clusters (e.g. ValveConfigurationAndControl) handle commands via a `chip::app::Clusters::Foo::Delegate` virtual interface rather than through `esp_matter::attribute::update()`. For each such cluster, its `matter_*.cpp` file defines a `Swift*Delegate` class (inheriting the cluster's specific base) that stores a single `matter_command_cb_t` function pointer and dispatches from virtual methods using cluster-specific `ESP_MATTER_*_CMD_*` constants as `command_id`. On the Swift side, a single shared `CommandCallbacks` class (in `MatterDevice.swift`) holds a `(UInt16, UInt8) -> Void` dispatch closure — all delegate-pattern endpoints reuse it instead of per-cluster callback box classes. The `void *delegate` field in each cluster's `config_t` is how esp_matter forwards the pointer to `Cluster::SetDefaultDelegate()` during `esp_matter::start()`. Adding a new delegate-pattern cluster requires: (1) a new `Swift*Delegate` C++ class in that cluster's `.cpp` file, (2) a factory function + `ESP_MATTER_*_CMD_*` defines in its `.h` file, (3) a new `*Endpoint.swift` that reuses `CommandCallbacks`.

**ClosureControl cluster has a custom init CB** — `ClosureEndpoint` (`ClosureEndpoint.swift` / `matter_closure.cpp`) uses a more complex delegate pattern. The `ClusterLogic` / `Interface` / `MatterContext` trio must all be constructed AND `ClusterLogic::Init()` must be called — but esp_matter 1.5.0's `ClosureControlDelegateInitCB` omits the `Init()` call, causing an abort on the first Stop or MoveTo command. Fix: `esp_matter_endpoint_closure_create` passes `delegate=nullptr` to suppress the stock CB, then calls `esp_matter::cluster::set_delegate_and_init_callback(cluster, SwiftClosureControlDelegateInitCB, d)` to register our custom CB that correctly calls `ClusterLogic::Init(conformance, initParams)`. The conformance `FeatureMap` is built from the caller's `feature_flags` via `BitFlags::SetRaw()` — a single source of truth matching both the ZAP feature map and the conformance object. A global table `gClosureTable[4]` maps endpoint IDs to `SwiftClosureDelegate*` (populated at factory time, logging an error and dropping the endpoint if the table is already full) so `set_main_state` / `set_current_position` can reach `ClusterLogic` after start. `ClosureEndpoint` uses its own `ClosureCallbacks` box (in `ClosureEndpoint.swift`) rather than `CommandCallbacks`, because the MoveTo command carries three optional parameters that don't fit the generic `(UInt16, UInt8) -> Void` signature.

**Pulled via the IDF Component Manager** — `idf_component.yml` declares `espressif/esp_matter` 1.5.0 (pinned because it targets ESP-IDF 5.4 only — IDF 6.0 breaks several of esp_matter's transitive deps including `json`, `mbedtls/entropy.h`, and `esp-serial-flasher`). Any consumer (including `swift-esp/test-app`) that lists `SwiftMatter` in `REQUIRES` triggers the manager to fetch esp_matter and its tree into `managed_components/`. `MatterDevice.run()` only succeeds when the host project enables BT, OpenThread, custom partition table, MBEDTLS_HKDF_C, etc. The test-app intentionally does **not** call `MatterDevice.run()`; it lists `SwiftMatter` as a REQUIRES dependency only, to validate that the component links cleanly.
