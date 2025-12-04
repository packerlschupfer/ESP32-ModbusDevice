#include "ModbusDevice.h"
#include "ModbusRegistry.h"
#include <cstring>
#include <algorithm>
#include <new>  // for std::nothrow

// Static mutex is now just a convenience pointer to the registry's bus mutex
SemaphoreHandle_t modbus::ModbusDevice::modbusMutex = nullptr;

namespace modbus {

// Constructor
ModbusDevice::ModbusDevice(uint8_t serverAddr)
    : serverAddress(serverAddr) {
    // Validate address
    if (serverAddr == 0 || serverAddr > MODBUS_MAX_SLAVE_ADDRESS) {
        MODBUSD_LOG_W("Invalid Modbus address %d, using 1", serverAddr);
        serverAddress = 1;
    }
    MODBUSD_LOG_D("ModbusDevice constructed for address %d", serverAddress);
}

// Destructor
ModbusDevice::~ModbusDevice() {
    // Unregister if still registered
    unregisterDevice();
    
    // Clean up sync resources if allocated
    if (syncContext && syncContext->semaphore) {
        vSemaphoreDelete(syncContext->semaphore);
    }
    if (syncMutex) {
        vSemaphoreDelete(syncMutex);
    }
}

// Set server address
ModbusResult<void> ModbusDevice::setServerAddress(uint8_t address) {
    if (address == 0 || address > MODBUS_MAX_SLAVE_ADDRESS) {
        return ModbusResult<void>::error(ModbusError::INVALID_ADDRESS);
    }
    
    // Unregister old address
    unregisterDevice();
    
    // Set new address
    serverAddress = address;
    
    // Re-register if in ready phase
    if (initPhase == InitPhase::READY) {
        ModbusError err = registerDevice();
        if (err != ModbusError::SUCCESS) {
            return ModbusResult<void>::error(err);
        }
    }
    
    return ModbusResult<void>::ok();
}

// Register device
ModbusError ModbusDevice::registerDevice() {
    if (ModbusRegistry::getInstance().registerDevice(serverAddress, this)) {
        return ModbusError::SUCCESS;
    }
    return ModbusError::MUTEX_ERROR;
}

// Unregister device
ModbusError ModbusDevice::unregisterDevice() {
    ModbusRegistry::getInstance().unregisterDevice(serverAddress);
    return ModbusError::SUCCESS;
}

// Ensure sync resources are ready
void ModbusDevice::ensureSyncReady() {
    if (!syncContext) {
        syncContext = std::make_unique<SyncContext>();
        syncContext->semaphore = xSemaphoreCreateBinary();
        if (!syncContext->semaphore) {
            MODBUSD_LOG_E("Failed to create sync semaphore");
            syncContext.reset();
            return;
        }
    }
    if (!syncMutex) {
        syncMutex = xSemaphoreCreateMutex();
        if (!syncMutex) {
            MODBUSD_LOG_E("Failed to create sync mutex");
        }
    }
}

// Acquire bus mutex
bool ModbusDevice::acquireBusMutex(uint32_t timeoutMs) {
    // Lazy initialization from registry's bus mutex
    if (!modbusMutex) {
        modbusMutex = ModbusRegistry::getInstance().getBusMutex();
        if (!modbusMutex) {
            MODBUSD_LOG_E("Modbus mutex not initialized");
            return false;
        }
    }
    if (xSemaphoreTake(modbusMutex, pdMS_TO_TICKS(timeoutMs)) != pdTRUE) {
        MODBUSD_LOG_W("Bus mutex timeout after %lu ms", (unsigned long)timeoutMs);
        return false;
    }
    return true;
}

// Release bus mutex
void ModbusDevice::releaseBusMutex() {
    if (modbusMutex) {
        xSemaphoreGive(modbusMutex);
    }
}

// Send request (legacy - acquires mutex, for backward compatibility)
esp_err_t ModbusDevice::sendRequest(uint8_t fc, uint16_t addr, uint16_t count, uint16_t* data) {
    if (!acquireBusMutex(MODBUS_LEGACY_MUTEX_TIMEOUT_MS)) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t result = sendRequestInternal(fc, addr, count, data);

    releaseBusMutex();

    return result;
}

// Send request internal (caller must hold modbusMutex)
esp_err_t ModbusDevice::sendRequestInternal(uint8_t fc, uint16_t addr, uint16_t count, uint16_t* data) {
    // Default to RELAY priority for backward compatibility
    return sendRequestWithPriority(fc, addr, count, esp32Modbus::RELAY, data);
}

esp_err_t ModbusDevice::sendRequestWithPriority(uint8_t fc, uint16_t addr, uint16_t count,
                                                esp32Modbus::ModbusPriority priority,
                                                uint16_t* data) {
    auto* rtu = ModbusRegistry::getInstance().getModbusRTU();
    if (!rtu) {
        MODBUSD_LOG_E("ModbusRTU not set in registry");
        return ESP_FAIL;
    }

    esp_err_t result = ESP_FAIL;

    switch (fc) {
        case 0x03:
            result = rtu->readHoldingRegistersWithPriority(serverAddress, addr, count, priority) ? ESP_OK : ESP_FAIL;
            break;
        case 0x04:
            result = rtu->readInputRegistersWithPriority(serverAddress, addr, count, priority) ? ESP_OK : ESP_FAIL;
            break;
        case 0x01:
            result = rtu->readCoilsWithPriority(serverAddress, addr, count, priority) ? ESP_OK : ESP_FAIL;
            break;
        case 0x02:
            result = rtu->readDiscreteInputsWithPriority(serverAddress, addr, count, priority) ? ESP_OK : ESP_FAIL;
            break;
        case 0x06:
            if (data) {
                result = rtu->writeSingleHoldingRegisterWithPriority(serverAddress, addr, data[0], priority) ? ESP_OK : ESP_FAIL;
            }
            break;
        case 0x10:
            if (data && count > 0) {
                uint8_t* byteData = new (std::nothrow) uint8_t[count * 2];
                if (!byteData) {
                    MODBUSD_LOG_E("Failed to allocate %u bytes for FC10", count * 2);
                    result = ESP_ERR_NO_MEM;
                    break;
                }
                for (uint16_t i = 0; i < count; i++) {
                    byteData[i * 2] = (data[i] >> 8) & 0xFF;
                    byteData[i * 2 + 1] = data[i] & 0xFF;
                }
                result = rtu->writeMultHoldingRegistersWithPriority(serverAddress, addr, count, byteData, priority) ? ESP_OK : ESP_FAIL;
                delete[] byteData;
            }
            break;
        case 0x05:
            if (data) {
                result = rtu->writeSingleCoilWithPriority(serverAddress, addr, data[0] != 0, priority) ? ESP_OK : ESP_FAIL;
            }
            break;
        case 0x0F:
            if (data && count > 0) {
                bool* boolData = new (std::nothrow) bool[count];
                if (!boolData) {
                    MODBUSD_LOG_E("Failed to allocate %u bools for FC15", count);
                    result = ESP_ERR_NO_MEM;
                    break;
                }
                for (uint16_t i = 0; i < count; i++) {
                    uint16_t wordIndex = i / 16;
                    uint16_t bitIndex = i % 16;
                    boolData[i] = (data[wordIndex] & (1 << bitIndex)) != 0;
                }
                result = rtu->writeMultipleCoilsWithPriority(serverAddress, addr, count, boolData, priority) ? ESP_OK : ESP_FAIL;
                delete[] boolData;
            }
            break;
    }

    totalRequests++;
    if (result != ESP_OK) {
        lastError = ModbusError::COMMUNICATION_ERROR;
    }

    return result;
}

// Wait for response
ModbusResult<std::vector<uint8_t>> ModbusDevice::waitForResponse(TickType_t timeout) {
    if (!syncContext || !syncContext->semaphore) {
        return ModbusResult<std::vector<uint8_t>>::error(ModbusError::NOT_INITIALIZED);
    }
    
    // Clear previous state
    syncContext->responseReceived = false;
    syncContext->errorOccurred = false;
    syncContext->responseData.clear();
    
    // Wait for response
    if (xSemaphoreTake(syncContext->semaphore, timeout) == pdTRUE) {
        if (syncContext->errorOccurred) {
            return ModbusResult<std::vector<uint8_t>>::error(syncContext->error);
        }
        if (syncContext->responseReceived) {
            successfulRequests++;
            return ModbusResult<std::vector<uint8_t>>::ok(syncContext->responseData);
        }
    }
    
    timeouts++;
    lastError = ModbusError::TIMEOUT;
    return ModbusResult<std::vector<uint8_t>>::error(ModbusError::TIMEOUT);
}

// Read holding registers
ModbusResult<std::vector<uint16_t>> ModbusDevice::readHoldingRegisters(uint16_t address, uint16_t count) {
    // Default to RELAY priority for backward compatibility
    return readHoldingRegistersWithPriority(address, count, esp32Modbus::RELAY);
}

ModbusResult<std::vector<uint16_t>> ModbusDevice::readHoldingRegistersWithPriority(uint16_t address, uint16_t count, esp32Modbus::ModbusPriority priority) {
    if (count == 0 || count > MODBUS_MAX_REGISTER_COUNT) {
        return ModbusResult<std::vector<uint16_t>>::error(ModbusError::INVALID_PARAMETER);
    }

    // Acquire bus mutex for entire transaction (request + response)
    if (!acquireBusMutex(MODBUS_MUTEX_TIMEOUT_MS)) {
        MODBUSD_LOG_W("Failed to acquire bus mutex for readHoldingRegisters");
        return ModbusResult<std::vector<uint16_t>>::error(ModbusError::MUTEX_ERROR);
    }

    ensureSyncReady();

    if (sendRequestWithPriority(0x03, address, count, priority) != ESP_OK) {
        releaseBusMutex();
        return ModbusResult<std::vector<uint16_t>>::error(ModbusError::COMMUNICATION_ERROR);
    }

    auto result = waitForResponse();

    releaseBusMutex();  // Release after response received

    if (!result.isOk()) {
        return ModbusResult<std::vector<uint16_t>>::error(result.error());
    }

    // Convert bytes to uint16_t
    std::vector<uint16_t> values;
    const auto& data = result.value();
    for (size_t i = 0; i + 1 < data.size(); i += 2) {
        values.push_back((static_cast<uint16_t>(data[i]) << 8) | data[i + 1]);
    }

    return ModbusResult<std::vector<uint16_t>>::ok(values);
}

// Read input registers
ModbusResult<std::vector<uint16_t>> ModbusDevice::readInputRegisters(uint16_t address, uint16_t count) {
    // Default to RELAY priority for backward compatibility
    return readInputRegistersWithPriority(address, count, esp32Modbus::RELAY);
}

ModbusResult<std::vector<uint16_t>> ModbusDevice::readInputRegistersWithPriority(uint16_t address, uint16_t count, esp32Modbus::ModbusPriority priority) {
    if (count == 0 || count > MODBUS_MAX_REGISTER_COUNT) {
        return ModbusResult<std::vector<uint16_t>>::error(ModbusError::INVALID_PARAMETER);
    }

    // Acquire bus mutex for entire transaction (request + response)
    if (!acquireBusMutex(MODBUS_MUTEX_TIMEOUT_MS)) {
        MODBUSD_LOG_W("Failed to acquire bus mutex for readInputRegisters");
        return ModbusResult<std::vector<uint16_t>>::error(ModbusError::MUTEX_ERROR);
    }

    ensureSyncReady();

    if (sendRequestWithPriority(0x04, address, count, priority) != ESP_OK) {
        releaseBusMutex();
        return ModbusResult<std::vector<uint16_t>>::error(ModbusError::COMMUNICATION_ERROR);
    }

    auto result = waitForResponse();

    releaseBusMutex();  // Release after response received

    if (!result.isOk()) {
        return ModbusResult<std::vector<uint16_t>>::error(result.error());
    }

    // Convert bytes to uint16_t
    std::vector<uint16_t> values;
    const auto& data = result.value();
    for (size_t i = 0; i + 1 < data.size(); i += 2) {
        values.push_back((static_cast<uint16_t>(data[i]) << 8) | data[i + 1]);
    }

    return ModbusResult<std::vector<uint16_t>>::ok(values);
}

// Write single register
ModbusResult<void> ModbusDevice::writeSingleRegister(uint16_t address, uint16_t value) {
    // Default to RELAY priority for backward compatibility
    return writeSingleRegisterWithPriority(address, value, esp32Modbus::RELAY);
}

ModbusResult<void> ModbusDevice::writeSingleRegisterWithPriority(uint16_t address, uint16_t value, esp32Modbus::ModbusPriority priority) {
    // Acquire bus mutex for entire transaction (request + response)
    if (!acquireBusMutex(MODBUS_MUTEX_TIMEOUT_MS)) {
        MODBUSD_LOG_W("Failed to acquire bus mutex for writeSingleRegister");
        return ModbusResult<void>::error(ModbusError::MUTEX_ERROR);
    }

    ensureSyncReady();

    uint16_t data = value;
    if (sendRequestWithPriority(0x06, address, 1, priority, &data) != ESP_OK) {
        releaseBusMutex();
        return ModbusResult<void>::error(ModbusError::COMMUNICATION_ERROR);
    }

    auto result = waitForResponse();

    releaseBusMutex();  // Release after response received

    if (!result.isOk()) {
        return ModbusResult<void>::error(result.error());
    }

    return ModbusResult<void>::ok();
}

// Write multiple registers
ModbusResult<void> ModbusDevice::writeMultipleRegisters(uint16_t address, const std::vector<uint16_t>& values) {
    if (values.empty() || values.size() > MODBUS_MAX_WRITE_REGISTER_COUNT) {
        return ModbusResult<void>::error(ModbusError::INVALID_PARAMETER);
    }

    // Acquire bus mutex for entire transaction (request + response)
    if (!acquireBusMutex(MODBUS_MUTEX_TIMEOUT_MS)) {
        MODBUSD_LOG_W("Failed to acquire bus mutex for writeMultipleRegisters");
        return ModbusResult<void>::error(ModbusError::MUTEX_ERROR);
    }

    ensureSyncReady();

    uint16_t* data = const_cast<uint16_t*>(values.data());
    if (sendRequestInternal(0x10, address, static_cast<uint16_t>(values.size()), data) != ESP_OK) {
        releaseBusMutex();
        return ModbusResult<void>::error(ModbusError::COMMUNICATION_ERROR);
    }

    auto result = waitForResponse();

    releaseBusMutex();  // Release after response received

    if (!result.isOk()) {
        return ModbusResult<void>::error(result.error());
    }

    return ModbusResult<void>::ok();
}

// Read coils
ModbusResult<std::vector<bool>> ModbusDevice::readCoils(uint16_t address, uint16_t count) {
    if (count == 0 || count > MODBUS_MAX_COIL_COUNT) {
        return ModbusResult<std::vector<bool>>::error(ModbusError::INVALID_PARAMETER);
    }

    // Acquire bus mutex for entire transaction (request + response)
    if (!acquireBusMutex(MODBUS_MUTEX_TIMEOUT_MS)) {
        MODBUSD_LOG_W("Failed to acquire bus mutex for readCoils");
        return ModbusResult<std::vector<bool>>::error(ModbusError::MUTEX_ERROR);
    }

    ensureSyncReady();

    if (sendRequestInternal(0x01, address, count) != ESP_OK) {
        releaseBusMutex();
        return ModbusResult<std::vector<bool>>::error(ModbusError::COMMUNICATION_ERROR);
    }

    auto result = waitForResponse();

    releaseBusMutex();  // Release after response received

    if (!result.isOk()) {
        return ModbusResult<std::vector<bool>>::error(result.error());
    }

    // Convert bytes to bool vector
    std::vector<bool> values;
    const auto& data = result.value();
    for (uint16_t i = 0; i < count && i / 8 < data.size(); i++) {
        values.push_back((data[i / 8] & (1 << (i % 8))) != 0);
    }

    return ModbusResult<std::vector<bool>>::ok(values);
}

// Read discrete inputs
ModbusResult<std::vector<bool>> ModbusDevice::readDiscreteInputs(uint16_t address, uint16_t count) {
    if (count == 0 || count > MODBUS_MAX_COIL_COUNT) {
        return ModbusResult<std::vector<bool>>::error(ModbusError::INVALID_PARAMETER);
    }

    // Acquire bus mutex for entire transaction (request + response)
    if (!acquireBusMutex(MODBUS_MUTEX_TIMEOUT_MS)) {
        MODBUSD_LOG_W("Failed to acquire bus mutex for readDiscreteInputs");
        return ModbusResult<std::vector<bool>>::error(ModbusError::MUTEX_ERROR);
    }

    ensureSyncReady();

    if (sendRequestInternal(0x02, address, count) != ESP_OK) {
        releaseBusMutex();
        return ModbusResult<std::vector<bool>>::error(ModbusError::COMMUNICATION_ERROR);
    }

    auto result = waitForResponse();

    releaseBusMutex();  // Release after response received

    if (!result.isOk()) {
        return ModbusResult<std::vector<bool>>::error(result.error());
    }

    // Convert bytes to bool vector
    std::vector<bool> values;
    const auto& data = result.value();
    for (uint16_t i = 0; i < count && i / 8 < data.size(); i++) {
        values.push_back((data[i / 8] & (1 << (i % 8))) != 0);
    }

    return ModbusResult<std::vector<bool>>::ok(values);
}

// Write single coil
ModbusResult<void> ModbusDevice::writeSingleCoil(uint16_t address, bool value) {
    // Default to RELAY priority for backward compatibility
    return writeSingleCoilWithPriority(address, value, esp32Modbus::RELAY);
}

ModbusResult<void> ModbusDevice::writeSingleCoilWithPriority(uint16_t address, bool value, esp32Modbus::ModbusPriority priority) {
    // Acquire bus mutex for entire transaction (request + response)
    if (!acquireBusMutex(MODBUS_MUTEX_TIMEOUT_MS)) {
        MODBUSD_LOG_W("Failed to acquire bus mutex for writeSingleCoil");
        return ModbusResult<void>::error(ModbusError::MUTEX_ERROR);
    }

    ensureSyncReady();

    uint16_t data = value ? 1 : 0;
    if (sendRequestWithPriority(0x05, address, 1, priority, &data) != ESP_OK) {
        releaseBusMutex();
        return ModbusResult<void>::error(ModbusError::COMMUNICATION_ERROR);
    }

    auto result = waitForResponse();

    releaseBusMutex();  // Release after response received

    if (!result.isOk()) {
        return ModbusResult<void>::error(result.error());
    }

    return ModbusResult<void>::ok();
}

// Write multiple coils
ModbusResult<void> ModbusDevice::writeMultipleCoils(uint16_t address, const std::vector<bool>& values) {
    if (values.empty() || values.size() > MODBUS_MAX_WRITE_COIL_COUNT) {
        return ModbusResult<void>::error(ModbusError::INVALID_PARAMETER);
    }

    // Acquire bus mutex for entire transaction (request + response)
    if (!acquireBusMutex(MODBUS_MUTEX_TIMEOUT_MS)) {
        MODBUSD_LOG_W("Failed to acquire bus mutex for writeMultipleCoils");
        return ModbusResult<void>::error(ModbusError::MUTEX_ERROR);
    }

    ensureSyncReady();

    // Pack bool vector into uint16_t array
    size_t wordCount = (values.size() + 15) / 16;
    std::vector<uint16_t> packedData(wordCount, 0);

    for (size_t i = 0; i < values.size(); i++) {
        if (values[i]) {
            size_t wordIndex = i / 16;
            size_t bitIndex = i % 16;
            packedData[wordIndex] |= (1 << bitIndex);
        }
    }

    if (sendRequestInternal(0x0F, address, static_cast<uint16_t>(values.size()), packedData.data()) != ESP_OK) {
        releaseBusMutex();
        return ModbusResult<void>::error(ModbusError::COMMUNICATION_ERROR);
    }

    auto result = waitForResponse();

    releaseBusMutex();  // Release after response received

    if (!result.isOk()) {
        return ModbusResult<void>::error(result.error());
    }

    return ModbusResult<void>::ok();
}

// Get statistics
ModbusDevice::Statistics ModbusDevice::getStatistics() const {
    Statistics stats;
    stats.totalRequests = totalRequests.load();
    stats.successfulRequests = successfulRequests.load();
    stats.failedRequests = stats.totalRequests - stats.successfulRequests;
    stats.timeouts = timeouts.load();
    stats.crcErrors = crcErrors.load();
    return stats;
}

// Reset statistics
void ModbusDevice::resetStatistics() {
    totalRequests = 0;
    successfulRequests = 0;
    timeouts = 0;
    crcErrors = 0;
}

// Handle Modbus response
void ModbusDevice::handleModbusResponse(uint8_t functionCode, uint16_t address,
                                        const uint8_t* data, size_t length) {
    // During CONFIGURING phase, this is expected and normal
    if (initPhase == InitPhase::CONFIGURING) {
        MODBUSD_LOG_D("Response during config phase: FC=%02X, Addr=%04X", functionCode, address);
    }
    
    // Override in derived classes for custom handling
}

// Handle Modbus error
void ModbusDevice::handleModbusError(ModbusError error) {
    lastError = error;
    
    // Log significant errors
    if (error != ModbusError::SUCCESS) {
        MODBUSD_LOG_W("Modbus error for device %d: %s", serverAddress, getModbusErrorString(error));
    }
    
    // Override in derived classes for custom handling
}

// Internal data handler
void ModbusDevice::handleData(uint8_t serverAddress, esp32Modbus::FunctionCode fc,
                               uint16_t startingAddress, const uint8_t* data, size_t length) {
    // Check for sync response
    if (syncContext && syncMutex) {
        if (xSemaphoreTake(syncMutex, 0) == pdTRUE) {
            if (!syncContext->responseReceived && data != nullptr) {
                // Check if this is a write operation
                bool isWriteOp = (fc == esp32Modbus::WRITE_HOLD_REGISTER || 
                                  fc == esp32Modbus::WRITE_MULT_REGISTERS ||
                                  fc == esp32Modbus::WRITE_COIL ||
                                  fc == esp32Modbus::WRITE_MULT_COILS);
                
                // Accept empty responses for write operations
                if (length > 0 || isWriteOp) {
                    syncContext->responseData.clear();
                    if (length > 0) {
                        syncContext->responseData.insert(syncContext->responseData.end(), data, data + length);
                    }
                    syncContext->responseReceived = true;
                    xSemaphoreGive(syncContext->semaphore);
                }
            }
            xSemaphoreGive(syncMutex);
        }
    }
    
    // Call virtual handler
    handleModbusResponse(static_cast<uint8_t>(fc), startingAddress, data, length);
}

// Internal error handler
void ModbusDevice::handleError(esp32Modbus::Error error) {
    ModbusError modbusError = mapError(error);
    
    // Update sync context if waiting
    if (syncContext && syncMutex) {
        if (xSemaphoreTake(syncMutex, 0) == pdTRUE) {
            if (!syncContext->responseReceived && !syncContext->errorOccurred) {
                syncContext->error = modbusError;
                syncContext->errorOccurred = true;
                xSemaphoreGive(syncContext->semaphore);
            }
            xSemaphoreGive(syncMutex);
        }
    }
    
    // Update statistics
    if (modbusError == ModbusError::CRC_ERROR) {
        crcErrors++;
    }
    
    // Call virtual handler
    handleModbusError(modbusError);
}

// Map error codes
ModbusError ModbusDevice::mapError(esp32Modbus::Error error) {
    switch(error) {
        case esp32Modbus::SUCCESS: return ModbusError::SUCCESS;
        case esp32Modbus::TIMEOUT: return ModbusError::TIMEOUT;
        case esp32Modbus::CRC_ERROR: return ModbusError::CRC_ERROR;
        case esp32Modbus::INVALID_RESPONSE: return ModbusError::INVALID_RESPONSE;
        case esp32Modbus::QUEUE_FULL: return ModbusError::QUEUE_FULL;
        case esp32Modbus::MEMORY_ALLOCATION_FAILED: return ModbusError::RESOURCE_ERROR;
        case esp32Modbus::ILLEGAL_FUNCTION: return ModbusError::ILLEGAL_FUNCTION;
        case esp32Modbus::ILLEGAL_DATA_ADDRESS: return ModbusError::ILLEGAL_DATA_ADDRESS;
        case esp32Modbus::ILLEGAL_DATA_VALUE: return ModbusError::ILLEGAL_DATA_VALUE;
        case esp32Modbus::SERVER_DEVICE_FAILURE: return ModbusError::SLAVE_DEVICE_FAILURE;
        case esp32Modbus::INVALID_SLAVE:
        case esp32Modbus::INVALID_FUNCTION:
        case esp32Modbus::INVALID_PARAMETER: return ModbusError::INVALID_PARAMETER;
        case esp32Modbus::COMM_ERROR:
        default: return ModbusError::COMMUNICATION_ERROR;
    }
}

} // namespace modbus

