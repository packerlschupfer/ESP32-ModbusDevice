/**
 * @file new-architecture-demo.ino
 * @brief Demonstrates the new ModbusDevice architecture with both simple and queued devices
 * 
 * This example shows:
 * 1. Simple temperature sensor using SimpleModbusDevice (no queuing)
 * 2. Complex multi-channel device using QueuedModbusDevice (like RYN4/MB8ART)
 * 3. Memory-efficient initialization without queue allocation
 * 4. Hybrid sync/async operation patterns
 */

#include <Arduino.h>
#include <esp32ModbusRTU.h>
#include "SimpleModbusDevice.h"
#include "QueuedModbusDevice.h"

// Example 1: Simple Temperature Sensor (synchronous only)
class TemperatureSensor : public SimpleModbusDevice {
private:
    float lastTemperature;
    
public:
    TemperatureSensor(uint8_t addr) : SimpleModbusDevice(addr), lastTemperature(0.0f) {}
    
    // Initialize using synchronous reads - no queue needed!
    DeviceResult<void> initialize() override {
        Serial.printf("Initializing temperature sensor at address %d\n", serverAddress);
        
        // Read device ID register synchronously
        auto deviceId = readRegisterSync<uint16_t>(0x0000, pdMS_TO_TICKS(100));
        if (!deviceId.isOk()) {
            Serial.println("Failed to read device ID");
            return DeviceResult<void>(DeviceError::COMMUNICATION_ERROR);
        }
        
        Serial.printf("Device ID: 0x%04X\n", deviceId.value);
        
        // Read configuration register
        auto config = readRegisterSync<uint16_t>(0x0001, pdMS_TO_TICKS(100));
        if (!config.isOk()) {
            Serial.println("Failed to read config");
            return DeviceResult<void>(DeviceError::COMMUNICATION_ERROR);
        }
        
        Serial.printf("Config: 0x%04X\n", config.value);
        
        // Mark as initialized
        signalInitializationComplete();
        return DeviceResult<void>(DeviceError::SUCCESS);
    }
    
    // Request temperature reading
    DeviceResult<void> requestData() override {
        // Read temperature register (assume 16-bit value, 0.1°C resolution)
        auto tempRaw = readRegisterSync<uint16_t>(0x0010, pdMS_TO_TICKS(200));
        if (tempRaw.isOk()) {
            lastTemperature = tempRaw.value / 10.0f;
            Serial.printf("Temperature: %.1f°C\n", lastTemperature);
            return DeviceResult<void>(DeviceError::SUCCESS);
        }
        return DeviceResult<void>(DeviceError::COMMUNICATION_ERROR);
    }
    
    // Get temperature data
    DataResult getData(DeviceDataType dataType) override {
        if (dataType == DeviceDataType::TEMPERATURE) {
            return DataResult{true, {lastTemperature}};
        }
        return DataResult{false, {}};
    }
};

// Example 2: Complex Multi-Channel Device (like RYN4/MB8ART)
class MultiChannelDevice : public QueuedModbusDevice {
private:
    uint8_t numChannels;
    std::vector<float> channelData;
    
public:
    MultiChannelDevice(uint8_t addr) 
        : QueuedModbusDevice(addr), numChannels(0) {}
    
    // Initialize using synchronous reads, then enable queuing
    DeviceResult<void> initialize() override {
        Serial.printf("Initializing multi-channel device at address %d\n", serverAddress);
        
        // Phase 1: Synchronous initialization (no queue yet!)
        
        // Read device info
        auto deviceInfo = readRegisterSync<uint32_t>(0x0000, pdMS_TO_TICKS(200));
        if (!deviceInfo.isOk()) {
            Serial.println("Failed to read device info");
            return DeviceResult<void>(DeviceError::COMMUNICATION_ERROR);
        }
        
        // Extract number of channels from device info
        numChannels = (deviceInfo.value >> 16) & 0xFF;
        Serial.printf("Device has %d channels\n", numChannels);
        
        // Allocate channel data storage
        channelData.resize(numChannels, 0.0f);
        
        // Read calibration data for each channel (still synchronous)
        for (uint8_t ch = 0; ch < numChannels; ch++) {
            auto calData = readRegisterSync<uint16_t>(0x0100 + ch, pdMS_TO_TICKS(100));
            if (!calData.isOk()) {
                Serial.printf("Failed to read cal data for channel %d\n", ch);
                return DeviceResult<void>(DeviceError::COMMUNICATION_ERROR);
            }
            Serial.printf("Channel %d calibration: 0x%04X\n", ch, calData.value);
        }
        
        // Phase 2: Enable queued mode for normal operation
        if (!enableQueuedMode(15)) {  // Queue depth of 15 for burst data
            Serial.println("Failed to enable queued mode");
            return DeviceResult<void>(DeviceError::RESOURCE_ERROR);
        }
        
        Serial.println("Multi-channel device initialized with queue");
        signalInitializationComplete();
        return DeviceResult<void>(DeviceError::SUCCESS);
    }
    
