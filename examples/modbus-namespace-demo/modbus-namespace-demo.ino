/**
 * @file modbus-namespace-demo.ino
 * @brief Demonstrates the ModbusDevice library with namespace usage
 * 
 * This example shows:
 * 1. How to use the modbus namespace
 * 2. Simple temperature sensor using modbus::SimpleModbusDevice
 * 3. Result<T,E> error handling pattern
 * 4. Clean initialization without IDeviceInstance
 */

#include <Arduino.h>
#include <esp32ModbusRTU.h>
#include "SimpleModbusDevice.h"
#include "QueuedModbusDevice.h"

// Use the modbus namespace
using namespace modbus;

// Global Modbus RTU instance
esp32ModbusRTU modbus(&Serial2, 16); // DE/RE pin on GPIO 16

/**
 * Example 1: Simple Temperature Sensor
 * Demonstrates basic sensor implementation
 */
class TemperatureSensor : public SimpleModbusDevice {
public:
    TemperatureSensor(uint8_t addr) : SimpleModbusDevice(addr) {}
    
    // Configure the sensor
    bool configure() override {
        // Set up channel for temperature reading
        configureChannels(1, 0x0000, 0.1f); // 1 channel at register 0x0000, scale 0.1
        return true;
    }
};

/**
 * Example 2: Multi-channel Analog Input Device
 * Demonstrates multiple channel support
 */
class AnalogInputDevice : public SimpleModbusDevice {
public:
    AnalogInputDevice(uint8_t addr) : SimpleModbusDevice(addr) {}
    
    bool configure() override {
        // Configure 4 analog input channels starting at register 0x1000
        configureChannels(4, 0x1000, 1.0f); // No scaling
        return true;
    }
    
    // Override to provide channel names
    const char* getChannelName(size_t channel) const override {
        static const char* names[] = {"AI1", "AI2", "AI3", "AI4"};
        return (channel < 4) ? names[channel] : "";
    }
    
    const char* getChannelUnits(size_t channel) const override {
        return "V"; // Voltage
    }
};

/**
 * Example 3: Async Device with Queue
 * Demonstrates QueuedModbusDevice for complex async operations
 */
class AsyncController : public QueuedModbusDevice {
private:
    std::vector<float> outputs;
    
public:
    AsyncController(uint8_t addr) : QueuedModbusDevice(addr) {
        outputs.resize(4, 0.0f);
    }
    
    // Perform initialization
    bool initialize() override {
        Serial.printf("Initializing async controller at address %d\n", getServerAddress());
        
        // Enable async mode with queue
        auto result = initializeQueue(10);
        if (!result.isOk()) {
            Serial.println("Failed to create queue");
            return false;
        }
        
        // Register the device
        setInitPhase(InitPhase::CONFIGURING);
        if (registerDevice() != ModbusError::SUCCESS) {
            Serial.println("Failed to register device");
            setInitPhase(InitPhase::ERROR);
            return false;
        }
        
        // Read device info asynchronously
        ModbusRequest req;
        req.functionCode = 0x03; // Read holding registers
        req.address = 0x0000;
        req.data = {0x00, 0x01}; // Read 1 register
        req.id = 1;
        
        if (!enqueueRequest(req).isOk()) {
            Serial.println("Failed to enqueue request");
            return false;
        }
        
        setInitPhase(InitPhase::READY);
        return true;
    }
    
    // Handle async responses
    void handleModbusResponse(uint8_t functionCode, uint16_t address,
                            const uint8_t* data, size_t length) override {
        Serial.printf("Received response: FC=%02X, Addr=%04X, Len=%d\n",
                     functionCode, address, length);
        
        // Process based on address
        if (address == 0x0000 && length >= 2) {
            uint16_t deviceId = (data[0] << 8) | data[1];
            Serial.printf("Device ID: 0x%04X\n", deviceId);
        }
    }
    
    void handleModbusError(ModbusError error) override {
        Serial.printf("Modbus error: %s\n", getModbusErrorString(error));
    }
};

// Example devices
TemperatureSensor* tempSensor = nullptr;
AnalogInputDevice* analogInputs = nullptr;
AsyncController* controller = nullptr;

// Callback functions for Modbus library
void mainHandleData(uint8_t serverAddress, esp32Modbus::FunctionCode fc,
                   uint16_t startingAddress, const uint8_t* data, size_t length) {
    // Forward to device handlers
    modbus::ModbusDevice* device = nullptr;
    
    {
        MutexGuard guard(deviceMapMutex);
        auto it = globalDeviceMap.find(serverAddress);
        if (it != globalDeviceMap.end()) {
            device = it->second;
        }
    }
    
    if (device) {
        device->handleData(serverAddress, fc, startingAddress, data, length);
    }
}

void handleError(uint8_t serverAddress, esp32Modbus::Error error) {
    // Forward to device error handlers
    modbus::ModbusDevice* device = nullptr;
    
    {
        MutexGuard guard(deviceMapMutex);
        auto it = globalDeviceMap.find(serverAddress);
        if (it != globalDeviceMap.end()) {
            device = it->second;
        }
    }
    
    if (device) {
        device->handleError(serverAddress, error);
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    
    Serial.println("ModbusDevice Namespace Demo");
    Serial.println("==========================");
    
    // Configure RS485
    Serial2.begin(9600, SERIAL_8N1, 17, 18); // RX=17, TX=18
    
    // Initialize Modbus
    modbus.onData(mainHandleData);
    modbus.onError(handleError);
    modbus.begin();
    
    // Set global Modbus instance
    setGlobalModbusRTU(&modbus);
    
    // Create devices
    tempSensor = new TemperatureSensor(1);
    analogInputs = new AnalogInputDevice(2);
    controller = new AsyncController(3);
    
    // Initialize devices
    if (!tempSensor->initialize()) {
        Serial.println("Failed to initialize temperature sensor");
    }
    
    if (!analogInputs->initialize()) {
        Serial.println("Failed to initialize analog inputs");
    }
    
    if (!controller->initialize()) {
        Serial.println("Failed to initialize controller");
    }
    
    Serial.println("Setup complete!");
}

void loop() {
    static uint32_t lastUpdate = 0;
    
    // Update every 5 seconds
    if (millis() - lastUpdate > 5000) {
        lastUpdate = millis();
        
        // Read temperature
        auto tempResult = tempSensor->readChannel(0);
        if (tempResult.isOk()) {
            Serial.printf("Temperature: %.1f°C\n", tempResult.value);
        } else {
            Serial.printf("Temperature read failed: %s\n", 
                         getModbusErrorString(tempResult.error));
        }
        
        // Read all analog inputs
        auto analogResult = analogInputs->readAllChannels();
        if (analogResult.isOk()) {
            Serial.print("Analog inputs: ");
            for (size_t i = 0; i < analogResult.value.size(); i++) {
                Serial.printf("%.2fV ", analogResult.value[i]);
            }
            Serial.println();
        }
        
        // Process async controller queue
        controller->processQueue();
    }
    
    // Let Modbus process
    modbus.task();
    
    delay(10);
}