/**
 * @file main.cpp
 * @brief Example showing event group synchronization with ModbusDevice
 * 
 * This example demonstrates how to use FreeRTOS event groups to handle
 * device initialization without polling.
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

// Event group for device initialization
EventGroupHandle_t deviceReadyEventGroup;
#define MB8ART_READY_BIT  (1 << 0)
#define RYN4_READY_BIT    (1 << 1)
#define MITHERM_READY_BIT (1 << 2)
#define ALL_DEVICES_READY (MB8ART_READY_BIT | RYN4_READY_BIT | MITHERM_READY_BIT)

// Error bits (optional)
#define MB8ART_ERROR_BIT  (1 << 16)
#define RYN4_ERROR_BIT    (1 << 17)
#define MITHERM_ERROR_BIT (1 << 18)

/**
 * @class MB8ART
 * @brief Temperature sensor using event group notification
 */
class MB8ART : public SimpleModbusDevice {
public:
    MB8ART(uint8_t addr) : SimpleModbusDevice(addr) {}
    
protected:
    bool configure() override {
        MODBUSD_LOG_I("Configuring MB8ART temperature sensor");
        
        // Read device info
        auto info = readHoldingRegisters(0x0000, 1);
        if (!info.isOk()) {
            MODBUSD_LOG_E("Failed to read device info");
            return false;
        }
        
        // Setup 8 temperature channels
        for (int i = 0; i < 8; i++) {
            char name[32];
            snprintf(name, sizeof(name), "Temperature %d", i + 1);
            addChannel(name, "°C", 0x0010 + i);
            setChannelRange(i, -50.0f, 150.0f);
        }
        
        return true;
    }
    
    float getScaleFactor(size_t channel) const override {
        return 0.1f;  // 0.1°C resolution
    }
    
    bool readChannelData() override {
        // Read all 8 channels at once
        auto result = readHoldingRegisters(0x0010, 8);
        if (!result.isOk()) {
            return false;
        }
        
        const auto& data = result.value;
        for (size_t i = 0; i < 8 && i < data.size(); i++) {
            values[i] = static_cast<int32_t>(data[i]);
        }
        
        return true;
    }
};

/**
 * @class RYN4
 * @brief Relay controller using event group notification
 */
class RYN4 : public SimpleModbusDevice {
public:
    RYN4(uint8_t addr) : SimpleModbusDevice(addr) {}
    
protected:
    bool configure() override {
        MODBUSD_LOG_I("Configuring RYN4 relay controller");
        
        // Configure device
        auto result = writeSingleRegister(0x0001, 0x0100);  // Enable all relays
        if (!result.isOk()) {
            MODBUSD_LOG_E("Failed to configure RYN4");
            return false;
        }
        
        // Add relay status channel
        addChannel("Relay Status", "bits", 0x0010);
        
        return true;
    }
    
    float getScaleFactor(size_t channel) const override {
        return 1.0f;
    }
};

/**
 * @class MiThermSensor
 * @brief Additional sensor showing how easy it is to add more devices
 */
class MiThermSensor : public SimpleModbusDevice {
public:
    MiThermSensor(uint8_t addr) : SimpleModbusDevice(addr) {}
    
protected:
    bool configure() override {
        MODBUSD_LOG_I("Configuring MiTherm sensor");
        
        // Setup channels
        addChannel("Temperature", "°C", 0x0001);
        addChannel("Humidity", "%", 0x0002);
        setChannelRange(0, -40.0f, 85.0f);
        setChannelRange(1, 0.0f, 100.0f);
        
        return true;
    }
    
    float getScaleFactor(size_t channel) const override {
        return channel == 0 ? 0.01f : 0.1f;  // Different scales for temp/humidity
    }
};

// Device instances
MB8ART* mb8art = nullptr;
RYN4* ryn4 = nullptr;
MiThermSensor* mitherm = nullptr;

