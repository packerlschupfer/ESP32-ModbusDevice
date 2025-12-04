#include "test_framework.h"
#include "mock_freertos.h"
#include "mock_logger.h"

// Mock esp32Modbus types
namespace esp32Modbus {
    enum FunctionCode {
        READ_COILS = 0x01,
        READ_INPUT_REGISTERS = 0x04,
        WRITE_SINGLE_COIL = 0x05,
        WRITE_SINGLE_REGISTER = 0x06
    };

    enum Error {
        SUCCESS = 0,
        TIMEOUT = 1,
        INVALID_RESPONSE = 2,
        CRC_ERROR = 3
    };
}

// Mock global ModbusRTU
esp32ModbusRTU* globalModbusRTU = nullptr;

// Include the source files for testing
#include "../src/ModbusRegistry.cpp"
#include "../src/ModbusDevice.cpp"

using namespace modbus;

// Helper to get registry instance
ModbusRegistry& registry() {
    return ModbusRegistry::getInstance();
}

// Test implementation of ModbusDevice
class TestModbusDevice : public ModbusDevice {
public:
    bool handleResponseCalled = false;
    bool handleErrorCalled = false;
    uint8_t lastFunctionCode = 0;
    uint16_t lastAddress = 0;
    ModbusError lastError = ModbusError::SUCCESS;

    TestModbusDevice(uint8_t addr) : ModbusDevice(addr) {}

protected:
    void handleModbusResponse(uint8_t functionCode, uint16_t address,
                            const uint8_t* data, size_t length) override {
        handleResponseCalled = true;
        lastFunctionCode = functionCode;
        lastAddress = address;
        ModbusDevice::handleModbusResponse(functionCode, address, data, length);
    }

    void handleModbusError(ModbusError error) override {
        handleErrorCalled = true;
        lastError = error;
        ModbusDevice::handleModbusError(error);
    }
};

// Reset global state before each test
void resetGlobalState() {
    // Unregister all devices from registry
    for (uint8_t addr = 1; addr <= 247; addr++) {
        registry().unregisterDevice(addr);
    }

    // Reset mock flags
    mockMutexShouldFail = false;
    mockMutexCreateShouldFail = false;
}

TEST(ModbusDevice_Construction) {
    resetGlobalState();

    TestModbusDevice device(0x01);

    ASSERT_EQ(0x01, device.getServerAddress());
    ASSERT_EQ(ModbusDevice::InitPhase::UNINITIALIZED, device.getInitPhase());
    ASSERT_FALSE(device.isConnected());
}

TEST(ModbusDevice_InvalidAddress) {
    resetGlobalState();

    // Test address 0 (invalid)
    TestModbusDevice device1(0);
    ASSERT_EQ(1, device1.getServerAddress()); // Should default to 1

    // Test address > 247 (invalid)
    TestModbusDevice device2(250);
    ASSERT_EQ(1, device2.getServerAddress()); // Should default to 1
}

TEST(ModbusDevice_RegisterDevice) {
    resetGlobalState();

    TestModbusDevice device(0x05);

    ModbusError result = device.registerDevice();
    ASSERT_EQ(ModbusError::SUCCESS, result);

    // Verify device is in the registry
    ASSERT_EQ(1, registry().getDeviceCount());
    ASSERT_EQ(&device, registry().getDevice(0x05));
}

TEST(ModbusDevice_UnregisterDevice) {
    resetGlobalState();

    TestModbusDevice device(0x05);

    // First register
    device.registerDevice();
    ASSERT_EQ(1, registry().getDeviceCount());

    // Then unregister
    ModbusError result = device.unregisterDevice();
    ASSERT_EQ(ModbusError::SUCCESS, result);
    ASSERT_EQ(0, registry().getDeviceCount());
}

