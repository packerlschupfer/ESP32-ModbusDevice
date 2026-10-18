/*
 * QueuedModbusDevice.cpp - part of the ESP32-ModbusDevice library
 *
 * Copyright (C) 2025-2026 packerlschupfer
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

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
