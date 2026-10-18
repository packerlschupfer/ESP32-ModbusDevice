/*
 * IModbusDevice.h - part of the ESP32-ModbusDevice library
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

#ifndef IMODBUSDEVICE_H
#define IMODBUSDEVICE_H

#include <cstdint>
#include <vector>
#include "ModbusTypes.h"

namespace modbus {

/**
 * @interface IModbusDevice
 * @brief Core interface for Modbus communication
 * 
 * This interface defines the essential Modbus communication contract without
 * forcing any specific initialization patterns or FreeRTOS dependencies.
 * 
 * Design principles:
 * - Focus on Modbus protocol operations only
 * - No forced synchronization patterns
 * - No initialization lifecycle management
 * - Clean separation of concerns
 */
class IModbusDevice {
public:
    virtual ~IModbusDevice() = default;
    
    /**
     * @brief Get the Modbus server address
     * @return The configured server address (1-247)
     */
    virtual uint8_t getServerAddress() const noexcept = 0;

    /**
     * @brief Set the Modbus server address
     * @param address The server address (1-247)
     * @return Result indicating success or error
     */
    [[nodiscard]] virtual ModbusResult<void> setServerAddress(uint8_t address) = 0;

    /**
     * @brief Read holding registers (FC 0x03)
     * @param address Starting register address
     * @param count Number of registers to read
     * @return Result containing register values or error
     */
    [[nodiscard]] virtual ModbusResult<std::vector<uint16_t>> readHoldingRegisters(uint16_t address, uint16_t count) = 0;

    /**
     * @brief Read input registers (FC 0x04)
     * @param address Starting register address
     * @param count Number of registers to read
     * @return Result containing register values or error
     */
    [[nodiscard]] virtual ModbusResult<std::vector<uint16_t>> readInputRegisters(uint16_t address, uint16_t count) = 0;

    /**
     * @brief Write single register (FC 0x06)
     * @param address Register address
     * @param value Value to write
     * @return Result indicating success or error
     */
    [[nodiscard]] virtual ModbusResult<void> writeSingleRegister(uint16_t address, uint16_t value) = 0;

    /**
     * @brief Write multiple registers (FC 0x10)
     * @param address Starting register address
     * @param values Values to write
     * @return Result indicating success or error
     */
    [[nodiscard]] virtual ModbusResult<void> writeMultipleRegisters(uint16_t address, const std::vector<uint16_t>& values) = 0;

    /**
     * @brief Read coils (FC 0x01)
     * @param address Starting coil address
     * @param count Number of coils to read
     * @return Result containing coil states or error
     */
    [[nodiscard]] virtual ModbusResult<std::vector<bool>> readCoils(uint16_t address, uint16_t count) = 0;

    /**
     * @brief Read discrete inputs (FC 0x02)
     * @param address Starting input address
     * @param count Number of inputs to read
     * @return Result containing input states or error
     */
    [[nodiscard]] virtual ModbusResult<std::vector<bool>> readDiscreteInputs(uint16_t address, uint16_t count) = 0;

    /**
     * @brief Write single coil (FC 0x05)
     * @param address Coil address
     * @param value Coil state
     * @return Result indicating success or error
     */
    [[nodiscard]] virtual ModbusResult<void> writeSingleCoil(uint16_t address, bool value) = 0;

    /**
     * @brief Write multiple coils (FC 0x0F)
     * @param address Starting coil address
     * @param values Coil states to write
     * @return Result indicating success or error
     */
    [[nodiscard]] virtual ModbusResult<void> writeMultipleCoils(uint16_t address, const std::vector<bool>& values) = 0;

    /**
     * @brief Check if device is currently communicating
     * @return true if device has active communication
     */
    virtual bool isConnected() const noexcept = 0;

    /**
     * @brief Get last error that occurred
     * @return The last ModbusError or SUCCESS if no error
     */
    virtual ModbusError getLastError() const noexcept = 0;

    /**
     * @brief Get communication statistics
     * @return Struct containing request/response statistics
     */
    struct Statistics {
        uint32_t totalRequests = 0;
        uint32_t successfulRequests = 0;
        uint32_t failedRequests = 0;
        uint32_t timeouts = 0;
        uint32_t crcErrors = 0;
    };
    [[nodiscard]] virtual Statistics getStatistics() const = 0;
    
    /**
     * @brief Reset communication statistics
     */
    virtual void resetStatistics() = 0;
};

} // namespace modbus

#endif // IMODBUSDEVICE_H