// System task that depends on devices being ready
void systemControlTask(void* pvParameters) {
    MODBUSD_LOG_I("System control task waiting for devices...");
    
    // Wait for all devices to be ready
    EventBits_t bits = xEventGroupWaitBits(
        deviceReadyEventGroup,
        ALL_DEVICES_READY,
        pdFALSE,  // Don't clear bits
        pdTRUE,   // Wait for all bits
        portMAX_DELAY
    );
    
    if (bits & ALL_DEVICES_READY) {
        MODBUSD_LOG_I("All devices ready! Starting system control.");
    }
    
    // Main control loop
    while (true) {
        // Read temperature from MB8ART
        if (mb8art) {
            auto result = mb8art->update();
            if (result.isOk()) {
                auto temp = mb8art->getFloat(0);  // Channel 0
                if (temp.isOk() && temp.value > 25.0f) {
                    // Turn on relay if temperature > 25°C
                    if (ryn4) {
                        ryn4->writeSingleRegister(0x0010, 0x0001);
                    }
                }
            }
        }
        
        // Read MiTherm data
        if (mitherm) {
            auto result = mitherm->update();
            if (result.isOk()) {
                auto temp = mitherm->getFloat(0);
                auto humidity = mitherm->getFloat(1);
                if (temp.isOk() && humidity.isOk()) {
                    MODBUSD_LOG_I("MiTherm: %.2f°C, %.1f%%", temp.value, humidity.value);
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    }
    
    MODBUSD_LOG_I("ModbusDevice Event Group Example Starting...");
    
    // Create event group
    deviceReadyEventGroup = xEventGroupCreate();
    if (!deviceReadyEventGroup) {
        MODBUSD_LOG_E("Failed to create event group!");
        return;
    }
    
    // Configure UART for Modbus
    Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
    modbus.setTimeout(1000);
    modbus.begin();
    
    // Set global Modbus instance
    setGlobalModbusRTU(&modbus);
    
    // Register callbacks
    modbus.onData(mainHandleData);
    modbus.onError(handleError);
    
    // Create MB8ART and configure event group
    mb8art = new MB8ART(0x01);
    mb8art->setEventGroup(deviceReadyEventGroup, MB8ART_READY_BIT, MB8ART_ERROR_BIT);
    
    // Create RYN4 and configure event group
    ryn4 = new RYN4(0x02);
    ryn4->setEventGroup(deviceReadyEventGroup, RYN4_READY_BIT, RYN4_ERROR_BIT);
    
    // Create MiTherm and configure event group
    mitherm = new MiThermSensor(0x03);
    mitherm->setEventGroup(deviceReadyEventGroup, MITHERM_READY_BIT, MITHERM_ERROR_BIT);
    
    // Start device initialization
    mb8art->initialize();
    ryn4->initialize();
    mitherm->initialize();
    
    // Create system control task
    xTaskCreate(systemControlTask, "SystemControl", 4096, NULL, 5, NULL);
    
    MODBUSD_LOG_I("Setup complete! Devices initializing asynchronously.");
}

void loop() {
    // Process Modbus
    modbus.task();
    
    // Show device states periodically
    static uint32_t lastStatus = 0;
    uint32_t now = millis();
    if (now - lastStatus >= 5000) {
        lastStatus = now;
        
        EventBits_t bits = xEventGroupGetBits(deviceReadyEventGroup);
        MODBUSD_LOG_I("Device status: MB8ART=%s, RYN4=%s, MiTherm=%s",
            (bits & MB8ART_READY_BIT) ? "READY" : 
            (bits & MB8ART_ERROR_BIT) ? "ERROR" : "INIT",
            (bits & RYN4_READY_BIT) ? "READY" : 
            (bits & RYN4_ERROR_BIT) ? "ERROR" : "INIT",
            (bits & MITHERM_READY_BIT) ? "READY" : 
            (bits & MITHERM_ERROR_BIT) ? "ERROR" : "INIT");
            
        // Check if any errors occurred
        if (bits & (MB8ART_ERROR_BIT | RYN4_ERROR_BIT | MITHERM_ERROR_BIT)) {
            MODBUSD_LOG_W("Some devices failed to initialize!");
        }
    }
    
    delay(10);
}