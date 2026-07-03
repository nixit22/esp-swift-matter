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

import ESP_Matter
import Platform

private let log = Logger(tag: "Matter")

private final class ClosureCallbacks {
    let onMoveTo:    (_ endpointId: UInt16, _ position: ClosureEndpoint.TargetPosition?,
                      _ latch: Bool?, _ speed: UInt8?) -> Void
    let onStop:      (_ endpointId: UInt16) -> Void
    let onCalibrate: (_ endpointId: UInt16) -> Void
    let isReady:     (_ endpointId: UInt16) -> Bool

    init(
        onMoveTo:    @escaping (_ endpointId: UInt16, _ position: ClosureEndpoint.TargetPosition?,
                                _ latch: Bool?, _ speed: UInt8?) -> Void,
        onStop:      @escaping (_ endpointId: UInt16) -> Void,
        onCalibrate: @escaping (_ endpointId: UInt16) -> Void,
        isReady:     @escaping (_ endpointId: UInt16) -> Bool
    ) {
        self.onMoveTo    = onMoveTo
        self.onStop      = onStop
        self.onCalibrate = onCalibrate
        self.isReady     = isReady
    }
}

extension MatterDevice.Endpoint {
    /// Creates a ClosureControl endpoint (Matter 1.5, device type 0x0230).
    /// C delegate trampolines are wired here; callbacks fire from the CHIP event loop.
    /// Aborts if endpoint creation fails.
    init(
        featureFlags: UInt32,
        initialPosition: UInt8,
        onMoveTo: @escaping (_ endpointId: UInt16, _ position: ClosureEndpoint.TargetPosition?,
                              _ latch: Bool?, _ speed: UInt8?) -> Void,
        onStop:        @escaping (_ endpointId: UInt16) -> Void,
        onCalibrate:   @escaping (_ endpointId: UInt16) -> Void,
        isReadyToMove: @escaping (_ endpointId: UInt16) -> Bool
    ) {
        let cb  = ClosureCallbacks(onMoveTo: onMoveTo, onStop: onStop,
                                   onCalibrate: onCalibrate, isReady: isReadyToMove)
        let ptr = Unmanaged.passRetained(cb).toOpaque()

        guard let ep = esp_matter_endpoint_closure_create(
            featureFlags: featureFlags,
            initialPosition: initialPosition,
            moveToCallback: { eid, hasPos, pos, hasLatch, latch, hasSpeed, speed, p in
                guard let p else { return }
                let cb = Unmanaged<ClosureCallbacks>.fromOpaque(p).takeUnretainedValue()
                let position = hasPos ? ClosureEndpoint.TargetPosition(rawValue: pos) : nil
                let latchVal: Bool? = hasLatch ? latch : nil
                let speedVal: UInt8? = hasSpeed ? speed : nil
                cb.onMoveTo(eid, position, latchVal, speedVal)
            },
            stopCalibrateCallback: { eid, cmd, p in
                guard let p else { return }
                let cb = Unmanaged<ClosureCallbacks>.fromOpaque(p).takeUnretainedValue()
                if cmd == 0x00 { cb.onStop(eid) } else { cb.onCalibrate(eid) }
            },
            isReadyCallback: { eid, p in
                guard let p else { return true }
                return Unmanaged<ClosureCallbacks>.fromOpaque(p).takeUnretainedValue().isReady(eid)
            },
            privData: ptr)
        else {
            Unmanaged<ClosureCallbacks>.fromOpaque(ptr).release()
            log.e("Failed to create closure endpoint")
            abort()
        }

        self.init(id: esp_matter_endpoint_get_id(ep), willUpdateAttribute: nil, didUpdateAttribute: nil)
    }
}