    // Request data from all channels (async)
    DeviceResult<void> requestData() override {
        // In real implementation, this would send Modbus requests
        // For demo, we'll simulate channel data updates
        
        // Check if queue is ready (MB8ART compatibility)
        if (!isQueueInitialized()) {
            Serial.println("Queue not initialized!");
            return DeviceResult<void>(DeviceError::NOT_INITIALIZED);
        }
        
        Serial.printf("Requesting data from %d channels\n", numChannels);
        
        // Simulate sending requests for all channels
        // Responses will be queued automatically
        
        return DeviceResult<void>(DeviceError::SUCCESS);
    }
    
    // Process queued responses
    void processModbusPacket(const ModbusPacket& packet) override {
        // Extract channel number from address
        if (packet.address >= 0x1000 && packet.address < 0x1000 + numChannels) {
            uint8_t channel = packet.address - 0x1000;
            if (packet.length >= 2) {
                uint16_t rawValue = (packet.data[0] << 8) | packet.data[1];
                channelData[channel] = rawValue / 100.0f;  // Assume 0.01 resolution
                Serial.printf("Channel %d: %.2f\n", channel, channelData[channel]);
            }
        }
    }
    
    // Get all channel data
    DataResult getData(DeviceDataType dataType) override {
        // Return all channel data regardless of type for this demo
        return DataResult{true, channelData};
    }
    
    // MB8ART compatibility check
    void checkQueueStatus() {
        Serial.printf("Queue initialized: %s, Queue depth: %d\n",
                     isQueueInitialized() ? "Yes" : "No",
                     getQueueDepth());
    }
};

// Global instances
esp32ModbusRTU modbus;
TemperatureSensor* tempSensor = nullptr;
MultiChannelDevice* multiDevice = nullptr;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== ModbusDevice New Architecture Demo ===\n");
    
    // Initialize Modbus RTU
    Serial2.begin(9600, SERIAL_8N1, 16, 17);  // RX=16, TX=17
    modbus.begin(&Serial2, 4);  // DE/RE pin = 4
    
    // IMPORTANT: Set the global ModbusRTU instance
    setGlobalModbusRTU(&modbus);
    
    // Set up Modbus callbacks to route responses
    modbus.onData(mainHandleData);
    modbus.onError(handleError);
    
    // Create devices
    tempSensor = new TemperatureSensor(0x01);
    multiDevice = new MultiChannelDevice(0x02);
    
    // Register devices (this allows mainHandleData to route responses)
    registerModbusDevice(0x01, tempSensor);
    registerModbusDevice(0x02, multiDevice);
    
    // Initialize devices
    Serial.println("\n--- Initializing Temperature Sensor ---");
    auto result1 = tempSensor->initialize();
    if (result1.isOk()) {
        Serial.println("Temperature sensor initialized successfully");
    } else {
        Serial.println("Temperature sensor initialization failed");
    }
    
    Serial.println("\n--- Initializing Multi-Channel Device ---");
    auto result2 = multiDevice->initialize();
    if (result2.isOk()) {
        Serial.println("Multi-channel device initialized successfully");
        multiDevice->checkQueueStatus();
    } else {
        Serial.println("Multi-channel device initialization failed");
    }
    
    Serial.println("\n--- Memory Usage Summary ---");
    Serial.printf("Simple device (temp sensor): ~%d bytes (no queue!)\n", 
                 sizeof(TemperatureSensor) + 100);  // Estimate
    Serial.printf("Queued device (multi-channel): ~%d bytes (with queue)\n",
                 sizeof(MultiChannelDevice) + 15 * sizeof(ModbusPacket) + 200);  // Estimate
    
    Serial.println("\nSetup complete. Starting main loop...\n");
}

void loop() {
    static uint32_t lastRead = 0;
    static uint32_t lastProcess = 0;
    
    // Read temperature every 5 seconds
    if (millis() - lastRead > 5000) {
        lastRead = millis();
        
        Serial.println("\n--- Reading Temperature ---");
        if (tempSensor->requestData().isOk()) {
            auto data = tempSensor->getData(DeviceDataType::TEMPERATURE);
            if (data.success && !data.values.empty()) {
                Serial.printf("Current temperature: %.1f°C\n", data.values[0]);
            }
        }
        
        Serial.println("\n--- Reading Multi-Channel Data ---");
        multiDevice->requestData();
    }
    
    // Process queued data every 100ms
    if (millis() - lastProcess > 100) {
        lastProcess = millis();
        
        // Process any queued packets
        multiDevice->processData();
    }
    
    // Let Modbus library process
    modbus.task();
    delay(10);
}