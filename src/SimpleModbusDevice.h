#ifndef SIMPLEMODBUSDEVICE_H
#define SIMPLEMODBUSDEVICE_H

#include "ModbusDevice.h"
#include "IModbusInput.h"
#include <map>
#include <string>

namespace modbus {

/**
 * @class SimpleModbusDevice
 * @brief Simple sensor device that reads data periodically
 * 
 * This class provides a clean implementation for simple Modbus sensors
 * without unnecessary complexity. Perfect for temperature sensors,
 * pressure sensors, and other basic input devices.
 * 
 * Example usage:
 * ```cpp
 * class TemperatureSensor : public SimpleModbusDevice {
 * public:
 *     TemperatureSensor(uint8_t addr) : SimpleModbusDevice(addr) {}
 *     
 *     bool configure() override {
 *         // Read device ID during configuration
 *         auto id = readHoldingRegisters(0x0000, 1);
 *         if (!id.isOk()) return false;
 *         
 *         // Setup channel info
 *         addChannel("Temperature", "°C", 0x0001);
 *         return true;
 *     }
 *     
 *     ModbusResult<float> getFloat(size_t channel) const override {
 *         if (channel >= values.size()) {
 *             return ModbusResult<float>::error(ModbusError::INVALID_PARAMETER);
 *         }
 *         return ModbusResult<float>::ok(values[channel] / 10.0f);
 *     }
 * };
 * ```
 */
class SimpleModbusDevice : public ModbusDevice, public IModbusAnalogInput {
public:
    /**
     * @brief Constructor
     * @param serverAddr Modbus server address
     */
    explicit SimpleModbusDevice(uint8_t serverAddr);
    
    /**
     * @brief Initialize the device
     * @return true if initialization successful
     */
    bool initialize();
    
    // IModbusInput implementation
    ModbusResult<void> update() override;
    bool hasValidData() const override { return lastUpdateTime > 0 && getInitPhase() == InitPhase::READY; }
    uint32_t getLastUpdateTime() const override { return lastUpdateTime; }
    uint32_t getDataAge() const override;
    size_t getChannelCount() const override { return channels.size(); }
    const char* getChannelName(size_t channel) const override;
    const char* getChannelUnits(size_t channel) const override;
    
    // IModbusAnalogInput implementation
    ModbusResult<float> getFloat(size_t channel = 0) const override;
    ModbusResult<int32_t> getRawValue(size_t channel = 0) const override;
    float getScaleFactor(size_t channel = 0) const override { return 1.0f; }
    bool getRange(size_t channel, float& min, float& max) const override;
    
protected:
    /**
     * @brief Channel information
     */
    struct ChannelInfo {
        std::string name;
        std::string units;
        uint16_t address;
        float minValue = -std::numeric_limits<float>::max();
        float maxValue = std::numeric_limits<float>::max();
    };
    
    /**
     * @brief Add a channel
     * @param name Channel name
     * @param units Channel units
     * @param address Modbus register address
     */
    void addChannel(const std::string& name, const std::string& units, uint16_t address);
    
    /**
     * @brief Set channel range
     * @param channel Channel index
     * @param min Minimum valid value
     * @param max Maximum valid value
     */
    void setChannelRange(size_t channel, float min, float max);
    
    /**
     * @brief Virtual method to configure device
     * Called during initialization to setup channels
     * @return true if configuration successful
     */
    virtual bool configure() = 0;
    
    /**
     * @brief Virtual method to read all channel data
     * Called by update() to refresh values
     * @return true if read successful
     */
    virtual bool readChannelData();
    
    // Data storage
    std::vector<ChannelInfo> channels;
    std::vector<int32_t> values;
    uint32_t lastUpdateTime = 0;
    
private:
    // Override base class handler to prevent warnings
    void handleModbusResponse(uint8_t functionCode, uint16_t address,
                            const uint8_t* data, size_t length) override {
        // During normal operation, we use synchronous reads
        // Responses are handled internally by waitForResponse()
    }
};

} // namespace modbus

#endif // SIMPLEMODBUSDEVICE_H