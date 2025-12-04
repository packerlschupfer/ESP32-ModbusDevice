#ifndef IMODBUSINPUT_H
#define IMODBUSINPUT_H

#include <cstdint>
#include <vector>
#include "ModbusTypes.h"

namespace modbus {

/**
 * @interface IModbusInput
 * @brief Interface for devices that read data from Modbus
 * 
 * This interface is designed for sensor-type devices that primarily
 * read data from Modbus servers. It provides a clean API for input
 * operations without forcing any specific patterns.
 */
class IModbusInput {
public:
    virtual ~IModbusInput() = default;
    
    /**
     * @brief Update data from the Modbus device
     * @return Result indicating success or specific error
     */
    virtual ModbusResult<void> update() = 0;
    
    /**
     * @brief Check if data is available and valid
     * @return true if recent valid data is available
     */
    virtual bool hasValidData() const = 0;
    
    /**
     * @brief Get the timestamp of last successful update
     * @return Timestamp in milliseconds (or 0 if never updated)
     */
    virtual uint32_t getLastUpdateTime() const = 0;
    
    /**
     * @brief Get age of current data in milliseconds
     * @return Time since last successful update
     */
    virtual uint32_t getDataAge() const = 0;
    
    /**
     * @brief Template method to get typed sensor data
     * @tparam T The data type to retrieve
     * @param channel Optional channel/index for multi-channel devices
     * @return Result containing the data or error
     */
    template<typename T>
    ModbusResult<T> getValue(size_t channel = 0) const {
        // This would be specialized by derived classes
        return ModbusResult<T>::error(ModbusError::NOT_SUPPORTED);
    }
    
    /**
     * @brief Get all available channels
     * @return Number of data channels this device provides
     */
    virtual size_t getChannelCount() const = 0;
    
    /**
     * @brief Get human-readable channel name
     * @param channel Channel index
     * @return Channel name or empty string if invalid
     */
    virtual const char* getChannelName(size_t channel) const = 0;
    
    /**
     * @brief Get channel units (e.g., "°C", "bar", "%")
     * @param channel Channel index
     * @return Unit string or empty string if unitless
     */
    virtual const char* getChannelUnits(size_t channel) const = 0;
};

/**
 * @interface IModbusAnalogInput
 * @brief Specialized interface for analog input devices
 * 
 * Provides convenient methods for devices that read analog values
 * like temperature sensors, pressure sensors, etc.
 */
class IModbusAnalogInput : public IModbusInput {
public:
    /**
     * @brief Get analog value as float
     * @param channel Channel index for multi-channel devices
     * @return Result containing the float value or error
     */
    virtual ModbusResult<float> getFloat(size_t channel = 0) const = 0;
    
    /**
     * @brief Get analog value as scaled integer
     * @param channel Channel index
     * @return Result containing the raw integer value or error
     */
    virtual ModbusResult<int32_t> getRawValue(size_t channel = 0) const = 0;
    
    /**
     * @brief Get scaling factor for raw to float conversion
     * @param channel Channel index
     * @return Scaling factor (e.g., 0.1 for tenths)
     */
    virtual float getScaleFactor(size_t channel = 0) const = 0;
    
    /**
     * @brief Get value range for validation
     * @param channel Channel index
     * @param min Output: minimum valid value
     * @param max Output: maximum valid value
     * @return true if range is defined
     */
    virtual bool getRange(size_t channel, float& min, float& max) const = 0;
};

/**
 * @interface IModbusDigitalInput
 * @brief Specialized interface for digital input devices
 * 
 * Provides convenient methods for devices that read digital states
 * like switches, alarms, status bits, etc.
 */
class IModbusDigitalInput : public IModbusInput {
public:
    /**
     * @brief Get digital state
     * @param channel Channel/bit index
     * @return Result containing the boolean state or error
     */
    virtual ModbusResult<bool> getState(size_t channel = 0) const = 0;
    
    /**
     * @brief Get multiple states as bitmask
     * @param startChannel Starting channel index
     * @param count Number of channels to read
     * @return Result containing states as bits or error
     */
    virtual ModbusResult<uint32_t> getStates(size_t startChannel = 0, size_t count = 32) const = 0;
    
    /**
     * @brief Check if any alarm/error state is active
     * @return true if any error condition is active
     */
    virtual bool hasActiveAlarm() const = 0;
    
    /**
     * @brief Get active alarm codes
     * @return Vector of active alarm/error codes
     */
    virtual std::vector<uint16_t> getActiveAlarms() const = 0;
};

} // namespace modbus

#endif // IMODBUSINPUT_H