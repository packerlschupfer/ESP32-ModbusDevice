#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <string.h>
#include <vector>
#include <functional>

/**
 * @brief Simple unit test framework for ESP32
 * 
 * Provides basic test assertions and test case management
 */

class TestFramework {
public:
    struct TestResult {
        const char* testName;
        bool passed;
        const char* failureMessage;
        int lineNumber;
        const char* fileName;
    };

    static TestFramework& getInstance() {
        static TestFramework instance;
        return instance;
    }

    void runAllTests() {
        printf("\n===== Running Unit Tests =====\n");
        
        int totalTests = 0;
        int passedTests = 0;
        
        for (auto& test : tests) {
            totalTests++;
            printf("\nRunning: %s\n", test.name);
            
            currentTestName = test.name;
            currentTestPassed = true;
            
            test.func();
            
            if (currentTestPassed) {
                printf("  ✓ PASSED\n");
                passedTests++;
            } else {
                printf("  ✗ FAILED\n");
            }
        }
        
        printf("\n===== Test Summary =====\n");
        printf("Total: %d, Passed: %d, Failed: %d\n", 
               totalTests, passedTests, totalTests - passedTests);
        
        if (passedTests == totalTests) {
            printf("All tests passed!\n");
        }
    }

    void registerTest(const char* name, std::function<void()> func) {
        tests.push_back({name, func});
    }

    void reportFailure(const char* message, int line, const char* file) {
        currentTestPassed = false;
        printf("  ASSERTION FAILED: %s\n", message);
        printf("  at %s:%d\n", file, line);
    }

    bool isCurrentTestPassing() const {
        return currentTestPassed;
    }

private:
    struct TestCase {
        const char* name;
        std::function<void()> func;
    };

    std::vector<TestCase> tests;
    const char* currentTestName = "";
    bool currentTestPassed = true;

    TestFramework() = default;
};

// Test registration macro
#define TEST(name) \
    void test_##name(); \
    struct TestRegistrar_##name { \
        TestRegistrar_##name() { \
            TestFramework::getInstance().registerTest(#name, test_##name); \
        } \
    } testRegistrar_##name; \
    void test_##name()

// Assertion macros
#define ASSERT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            TestFramework::getInstance().reportFailure( \
                "Expected true: " #condition, __LINE__, __FILE__); \
            return; \
        } \
    } while (0)

#define ASSERT_FALSE(condition) \
    do { \
        if (condition) { \
            TestFramework::getInstance().reportFailure( \
                "Expected false: " #condition, __LINE__, __FILE__); \
            return; \
        } \
    } while (0)

#define ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            char msg[256]; \
            snprintf(msg, sizeof(msg), "Expected %s == %s", #expected, #actual); \
            TestFramework::getInstance().reportFailure(msg, __LINE__, __FILE__); \
            return; \
        } \
    } while (0)

#define ASSERT_NE(expected, actual) \
    do { \
        if ((expected) == (actual)) { \
            char msg[256]; \
            snprintf(msg, sizeof(msg), "Expected %s != %s", #expected, #actual); \
            TestFramework::getInstance().reportFailure(msg, __LINE__, __FILE__); \
            return; \
        } \
    } while (0)

#define ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != nullptr) { \
            TestFramework::getInstance().reportFailure( \
                "Expected nullptr: " #ptr, __LINE__, __FILE__); \
            return; \
        } \
    } while (0)

#define ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == nullptr) { \
            TestFramework::getInstance().reportFailure( \
                "Expected non-null: " #ptr, __LINE__, __FILE__); \
            return; \
        } \
    } while (0)

// Run all tests macro
#define RUN_ALL_TESTS() TestFramework::getInstance().runAllTests()

#endif // TEST_FRAMEWORK_H