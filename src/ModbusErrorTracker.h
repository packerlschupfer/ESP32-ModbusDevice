#ifndef MODBUS_ERROR_TRACKER_H
#define MODBUS_ERROR_TRACKER_H

#include <cstdint>
#include <atomic>
#include "ModbusTypes.h"

// Configuration: Maximum number of tracked devices (no heap allocation)
#ifndef MODBUS_ERROR_TRACKER_MAX_DEVICES
#define MODBUS_ERROR_TRACKER_MAX_DEVICES 8
#endif

namespace modbus {

/**
 * @class ModbusErrorTracker
 * @brief Thread-safe error tracking for Modbus devices
 *
 * Tracks error statistics per device address. Designed for diagnostic purposes
 * to help distinguish bus noise (CRC errors) from device failures (timeouts).
 *
 * All methods are static and thread-safe using std::atomic counters.
 * No heap allocation - uses fixed-size static array.
 *
 * Usage in device libraries:
 * @code
 * auto result = modbusRead(...);
 * if (result.isError()) {
 *     ModbusErrorTracker::recordError(address,
 *         ModbusErrorTracker::categorizeError(result.error()));
 * } else {
 *     ModbusErrorTracker::recordSuccess(address);
 * }
 * @endcode
 *
 * Usage in main project:
 * @code
 * uint32_t errors = ModbusErrorTracker::getTotalErrors(0x01);
 * float rate = ModbusErrorTracker::getErrorRate(0x01);
 * @endcode
 */
class ModbusErrorTracker {
public:
    /**
     * @enum ErrorCategory
     * @brief Categories for classifying Modbus errors
     */
    enum class ErrorCategory {
        CRC_ERROR,      ///< Corrupted response (bus noise, EMI)
        TIMEOUT,        ///< No response within timeout (device offline)
        INVALID_DATA,   ///< Malformed response (device confused)
        DEVICE_ERROR,   ///< Device-reported Modbus exception codes
        OTHER           ///< Unclassified errors
    };

    /**
     * @brief Record an error for a device
     * @param deviceAddress Modbus device address (1-247)
     * @param category Error category
     */
    static void recordError(uint8_t deviceAddress, ErrorCategory category);

    /**
     * @brief Record a successful operation for a device
     * @param deviceAddress Modbus device address (1-247)
     */
    static void recordSuccess(uint8_t deviceAddress);

    /**
     * @brief Categorize a ModbusError into an ErrorCategory
     * @param error The ModbusError to categorize
     * @return Appropriate ErrorCategory
     */
    static ErrorCategory categorizeError(ModbusError error);

    /**
     * @brief Reset all statistics for a device
     * @param deviceAddress Modbus device address (1-247)
     */
    static void resetDevice(uint8_t deviceAddress);

    /**
     * @brief Reset statistics for all tracked devices
     */
    static void resetAll();

    // ===== Statistics Getters =====

    /**
     * @brief Get total error count for a device
     * @param deviceAddress Modbus device address
     * @return Sum of all error categories
     */
    static uint32_t getTotalErrors(uint8_t deviceAddress);

    /**
     * @brief Get CRC error count for a device
     * @param deviceAddress Modbus device address
     * @return CRC error count
     */
    static uint32_t getCrcErrors(uint8_t deviceAddress);

    /**
     * @brief Get timeout count for a device
     * @param deviceAddress Modbus device address
     * @return Timeout count
     */
    static uint32_t getTimeouts(uint8_t deviceAddress);

    /**
     * @brief Get invalid data error count for a device
     * @param deviceAddress Modbus device address
     * @return Invalid data error count
     */
    static uint32_t getInvalidDataErrors(uint8_t deviceAddress);

    /**
     * @brief Get device error count (Modbus exceptions) for a device
     * @param deviceAddress Modbus device address
     * @return Device error count
     */
    static uint32_t getDeviceErrors(uint8_t deviceAddress);

    /**
     * @brief Get other/unclassified error count for a device
     * @param deviceAddress Modbus device address
     * @return Other error count
     */
    static uint32_t getOtherErrors(uint8_t deviceAddress);

    /**
     * @brief Get successful operation count for a device
     * @param deviceAddress Modbus device address
     * @return Success count
     */
    static uint32_t getSuccessCount(uint8_t deviceAddress);

    /**
     * @brief Get timestamp of last error for a device
     * @param deviceAddress Modbus device address
     * @return Timestamp in milliseconds (from millis())
     */
    static uint32_t getLastErrorTime(uint8_t deviceAddress);

    /**
     * @brief Calculate error rate percentage for a device
     * @param deviceAddress Modbus device address
     * @return Error rate as percentage (0.0 - 100.0), or 0.0 if no operations
     */
    static float getErrorRate(uint8_t deviceAddress);

    /**
     * @brief Get the number of currently tracked devices
     * @return Number of devices with recorded statistics
     */
    static uint8_t getTrackedDeviceCount();

    /**
     * @brief Check if a device has any recorded statistics
     * @param deviceAddress Modbus device address
     * @return true if device has statistics, false otherwise
     */
    static bool isDeviceTracked(uint8_t deviceAddress);

    /**
     * @brief Convert ErrorCategory to human-readable string
     * @param category The error category
     * @return String representation
     */
    static const char* categoryToString(ErrorCategory category);

private:
    /**
     * @struct DeviceErrorStats
     * @brief Per-device error statistics (thread-safe)
     */
    struct DeviceErrorStats {
        uint8_t address{0};
        std::atomic<uint32_t> crcErrors{0};
        std::atomic<uint32_t> timeouts{0};
        std::atomic<uint32_t> invalidData{0};
        std::atomic<uint32_t> deviceErrors{0};
        std::atomic<uint32_t> otherErrors{0};
        std::atomic<uint32_t> successCount{0};
        std::atomic<uint32_t> lastErrorTime{0};
        std::atomic<bool> initialized{false};
    };

    static DeviceErrorStats deviceStats[MODBUS_ERROR_TRACKER_MAX_DEVICES];
    static std::atomic<uint8_t> numDevices;

    /**
     * @brief Find existing stats or create new entry for device
     * @param address Modbus device address
     * @return Pointer to stats, or nullptr if array full
     */
    static DeviceErrorStats* findOrCreateStats(uint8_t address);

    /**
     * @brief Find existing stats for device (read-only)
     * @param address Modbus device address
     * @return Pointer to stats, or nullptr if not found
     */
    static const DeviceErrorStats* findStats(uint8_t address);
};

} // namespace modbus

#endif // MODBUS_ERROR_TRACKER_H
