# ModbusDevice Library v2.0.0

A redesigned ESP32 Modbus RTU device library with clean initialization, focused interfaces, and no circular dependencies.

**Compatible with esp32ModbusRTU v0.1.0 and later**

## Features

- **Clean Architecture**: No circular dependencies or initialization workarounds
- **Focused Interfaces**: Separate interfaces for sensors (IModbusInput) and actuators (IModbusOutput)
- **Initialization Phases**: Clear state tracking (UNINITIALIZED → CONFIGURING → READY)
- **No False Warnings**: Responses during configuration are expected and handled properly
- **Minimal Resource Usage**: Resources allocated only when needed
- **Type-Safe API**: Modern C++ Result<T, E> pattern for error handling
- **Thread-Safe Operations**: Built-in thread safety with minimal overhead
- **Flexible Device Types**: SimpleModbusDevice for sensors, QueuedModbusDevice for async operation

## Installation

Add this library to your `platformio.ini`:

```ini
lib_deps = 
    ModbusDevice
    MutexGuard
    esp32ModbusRTU @ ^0.1.0
```

Or if using local development:
```ini
lib_deps = 
    file://../workspace_Class-ModbusDevice
    file://../workspace_Class-MutexGuard
```

## Basic Usage

### Creating a Simple Sensor

```cpp
#include "SimpleModbusDevice.h"

class TemperatureSensor : public SimpleModbusDevice {
public:
    TemperatureSensor(uint8_t addr) : SimpleModbusDevice(addr) {}
    
protected:
    bool configure() override {
        // Read device configuration during CONFIGURING phase
        auto id = readHoldingRegisters(0x0000, 1);
        if (!id.isOk()) return false;
        
        // Setup channel
        addChannel("Temperature", "°C", 0x0001);
        setChannelRange(0, -40.0f, 125.0f);
        return true;
    }
    
    float getScaleFactor(size_t channel) const override {
        return 0.1f;  // Convert from tenths
    }
};
```

### Using the Device

```cpp
// Create and initialize
TemperatureSensor sensor(0x01);
if (!sensor.initialize()) {
    // Handle initialization error
}

// Read data
auto result = sensor.update();
if (result.isOk()) {
    auto temp = sensor.getFloat(0);
    if (temp.isOk()) {
        printf("Temperature: %.1f°C\n", temp.value());
    }
}
```

### Creating an Async Device

```cpp
class ComplexSensor : public QueuedModbusDevice {
public:
    ComplexSensor(uint8_t addr) : QueuedModbusDevice(addr) {}
    
    bool initialize() {
        // Configure during CONFIGURING phase
        setInitPhase(InitPhase::CONFIGURING);
        registerDevice();
        
        auto config = readHoldingRegisters(0x1000, 1);
        if (!config.isOk()) return false;
        
        // Enable async mode after configuration
        setInitPhase(InitPhase::READY);
        return enableAsync(20);
    }
    
protected:
    void onAsyncResponse(uint8_t fc, uint16_t addr, 
                        const uint8_t* data, size_t len) override {
        // Handle async responses
    }
};
```

## Key Interfaces

### IModbusDevice
Core Modbus communication interface with methods for reading/writing registers and coils.

### IModbusInput
Interface for sensor devices that read data from Modbus servers.

### IModbusOutput  
Interface for actuator devices that write data to Modbus servers.

### Base Classes
- **ModbusDevice**: Core functionality with initialization phases
- **SimpleModbusDevice**: For simple sensors with channel-based data
- **QueuedModbusDevice**: For devices needing asynchronous operation

## Error Handling

The library uses a type-safe Result<T, E> pattern:

```cpp
// All operations return ModbusResult
auto result = device.readHoldingRegisters(0x0000, 10);
if (result.isOk()) {
    auto values = result.value();
    // Process values
} else {
    ModbusError error = result.error();
    // Handle error
}
```

## Initialization Phases

Devices track their initialization state:

```cpp
enum class InitPhase {
    UNINITIALIZED,    // Just constructed
    CONFIGURING,      // Reading configuration (responses expected)
    READY,           // Normal operation
    ERROR            // Initialization failed
};
```

