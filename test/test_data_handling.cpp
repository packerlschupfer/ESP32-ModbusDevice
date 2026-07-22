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

// Include the source files for testing
#include "../src/ModbusRegistry.cpp"
#include "../src/ModbusDevice.cpp"

using namespace modbus;

// Helper to get registry instance
ModbusRegistry& registry() {
    return ModbusRegistry::getInstance();
}

// Test device implementation
class DataTestDevice : public ModbusDevice {
public:
    bool handleDataCalled = false;
    bool handleErrorCalled = false;
    const uint8_t* receivedData = nullptr;
    size_t receivedLength = 0;
    ModbusError lastError = ModbusError::SUCCESS;

    DataTestDevice(uint8_t addr = 0x01) : ModbusDevice(addr) {}

protected:
    void handleModbusResponse(uint8_t functionCode, uint16_t address,
                             const uint8_t* data, size_t length) override {
        handleDataCalled = true;
        receivedData = data;
        receivedLength = length;
        ModbusDevice::handleModbusResponse(functionCode, address, data, length);
    }

    void handleModbusError(ModbusError error) override {
        handleErrorCalled = true;
        lastError = error;
        ModbusDevice::handleModbusError(error);
    }
};

// Helper to reset global state
void resetTestState() {
    // Unregister all devices from registry
    for (uint8_t addr = 1; addr <= 247; addr++) {
        registry().unregisterDevice(addr);
    }
    mockMutexShouldFail = false;
    mockMutexCreateShouldFail = false;
}

TEST(DataHandling_ValidData) {
    resetTestState();

    DataTestDevice device(0x01);
    uint8_t address = 0x01;
    device.registerDevice();

    uint8_t testData[] = {0x01, 0x02, 0x03, 0x04};
    mainHandleData(address, esp32Modbus::READ_INPUT_REGISTERS, 0x100, testData, sizeof(testData));

    ASSERT_TRUE(device.handleDataCalled);
    ASSERT_EQ(testData, device.receivedData);
    ASSERT_EQ(sizeof(testData), device.receivedLength);
}

TEST(DataHandling_NullDataWithZeroLength) {
    resetTestState();

    DataTestDevice device(0x01);
    uint8_t address = 0x01;
    device.registerDevice();

    // This should be valid - null data with zero length
    mainHandleData(address, esp32Modbus::READ_INPUT_REGISTERS, 0x100, nullptr, 0);

    ASSERT_TRUE(device.handleDataCalled);
    ASSERT_NULL(device.receivedData);
    ASSERT_EQ(0, device.receivedLength);
}

TEST(DataHandling_NullDataWithNonZeroLength) {
    resetTestState();

    DataTestDevice device(0x01);
    uint8_t address = 0x01;
    device.registerDevice();

    // This should fail validation
    mainHandleData(address, esp32Modbus::READ_INPUT_REGISTERS, 0x100, nullptr, 10);

    ASSERT_FALSE(device.handleDataCalled);
}

TEST(DataHandling_ExcessiveDataLength) {
    resetTestState();

    DataTestDevice device(0x01);
    uint8_t address = 0x01;
    device.registerDevice();

    uint8_t testData[300];  // Exceeds MAX_MODBUS_DATA_LENGTH (252)
    mainHandleData(address, esp32Modbus::READ_INPUT_REGISTERS, 0x100, testData, sizeof(testData));

    ASSERT_FALSE(device.handleDataCalled);
}

TEST(DataHandling_DeviceNotFound) {
    resetTestState();

    uint8_t testData[] = {0x01, 0x02};
    uint8_t address = 0x99;  // Not registered

    // Should not crash, just log error
    mainHandleData(address, esp32Modbus::READ_INPUT_REGISTERS, 0x100, testData, sizeof(testData));
}

TEST(ErrorHandling_ValidError) {
    resetTestState();

    DataTestDevice device(0x01);
    uint8_t address = 0x01;
    device.registerDevice();

    handleError(address, esp32Modbus::TIMEOUT);

    ASSERT_TRUE(device.handleErrorCalled);
    ASSERT_EQ(ModbusError::TIMEOUT, device.lastError);
}

TEST(ErrorHandling_DeviceNotFound) {
    resetTestState();

    uint8_t address = 0x99;  // Not registered

    // Should not crash, just log error
    handleError(address, esp32Modbus::TIMEOUT);
}

TEST(Callback_InvokeWithValidData) {
    resetTestState();

    DataTestDevice device(0x01);
    bool callbackInvoked = false;
    uint8_t receivedFunctionCode = 0;

    device.registerModbusResponseCallback([&](uint8_t functionCode, const uint8_t* data, uint16_t length) {
        callbackInvoked = true;
        receivedFunctionCode = functionCode;
    });

    ASSERT_TRUE(device.hasModbusResponseCallback());

    uint8_t testData[] = {0x01, 0x02};
    device.invokeModbusResponseCallback(0x04, testData, sizeof(testData));

    ASSERT_TRUE(callbackInvoked);
    ASSERT_EQ(0x04, receivedFunctionCode);
}

TEST(Callback_InvokeWithNullDataNonZeroLength) {
    resetTestState();

    DataTestDevice device(0x01);
    bool callbackInvoked = false;

    device.registerModbusResponseCallback([&](uint8_t functionCode, const uint8_t* data, uint16_t length) {
        callbackInvoked = true;
    });

    // Should not invoke callback due to validation failure
    device.invokeModbusResponseCallback(0x04, nullptr, 10);

    ASSERT_FALSE(callbackInvoked);
}

TEST(Registry_GetDevice) {
    resetTestState();

    DataTestDevice device1(0x01);
    DataTestDevice device2(0x05);

    device1.registerDevice();
    device2.registerDevice();

    ASSERT_EQ(&device1, registry().getDevice(0x01));
    ASSERT_EQ(&device2, registry().getDevice(0x05));
    ASSERT_EQ(nullptr, registry().getDevice(0x99)); // Not registered
}

TEST(Registry_HasDevice) {
    resetTestState();

    DataTestDevice device(0x05);
    device.registerDevice();

    ASSERT_TRUE(registry().hasDevice(0x05));
    ASSERT_FALSE(registry().hasDevice(0x01));
}

TEST(Registry_GetDeviceCount) {
    resetTestState();

    ASSERT_EQ(0, registry().getDeviceCount());

    DataTestDevice device1(0x01);
    DataTestDevice device2(0x02);
    DataTestDevice device3(0x03);

    device1.registerDevice();
    ASSERT_EQ(1, registry().getDeviceCount());

    device2.registerDevice();
    ASSERT_EQ(2, registry().getDeviceCount());

    device3.registerDevice();
    ASSERT_EQ(3, registry().getDeviceCount());

    device2.unregisterDevice();
    ASSERT_EQ(2, registry().getDeviceCount());
}
