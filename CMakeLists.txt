##
## Copyright (c) 2026 Nicolas Christe
##
## Permission is hereby granted, free of charge, to any person obtaining a copy
## of this software and associated documentation files (the "Software"), to deal
## in the Software without restriction, including without limitation the rights
## to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
## copies of the Software, and to permit persons to whom the Software is
## furnished to do so, subject to the following conditions:
##
## The above copyright notice and this permission notice shall be included in all
## copies or substantial portions of the Software.
##
## THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
## IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
## FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
## AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
## LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
## OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
## SOFTWARE.
##

idf_component_register(
    SRCS
        src/MatterDevice/matter_core.cpp
        src/RootClusters/TimeSynchronization/matter_time_synchronization.cpp
        src/Endpoints/TemperatureSensorEndpoint/matter_temperature_sensor.cpp
        src/Endpoints/HumiditySensorEndpoint/matter_humidity_sensor.cpp
        src/Endpoints/PressureSensorEndpoint/matter_pressure_sensor.cpp
        src/Endpoints/WaterValveEndpoint/matter_water_valve.cpp
        src/Endpoints/ClosureEndpoint/matter_closure.cpp
        src/Endpoints/ModeSelectEndpoint/matter_mode_select.cpp
        src/Endpoints/PowerSourceEndpoint/matter_power_source.cpp
        src/MatterDevice/MatterDevice.swift
        src/FactoryData/MatterFactoryData.swift
        src/RootClusters/TimeSynchronization/TimeSynchronization.swift
        src/Endpoints/TemperatureSensorEndpoint/TemperatureSensorEndpoint.swift
        src/Endpoints/HumiditySensorEndpoint/HumiditySensorEndpoint.swift
        src/Endpoints/PressureSensorEndpoint/PressureSensorEndpoint.swift
        src/Endpoints/WaterValveEndpoint/WaterValveEndpoint.swift
        src/Endpoints/ClosureEndpoint/ClosureEndpoint.swift
        src/Endpoints/ModeSelectEndpoint/ModeSelectEndpoint.swift
        src/Endpoints/PowerSourceEndpoint/PowerSourceEndpoint.swift
    PRIV_INCLUDE_DIRS
        src/MatterDevice
        src/RootClusters/TimeSynchronization
        src/Endpoints/TemperatureSensorEndpoint
        src/Endpoints/HumiditySensorEndpoint
        src/Endpoints/PressureSensorEndpoint
        src/Endpoints/WaterValveEndpoint
        src/Endpoints/ClosureEndpoint
        src/Endpoints/ModeSelectEndpoint
        src/Endpoints/PowerSourceEndpoint
    REQUIRES esp_matter esp-swift-platform esp-swift-nvs
    PRIV_REQUIRES esp-swift-support
)

swift_configure(
    MODULE_NAME Matter
)
