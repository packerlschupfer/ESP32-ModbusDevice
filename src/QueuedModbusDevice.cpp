#include "QueuedModbusDevice.h"
#include <cstring>

namespace modbus {

// Constructor
QueuedModbusDevice::QueuedModbusDevice(uint8_t serverAddr)
    : ModbusDevice(serverAddr) {
}

// Destructor - only place where queue is deleted
QueuedModbusDevice::~QueuedModbusDevice() {
    asyncMode = false;

    if (queue) {
        // Drain then delete
        ModbusPacket packet;
        while (xQueueReceive(queue, &packet, 0) == pdTRUE) {}
        vQueueDelete(queue);
        queue = nullptr;
    }
}

// Enable async mode
bool QueuedModbusDevice::enableAsync(size_t queueDepth) {
    // Queue already exists - just re-enable
    if (queue) {
        asyncMode = true;
        MODBUSD_LOG_I("Async mode re-enabled");
        return true;
    }

    // Create queue (persists until destructor)
    queue = xQueueCreate(queueDepth, sizeof(ModbusPacket));
    if (!queue) {
        MODBUSD_LOG_E("Failed to create queue with depth %d", queueDepth);
        return false;
    }

    asyncMode = true;
    MODBUSD_LOG_I("Async mode enabled with queue depth %d", queueDepth);
    return true;
}

// Disable async mode - drains queue but doesn't delete it
void QueuedModbusDevice::disableAsync() {
    asyncMode = false;

    if (queue) {
        // Drain queue
        ModbusPacket packet;
        while (xQueueReceive(queue, &packet, 0) == pdTRUE) {
            // Discard packets
        }
        MODBUSD_LOG_I("Async mode disabled");
    }
}

// Get queue depth
size_t QueuedModbusDevice::getQueueDepth() const {
    if (!queue) return 0;
    return uxQueueMessagesWaiting(queue);
}

// Process queue
size_t QueuedModbusDevice::processQueue(size_t maxPackets) {
    // Check asyncMode first - atomic read gates all queue usage
    if (!asyncMode || !queue) return 0;

    size_t processed = 0;
    ModbusPacket packet;

    while (xQueueReceive(queue, &packet, 0) == pdTRUE) {
        // Call virtual handler
        onAsyncResponse(packet.functionCode, packet.address,
                       packet.data, packet.length);

        processed++;

        if (maxPackets > 0 && processed >= maxPackets) {
            break;
        }
    }

    return processed;
}

// Handle Modbus response
void QueuedModbusDevice::handleModbusResponse(uint8_t functionCode, uint16_t address,
                                              const uint8_t* data, size_t length) {
    // During CONFIGURING phase or sync mode, use base class handling
    // asyncMode atomic read gates queue usage - no race with disableAsync()
    if (getInitPhase() == InitPhase::CONFIGURING || !asyncMode || !queue) {
        ModbusDevice::handleModbusResponse(functionCode, address, data, length);
        return;
    }

    // Build packet (data is copied, not pointer-stored)
    ModbusPacket packet;
    packet.functionCode = functionCode;
    packet.address = address;
    packet.length = (length > MODBUS_MAX_READ_SIZE) ? MODBUS_MAX_READ_SIZE : length;
    packet.timestamp = xTaskGetTickCount();

    if (data && packet.length > 0) {
        std::memcpy(packet.data, data, packet.length);
    }

    // Queue is never deleted during runtime, so this is safe
    if (xQueueSend(queue, &packet, 0) != pdTRUE) {
        onQueueFull();
    }
}

} // namespace modbus
