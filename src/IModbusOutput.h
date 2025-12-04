#ifndef IMODBUSOUTPUT_H
#define IMODBUSOUTPUT_H

#include <cstdint>
#include <vector>
#include "ModbusTypes.h"

namespace modbus {

/**
 * @interface IModbusOutput
 * @brief Interface for devices that write data to Modbus
 * 
 * This interface is designed for actuator-type devices that primarily
 * write data to Modbus servers. It provides a clean API for output
 * operations without forcing any specific patterns.
 */
class IModbusOutput {
public:
    virtual ~IModbusOutput() = default;
    
    /**
     * @brief Apply pending changes to the device
     * @return Result indicating success or specific error
     */
    virtual ModbusResult<void> apply() = 0;
    
    /**
     * @brief Check if there are pending changes
     * @return true if there are uncommitted changes
     */
    virtual bool hasPendingChanges() const = 0;
    
    /**
     * @brief Discard all pending changes
     */
    virtual void discardPendingChanges() = 0;
    
    /**
     * @brief Get the timestamp of last successful write
     * @return Timestamp in milliseconds (or 0 if never written)
     */
    virtual uint32_t getLastWriteTime() const = 0;
    
    /**
     * @brief Template method to set typed output data
     * @tparam T The data type to write
     * @param value The value to write
     * @param channel Optional channel/index for multi-channel devices
     * @return Result indicating success or error
     */
    template<typename T>
    ModbusResult<void> setValue(const T& value, size_t channel = 0) {
        // This would be specialized by derived classes
        return ModbusResult<void>::error(ModbusError::NOT_SUPPORTED);
    }
    
    /**
     * @brief Get number of output channels
     * @return Number of output channels this device provides
     */
    virtual size_t getChannelCount() const = 0;
    
    /**
     * @brief Get human-readable channel name
     * @param channel Channel index
     * @return Channel name or empty string if invalid
     */
    virtual const char* getChannelName(size_t channel) const = 0;
    
    /**
     * @brief Check if channel is writable
     * @param channel Channel index
     * @return true if channel accepts writes
     */
    virtual bool isChannelWritable(size_t channel) const = 0;
};

/**
 * @interface IModbusAnalogOutput
 * @brief Specialized interface for analog output devices
 * 
 * Provides convenient methods for devices that write analog values
 * like DACs, motor controllers, valve positioners, etc.
 */
class IModbusAnalogOutput : public IModbusOutput {
public:
    /**
     * @brief Set analog output value
     * @param value The float value to set
     * @param channel Channel index for multi-channel devices
     * @return Result indicating success or error
     */
    virtual ModbusResult<void> setFloat(float value, size_t channel = 0) = 0;
    
    /**
     * @brief Set raw analog value
     * @param value The raw integer value
     * @param channel Channel index
     * @return Result indicating success or error
     */
    virtual ModbusResult<void> setRawValue(int32_t value, size_t channel = 0) = 0;
    
    /**
     * @brief Get current setpoint
     * @param channel Channel index
     * @return Current setpoint value
     */
    virtual float getSetpoint(size_t channel = 0) const = 0;
    
    /**
     * @brief Get actual output value (if readable)
     * @param channel Channel index
     * @return Result containing actual value or error if not readable
     */
    virtual ModbusResult<float> getActualValue(size_t channel = 0) const = 0;
    
    /**
     * @brief Get output range for validation
     * @param channel Channel index
     * @param min Output: minimum valid value
     * @param max Output: maximum valid value
     * @return true if range is defined
     */
    virtual bool getRange(size_t channel, float& min, float& max) const = 0;
    
    /**
     * @brief Set output to safe/default value
     * @param channel Channel index
     * @return Result indicating success or error
     */
    virtual ModbusResult<void> setSafeValue(size_t channel = 0) = 0;
};

/**
 * @interface IModbusDigitalOutput
 * @brief Specialized interface for digital output devices
 * 
 * Provides convenient methods for devices that write digital states
 * like relays, solenoids, indicators, etc.
 */
class IModbusDigitalOutput : public IModbusOutput {
public:
    /**
     * @brief Set digital output state
     * @param state The boolean state to set
     * @param channel Channel/bit index
     * @return Result indicating success or error
     */
    virtual ModbusResult<void> setState(bool state, size_t channel = 0) = 0;
    
    /**
     * @brief Set multiple states at once
     * @param states Bitmask of states to set
     * @param startChannel Starting channel index
     * @param count Number of channels to set
     * @return Result indicating success or error
     */
    virtual ModbusResult<void> setStates(uint32_t states, size_t startChannel = 0, size_t count = 32) = 0;
    
    /**
     * @brief Toggle output state
     * @param channel Channel index
     * @return Result indicating success or error
     */
    virtual ModbusResult<void> toggle(size_t channel = 0) = 0;
    
    /**
     * @brief Pulse output (on then off)
     * @param durationMs Pulse duration in milliseconds
     * @param channel Channel index
     * @return Result indicating success or error
     */
    virtual ModbusResult<void> pulse(uint32_t durationMs, size_t channel = 0) = 0;
    
    /**
     * @brief Get current output state
     * @param channel Channel index
     * @return Current state (setpoint, not actual)
     */
    virtual bool getState(size_t channel = 0) const = 0;
    
    /**
     * @brief Set all outputs to safe state
     * @return Result indicating success or error
     */
    virtual ModbusResult<void> setAllSafe() = 0;
};

/**
 * @interface IModbusController
 * @brief Combined interface for devices that both read and write
 * 
 * This interface combines input and output capabilities for devices
 * like PID controllers, servo drives, etc. that need bidirectional
 * communication.
 */
class IModbusController : public IModbusInput, public IModbusOutput {
public:
    /**
     * @brief Perform a complete control cycle
     * @return Result indicating success or error
     */
    virtual ModbusResult<void> process() = 0;
    
    /**
     * @brief Check if controller is in automatic mode
     * @return true if in auto mode, false if manual
     */
    virtual bool isAutoMode() const = 0;
    
    /**
     * @brief Set controller mode
     * @param autoMode true for automatic, false for manual
     * @return Result indicating success or error
     */
    virtual ModbusResult<void> setMode(bool autoMode) = 0;
    
    /**
     * @brief Emergency stop - set all outputs to safe state
     * @return Result indicating success or error
     */
    virtual ModbusResult<void> emergencyStop() = 0;
};

} // namespace modbus

#endif // IMODBUSOUTPUT_H