/// Matter 1.5 closure endpoint (device type 0x0230, ClosureControl cluster).
///
/// Models an actuated closure: door, gate, window shutter, etc. The ClosureControl
/// cluster handles MoveTo / Stop / Calibrate commands from controllers. The firmware
/// actuates the hardware and calls ``setCurrentPosition(_:)`` and ``setMainState(_:)``
/// to report state back to Matter.
///
/// Note: `isReadyToMove` only gates entry into motion (polled in `WaitingForMotion`).
/// Auto-reversal on obstacle while moving is firmware-driven: the sensor task stops
/// the motor and calls `setMainState(.protected)` / `setCurrentPosition(.fullyOpen)`.
///
/// **Coop door example:**
/// ```swift
/// let matter = MatterDevice()
/// let door = ClosureEndpoint(matter,
///     featureFlags: ClosureEndpoint.features(positioning: true, protection: true),
///     initialPosition: .fullyClosed,
///     onMoveTo: { _, target, _, _ in
///         door.setMainState(.moving)
///         actuateDoor(toward: target)
///     },
///     onStop: { _ in stopMotor(); door.setMainState(.stopped) },
///     isReadyToMove: { _ in !obstacleSensor.isActive }
/// )
/// matter.run()
/// // After motor reaches target:
/// door.setCurrentPosition(.fullyOpen)
/// door.setMainState(.stopped)
/// ```
public struct ClosureEndpoint {
    private let endpoint: MatterDevice.Endpoint

    /// TargetPositionEnum raw values delivered to `onMoveTo`.
    public enum TargetPosition: UInt8 {
        /// Closure fully closed.
        case fullyClosed = 0
        /// Closure fully open.
        case fullyOpen   = 1
        /// Partial opening sized for pedestrian passage.
        case pedestrian  = 2
        /// Partial opening for ventilation.
        case ventilation = 3
        /// Manufacturer-defined position (e.g. a favourite preset).
        case signature   = 4
    }

    /// CurrentPositionEnum raw values for ``setCurrentPosition(_:)``.
    public enum CurrentPosition: UInt8 {
        /// Closure fully closed.
        case fullyClosed   = 0
        /// Closure fully open.
        case fullyOpen     = 1
        /// Closure open but neither fully open nor a named preset.
        case partiallyOpen = 2
        /// Partial opening sized for pedestrian passage.
        case pedestrian    = 3
        /// Partial opening for ventilation.
        case ventilation   = 4
        /// Manufacturer-defined position (e.g. a favourite preset).
        case signature     = 5
    }

    /// MainStateEnum raw values for ``setMainState(_:)``.
    public enum MainState: UInt8 {
        /// Motor idle; closure is at rest.
        case stopped          = 0
        /// Motor running; closure is in transit.
        case moving           = 1
        /// Waiting for `isReadyToMove` to return `true` before starting motion.
        case waitingForMotion = 2
        /// A hardware fault has been detected.
        case error            = 3
        /// Calibration sequence in progress.
        case calibrating      = 4
        /// Motion blocked by obstacle or protection sensor (named `protected_` to avoid the Swift keyword).
        case protected_       = 5
        /// Clutch disengaged; closure can be moved manually without motor.
        case disengaged       = 6
        /// Closure requires initial setup or calibration before normal use.
        case setupRequired    = 7
    }

    /// Builds a `featureFlags` bitmask from named ClosureControl feature options.
    ///
    /// At least one of `positioning` or `motionLatching` must be enabled —
    /// the Matter spec mandates one of these two features for a valid ClosureControl cluster.
    ///
    /// - Parameters:
    ///   - positioning: Enables `MoveTo` with `TargetPosition` values and exposes
    ///     `OverallCurrentState.position`.
    ///   - motionLatching: Enables latch/unlatch semantics on `MoveTo`.
    ///   - instantaneous: Closure opens/closes without a transit phase.
    ///   - speed: Enables the `speed` argument on `MoveTo`.
    ///   - ventilation: Enables the `.ventilation` target position.
    ///   - pedestrian: Enables the `.pedestrian` target position.
    ///   - calibration: Enables the `Calibrate` command and ``MainState/calibrating``.
    ///   - protection: Enables obstacle/protection detection and ``MainState/protected_``.
    ///   - manuallyOperable: Indicates the closure can be moved manually (e.g. with a handle).
    public static func features(
        positioning: Bool      = false,
        motionLatching: Bool   = false,
        instantaneous: Bool    = false,
        speed: Bool            = false,
        ventilation: Bool      = false,
        pedestrian: Bool       = false,
        calibration: Bool      = false,
        protection: Bool       = false,
        manuallyOperable: Bool = false
    ) -> UInt32 {
        var f: UInt32 = 0
        if positioning      { f |= UInt32(ESP_MATTER_CLOSURE_FEATURE_POSITIONING) }
        if motionLatching   { f |= UInt32(ESP_MATTER_CLOSURE_FEATURE_MOTION_LATCHING) }
        if instantaneous    { f |= UInt32(ESP_MATTER_CLOSURE_FEATURE_INSTANTANEOUS) }
        if speed            { f |= UInt32(ESP_MATTER_CLOSURE_FEATURE_SPEED) }
        if ventilation      { f |= UInt32(ESP_MATTER_CLOSURE_FEATURE_VENTILATION) }
        if pedestrian       { f |= UInt32(ESP_MATTER_CLOSURE_FEATURE_PEDESTRIAN) }
        if calibration      { f |= UInt32(ESP_MATTER_CLOSURE_FEATURE_CALIBRATION) }
        if protection       { f |= UInt32(ESP_MATTER_CLOSURE_FEATURE_PROTECTION) }
        if manuallyOperable { f |= UInt32(ESP_MATTER_CLOSURE_FEATURE_MANUALLY_OPERABLE) }
        return f
    }

