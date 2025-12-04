/**
 * @file main.cpp
 * @brief Example showing the redesigned ModbusDevice architecture
 * 
 * This example demonstrates the clean, simplified approach of the new
 * ModbusDevice design without circular dependencies or false warnings.
 */

#include <Arduino.h>
#include <esp32ModbusRTU.h>
#include "SimpleModbusDevice.h"
#include "ModbusDeviceLogging.h"

// UART pins for Modbus
#define RX_PIN 16
#define TX_PIN 17
#define RTS_PIN 4

// Global Modbus instance
esp32ModbusRTU modbus(&Serial2, RTS_PIN);

/**
 * @class TemperatureSensor
 * @brief Simple temperature sensor implementation
 * 
 * Reads temperature from register 0x0001 with 0.1°C resolution
 */
class TemperatureSensor : public SimpleModbusDevice {
public:
    TemperatureSensor(uint8_t addr) : SimpleModbusDevice(addr) {}
    
protected:
    bool configure() override {
        MODBUSD_LOG_I("Configuring temperature sensor");
        
        // Read device ID to verify communication
        auto id = readHoldingRegisters(0x0000, 1);
        if (!id.isOk()) {
            MODBUSD_LOG_E("Failed to read device ID");
            return false;
        }
        
        MODBUSD_LOG_I("Device ID: 0x%04X", id.value[0]);
        
        // Setup channel
        addChannel("Temperature", "°C", 0x0001);
        setChannelRange(0, -40.0f, 125.0f);  // Typical sensor range
        
        return true;
    }
    
    float getScaleFactor(size_t channel) const override {
        return 0.1f;  // Value is in tenths of degree
    }
};

/**
 * @class PressureSensor
 * @brief Multi-channel pressure sensor
 * 
 * Reads 4 pressure channels starting at register 0x0010
 */
class PressureSensor : public SimpleModbusDevice {
public:
    PressureSensor(uint8_t addr) : SimpleModbusDevice(addr) {}
    
protected:
    bool configure() override {
        MODBUSD_LOG_I("Configuring pressure sensor");
        
        // Read configuration register
        auto config = readHoldingRegisters(0x0000, 1);
        if (!config.isOk()) {
            MODBUSD_LOG_E("Failed to read configuration");
            return false;
        }
        
        // Setup 4 pressure channels
        for (int i = 0; i < 4; i++) {
            std::string name = "Pressure " + std::to_string(i + 1);
            addChannel(name, "bar", 0x0010 + i);
            setChannelRange(i, 0.0f, 10.0f);  // 0-10 bar range
        }
        
        return true;
    }
    
    float getScaleFactor(size_t channel) const override {
        return 0.01f;  // Value is in hundredths of bar
    }
    
    bool readChannelData() override {
        // Read all 4 channels in one request
        auto result = readHoldingRegisters(0x0010, 4);
        if (!result.isOk()) {
            MODBUSD_LOG_E("Failed to read pressure data");
            return false;
        }
        
        // Update all values
        const auto& data = result.value;
        for (size_t i = 0; i < 4 && i < data.size(); i++) {
            values[i] = static_cast<int32_t>(data[i]);
        }
        
        return true;
    }
};

// Device instances
TemperatureSensor* tempSensor = nullptr;
PressureSensor* pressureSensor = nullptr;

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    }
    
    MODBUSD_LOG_I("ModbusDevice Redesign Example Starting...");
    
    // Configure UART for Modbus
    Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
    modbus.setTimeout(1000);
    modbus.begin();
    
    // Set global Modbus instance
    setGlobalModbusRTU(&modbus);
    
    // Register callbacks
    modbus.onData(mainHandleData);
    modbus.onError(handleError);
    
    // Create and initialize temperature sensor
    tempSensor = new TemperatureSensor(0x01);
    if (!tempSensor->initialize()) {
        MODBUSD_LOG_E("Failed to initialize temperature sensor");
    }
    
    // Create and initialize pressure sensor
    pressureSensor = new PressureSensor(0x02);
    if (!pressureSensor->initialize()) {
        MODBUSD_LOG_E("Failed to initialize pressure sensor");
    }
    
    MODBUSD_LOG_I("Setup complete!");
}

void loop() {
    static uint32_t lastUpdate = 0;
    uint32_t now = millis();
    
    // Update sensors every 5 seconds
    if (now - lastUpdate >= 5000) {
        lastUpdate = now;
        
        // Update temperature sensor
        if (tempSensor && tempSensor->getInitPhase() == ModbusDevice::InitPhase::READY) {
            auto result = tempSensor->update();
            if (result.isOk()) {
                auto temp = tempSensor->getFloat(0);
                if (temp.isOk()) {
                    MODBUSD_LOG_I("Temperature: %.1f°C", temp.value);
                }
            } else {
                MODBUSD_LOG_E("Temperature update failed");
            }
        }
        
        // Update pressure sensor
        if (pressureSensor && pressureSensor->getInitPhase() == ModbusDevice::InitPhase::READY) {
            auto result = pressureSensor->update();
            if (result.isOk()) {
                MODBUSD_LOG_I("Pressure readings:");
                for (size_t i = 0; i < pressureSensor->getChannelCount(); i++) {
                    auto pressure = pressureSensor->getFloat(i);
                    if (pressure.isOk()) {
                        MODBUSD_LOG_I("  %s: %.2f %s", 
                            pressureSensor->getChannelName(i),
                            pressure.value,
                            pressureSensor->getChannelUnits(i));
                    }
                }
            } else {
                MODBUSD_LOG_E("Pressure update failed");
            }
        }
        
        // Show statistics
        if (tempSensor) {
            auto stats = tempSensor->getStatistics();
            MODBUSD_LOG_I("Temp sensor stats: %d/%d successful (%.1f%%)",
                stats.successfulRequests, stats.totalRequests,
                stats.totalRequests > 0 ? 
                    (100.0f * stats.successfulRequests / stats.totalRequests) : 0.0f);
        }
    }
    
    // Process Modbus
    modbus.task();
    
    // Small delay
    delay(10);
}