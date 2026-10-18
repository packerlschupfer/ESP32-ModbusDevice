#ifndef MOCK_FREERTOS_H
#define MOCK_FREERTOS_H

#include <stdint.h>
#include <stdlib.h>

// Mock FreeRTOS types
typedef void* SemaphoreHandle_t;
typedef void* EventGroupHandle_t;
typedef uint32_t TickType_t;
typedef uint32_t EventBits_t;
typedef int BaseType_t;

// Mock constants
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS pdTRUE
#define pdFAIL pdFALSE
#define portMAX_DELAY 0xFFFFFFFF
#define BIT0 (1 << 0)

// Mock tick conversion
#define pdMS_TO_TICKS(ms) (ms)

// Mock structures for testing
struct MockSemaphore {
    bool taken;
    bool valid;
    
    MockSemaphore() : taken(false), valid(true) {}
};

struct MockEventGroup {
    EventBits_t bits;
    bool valid;
    
    MockEventGroup() : bits(0), valid(true) {}
};

// Global test control flags
extern bool mockMutexShouldFail;
extern bool mockMutexCreateShouldFail;

// Mock semaphore/mutex functions
inline SemaphoreHandle_t xSemaphoreCreateMutex() {
    if (mockMutexCreateShouldFail) {
        return nullptr;
    }
    return new MockSemaphore();
}

inline BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait) {
    if (!xSemaphore || mockMutexShouldFail) {
        return pdFALSE;
    }
    
    MockSemaphore* sem = static_cast<MockSemaphore*>(xSemaphore);
    if (!sem->valid || sem->taken) {
        return pdFALSE;
    }
    
    sem->taken = true;
    return pdTRUE;
}

inline BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore) {
    if (!xSemaphore) {
        return pdFALSE;
    }
    
    MockSemaphore* sem = static_cast<MockSemaphore*>(xSemaphore);
    if (!sem->valid || !sem->taken) {
        return pdFALSE;
    }
    
    sem->taken = false;
    return pdTRUE;
}

inline void vSemaphoreDelete(SemaphoreHandle_t xSemaphore) {
    if (xSemaphore) {
        delete static_cast<MockSemaphore*>(xSemaphore);
    }
}

// Mock event group functions
inline EventGroupHandle_t xEventGroupCreate() {
    if (mockMutexCreateShouldFail) {
        return nullptr;
    }
    return new MockEventGroup();
}

inline EventBits_t xEventGroupSetBits(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet) {
    if (!xEventGroup) {
        return 0;
    }
    
    MockEventGroup* eg = static_cast<MockEventGroup*>(xEventGroup);
    eg->bits |= uxBitsToSet;
    return eg->bits;
}

inline EventBits_t xEventGroupClearBits(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear) {
    if (!xEventGroup) {
        return 0;
    }
    
    MockEventGroup* eg = static_cast<MockEventGroup*>(xEventGroup);
    EventBits_t prevBits = eg->bits;
    eg->bits &= ~uxBitsToClear;
    return prevBits;
}

inline EventBits_t xEventGroupWaitBits(EventGroupHandle_t xEventGroup, 
                                      const EventBits_t uxBitsToWaitFor,
                                      const BaseType_t xClearOnExit,
                                      const BaseType_t xWaitForAllBits,
                                      TickType_t xTicksToWait) {
    if (!xEventGroup) {
        return 0;
    }
    
    MockEventGroup* eg = static_cast<MockEventGroup*>(xEventGroup);
    
    EventBits_t matchedBits = eg->bits & uxBitsToWaitFor;
    
    if (xWaitForAllBits) {
        if (matchedBits == uxBitsToWaitFor) {
            if (xClearOnExit) {
                eg->bits &= ~uxBitsToWaitFor;
            }
            return matchedBits;
        }
    } else {
        if (matchedBits != 0) {
            if (xClearOnExit) {
                eg->bits &= ~matchedBits;
            }
            return matchedBits;
        }
    }
    
    // For mock, return 0 if bits don't match (timeout)
    return 0;
}

inline EventBits_t xEventGroupGetBits(EventGroupHandle_t xEventGroup) {
    if (!xEventGroup) {
        return 0;
    }
    
    MockEventGroup* eg = static_cast<MockEventGroup*>(xEventGroup);
    return eg->bits;
}

inline void vEventGroupDelete(EventGroupHandle_t xEventGroup) {
    if (xEventGroup) {
        delete static_cast<MockEventGroup*>(xEventGroup);
    }
}

#endif // MOCK_FREERTOS_H