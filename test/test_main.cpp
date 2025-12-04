#include "test_framework.h"

// Forward declare test files to ensure they're linked
extern void test_mutex_guard_tests();
extern void test_device_registration_tests();
extern void test_data_handling_tests();

int main() {
    printf("\n========================================\n");
    printf("ModbusDevice Library Unit Tests\n");
    printf("========================================\n");
    
    // Run all registered tests
    RUN_ALL_TESTS();
    
    printf("\n========================================\n");
    
    return 0;
}