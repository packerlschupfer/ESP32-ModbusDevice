#ifndef MODBUSDEVICE_H
#define MODBUSDEVICE_H

#include "ModbusDeviceLogging.h"
#include "IModbusDevice.h"
#include "ModbusRegistry.h"
#include <esp32ModbusRTU.h>
#include <memory>
#include <atomic>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include <MutexGuard.h>
#include "ModbusTypes.h"

// Forward declare global callback functions (legacy - use ModbusRegistry instead)
void mainHandleData(uint8_t serverAddress, esp32Modbus::FunctionCode fc,
                   uint16_t startingAddress, const uint8_t* data, size_t length);
void handleError(uint8_t serverAddress, esp32Modbus::Error error);

namespace modbus {

/**
 * @class ModbusDevice
 * @brief Base class for Modbus RTU devices
 * 
 * This class provides the core Modbus communication functionality without
 * imposing initialization patterns or unnecessary complexity.
 * 
 * Key improvements in v2.0.0:
 * - No circular dependency issues
 * - Clean initialization phases
 * - No false warnings during init
 * - Minimal resource usage
 * - Clear separation of concerns
 */
class ModbusDevice : public IModbusDevice {
public:
    /**
     * @enum InitPhase
     * @brief Tracks device initialization phase
     */
    enum class InitPhase {
        UNINITIALIZED,    ///< Device just constructed
        CONFIGURING,      ///< Reading configuration from device
        READY,           ///< Device ready for normal operation
        ERROR            ///< Initialization failed
    };
    
    /**
     * @brief Constructor
     * @param serverAddr Modbus server address (1-247)
     */
    explicit ModbusDevice(uint8_t serverAddr);
    
    /**
     * @brief Destructor
     */
    virtual ~ModbusDevice();
    
    // Delete copy operations
    ModbusDevice(const ModbusDevice&) = delete;
    ModbusDevice& operator=(const ModbusDevice&) = delete;
    
    // IModbusDevice implementation
    uint8_t getServerAddress() const noexcept override { return serverAddress; }
    [[nodiscard]] ModbusResult<void> setServerAddress(uint8_t address) override;

    // ===== Standard API (uses default RELAY priority) =====
    [[nodiscard]] ModbusResult<std::vector<uint16_t>> readHoldingRegisters(uint16_t address, uint16_t count) override;
    [[nodiscard]] ModbusResult<std::vector<uint16_t>> readInputRegisters(uint16_t address, uint16_t count) override;
    [[nodiscard]] ModbusResult<void> writeSingleRegister(uint16_t address, uint16_t value) override;
    [[nodiscard]] ModbusResult<void> writeMultipleRegisters(uint16_t address, const std::vector<uint16_t>& values) override;

    [[nodiscard]] ModbusResult<std::vector<bool>> readCoils(uint16_t address, uint16_t count) override;
    [[nodiscard]] ModbusResult<std::vector<bool>> readDiscreteInputs(uint16_t address, uint16_t count) override;
    [[nodiscard]] ModbusResult<void> writeSingleCoil(uint16_t address, bool value) override;
    [[nodiscard]] ModbusResult<void> writeMultipleCoils(uint16_t address, const std::vector<bool>& values) override;

    // ===== Priority API (allows specifying request priority) =====
    [[nodiscard]] ModbusResult<std::vector<uint16_t>> readHoldingRegistersWithPriority(uint16_t address, uint16_t count, esp32Modbus::ModbusPriority priority);
    [[nodiscard]] ModbusResult<std::vector<uint16_t>> readInputRegistersWithPriority(uint16_t address, uint16_t count, esp32Modbus::ModbusPriority priority);
    [[nodiscard]] ModbusResult<void> writeSingleRegisterWithPriority(uint16_t address, uint16_t value, esp32Modbus::ModbusPriority priority);
    [[nodiscard]] ModbusResult<void> writeSingleCoilWithPriority(uint16_t address, bool value, esp32Modbus::ModbusPriority priority);

    bool isConnected() const noexcept override { return lastError == ModbusError::SUCCESS && initPhase == InitPhase::READY; }
    ModbusError getLastError() const noexcept override { return lastError; }
    [[nodiscard]] Statistics getStatistics() const override;
    void resetStatistics() override;
    
    /**
     * @brief Get current initialization phase
     * @return Current InitPhase
     */
    InitPhase getInitPhase() const noexcept { return initPhase; }
    
    /**
     * @brief Set initialization phase
     * @param phase New initialization phase
     */
    void setInitPhase(InitPhase phase);
    
    /**
     * @brief Set event group for initialization notification
     * @param eventGroup FreeRTOS event group handle
     * @param readyBit Bit to set when device becomes ready
     * @param errorBit Bit to set when initialization fails (optional)
     */
    void setEventGroup(EventGroupHandle_t eventGroup, EventBits_t readyBit, EventBits_t errorBit = 0);

    /**
     * @brief Get the external event group configured for this device
     * @return Event group handle, or nullptr if not configured
     */
    EventGroupHandle_t getExternalEventGroup() const { return eventGroup; }

