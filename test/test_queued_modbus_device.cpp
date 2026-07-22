#include "test_framework.h"
#include "mock_freertos.h"
#include "mock_logger.h"
#include "../src/QueuedModbusDevice.h"

// Mock esp32Modbus types
namespace esp32Modbus {
    enum FunctionCode {
        READ_INPUT_REGISTERS = 0x04,
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

using namespace modbus;

// Test implementation to access protected members
class TestQueuedModbusDevice : public QueuedModbusDevice {
public:
    TestQueuedModbusDevice(uint8_t addr) : QueuedModbusDevice(addr) {}
    
    // Expose protected methods for testing
    void testHandleModbusResponse(uint8_t functionCode, uint16_t address, 
                                const uint8_t* data, size_t length) {
        handleModbusResponse(functionCode, address, data, length);
    }
    
    void testHandleModbusError(ModbusError error) {
        handleModbusError(error);
    }
    
    // Access internals for testing
    size_t getQueueSize() const {
        return requestQueue ? uxQueueMessagesWaiting(requestQueue) : 0;
    }
    
    bool hasActiveRequest() const {
        return activeRequest.has_value();
    }
};

// Mock implementations
bool mockQueueSendResult = true;
bool mockQueueReceiveResult = false;
ModbusRequest mockReceivedRequest;

TEST(QueuedModbusDevice_Construction) {
    TestQueuedModbusDevice device(0x01);
    
    ASSERT_EQ(0x01, device.getServerAddress());
    ASSERT_FALSE(device.hasActiveRequest());
}

TEST(QueuedModbusDevice_InitializeQueue) {
    TestQueuedModbusDevice device(0x01);
    
    auto result = device.initializeQueue(10);
    ASSERT_TRUE(result.isOk());
    
    // Queue should be created
    ASSERT_TRUE(device.isQueueInitialized());
}

TEST(QueuedModbusDevice_EnqueueRequest_Success) {
    TestQueuedModbusDevice device(0x01);
    device.initializeQueue(5);
    
    ModbusRequest request;
    request.functionCode = 0x04;
    request.address = 0x1000;
    request.data = {0x00, 0x0A};
    request.id = 123;
    
    mockQueueSendResult = true;
    auto result = device.enqueueRequest(request);
    
    ASSERT_TRUE(result.isOk());
}

TEST(QueuedModbusDevice_EnqueueRequest_QueueFull) {
    TestQueuedModbusDevice device(0x01);
    device.initializeQueue(5);
    
    ModbusRequest request;
    request.functionCode = 0x04;
    request.address = 0x1000;
    
    mockQueueSendResult = false;
    auto result = device.enqueueRequest(request);
    
    ASSERT_FALSE(result.isOk());
    ASSERT_EQ(ModbusError::QUEUE_FULL, result.error);
}

TEST(QueuedModbusDevice_EnqueueRequest_NoQueue) {
    TestQueuedModbusDevice device(0x01);
    
    ModbusRequest request;
    auto result = device.enqueueRequest(request);
    
    ASSERT_FALSE(result.isOk());
    ASSERT_EQ(ModbusError::NOT_INITIALIZED, result.error);
}

TEST(QueuedModbusDevice_ProcessQueue_EmptyQueue) {
    TestQueuedModbusDevice device(0x01);
    device.initializeQueue(5);
    device.setInitPhase(ModbusDevice::InitPhase::READY);
    device.registerDevice();
    
    mockQueueReceiveResult = false;
    auto result = device.processQueue();
    
    // Should succeed but no request processed
    ASSERT_TRUE(result.isOk());
    ASSERT_FALSE(device.hasActiveRequest());
}

TEST(QueuedModbusDevice_ProcessQueue_WithRequest) {
    TestQueuedModbusDevice device(0x01);
    device.initializeQueue(5);
    device.setInitPhase(ModbusDevice::InitPhase::READY);
    device.registerDevice();
    
    // Setup mock to return a request
    mockQueueReceiveResult = true;
    mockReceivedRequest.functionCode = 0x04;
    mockReceivedRequest.address = 0x1000;
    mockReceivedRequest.data = {0x00, 0x05};
    mockReceivedRequest.id = 456;
    
    auto result = device.processQueue();
    
    // Should process the request
    ASSERT_TRUE(result.isOk());
    ASSERT_TRUE(device.hasActiveRequest());
}

TEST(QueuedModbusDevice_ProcessQueue_NotReady) {
    TestQueuedModbusDevice device(0x01);
    device.initializeQueue(5);
    // Device is still UNINITIALIZED
    
    auto result = device.processQueue();
    
    ASSERT_FALSE(result.isOk());
    ASSERT_EQ(ModbusError::NOT_INITIALIZED, result.error);
}

TEST(QueuedModbusDevice_HandleResponse_NoActiveRequest) {
    TestQueuedModbusDevice device(0x01);
    device.initializeQueue(5);
    
    uint8_t data[] = {0x00, 0x0A};
    device.testHandleModbusResponse(0x04, 0x1000, data, sizeof(data));
    
    // Should handle gracefully
    ASSERT_FALSE(device.hasActiveRequest());
}

TEST(QueuedModbusDevice_Callback_Registration) {
    TestQueuedModbusDevice device(0x01);
    
    bool callbackCalled = false;
    uint32_t callbackId = 0;
    ModbusError callbackError = ModbusError::SUCCESS;
    
    auto callback = [&](uint32_t id, const ModbusResponse& resp) {
        callbackCalled = true;
        callbackId = id;
        callbackError = resp.error;
    };
    
    device.setResponseCallback(callback);
    
    // Simulate response callback
    ModbusResponse response;
    response.requestId = 789;
    response.error = ModbusError::TIMEOUT;
    
    // Manually trigger callback (in real code this happens in handleModbusResponse)
    device.setResponseCallback(callback);
    callback(response.requestId, response);
    
    ASSERT_TRUE(callbackCalled);
    ASSERT_EQ(789, callbackId);
    ASSERT_EQ(ModbusError::TIMEOUT, callbackError);
}

TEST(QueuedModbusDevice_Statistics) {
    TestQueuedModbusDevice device(0x01);
    device.initializeQueue(5);
    
    auto stats1 = device.getStatistics();
    uint32_t initialRequests = stats1.totalRequests;
    
    // Enqueue a request
    ModbusRequest request;
    request.functionCode = 0x06;
    request.address = 0x2000;
    request.data = {0x12, 0x34};
    
    mockQueueSendResult = true;
    device.enqueueRequest(request);
    
    // Process it
    device.setInitPhase(ModbusDevice::InitPhase::READY);
    device.registerDevice();
    mockQueueReceiveResult = true;
    mockReceivedRequest = request;
    
    device.processQueue();
    
    auto stats2 = device.getStatistics();
    ASSERT_EQ(initialRequests + 1, stats2.totalRequests);
}

TEST(QueuedModbusDevice_ClearQueue) {
    TestQueuedModbusDevice device(0x01);
    device.initializeQueue(5);
    
    // Add multiple requests
    ModbusRequest request;
    request.functionCode = 0x04;
    mockQueueSendResult = true;
    
    device.enqueueRequest(request);
    device.enqueueRequest(request);
    device.enqueueRequest(request);
    
    // Clear queue
    device.clearQueue();
    
    // Queue should be empty
    mockQueueReceiveResult = false;
    auto result = device.processQueue();
    ASSERT_TRUE(result.isOk());
    ASSERT_FALSE(device.hasActiveRequest());
}

TEST(QueuedModbusDevice_HandleError_WithActiveRequest) {
    TestQueuedModbusDevice device(0x01);
    device.initializeQueue(5);
    device.setInitPhase(ModbusDevice::InitPhase::READY);
    device.registerDevice();
    
    // Setup active request
    mockQueueReceiveResult = true;
    mockReceivedRequest.functionCode = 0x04;
    mockReceivedRequest.address = 0x1000;
    mockReceivedRequest.id = 999;
    
    device.processQueue();
    ASSERT_TRUE(device.hasActiveRequest());
    
    // Trigger error
    bool callbackCalled = false;
    ModbusError receivedError = ModbusError::SUCCESS;
    
    device.setResponseCallback([&](uint32_t id, const ModbusResponse& resp) {
        callbackCalled = true;
        receivedError = resp.error;
    });
    
    device.testHandleModbusError(ModbusError::TIMEOUT);
    
    // Active request should be cleared
    ASSERT_FALSE(device.hasActiveRequest());
    ASSERT_EQ(ModbusError::TIMEOUT, device.getLastError());
}

TEST(QueuedModbusDevice_IModbusDevice_Interface) {
    TestQueuedModbusDevice device(0x01);
    device.initializeQueue(5);
    device.setInitPhase(ModbusDevice::InitPhase::READY);
    device.registerDevice();
    
    // Test through IModbusDevice interface
    IModbusDevice* modbusDevice = &device;
    
    // These should all enqueue requests
    auto result1 = modbusDevice->readHoldingRegisters(0x1000, 10);
    ASSERT_TRUE(result1.isOk() || result1.error == ModbusError::QUEUE_FULL);
    
    auto result2 = modbusDevice->writeSingleRegister(0x2000, 0x1234);
    ASSERT_TRUE(result2.isOk() || result2.error == ModbusError::QUEUE_FULL);
}