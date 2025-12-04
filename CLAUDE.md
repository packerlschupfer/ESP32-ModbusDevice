# ModbusDevice - CLAUDE.md

## Overview
Base class for Modbus RTU devices providing core communication functionality. Used by RYN4 (relays) and MB8ART (temperature sensors).

## Key Features
- Modbus RTU master communication
- Result type using `common::Result<T, ModbusError>`
- Initialization phases (UNINITIALIZED → CONFIGURING → READY)
- Response callback system via ModbusRegistry
- Thread-safe operations

## Architecture

### Class Hierarchy
```
IModbusDevice (interface)
    ↓
ModbusDevice (base implementation)
    ↓
QueuedModbusDevice (async queue support)
    ↓
RYN4 / MB8ART (concrete devices)
```

### ModbusRegistry
Global registry for device callbacks (replaces old globalDeviceMap):
```cpp
ModbusRegistry::getInstance().registerDevice(address, device);
ModbusRegistry::getInstance().unregisterDevice(address);
```

## Usage
```cpp
class MyDevice : public ModbusDevice {
public:
    MyDevice(uint8_t addr) : ModbusDevice(addr) {}

    void readSensor() {
        auto result = readHoldingRegisters(0x00, 2);
        if (result.isOk()) {
            auto values = result.value();
            // Process values
        }
    }
};
```

## Modbus Functions
- `readHoldingRegisters(address, count)`
- `readInputRegisters(address, count)`
- `writeSingleRegister(address, value)`
- `writeMultipleRegisters(address, values)`
- `readCoils(address, count)`
- `writeSingleCoil(address, value)`

## Error Codes
```cpp
enum class ModbusError {
    SUCCESS,
    TIMEOUT,
    CRC_ERROR,
    INVALID_RESPONSE,
    NOT_INITIALIZED,
    // ... more
};
```

## Thread Safety
- Uses MutexGuard for critical sections
- Response callbacks via ModbusRegistry are thread-safe
- Atomic init phase tracking

## Build Configuration
```ini
build_flags =
    -DMODBUSDEVICE_DEBUG  ; Enable debug logging
```