## Logging Configuration

This library supports flexible logging configuration:

### Using ESP-IDF Logging (Default)
No configuration needed. The library will use ESP-IDF logging.

### Using Custom Logger
Define `USE_CUSTOM_LOGGER` in your build flags:
```ini
build_flags = -DUSE_CUSTOM_LOGGER
```

### Debug Logging
To enable debug/verbose logging for this library:
```ini
build_flags = -DMODBUSDEVICE_DEBUG
```

### Advanced Debug Options
For detailed protocol debugging:
```ini
build_flags = 
    -DMODBUSDEVICE_DEBUG           # Enable general debug logging
    -DMODBUSDEVICE_DEBUG_PROTOCOL  # Protocol-level debugging
    -DMODBUSDEVICE_DEBUG_TIMING    # Performance timing
    -DMODBUSDEVICE_DEBUG_BUFFER    # Buffer dumps
```

### Complete Example
```ini
[env:debug]
build_flags = 
    -DUSE_CUSTOM_LOGGER      # Use custom logger
    -DMODBUSDEVICE_DEBUG     # Enable debug for this library
```

For more logging details, see [LOGGING.md](LOGGING.md).

## Complete Example

```cpp
#include <Arduino.h>
#include <esp32ModbusRTU.h>
#include "SimpleModbusDevice.h"

// Multi-channel pressure sensor
class PressureSensor : public SimpleModbusDevice {
public:
    PressureSensor(uint8_t addr) : SimpleModbusDevice(addr) {}
    
protected:
    bool configure() override {
        // Read config during CONFIGURING phase - no warnings!
        auto config = readHoldingRegisters(0x0000, 1);
        if (!config.isOk()) return false;
        
        // Setup 4 pressure channels
        for (int i = 0; i < 4; i++) {
            addChannel("Pressure " + std::to_string(i+1), "bar", 0x0010 + i);
            setChannelRange(i, 0.0f, 10.0f);
        }
        return true;
    }
    
    float getScaleFactor(size_t channel) const override {
        return 0.01f;  // Hundredths of bar
    }
    
    bool readChannelData() override {
        // Read all 4 channels in one request
        auto result = readHoldingRegisters(0x0010, 4);
        if (!result.isOk()) return false;
        
        const auto& data = result.value();
        for (size_t i = 0; i < 4 && i < data.size(); i++) {
            values[i] = data[i];
        }
        return true;
    }
};

// Setup
esp32ModbusRTU modbus(&Serial2, RTS_PIN);
PressureSensor* sensor;

void setup() {
    // Setup Modbus
    Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
    modbus.begin();
    setGlobalModbusRTU(&modbus);
    modbus.onData(mainHandleData);
    modbus.onError(handleError);
    
    // Create and initialize sensor
    sensor = new PressureSensor(0x01);
    if (!sensor->initialize()) {
        Serial.println("Failed to initialize sensor");
    }
}

void loop() {
    // Update sensor
    if (sensor->update().isOk()) {
        for (size_t i = 0; i < sensor->getChannelCount(); i++) {
            auto pressure = sensor->getFloat(i);
            if (pressure.isOk()) {
                Serial.printf("%s: %.2f %s\n",
                    sensor->getChannelName(i),
                    pressure.value(),
                    sensor->getChannelUnits(i));
            }
        }
    }
    
    modbus.task();
    delay(1000);
}
```

## Best Practices

1. **Use appropriate base class**: SimpleModbusDevice for sensors, QueuedModbusDevice for async
2. **Configure during CONFIGURING phase**: Read device config without warnings
3. **Check all Results**: Use isOk() before accessing values
4. **Define channels clearly**: Name, units, and address for each data point
5. **Handle errors gracefully**: All operations can fail
6. **Enable debug logging** during development

## Key Improvements in v2.0.0

- ✅ No more circular dependencies
- ✅ No more false initialization warnings  
- ✅ No more signalInitializationComplete() workarounds
- ✅ Clean, intuitive API
- ✅ Focused interfaces for different device types
- ✅ Minimal resource usage
- ✅ Type-safe error handling

## License

See the LICENSE file for details.