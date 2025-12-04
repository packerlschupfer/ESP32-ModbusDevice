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
    -DMODBUS_BAUD_RATE=9600  ; Baud rate (inter-frame delay auto-calculated)
```

## Timing Configuration
The library enforces Modbus RTU inter-frame delay (3.5 character times) when releasing the bus mutex. The delay is calculated automatically from the baud rate.

**Formula:** `(38500 / baud_rate) + 1` (in ms, +1ms safety margin)

| Baud Rate | Calculated Delay |
|-----------|------------------|
| 9600      | 5 ms (default)   |
| 19200     | 3 ms             |
| 38400     | 2 ms             |
| 115200    | 1 ms             |

Configure in your project's `platformio.ini`:
```ini
build_flags = -DMODBUS_BAUD_RATE=9600  ; Set your baud rate (delay calculated automatically)
```

Or override the delay directly:
```ini
build_flags = -DMODBUS_INTER_FRAME_DELAY_MS=5  ; Manual override
```
