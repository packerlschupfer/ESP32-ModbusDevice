#include "ModbusErrorTracker.h"
#include "ModbusDeviceLogging.h"
#include <Arduino.h>  // for millis()

namespace modbus {

// Static member initialization
ModbusErrorTracker::DeviceErrorStats ModbusErrorTracker::deviceStats[MODBUS_ERROR_TRACKER_MAX_DEVICES];
std::atomic<uint8_t> ModbusErrorTracker::numDevices{0};

ModbusErrorTracker::DeviceErrorStats* ModbusErrorTracker::findOrCreateStats(uint8_t address) {
    // First, search for existing entry
    uint8_t count = numDevices.load();
    for (uint8_t i = 0; i < count; i++) {
        if (deviceStats[i].initialized.load() && deviceStats[i].address == address) {
            return &deviceStats[i];
        }
    }

    // Not found - try to create new entry
    // Use compare_exchange to atomically claim a slot
    uint8_t expected = count;
    while (expected < MODBUS_ERROR_TRACKER_MAX_DEVICES) {
        if (numDevices.compare_exchange_weak(expected, expected + 1)) {
            // Successfully claimed slot 'expected'
            DeviceErrorStats* stats = &deviceStats[expected];
            stats->address = address;
            stats->crcErrors = 0;
            stats->timeouts = 0;
            stats->invalidData = 0;
            stats->deviceErrors = 0;
            stats->otherErrors = 0;
            stats->successCount = 0;
            stats->lastErrorTime = 0;
            stats->initialized = true;
            return stats;
        }
        // Another thread incremented numDevices, retry
    }

    // Array full
    MODBUSD_LOG_W("ModbusErrorTracker: Max devices (%d) reached, cannot track device 0x%02X",
                  MODBUS_ERROR_TRACKER_MAX_DEVICES, address);
    return nullptr;
}

const ModbusErrorTracker::DeviceErrorStats* ModbusErrorTracker::findStats(uint8_t address) {
    uint8_t count = numDevices.load();
    for (uint8_t i = 0; i < count; i++) {
        if (deviceStats[i].initialized.load() && deviceStats[i].address == address) {
            return &deviceStats[i];
        }
    }
    return nullptr;
}

ModbusErrorTracker::ErrorCategory ModbusErrorTracker::categorizeError(ModbusError error) {
    switch (error) {
        case ModbusError::CRC_ERROR:
            return ErrorCategory::CRC_ERROR;

        case ModbusError::TIMEOUT:
            return ErrorCategory::TIMEOUT;

        case ModbusError::INVALID_RESPONSE:
        case ModbusError::INVALID_DATA_LENGTH:
        case ModbusError::INVALID_PARAMETER:
            return ErrorCategory::INVALID_DATA;

        case ModbusError::SLAVE_DEVICE_FAILURE:
        case ModbusError::ILLEGAL_FUNCTION:
        case ModbusError::ILLEGAL_DATA_ADDRESS:
        case ModbusError::ILLEGAL_DATA_VALUE:
            return ErrorCategory::DEVICE_ERROR;

        case ModbusError::COMMUNICATION_ERROR:
        case ModbusError::NOT_INITIALIZED:
        case ModbusError::QUEUE_FULL:
        case ModbusError::RESOURCE_ERROR:
        case ModbusError::NULL_POINTER:
        case ModbusError::NOT_SUPPORTED:
        case ModbusError::MUTEX_ERROR:
        case ModbusError::DEVICE_NOT_FOUND:
        case ModbusError::RESOURCE_CREATION_FAILED:
        case ModbusError::INVALID_ADDRESS:
        default:
            return ErrorCategory::OTHER;
    }
}

void ModbusErrorTracker::recordError(uint8_t deviceAddress, ErrorCategory category) {
    DeviceErrorStats* stats = findOrCreateStats(deviceAddress);
    if (!stats) return;

    switch (category) {
        case ErrorCategory::CRC_ERROR:
            stats->crcErrors++;
            break;
        case ErrorCategory::TIMEOUT:
            stats->timeouts++;
            break;
        case ErrorCategory::INVALID_DATA:
            stats->invalidData++;
            break;
        case ErrorCategory::DEVICE_ERROR:
            stats->deviceErrors++;
            break;
        case ErrorCategory::OTHER:
            stats->otherErrors++;
            break;
    }

    stats->lastErrorTime = millis();
}

void ModbusErrorTracker::recordSuccess(uint8_t deviceAddress) {
    DeviceErrorStats* stats = findOrCreateStats(deviceAddress);
    if (!stats) return;
    stats->successCount++;
}

void ModbusErrorTracker::resetDevice(uint8_t deviceAddress) {
    DeviceErrorStats* stats = findOrCreateStats(deviceAddress);
    if (!stats) return;

    stats->crcErrors = 0;
    stats->timeouts = 0;
    stats->invalidData = 0;
    stats->deviceErrors = 0;
    stats->otherErrors = 0;
    stats->successCount = 0;
    stats->lastErrorTime = 0;
}

void ModbusErrorTracker::resetAll() {
    uint8_t count = numDevices.load();
    for (uint8_t i = 0; i < count; i++) {
        if (deviceStats[i].initialized.load()) {
            deviceStats[i].crcErrors = 0;
            deviceStats[i].timeouts = 0;
            deviceStats[i].invalidData = 0;
            deviceStats[i].deviceErrors = 0;
            deviceStats[i].otherErrors = 0;
            deviceStats[i].successCount = 0;
            deviceStats[i].lastErrorTime = 0;
        }
    }
}

uint32_t ModbusErrorTracker::getTotalErrors(uint8_t deviceAddress) {
    const DeviceErrorStats* stats = findStats(deviceAddress);
    if (!stats) return 0;

    return stats->crcErrors.load() + stats->timeouts.load() +
           stats->invalidData.load() + stats->deviceErrors.load() +
           stats->otherErrors.load();
}

uint32_t ModbusErrorTracker::getCrcErrors(uint8_t deviceAddress) {
    const DeviceErrorStats* stats = findStats(deviceAddress);
    return stats ? stats->crcErrors.load() : 0;
}

uint32_t ModbusErrorTracker::getTimeouts(uint8_t deviceAddress) {
    const DeviceErrorStats* stats = findStats(deviceAddress);
    return stats ? stats->timeouts.load() : 0;
}

uint32_t ModbusErrorTracker::getInvalidDataErrors(uint8_t deviceAddress) {
    const DeviceErrorStats* stats = findStats(deviceAddress);
    return stats ? stats->invalidData.load() : 0;
}

uint32_t ModbusErrorTracker::getDeviceErrors(uint8_t deviceAddress) {
    const DeviceErrorStats* stats = findStats(deviceAddress);
    return stats ? stats->deviceErrors.load() : 0;
}

uint32_t ModbusErrorTracker::getOtherErrors(uint8_t deviceAddress) {
    const DeviceErrorStats* stats = findStats(deviceAddress);
    return stats ? stats->otherErrors.load() : 0;
}

uint32_t ModbusErrorTracker::getSuccessCount(uint8_t deviceAddress) {
    const DeviceErrorStats* stats = findStats(deviceAddress);
    return stats ? stats->successCount.load() : 0;
}

uint32_t ModbusErrorTracker::getLastErrorTime(uint8_t deviceAddress) {
    const DeviceErrorStats* stats = findStats(deviceAddress);
    return stats ? stats->lastErrorTime.load() : 0;
}

float ModbusErrorTracker::getErrorRate(uint8_t deviceAddress) {
    const DeviceErrorStats* stats = findStats(deviceAddress);
    if (!stats) return 0.0f;

    uint32_t errors = stats->crcErrors.load() + stats->timeouts.load() +
                      stats->invalidData.load() + stats->deviceErrors.load() +
                      stats->otherErrors.load();
    uint32_t successes = stats->successCount.load();
    uint32_t total = errors + successes;

    if (total == 0) return 0.0f;
    return (static_cast<float>(errors) / static_cast<float>(total)) * 100.0f;
}

uint8_t ModbusErrorTracker::getTrackedDeviceCount() {
    return numDevices.load();
}

bool ModbusErrorTracker::isDeviceTracked(uint8_t deviceAddress) {
    return findStats(deviceAddress) != nullptr;
}

const char* ModbusErrorTracker::categoryToString(ErrorCategory category) {
    switch (category) {
        case ErrorCategory::CRC_ERROR:    return "CRC_ERROR";
        case ErrorCategory::TIMEOUT:      return "TIMEOUT";
        case ErrorCategory::INVALID_DATA: return "INVALID_DATA";
        case ErrorCategory::DEVICE_ERROR: return "DEVICE_ERROR";
        case ErrorCategory::OTHER:        return "OTHER";
        default:                          return "UNKNOWN";
    }
}

} // namespace modbus