    /**
     * @brief Get the ready bit configured for this device
     * @return Ready bit mask
     */
    EventBits_t getReadyBit() const { return readyBit; }

    /**
     * @brief Get the error bit configured for this device
     * @return Error bit mask
     */
    EventBits_t getErrorBit() const { return errorBit; }

    /**
     * @brief Register device globally for callback routing
     * @return ModbusError indicating success or error
     */
    ModbusError registerDevice();
    
    /**
     * @brief Unregister device from global map
     * @return ModbusError indicating success or error
     */
    ModbusError unregisterDevice();
    
protected:
    /**
     * @brief Handle incoming Modbus response
     * 
     * This method is called when a response is received. During CONFIGURING
     * phase, responses are expected and normal. During READY phase, responses
     * trigger normal data processing.
     * 
     * @param functionCode Modbus function code
     * @param address Register/coil address
     * @param data Response data
     * @param length Data length
     */
    virtual void handleModbusResponse(uint8_t functionCode, uint16_t address, 
                                    const uint8_t* data, size_t length);
    
    /**
     * @brief Handle Modbus errors
     * @param error The error that occurred
     */
    virtual void handleModbusError(ModbusError error);
    
    /**
     * @brief Send Modbus request (acquires bus mutex internally)
     * @param fc Function code
     * @param addr Register/coil address
     * @param count Number of items
     * @param data Optional data for write operations
     * @return ESP_OK on success
     * @deprecated Use transaction methods that hold mutex across request+response
     */
    esp_err_t sendRequest(uint8_t fc, uint16_t addr, uint16_t count, uint16_t* data = nullptr);

    /**
     * @brief Send Modbus request without acquiring mutex (caller must hold modbusMutex)
     * @param fc Function code
     * @param addr Register/coil address
     * @param count Number of items
     * @param data Optional data for write operations
     * @return ESP_OK on success
     */
    esp_err_t sendRequestInternal(uint8_t fc, uint16_t addr, uint16_t count, uint16_t* data = nullptr);

    /**
     * @brief Send Modbus request with priority (new API)
     * @param fc Function code
     * @param addr Starting address
     * @param count Number of items
     * @param priority Request priority (EMERGENCY/SENSOR/RELAY/STATUS)
     * @param data Optional data for write operations
     * @return ESP_OK on success
     */
    esp_err_t sendRequestWithPriority(uint8_t fc, uint16_t addr, uint16_t count,
                                      esp32Modbus::ModbusPriority priority,
                                      uint16_t* data = nullptr);

    /**
     * @brief Acquire the global Modbus bus mutex
     * @param timeoutMs Timeout in milliseconds
     * @return true if mutex acquired, false on timeout
     */
    static bool acquireBusMutex(uint32_t timeoutMs = 2000);

    /**
     * @brief Release the global Modbus bus mutex
     */
    static void releaseBusMutex();
    
    /**
     * @brief Wait for synchronous response
     * @param timeout Timeout in ticks
     * @return Result containing response data or error
     */
    ModbusResult<std::vector<uint8_t>> waitForResponse(TickType_t timeout = pdMS_TO_TICKS(1000));
    
private:
    friend void ::mainHandleData(uint8_t, esp32Modbus::FunctionCode, uint16_t, const uint8_t*, size_t);
    friend void ::handleError(uint8_t, esp32Modbus::Error);
    
    // Core members
    uint8_t serverAddress;
    std::atomic<InitPhase> initPhase{InitPhase::UNINITIALIZED};
    std::atomic<ModbusError> lastError{ModbusError::SUCCESS};
    
    // Statistics
    std::atomic<uint32_t> totalRequests{0};
    std::atomic<uint32_t> successfulRequests{0};
    std::atomic<uint32_t> timeouts{0};
    std::atomic<uint32_t> crcErrors{0};
    
    // Synchronous operation support
    struct SyncContext {
        std::vector<uint8_t> responseData;
        std::atomic<bool> responseReceived{false};
        std::atomic<bool> errorOccurred{false};
        ModbusError error{ModbusError::SUCCESS};
        SemaphoreHandle_t semaphore{nullptr};
    };
    std::unique_ptr<SyncContext> syncContext;
    SemaphoreHandle_t syncMutex{nullptr};
    
    /**
     * @brief Ensure sync resources are ready
     */
    void ensureSyncReady();
    
    /**
     * @brief Internal callback handler
     */
    void handleData(uint8_t serverAddress, esp32Modbus::FunctionCode fc,
                   uint16_t startingAddress, const uint8_t* data, size_t length);
    
    /**
     * @brief Internal error handler
     */
    void handleError(esp32Modbus::Error error);
    
    /**
     * @brief Map esp32Modbus error to ModbusError
     */
    static ModbusError mapError(esp32Modbus::Error error);
    
    // Static Modbus mutex for thread safety
    static SemaphoreHandle_t modbusMutex;
    
    // Event group support
    EventGroupHandle_t eventGroup{nullptr};
    EventBits_t readyBit{0};
    EventBits_t errorBit{0};
};

} // namespace modbus

// Helper function to get ModbusError string
const char* getModbusErrorString(modbus::ModbusError error);

#endif // MODBUSDEVICE_H