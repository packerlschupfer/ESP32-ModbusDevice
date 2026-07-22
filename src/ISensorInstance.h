/*
 * ISensorInstance.h - part of the ESP32-ModbusDevice library
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

#ifndef ISENSORINSTANCE_H
#define ISENSORINSTANCE_H

#include "freertos/semphr.h"

/**
 * @brief Interface for sensor instances that provide data acquisition
 *
 * This interface defines the contract for sensor devices that can be
 * initialized, queried for data, and processed. Implementations should
 * ensure thread-safe operation via the provided mutex.
 *
 * @note All implementations must be thread-safe for use in FreeRTOS environments
 */
class ISensorInstance {
public:
    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~ISensorInstance() = default;

    /**
     * @brief Initialize the sensor instance
     *
     * Performs any necessary setup for the sensor. Should be called
     * once before requesting data.
     */
    virtual void initialize() = 0;

    /**
     * @brief Check if the sensor has been initialized
     * @return true if initialize() has been successfully called
     */
    virtual bool isInitialized() const = 0;

    /**
     * @brief Request new data from the sensor
     *
     * Initiates an asynchronous data request. Use waitForData()
     * to wait for the data to be available.
     *
     * @return true if the request was successfully initiated
     */
    virtual bool requestData() = 0;

    /**
     * @brief Wait for data to become available
     *
     * Blocks until the previously requested data is available
     * or a timeout occurs.
     *
     * @return true if data is available, false on timeout
     */
    virtual bool waitForData() = 0;

    /**
     * @brief Process the received sensor data
     *
     * Called after waitForData() returns true to process
     * and store the received data.
     */
    virtual void processData() = 0;

    /**
     * @brief Wait for the sensor to complete initialization
     *
     * Blocks until the sensor is fully initialized and ready
     * to accept data requests.
     */
    virtual void waitForInitialization() = 0;

    /**
     * @brief Get the mutex handle for thread-safe access
     *
     * Returns the FreeRTOS mutex handle that protects this
     * sensor instance's shared resources.
     *
     * @return SemaphoreHandle_t the mutex handle, or nullptr if not available
     */
    virtual SemaphoreHandle_t getMutexInstance() const = 0;
};

#endif // ISENSORINSTANCE_H
