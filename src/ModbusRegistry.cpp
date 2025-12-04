#include "ModbusRegistry.h"
#include "ModbusDevice.h"
#include "ModbusDeviceLogging.h"
#include "MutexGuard.h"

namespace modbus {

ModbusRegistry::ModbusRegistry() {
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
        MODBUSD_LOG_E("Failed to create ModbusRegistry mutex");
        return;
    }

    busMutex_ = xSemaphoreCreateMutex();
    if (!busMutex_) {
        MODBUSD_LOG_E("Failed to create ModbusRegistry bus mutex");
        // Clean up the first mutex since second failed
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
    }
}

ModbusRegistry::~ModbusRegistry() {
    if (mutex_) {
        vSemaphoreDelete(mutex_);
    }
    if (busMutex_) {
        vSemaphoreDelete(busMutex_);
    }
}

void ModbusRegistry::setModbusRTU(esp32ModbusRTU* rtu) {
    if (!mutex_) return;

    MutexGuard lock(mutex_);
    if (lock.hasLock()) {
        modbusRTU_ = rtu;
        MODBUSD_LOG_I("ModbusRTU instance set");
    }
}

bool ModbusRegistry::registerDevice(uint8_t address, ModbusDevice* device) {
    if (!device || address == 0 || address > 247) {
        return false;
    }

    if (!mutex_) {
        MODBUSD_LOG_E("Registry mutex not initialized");
        return false;
    }

    MutexGuard lock(mutex_);
    if (!lock.hasLock()) {
        return false;
    }

    deviceMap_[address] = device;
    MODBUSD_LOG_I("Device registered at address %d", address);
    return true;
}

bool ModbusRegistry::unregisterDevice(uint8_t address) {
    if (!mutex_) {
        return false;
    }

    MutexGuard lock(mutex_);
    if (!lock.hasLock()) {
        return false;
    }

    auto it = deviceMap_.find(address);
    if (it != deviceMap_.end()) {
        deviceMap_.erase(it);
        MODBUSD_LOG_I("Device unregistered from address %d", address);
        return true;
    }

    return false;
}

ModbusDevice* ModbusRegistry::getDevice(uint8_t address) const {
    if (!mutex_) {
        return nullptr;
    }

    ModbusDevice* device = nullptr;

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
        auto it = deviceMap_.find(address);
        if (it != deviceMap_.end()) {
            device = it->second;
        }
        xSemaphoreGive(mutex_);
    }

    return device;
}

bool ModbusRegistry::hasDevice(uint8_t address) const {
    return getDevice(address) != nullptr;
}

size_t ModbusRegistry::getDeviceCount() const {
    if (!mutex_) {
        return 0;
    }

    size_t count = 0;

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
        count = deviceMap_.size();
        xSemaphoreGive(mutex_);
    }

    return count;
}

bool ModbusRegistry::acquireBusMutex(uint32_t timeoutMs) {
    if (!busMutex_) {
        MODBUSD_LOG_E("Bus mutex not initialized");
        return false;
    }
    return xSemaphoreTake(busMutex_, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

void ModbusRegistry::releaseBusMutex() {
    if (busMutex_) {
        xSemaphoreGive(busMutex_);
    }
}

} // namespace modbus
