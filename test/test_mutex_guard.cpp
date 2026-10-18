#include "test_framework.h"
#include "mock_freertos.h"
#include "mock_logger.h"

// Define logger instance
Logger logger;

// Mock control flags
bool mockMutexShouldFail = false;
bool mockMutexCreateShouldFail = false;

// Include the header under test
#include "../src/ModbusDevice.h"

// Reset mock state before each test
void resetMockState() {
    mockMutexShouldFail = false;
    mockMutexCreateShouldFail = false;
}

TEST(MutexGuard_ConstructorAcquiresMutex) {
    resetMockState();
    
    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    ASSERT_NOT_NULL(mutex);
    
    {
        MutexGuard guard(mutex);
        ASSERT_TRUE(guard.hasLock());
        
        // Verify mutex is taken
        MockSemaphore* mockMutex = static_cast<MockSemaphore*>(mutex);
        ASSERT_TRUE(mockMutex->taken);
    }
    
    // Verify mutex is released after guard destruction
    MockSemaphore* mockMutex = static_cast<MockSemaphore*>(mutex);
    ASSERT_FALSE(mockMutex->taken);
    
    vSemaphoreDelete(mutex);
}

TEST(MutexGuard_HandlesNullMutex) {
    resetMockState();
    
    MutexGuard guard(nullptr);
    ASSERT_FALSE(guard.hasLock());
}

TEST(MutexGuard_HandlesMutexAcquisitionFailure) {
    resetMockState();
    
    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    ASSERT_NOT_NULL(mutex);
    
    // Make mutex acquisition fail
    mockMutexShouldFail = true;
    
    {
        MutexGuard guard(mutex);
        ASSERT_FALSE(guard.hasLock());
    }
    
    vSemaphoreDelete(mutex);
}

TEST(MutexGuard_DestructorOnlyReleasesOwnedMutex) {
    resetMockState();
    
    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    ASSERT_NOT_NULL(mutex);
    
    // Take mutex manually
    ASSERT_EQ(pdTRUE, xSemaphoreTake(mutex, portMAX_DELAY));
    
    {
        // Guard should fail to acquire already-taken mutex
        MutexGuard guard(mutex, 0);  // 0 timeout
        ASSERT_FALSE(guard.hasLock());
    }
    
    // Mutex should still be taken after guard destruction
    MockSemaphore* mockMutex = static_cast<MockSemaphore*>(mutex);
    ASSERT_TRUE(mockMutex->taken);
    
    // Release and cleanup
    xSemaphoreGive(mutex);
    vSemaphoreDelete(mutex);
}

TEST(MutexGuard_SupportsTimeout) {
    resetMockState();
    
    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    ASSERT_NOT_NULL(mutex);
    
    {
        MutexGuard guard(mutex, 1000);  // 1 second timeout
        ASSERT_TRUE(guard.hasLock());
    }
    
    vSemaphoreDelete(mutex);
}