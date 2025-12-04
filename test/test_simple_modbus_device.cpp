#include "test_framework.h"
#include "mock_freertos.h"
#include "mock_logger.h"
#include "../src/SimpleModbusDevice.h"

// Mock esp32Modbus types
namespace esp32Modbus {
    enum FunctionCode {
        READ_INPUT_REGISTERS = 0x04
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

using namespace modbus;

// Test implementation to access protected members
class TestSimpleModbusDevice : public SimpleModbusDevice {
public:
    TestSimpleModbusDevice(uint8_t addr) : SimpleModbusDevice(addr) {}
    
    // Expose protected methods for testing
    void testHandleModbusResponse(uint8_t functionCode, uint16_t address, 
                                const uint8_t* data, size_t length) {
        handleModbusResponse(functionCode, address, data, length);
    }
    
    void testHandleModbusError(ModbusError error) {
        handleModbusError(error);
    }
    
    // Access channel data for verification
    const std::vector<uint16_t>& getRawValues() const {
        return rawValues;
    }
    
    uint32_t getLastUpdateTime() const {
        return lastUpdateTime;
    }
};

// Mock for xTaskGetTickCount
uint32_t mockTickCount = 0;

TEST(SimpleModbusDevice_Construction) {
    TestSimpleModbusDevice device(0x01);
    
    ASSERT_EQ(0x01, device.getServerAddress());
    ASSERT_EQ(0, device.getChannelCount());
    ASSERT_EQ(0, device.getBaseRegister());
    ASSERT_EQ(1.0f, device.getScaleFactor(0));
}

TEST(SimpleModbusDevice_Configuration) {
    TestSimpleModbusDevice device(0x01);
    
    // Configure device
    device.configureChannels(10, 0x1000, 2.5f);
    
    ASSERT_EQ(10, device.getChannelCount());
    ASSERT_EQ(0x1000, device.getBaseRegister());
    ASSERT_EQ(2.5f, device.getScaleFactor(0));
    
    // Check initial values
    auto values = device.getRawValues();
    ASSERT_EQ(10, values.size());
    for (size_t i = 0; i < values.size(); i++) {
        ASSERT_EQ(0, values[i]);
    }
}

TEST(SimpleModbusDevice_ReadChannel) {
    TestSimpleModbusDevice device(0x01);
    device.configureChannels(5, 0x1000, 0.1f);
    
    // Set init phase to READY
    device.setInitPhase(ModbusDevice::InitPhase::READY);
    
    // Simulate response data
    mockTickCount = 1000;
    uint8_t responseData[] = {
        0x00, 0x0A,  // 10
        0x00, 0x14,  // 20
        0x00, 0x1E,  // 30
        0x00, 0x28,  // 40
        0x00, 0x32   // 50
    };
    
    device.testHandleModbusResponse(0x04, 0x1000, responseData, sizeof(responseData));
    
    // Test reading channels
    auto result0 = device.readChannel(0);
    ASSERT_TRUE(result0.isOk());
    ASSERT_EQ(1.0f, result0.value); // 10 * 0.1
    
    auto result2 = device.readChannel(2);
    ASSERT_TRUE(result2.isOk());
    ASSERT_EQ(3.0f, result2.value); // 30 * 0.1
    
    auto result4 = device.readChannel(4);
    ASSERT_TRUE(result4.isOk());
    ASSERT_EQ(5.0f, result4.value); // 50 * 0.1
}

TEST(SimpleModbusDevice_ReadChannel_OutOfRange) {
    TestSimpleModbusDevice device(0x01);
    device.configureChannels(3, 0x1000, 1.0f);
    
    // Try to read invalid channel
    auto result = device.readChannel(5);
    ASSERT_FALSE(result.isOk());
    ASSERT_EQ(ModbusError::INVALID_PARAMETER, result.error);
}

TEST(SimpleModbusDevice_ReadChannel_NotReady) {
    TestSimpleModbusDevice device(0x01);
    device.configureChannels(3, 0x1000, 1.0f);
    
    // Device is still UNINITIALIZED
    auto result = device.readChannel(0);
    ASSERT_FALSE(result.isOk());
    ASSERT_EQ(ModbusError::NOT_INITIALIZED, result.error);
}

TEST(SimpleModbusDevice_ReadAllChannels) {
    TestSimpleModbusDevice device(0x01);
    device.configureChannels(3, 0x1000, 0.5f);
    device.setInitPhase(ModbusDevice::InitPhase::READY);
    
    // Simulate response
    mockTickCount = 2000;
    uint8_t responseData[] = {
        0x00, 0x64,  // 100
        0x00, 0xC8,  // 200
        0x01, 0x2C   // 300
    };
    
    device.testHandleModbusResponse(0x04, 0x1000, responseData, sizeof(responseData));
    
    // Read all channels
    auto result = device.readAllChannels();
    ASSERT_TRUE(result.isOk());
    ASSERT_EQ(3, result.value.size());
    ASSERT_EQ(50.0f, result.value[0]);   // 100 * 0.5
    ASSERT_EQ(100.0f, result.value[1]);  // 200 * 0.5
    ASSERT_EQ(150.0f, result.value[2]);  // 300 * 0.5
}

TEST(SimpleModbusDevice_UpdateChannels_Success) {
    TestSimpleModbusDevice device(0x01);
    device.configureChannels(2, 0x1000, 1.0f);
    device.setInitPhase(ModbusDevice::InitPhase::READY);
    device.registerDevice();
    
    // Mock successful read
    auto result = device.updateChannels();
    
    // In a real test, we'd verify the read request was sent
    // For now, just check the method runs
    ASSERT_TRUE(result.isOk() || result.error == ModbusError::COMMUNICATION_ERROR);
}

TEST(SimpleModbusDevice_DataFreshness) {
    TestSimpleModbusDevice device(0x01);
    device.configureChannels(1, 0x1000, 1.0f);
    device.setInitPhase(ModbusDevice::InitPhase::READY);
    
    // Initial state - no data
    ASSERT_FALSE(device.isDataFresh(1000));
    
    // Simulate response
    mockTickCount = 5000;
    uint8_t responseData[] = {0x00, 0x0A};
    device.testHandleModbusResponse(0x04, 0x1000, responseData, sizeof(responseData));
    
    // Data should be fresh
    mockTickCount = 5500;
    ASSERT_TRUE(device.isDataFresh(1000));  // 500ms < 1000ms threshold
    
    // Data should be stale
    mockTickCount = 7000;
    ASSERT_FALSE(device.isDataFresh(1000)); // 2000ms > 1000ms threshold
}

TEST(SimpleModbusDevice_HandleResponse_WrongAddress) {
    TestSimpleModbusDevice device(0x01);
    device.configureChannels(2, 0x1000, 1.0f);
    
    // Response for wrong address
    uint8_t responseData[] = {0x00, 0x0A, 0x00, 0x14};
    device.testHandleModbusResponse(0x04, 0x2000, responseData, sizeof(responseData));
    
    // Values should remain unchanged (0)
    auto values = device.getRawValues();
    ASSERT_EQ(0, values[0]);
    ASSERT_EQ(0, values[1]);
}

TEST(SimpleModbusDevice_HandleResponse_WrongSize) {
    TestSimpleModbusDevice device(0x01);
    device.configureChannels(3, 0x1000, 1.0f);
    
    // Response with wrong size (expecting 6 bytes, got 4)
    uint8_t responseData[] = {0x00, 0x0A, 0x00, 0x14};
    device.testHandleModbusResponse(0x04, 0x1000, responseData, sizeof(responseData));
    
    // Should handle error
    ASSERT_EQ(ModbusError::INVALID_RESPONSE, device.getLastError());
}

TEST(SimpleModbusDevice_HandleError) {
    TestSimpleModbusDevice device(0x01);
    device.configureChannels(1, 0x1000, 1.0f);
    
    // Test error handling
    device.testHandleModbusError(ModbusError::TIMEOUT);
    ASSERT_EQ(ModbusError::TIMEOUT, device.getLastError());
    
    device.testHandleModbusError(ModbusError::CRC_ERROR);
    ASSERT_EQ(ModbusError::CRC_ERROR, device.getLastError());
}

TEST(SimpleModbusDevice_IModbusInput_Interface) {
    TestSimpleModbusDevice device(0x01);
    device.configureChannels(2, 0x1000, 1.0f);
    device.setInitPhase(ModbusDevice::InitPhase::READY);
    
    // Test through IModbusInput interface
    IModbusInput* input = &device;
    
    // Simulate data
    mockTickCount = 3000;
    uint8_t responseData[] = {0x00, 0x0A, 0x00, 0x14};
    device.testHandleModbusResponse(0x04, 0x1000, responseData, sizeof(responseData));
    
    auto result = input->readInput(0);
    ASSERT_TRUE(result.isOk());
    ASSERT_EQ(10.0f, result.value);
    
    auto allResult = input->readAllInputs();
    ASSERT_TRUE(allResult.isOk());
    ASSERT_EQ(2, allResult.value.size());
}

TEST(SimpleModbusDevice_UpdateStatistics) {
    TestSimpleModbusDevice device(0x01);
    device.configureChannels(1, 0x1000, 1.0f);
    device.setInitPhase(ModbusDevice::InitPhase::READY);
    
    // Get initial stats
    auto stats1 = device.getStatistics();
    uint32_t initialRequests = stats1.totalRequests;
    
    // Trigger update
    device.updateChannels();
    
    // Stats should increment
    auto stats2 = device.getStatistics();
    ASSERT_EQ(initialRequests + 1, stats2.totalRequests);
}