TEST(ModbusDevice_InitPhases) {
    resetGlobalState();

    TestModbusDevice device(0x01);

    // Check initial phase
    ASSERT_EQ(ModbusDevice::InitPhase::UNINITIALIZED, device.getInitPhase());
    ASSERT_FALSE(device.isConnected());

    // Move to CONFIGURING
    device.setInitPhase(ModbusDevice::InitPhase::CONFIGURING);
    ASSERT_EQ(ModbusDevice::InitPhase::CONFIGURING, device.getInitPhase());
    ASSERT_FALSE(device.isConnected());

    // Move to READY
    device.setInitPhase(ModbusDevice::InitPhase::READY);
    ASSERT_EQ(ModbusDevice::InitPhase::READY, device.getInitPhase());
    ASSERT_TRUE(device.isConnected()); // Should be connected when READY with no errors

    // Move to ERROR
    device.setInitPhase(ModbusDevice::InitPhase::ERROR);
    ASSERT_EQ(ModbusDevice::InitPhase::ERROR, device.getInitPhase());
    ASSERT_FALSE(device.isConnected());
}

TEST(ModbusDevice_SetServerAddress) {
    resetGlobalState();

    TestModbusDevice device(0x01);
    device.registerDevice();

    // Change to valid address
    auto result = device.setServerAddress(0x10);
    ASSERT_TRUE(result.isOk());
    ASSERT_EQ(0x10, device.getServerAddress());

    // Device should be re-registered at new address
    ASSERT_EQ(nullptr, registry().getDevice(0x01));
    ASSERT_EQ(&device, registry().getDevice(0x10));

    // Try invalid address
    result = device.setServerAddress(0);
    ASSERT_FALSE(result.isOk());
    ASSERT_EQ(ModbusError::INVALID_ADDRESS, result.error());
    ASSERT_EQ(0x10, device.getServerAddress()); // Should not change
}

TEST(ModbusDevice_Statistics) {
    resetGlobalState();

    TestModbusDevice device(0x01);

    // Check initial statistics
    auto stats = device.getStatistics();
    ASSERT_EQ(0, stats.totalRequests);
    ASSERT_EQ(0, stats.successfulRequests);
    ASSERT_EQ(0, stats.failedRequests);
    ASSERT_EQ(0, stats.timeouts);
    ASSERT_EQ(0, stats.crcErrors);

    // Reset statistics
    device.resetStatistics();
    stats = device.getStatistics();
    ASSERT_EQ(0, stats.totalRequests);
}

TEST(ModbusDevice_HandleResponse_DuringConfig) {
    resetGlobalState();

    TestModbusDevice device(0x01);
    device.registerDevice();
    device.setInitPhase(ModbusDevice::InitPhase::CONFIGURING);

    // During CONFIGURING phase, responses should be handled without warnings
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    mainHandleData(0x01, esp32Modbus::READ_INPUT_REGISTERS, 0x1000, data, sizeof(data));

    ASSERT_TRUE(device.handleResponseCalled);
    ASSERT_EQ(0x04, device.lastFunctionCode); // READ_INPUT_REGISTERS
    ASSERT_EQ(0x1000, device.lastAddress);
}

TEST(ModbusDevice_HandleError) {
    resetGlobalState();

    TestModbusDevice device(0x01);
    device.registerDevice();

    // Trigger error handling
    handleError(0x01, esp32Modbus::TIMEOUT);

    ASSERT_TRUE(device.handleErrorCalled);
    ASSERT_EQ(ModbusError::TIMEOUT, device.lastError);
    ASSERT_EQ(ModbusError::TIMEOUT, device.getLastError());
}

TEST(ModbusDevice_MapError) {
    resetGlobalState();

    TestModbusDevice device(0x01);

    // Test error mapping via error handler
    device.registerDevice();

    handleError(0x01, esp32Modbus::CRC_ERROR);
    ASSERT_EQ(ModbusError::CRC_ERROR, device.getLastError());

    handleError(0x01, esp32Modbus::TIMEOUT);
    ASSERT_EQ(ModbusError::TIMEOUT, device.getLastError());

    handleError(0x01, esp32Modbus::INVALID_RESPONSE);
    ASSERT_EQ(ModbusError::INVALID_RESPONSE, device.getLastError());
}