// Global callback functions (outside namespace)
void mainHandleData(uint8_t serverAddress, esp32Modbus::FunctionCode fc,
                    uint16_t startingAddress, const uint8_t* data, size_t length) {
    auto* device = modbus::ModbusRegistry::getInstance().getDevice(serverAddress);
    if (device) {
        device->handleData(serverAddress, fc, startingAddress, data, length);
    }
}

void handleError(uint8_t serverAddress, esp32Modbus::Error error) {
    auto* device = modbus::ModbusRegistry::getInstance().getDevice(serverAddress);
    if (device) {
        device->handleError(error);
    }
}

// Helper function (outside namespace for global access)
const char* getModbusErrorString(modbus::ModbusError error) {
    using namespace modbus;
    switch(error) {
        case ModbusError::SUCCESS: return "Success";
        case ModbusError::ILLEGAL_FUNCTION: return "Illegal function";
        case ModbusError::ILLEGAL_DATA_ADDRESS: return "Illegal data address";
        case ModbusError::ILLEGAL_DATA_VALUE: return "Illegal data value";
        case ModbusError::SLAVE_DEVICE_FAILURE: return "Slave device failure";
        case ModbusError::TIMEOUT: return "Timeout";
        case ModbusError::CRC_ERROR: return "CRC error";
        case ModbusError::INVALID_RESPONSE: return "Invalid response";
        case ModbusError::QUEUE_FULL: return "Queue full";
        case ModbusError::NOT_INITIALIZED: return "Not initialized";
        case ModbusError::COMMUNICATION_ERROR: return "Communication error";
        case ModbusError::INVALID_PARAMETER: return "Invalid parameter";
        case ModbusError::RESOURCE_ERROR: return "Resource error";
        case ModbusError::NULL_POINTER: return "Null pointer";
        case ModbusError::NOT_SUPPORTED: return "Not supported";
        case ModbusError::MUTEX_ERROR: return "Mutex error";
        case ModbusError::INVALID_DATA_LENGTH: return "Invalid data length";
        case ModbusError::DEVICE_NOT_FOUND: return "Device not found";
        case ModbusError::RESOURCE_CREATION_FAILED: return "Resource creation failed";
        case ModbusError::INVALID_ADDRESS: return "Invalid address";
        default: return "Unknown error";
    }
}

