#include "SimpleModbusDevice.h"

namespace modbus {

// Constructor
SimpleModbusDevice::SimpleModbusDevice(uint8_t serverAddr)
    : ModbusDevice(serverAddr) {
}

// Initialize
bool SimpleModbusDevice::initialize() {
    MODBUSD_LOG_I("Initializing SimpleModbusDevice at address %d", getServerAddress());
    
    // Set configuration phase
    setInitPhase(InitPhase::CONFIGURING);
    
    // Register device for callbacks
    if (registerDevice() != ModbusError::SUCCESS) {
        MODBUSD_LOG_E("Failed to register device");
        setInitPhase(InitPhase::ERROR);
        return false;
    }
    
    // Call derived class configuration
    if (!configure()) {
        MODBUSD_LOG_E("Device configuration failed");
        setInitPhase(InitPhase::ERROR);
        return false;
    }
    
    // Resize value storage
    values.resize(channels.size(), 0);
    
    // Mark as ready
    setInitPhase(InitPhase::READY);
    MODBUSD_LOG_I("Device initialized successfully with %d channels", channels.size());
    
    return true;
}

// Update data
ModbusResult<void> SimpleModbusDevice::update() {
    if (getInitPhase() != InitPhase::READY) {
        return ModbusResult<void>::error(ModbusError::NOT_INITIALIZED);
    }
    
    // Read all channel data
    if (!readChannelData()) {
        return ModbusResult<void>::error(getLastError());
    }
    
    // Update timestamp
    lastUpdateTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    return ModbusResult<void>::ok();
}

// Get data age
uint32_t SimpleModbusDevice::getDataAge() const {
    if (lastUpdateTime == 0) return UINT32_MAX;
    
    uint32_t currentTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (currentTime >= lastUpdateTime) {
        return currentTime - lastUpdateTime;
    }
    
    // Handle tick wraparound
    return (UINT32_MAX - lastUpdateTime) + currentTime;
}

// Get channel name
const char* SimpleModbusDevice::getChannelName(size_t channel) const {
    if (channel >= channels.size()) return "";
    return channels[channel].name.c_str();
}

// Get channel units
const char* SimpleModbusDevice::getChannelUnits(size_t channel) const {
    if (channel >= channels.size()) return "";
    return channels[channel].units.c_str();
}

// Get float value
ModbusResult<float> SimpleModbusDevice::getFloat(size_t channel) const {
    auto raw = getRawValue(channel);
    if (!raw.isOk()) {
        return ModbusResult<float>::error(raw.error());
    }
    
    float value = static_cast<float>(raw.value()) * getScaleFactor(channel);
    
    // Validate range if defined
    float min, max;
    if (getRange(channel, min, max)) {
        if (value < min || value > max) {
            MODBUSD_LOG_W("Channel %d value %.2f out of range [%.2f, %.2f]", 
                         channel, value, min, max);
        }
    }
    
    return ModbusResult<float>::ok(value);
}

// Get raw value
ModbusResult<int32_t> SimpleModbusDevice::getRawValue(size_t channel) const {
    if (channel >= values.size()) {
        return ModbusResult<int32_t>::error(ModbusError::INVALID_PARAMETER);
    }
    
    if (!hasValidData()) {
        return ModbusResult<int32_t>::error(ModbusError::NOT_INITIALIZED);
    }
    
    return ModbusResult<int32_t>::ok(values[channel]);
}

// Get range
bool SimpleModbusDevice::getRange(size_t channel, float& min, float& max) const {
    if (channel >= channels.size()) return false;
    
    min = channels[channel].minValue;
    max = channels[channel].maxValue;
    
    // Check if range is actually defined
    return (min > -std::numeric_limits<float>::max() || 
            max < std::numeric_limits<float>::max());
}

// Add channel
void SimpleModbusDevice::addChannel(const std::string& name, const std::string& units, uint16_t address) {
    ChannelInfo info;
    info.name = name;
    info.units = units;
    info.address = address;
    channels.push_back(info);
}

// Set channel range
void SimpleModbusDevice::setChannelRange(size_t channel, float min, float max) {
    if (channel < channels.size()) {
        channels[channel].minValue = min;
        channels[channel].maxValue = max;
    }
}

// Read channel data (default implementation)
bool SimpleModbusDevice::readChannelData() {
    // Default implementation reads each channel individually
    for (size_t i = 0; i < channels.size(); i++) {
        auto result = readHoldingRegisters(channels[i].address, 1);
        if (!result.isOk()) {
            MODBUSD_LOG_E("Failed to read channel %d at address 0x%04X", i, channels[i].address);
            return false;
        }
        
        if (!result.value().empty()) {
            values[i] = static_cast<int32_t>(result.value()[0]);
        }
    }
    
    return true;
}

} // namespace modbus