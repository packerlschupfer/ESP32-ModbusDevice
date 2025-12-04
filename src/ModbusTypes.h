#ifndef MODBUSTYPES_H
#define MODBUSTYPES_H

#include <cstdint>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Result.h"  // common::Result from LibraryCommon

// Configuration constants
#ifndef MODBUS_MAX_READ_SIZE
#define MODBUS_MAX_READ_SIZE 252  // Max Modbus RTU payload
#endif

#ifndef MODBUS_SYNC_TIMEOUT_DEFAULT
#define MODBUS_SYNC_TIMEOUT_DEFAULT pdMS_TO_TICKS(200)
#endif

// Modbus address limits (per Modbus specification)
#ifndef MODBUS_MAX_SLAVE_ADDRESS
#define MODBUS_MAX_SLAVE_ADDRESS 247
#endif

// Read/write limits per Modbus specification
#ifndef MODBUS_MAX_REGISTER_COUNT
#define MODBUS_MAX_REGISTER_COUNT 125   // Max registers per read (FC 0x03/0x04)
#endif

#ifndef MODBUS_MAX_WRITE_REGISTER_COUNT
#define MODBUS_MAX_WRITE_REGISTER_COUNT 123  // Max registers per write (FC 0x10)
#endif

#ifndef MODBUS_MAX_COIL_COUNT
#define MODBUS_MAX_COIL_COUNT 2000      // Max coils per read/write
#endif

#ifndef MODBUS_MAX_WRITE_COIL_COUNT
#define MODBUS_MAX_WRITE_COIL_COUNT 1968  // Max coils per write (FC 0x0F)
#endif

// Mutex timeouts (milliseconds)
#ifndef MODBUS_MUTEX_TIMEOUT_MS
#define MODBUS_MUTEX_TIMEOUT_MS 2000    // Default mutex timeout for transactions
#endif

#ifndef MODBUS_LEGACY_MUTEX_TIMEOUT_MS
#define MODBUS_LEGACY_MUTEX_TIMEOUT_MS 1000  // Legacy sendRequest timeout
#endif

// Modbus baud rate for inter-frame delay calculation
// Define this in your project to match your RS485 baud rate
#ifndef MODBUS_BAUD_RATE
#define MODBUS_BAUD_RATE 9600  // Default baud rate
#endif

// Inter-frame delay for Modbus RTU (3.5 character times per specification)
// Formula: (3.5 chars × 11 bits/char × 1000ms/s) / baud_rate = 38500 / baud_rate
// +1ms safety margin for bus settling and timing jitter
// At 9600 baud = 5ms, at 19200 = 3ms, at 115200 = 1ms
#ifndef MODBUS_INTER_FRAME_DELAY_MS
#define MODBUS_INTER_FRAME_DELAY_MS ((38500UL / MODBUS_BAUD_RATE) + 1)
#endif

namespace modbus {

/**
 * @enum ModbusError
 * @brief Enhanced error codes for Modbus operations
 */
enum class ModbusError {
    SUCCESS = 0,              ///< Operation completed successfully
    
    // Modbus exception codes (0x01-0x08) - matching Modbus specification
    ILLEGAL_FUNCTION = 0x01,      ///< Modbus exception 01: Illegal function
    ILLEGAL_DATA_ADDRESS = 0x02,  ///< Modbus exception 02: Illegal data address
    ILLEGAL_DATA_VALUE = 0x03,    ///< Modbus exception 03: Illegal data value
    SLAVE_DEVICE_FAILURE = 0x04,  ///< Modbus exception 04: Slave device failure
    // 0x05-0x08 reserved for other Modbus exceptions
    
    // Library-specific error codes (0x80+) to avoid conflicts
    TIMEOUT = 0x80,               ///< Operation timed out
    CRC_ERROR,                    ///< CRC check failed  
    INVALID_RESPONSE,             ///< Response format invalid
    QUEUE_FULL,                   ///< Queue is full (for async operations)
    NOT_INITIALIZED,              ///< Device not initialized
    COMMUNICATION_ERROR,          ///< General communication error
    INVALID_PARAMETER,            ///< Invalid parameter provided
    RESOURCE_ERROR,               ///< Failed to create/access resource
    NULL_POINTER,                 ///< Null pointer provided
    NOT_SUPPORTED,                ///< Operation not supported
    MUTEX_ERROR,                  ///< Mutex operation failed
    INVALID_DATA_LENGTH,          ///< Data length exceeds limits
    DEVICE_NOT_FOUND,             ///< Device not registered
    RESOURCE_CREATION_FAILED,     ///< Failed to create FreeRTOS resource
    INVALID_ADDRESS               ///< Invalid Modbus address (0 or > 247)
};

/**
 * @brief Result type for Modbus operations using common::Result
 * @tparam T The type of the value on success
 *
 * Uses common::Result from LibraryCommon with ModbusError as error type.
 */
template<typename T>
using ModbusResult = common::Result<T, ModbusError>;

/**
 * @struct ModbusPacket
 * @brief Packet structure for queued Modbus operations
 * 
 * This structure encapsulates a Modbus response for asynchronous processing.
 * Used by QueuedModbusDevice to decouple request/response handling.
 */
struct ModbusPacket {
    uint8_t functionCode;    ///< Modbus function code (0x01-0x10 typically)
    uint16_t address;        ///< Starting register/coil address
    uint8_t data[MODBUS_MAX_READ_SIZE]; ///< Response data buffer
    size_t length;           ///< Actual length of data received
    uint32_t timestamp;      ///< Tick count when packet was received (for timeout handling)
    
    /**
     * @brief Default constructor
     */
    ModbusPacket() : functionCode(0), address(0), length(0), timestamp(0) {
        memset(data, 0, sizeof(data));
    }
    
    /**
     * @brief Check if packet is valid
     * @return true if packet contains data
     */
    bool isValid() const { return length > 0; }
    
    /**
     * @brief Get age of packet in ticks
     * @return Number of ticks since packet was received
     */
    uint32_t getAge() const { 
        return xTaskGetTickCount() - timestamp; 
    }
};

} // namespace modbus

#endif // MODBUSTYPES_H