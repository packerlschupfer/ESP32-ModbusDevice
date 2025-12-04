#include "QueuedModbusDevice.h"
#include <cstring>

namespace modbus {

// Constructor
QueuedModbusDevice::QueuedModbusDevice(uint8_t serverAddr)
    : ModbusDevice(serverAddr) {
}

// Destructor
QueuedModbusDevice::~QueuedModbusDevice() {
    disableAsync();
}

// Enable async mode
bool QueuedModbusDevice::enableAsync(size_t queueDepth) {
    if (queue) {
        MODBUSD_LOG_W("Async mode already enabled");
        return true;
    }
    
    queue = xQueueCreate(queueDepth, sizeof(ModbusPacket));
    if (!queue) {
        MODBUSD_LOG_E("Failed to create queue with depth %d", queueDepth);
        return false;
    }
    
    asyncMode = true;
    MODBUSD_LOG_I("Async mode enabled with queue depth %d", queueDepth);
    return true;
}

// Disable async mode
void QueuedModbusDevice::disableAsync() {
    asyncMode = false;
    
    if (queue) {
        // Drain queue
        ModbusPacket packet;
        while (xQueueReceive(queue, &packet, 0) == pdTRUE) {
            // Discard packets
        }
        
        vQueueDelete(queue);
        queue = nullptr;
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
    if (!queue) return 0;
    
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
    if (getInitPhase() == InitPhase::CONFIGURING || !asyncMode || !queue) {
        ModbusDevice::handleModbusResponse(functionCode, address, data, length);
        return;
    }
    
    // Async mode - queue the packet
    ModbusPacket packet;
    packet.functionCode = functionCode;
    packet.address = address;
    packet.length = (length > MODBUS_MAX_READ_SIZE) ? MODBUS_MAX_READ_SIZE : length;
    packet.timestamp = xTaskGetTickCount();
    
    if (data && packet.length > 0) {
        std::memcpy(packet.data, data, packet.length);
    }
    
    // Try to queue
    if (xQueueSend(queue, &packet, 0) != pdTRUE) {
        onQueueFull();
    }
}

} // namespace modbus