    /// Creates a closure endpoint on `matter`.
    ///
    /// - Parameters:
    ///   - matter: Active ``MatterDevice`` node. Must be created before endpoints.
    ///   - featureFlags: OR of `ESP_MATTER_CLOSURE_FEATURE_*` bits or use ``features(...)``.
    ///                   Must include POSITIONING or MOTION_LATCHING.
    ///   - initialPosition: Position known at boot; `nil` if unknown. Providing a value
    ///                      lets MoveTo succeed immediately without a prior `setCurrentPosition` call.
    ///   - onMoveTo: Called from the CHIP event loop on MoveTo command.
    ///               `position` is the requested target (`nil` if omitted in the command).
    ///               `latch` is `true`/`false` to latch/unlatch, or `nil` if omitted.
    ///               `speed` is ThreeLevelAutoEnum raw value (0=Low 1=Medium 2=High 3=Auto), or `nil`.
    ///   - onStop: Called on Stop command. Default: no-op.
    ///   - onCalibrate: Called on Calibrate command. Default: no-op.
    ///   - isReadyToMove: Polled in `WaitingForMotion` state. Return `false` to block entry into
    ///                    motion (e.g. obstacle sensor active). Default: always ready.
    public init(
        _ matter: MatterDevice,
        featureFlags: UInt32 = UInt32(ESP_MATTER_CLOSURE_FEATURE_POSITIONING),
        initialPosition: CurrentPosition? = nil,
        onMoveTo: @escaping (_ endpointId: UInt16, _ position: TargetPosition?,
                              _ latch: Bool?, _ speed: UInt8?) -> Void = { _, _, _, _ in },
        onStop:      @escaping (_ endpointId: UInt16) -> Void = { _ in },
        onCalibrate: @escaping (_ endpointId: UInt16) -> Void = { _ in },
        isReadyToMove: @escaping (_ endpointId: UInt16) -> Bool = { _ in true }
    ) {
        endpoint = MatterDevice.Endpoint(
            featureFlags: featureFlags,
            initialPosition: initialPosition.map { $0.rawValue } ?? 0xFF,
            onMoveTo: onMoveTo,
            onStop: onStop,
            onCalibrate: onCalibrate,
            isReadyToMove: isReadyToMove
        )
        matter.register(endpoint: endpoint)
    }

    /// Updates ClosureControl MainState. Acquires the CHIP stack lock.
    ///
    /// Call after any hardware state change to keep the Matter attribute in sync.
    public func setMainState(_ state: MainState) {
        ESP_ERROR_CHECK(
            esp_matter_closure_set_main_state(endpointId: endpoint.id, mainState: state.rawValue))
    }

    /// Updates ClosureControl OverallCurrentState.position. Acquires the CHIP stack lock.
    ///
    /// Pass `nil` to report position as unknown. Must be called at least once before
    /// MoveTo succeeds if `initialPosition` was `nil` at init.
    public func setCurrentPosition(_ position: CurrentPosition?) {
        let hasPos: Bool = position != nil
        let pos: UInt8 = position.map { $0.rawValue } ?? 0
        ESP_ERROR_CHECK(
            esp_matter_closure_set_current_position(
                endpointId: endpoint.id, hasPosition: hasPos, position: pos))
    }
}
