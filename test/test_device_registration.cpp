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
        INVALID_RESPONSE = 2
    };
}

// Include the source files for testing
#include "../src/ModbusRegistry.cpp"
#include "../src/ModbusDevice.cpp"

using namespace modbus;

// Test implementation of ModbusDevice
class TestModbusDevice : public ModbusDevice {
public:
    bool handleDataCalled = false;
    bool handleErrorCalled = false;
    uint8_t lastServerAddress = 0;
    esp32Modbus::FunctionCode lastFunctionCode;

    TestModbusDevice(uint8_t addr = 0x01) : ModbusDevice(addr) {}

protected:
    void handleModbusResponse(uint8_t functionCode, uint16_t address,
                             const uint8_t* data, size_t length) override {
        handleDataCalled = true;
        lastFunctionCode = static_cast<esp32Modbus::FunctionCode>(functionCode);
        ModbusDevice::handleModbusResponse(functionCode, address, data, length);
    }

    void handleModbusError(ModbusError error) override {
        handleErrorCalled = true;
        ModbusDevice::handleModbusError(error);
    }
};

// Helper to get registry instance
ModbusRegistry& registry() {
    return ModbusRegistry::getInstance();
}

// Reset global state before each test
void resetGlobalState() {
    // Unregister all devices from registry
    // Note: We need to iterate through addresses since we can't clear directly
    for (uint8_t addr = 1; addr <= 247; addr++) {
        registry().unregisterDevice(addr);
    }

    // Reset mock flags
    mockMutexShouldFail = false;
    mockMutexCreateShouldFail = false;
}

TEST(DeviceRegistration_Success) {
    resetGlobalState();

    TestModbusDevice device(0x01);
    uint8_t address = 0x01;

    ModbusError result = device.registerDevice();
    ASSERT_EQ(ModbusError::SUCCESS, result);

    // Verify device is in the registry
    ASSERT_EQ(1, registry().getDeviceCount());
    ASSERT_EQ(&device, registry().getDevice(address));
}

TEST(DeviceRegistration_NullDevice) {
    resetGlobalState();

    uint8_t address = 0x01;

    // Register null device directly via registry
    bool result = registry().registerDevice(address, nullptr);
    ASSERT_FALSE(result);

    // Verify device was not added
    ASSERT_EQ(0, registry().getDeviceCount());
}

TEST(DeviceUnregistration_Success) {
    resetGlobalState();

    TestModbusDevice device(0x01);
    uint8_t address = 0x01;

    // First register the device
    device.registerDevice();
    ASSERT_EQ(1, registry().getDeviceCount());

    // Now unregister it
    ModbusError result = device.unregisterDevice();
    ASSERT_EQ(ModbusError::SUCCESS, result);

    // Verify device was removed
    ASSERT_EQ(0, registry().getDeviceCount());
}

TEST(DeviceUnregistration_NotFound) {
    resetGlobalState();

    uint8_t address = 0x99;

    // Unregister non-existent device
    bool result = registry().unregisterDevice(address);
    ASSERT_FALSE(result);
}

TEST(DeviceRegistration_MultipleDevices) {
    resetGlobalState();

    TestModbusDevice device1(0x01);
    TestModbusDevice device2(0x02);
    TestModbusDevice device3(0x03);

    ASSERT_EQ(ModbusError::SUCCESS, device1.registerDevice());
    ASSERT_EQ(ModbusError::SUCCESS, device2.registerDevice());
    ASSERT_EQ(ModbusError::SUCCESS, device3.registerDevice());

    ASSERT_EQ(3, registry().getDeviceCount());
    ASSERT_EQ(&device1, registry().getDevice(0x01));
    ASSERT_EQ(&device2, registry().getDevice(0x02));
    ASSERT_EQ(&device3, registry().getDevice(0x03));
}

TEST(DeviceRegistration_ReplaceExisting) {
    resetGlobalState();

    TestModbusDevice device1(0x01);
    TestModbusDevice device2(0x01);  // Same address

    // Register first device
    ASSERT_EQ(ModbusError::SUCCESS, device1.registerDevice());
    ASSERT_EQ(&device1, registry().getDevice(0x01));

    // Register second device with same address (should replace)
    ASSERT_TRUE(registry().registerDevice(0x01, &device2));
    ASSERT_EQ(&device2, registry().getDevice(0x01));
    ASSERT_EQ(1, registry().getDeviceCount());
}

TEST(DeviceRegistration_HasDevice) {
    resetGlobalState();

    TestModbusDevice device(0x05);

    ASSERT_FALSE(registry().hasDevice(0x05));

    device.registerDevice();

    ASSERT_TRUE(registry().hasDevice(0x05));
    ASSERT_FALSE(registry().hasDevice(0x06));
}

TEST(DeviceRegistration_InvalidAddress) {
    resetGlobalState();

    // Address 0 is invalid for Modbus
    TestModbusDevice device(0);

    // ModbusDevice constructor should have adjusted to default address
    ASSERT_EQ(1, device.getServerAddress());
}
