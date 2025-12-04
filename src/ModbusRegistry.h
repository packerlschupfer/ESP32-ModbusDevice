#ifndef MODBUSREGISTRY_H
#define MODBUSREGISTRY_H

/**
 * @file ModbusRegistry.h
 * @brief Thread-safe singleton registry for Modbus devices
 *
 * Provides centralized management of Modbus devices and the global
 * ModbusRTU instance, replacing the previous unsafe global variables.
 */

#include <unordered_map>
#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Forward declarations
class esp32ModbusRTU;

namespace modbus {

class ModbusDevice;

/**
 * @class ModbusRegistry
 * @brief Singleton registry for managing Modbus devices and RTU instance
 *
 * Thread-safe registry that provides:
 * - Device registration/unregistration by address
 * - Device lookup by address
 * - Global ModbusRTU instance management
 * - Bus mutex for thread-safe communication
 *
 * Uses Meyer's singleton pattern for safe initialization.
 */
class ModbusRegistry {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the ModbusRegistry singleton
     * @note Thread-safe (C++11 guarantee)
     */
    static ModbusRegistry& getInstance() {
        static ModbusRegistry instance;
        return instance;
    }

    // Delete copy/move operations
    ModbusRegistry(const ModbusRegistry&) = delete;
    ModbusRegistry& operator=(const ModbusRegistry&) = delete;
    ModbusRegistry(ModbusRegistry&&) = delete;
    ModbusRegistry& operator=(ModbusRegistry&&) = delete;

    /**
     * @brief Set the global ModbusRTU instance
     * @param rtu Pointer to the ModbusRTU instance
     * @note Thread-safe
     */
    void setModbusRTU(esp32ModbusRTU* rtu);

    /**
     * @brief Get the global ModbusRTU instance
     * @return Pointer to the ModbusRTU instance, or nullptr if not set
     * @note Thread-safe
     */
    esp32ModbusRTU* getModbusRTU() const noexcept { return modbusRTU_; }

    /**
     * @brief Register a device at a specific address
     * @param address Modbus address (1-247)
     * @param device Pointer to the device
     * @return true if registered successfully, false on error
     * @note Thread-safe
     */
    bool registerDevice(uint8_t address, ModbusDevice* device);

    /**
     * @brief Unregister a device from the registry
     * @param address Modbus address to unregister
     * @return true if unregistered successfully, false if not found
     * @note Thread-safe
     */
    bool unregisterDevice(uint8_t address);

    /**
     * @brief Get a device by address
     * @param address Modbus address to look up
     * @return Pointer to the device, or nullptr if not found
     * @note Thread-safe
     */
    ModbusDevice* getDevice(uint8_t address) const;

    /**
     * @brief Check if a device is registered at an address
     * @param address Modbus address to check
     * @return true if a device is registered at this address
     * @note Thread-safe
     */
    bool hasDevice(uint8_t address) const;

    /**
     * @brief Get the number of registered devices
     * @return Number of devices in the registry
     * @note Thread-safe
     */
    size_t getDeviceCount() const;

    /**
     * @brief Get the device map mutex (for callback routing)
     * @return Mutex handle
     */
    SemaphoreHandle_t getMutex() const noexcept { return mutex_; }

    /**
     * @brief Get the global bus mutex for Modbus communication
     * @return Bus mutex handle
     */
    SemaphoreHandle_t getBusMutex() const noexcept { return busMutex_; }

    /**
     * @brief Acquire the bus mutex
     * @param timeoutMs Timeout in milliseconds
     * @return true if acquired, false on timeout
     */
    bool acquireBusMutex(uint32_t timeoutMs = 2000);

    /**
     * @brief Release the bus mutex
     */
    void releaseBusMutex() noexcept;

private:
    ModbusRegistry();
    ~ModbusRegistry();

    std::unordered_map<uint8_t, ModbusDevice*> deviceMap_;
    mutable SemaphoreHandle_t mutex_;
    SemaphoreHandle_t busMutex_;
    esp32ModbusRTU* modbusRTU_ = nullptr;
};

} // namespace modbus

#endif // MODBUSREGISTRY_H
