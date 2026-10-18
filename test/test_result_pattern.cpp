#include "test_framework.h"
#include "../src/ModbusTypes.h"

using namespace modbus;

// Test the Result<T, E> pattern

TEST(Result_BasicUsage) {
    // Test successful result
    ModbusResult<int> successResult = ModbusResult<int>::ok(42);
    
    ASSERT_TRUE(successResult.isOk());
    ASSERT_FALSE(successResult.isError());
    ASSERT_EQ(42, successResult.value);
    ASSERT_EQ(ModbusError::SUCCESS, successResult.error);
    
    // Test error result
    ModbusResult<int> errorResult = ModbusResult<int>::err(ModbusError::TIMEOUT);
    
    ASSERT_FALSE(errorResult.isOk());
    ASSERT_TRUE(errorResult.isError());
    ASSERT_EQ(ModbusError::TIMEOUT, errorResult.error);
}

TEST(Result_VoidSpecialization) {
    // Test successful void result
    ModbusResult<void> successResult = ModbusResult<void>::ok();
    
    ASSERT_TRUE(successResult.isOk());
    ASSERT_FALSE(successResult.isError());
    ASSERT_EQ(ModbusError::SUCCESS, successResult.error);
    
    // Test error void result
    ModbusResult<void> errorResult = ModbusResult<void>::err(ModbusError::CRC_ERROR);
    
    ASSERT_FALSE(errorResult.isOk());
    ASSERT_TRUE(errorResult.isError());
    ASSERT_EQ(ModbusError::CRC_ERROR, errorResult.error);
}

TEST(Result_BoolOperator) {
    ModbusResult<int> successResult = ModbusResult<int>::ok(100);
    ModbusResult<int> errorResult = ModbusResult<int>::err(ModbusError::INVALID_PARAMETER);
    
    // Test bool operator
    if (successResult) {
        // This should execute
        ASSERT_TRUE(true);
    } else {
        ASSERT_TRUE(false); // Should not reach here
    }
    
    if (errorResult) {
        ASSERT_TRUE(false); // Should not reach here
    } else {
        // This should execute
        ASSERT_TRUE(true);
    }
}

TEST(Result_ValueOr) {
    ModbusResult<int> successResult = ModbusResult<int>::ok(42);
    ModbusResult<int> errorResult = ModbusResult<int>::err(ModbusError::NOT_INITIALIZED);
    
    ASSERT_EQ(42, successResult.valueOr(100));
    ASSERT_EQ(100, errorResult.valueOr(100));
}

TEST(Result_VectorType) {
    std::vector<uint16_t> testData = {0x1234, 0x5678, 0xABCD};
    
    ModbusResult<std::vector<uint16_t>> successResult = 
        ModbusResult<std::vector<uint16_t>>::ok(testData);
    
    ASSERT_TRUE(successResult.isOk());
    ASSERT_EQ(3, successResult.value.size());
    ASSERT_EQ(0x1234, successResult.value[0]);
    ASSERT_EQ(0x5678, successResult.value[1]);
    ASSERT_EQ(0xABCD, successResult.value[2]);
}

TEST(Result_ErrorPropagation) {
    // Simulate error propagation pattern
    auto operation1 = []() -> ModbusResult<int> {
        return ModbusResult<int>::err(ModbusError::COMMUNICATION_ERROR);
    };
    
    auto operation2 = [](int value) -> ModbusResult<float> {
        return ModbusResult<float>::ok(value * 1.5f);
    };
    
    // Test error propagation
    auto result1 = operation1();
    if (!result1.isOk()) {
        // Propagate error
        ModbusResult<float> propagatedError = 
            ModbusResult<float>::err(result1.error);
        
        ASSERT_FALSE(propagatedError.isOk());
        ASSERT_EQ(ModbusError::COMMUNICATION_ERROR, propagatedError.error);
    } else {
        auto result2 = operation2(result1.value);
        ASSERT_TRUE(false); // Should not reach here
    }
}

TEST(Result_ChainedOperations) {
    // Test chained operations with Result
    auto readRegister = [](uint16_t addr) -> ModbusResult<uint16_t> {
        if (addr == 0x1000) {
            return ModbusResult<uint16_t>::ok(0x1234);
        }
        return ModbusResult<uint16_t>::err(ModbusError::INVALID_DATA_ADDRESS);
    };
    
    auto convertToFloat = [](uint16_t value) -> ModbusResult<float> {
        if (value == 0) {
            return ModbusResult<float>::err(ModbusError::INVALID_DATA_VALUE);
        }
        return ModbusResult<float>::ok(value / 10.0f);
    };
    
    // Test successful chain
    auto result1 = readRegister(0x1000);
    ASSERT_TRUE(result1.isOk());
    
    auto result2 = convertToFloat(result1.value);
    ASSERT_TRUE(result2.isOk());
    ASSERT_EQ(123.4f, result2.value);
    
    // Test error in first operation
    auto result3 = readRegister(0x2000);
    ASSERT_FALSE(result3.isOk());
    ASSERT_EQ(ModbusError::INVALID_DATA_ADDRESS, result3.error);
}

TEST(ModbusError_Values) {
    // Test that ModbusError values are correctly defined
    ASSERT_EQ(0x00, static_cast<int>(ModbusError::SUCCESS));
    ASSERT_EQ(0x01, static_cast<int>(ModbusError::ILLEGAL_FUNCTION));
    ASSERT_EQ(0x02, static_cast<int>(ModbusError::ILLEGAL_DATA_ADDRESS));
    ASSERT_EQ(0x03, static_cast<int>(ModbusError::ILLEGAL_DATA_VALUE));
    ASSERT_EQ(0x04, static_cast<int>(ModbusError::SLAVE_DEVICE_FAILURE));
    
    // Library-specific errors should be 0x80+
    ASSERT_TRUE(static_cast<int>(ModbusError::TIMEOUT) >= 0x80);
    ASSERT_TRUE(static_cast<int>(ModbusError::CRC_ERROR) > static_cast<int>(ModbusError::TIMEOUT));
}