// Set initialization phase with event group notification
void modbus::ModbusDevice::setInitPhase(InitPhase phase) {
    InitPhase oldPhase = initPhase.exchange(phase);
    
    // Skip if no change
    if (oldPhase == phase) return;
    
    // Notify via event group if configured
    if (eventGroup) {
        if (phase == InitPhase::READY && readyBit) {
            xEventGroupSetBits(eventGroup, readyBit);
            MODBUSD_LOG_D("Device %d set ready bit 0x%X", serverAddress, readyBit);
        } else if (phase == InitPhase::ERROR && errorBit) {
            xEventGroupSetBits(eventGroup, errorBit);
            MODBUSD_LOG_D("Device %d set error bit 0x%X", serverAddress, errorBit);
        }
    }
}

// Set event group for initialization notification
void modbus::ModbusDevice::setEventGroup(EventGroupHandle_t group, EventBits_t ready, EventBits_t error) {
    eventGroup = group;
    readyBit = ready;
    errorBit = error;
    
    // If already initialized, set bits immediately
    InitPhase currentPhase = getInitPhase();
    if (eventGroup) {
        if (currentPhase == InitPhase::READY && readyBit) {
            xEventGroupSetBits(eventGroup, readyBit);
        } else if (currentPhase == InitPhase::ERROR && errorBit) {
            xEventGroupSetBits(eventGroup, errorBit);
        }
    }
}