#ifndef QUEUEDMODBUSDEVICE_H
#define QUEUEDMODBUSDEVICE_H

#include "ModbusDevice.h"
#include "IModbusInput.h"
#include "freertos/queue.h"
#include <atomic>

namespace modbus {

/**
 * @class QueuedModbusDevice
 * @brief Asynchronous Modbus device with optional queuing
 * 
 * This class extends ModbusDevice to add asynchronous operation
 * through queuing. It provides a clean separation between initialization
 * (synchronous) and normal operation (asynchronous).
 * 
 * Key features:
 * - Clean initialization without warnings
 * - Lazy queue allocation
 * - Support for async response handling
 * - Compatible with MB8ART/RYN4 patterns
 */
class QueuedModbusDevice : public ModbusDevice {
public:
    /**
     * @brief Constructor
     * @param serverAddr Modbus server address
     */
    explicit QueuedModbusDevice(uint8_t serverAddr);
    
    /**
     * @brief Destructor
     */
    virtual ~QueuedModbusDevice();
    
    /**
     * @brief Enable asynchronous mode
     * @param queueDepth Queue depth for async responses
     * @return true if successful
     */
    bool enableAsync(size_t queueDepth = 10);
    
    /**
     * @brief Disable asynchronous mode
     */
    void disableAsync();
    
    /**
     * @brief Check if async mode is enabled
     * @return true if queue is initialized
     */
    bool isAsyncEnabled() const { return queue != nullptr; }
    
    /**
     * @brief Get current queue depth
     * @return Number of packets in queue
     */
    size_t getQueueDepth() const;
    
    /**
     * @brief Process queued packets
     * @param maxPackets Maximum packets to process (0 = all)
     * @return Number of packets processed
     */
    size_t processQueue(size_t maxPackets = 0);
    
protected:
    /**
     * @brief Modbus packet structure for queuing
     */
    struct ModbusPacket {
        uint8_t functionCode;
        uint16_t address;
        uint8_t data[MODBUS_MAX_READ_SIZE];
        size_t length;
        TickType_t timestamp;
    };
    
    /**
     * @brief Handle incoming Modbus response
     * 
     * Routes to sync or async handling based on mode
     */
    void handleModbusResponse(uint8_t functionCode, uint16_t address,
                            const uint8_t* data, size_t length) override;
    
    /**
     * @brief Process async response packet
     * 
     * Called when packet is dequeued. Override in derived classes.
     * 
     * @param packet The packet to process
     */
    virtual void onAsyncResponse(uint8_t functionCode, uint16_t address,
                                const uint8_t* data, size_t length) = 0;
    
    /**
     * @brief Called when queue is full
     * 
     * Override to handle queue overflow conditions
     */
    virtual void onQueueFull() {
        MODBUSD_LOG_W("Queue full for device %d", getServerAddress());
    }
    
private:
    QueueHandle_t queue = nullptr;
    std::atomic<bool> asyncMode{false};
    
    // Prevent copying
    QueuedModbusDevice(const QueuedModbusDevice&) = delete;
    QueuedModbusDevice& operator=(const QueuedModbusDevice&) = delete;
};

/**
 * @class QueuedModbusInputDevice
 * @brief Async Modbus device implementing IModbusInput
 * 
 * Convenience class for sensor devices that need async operation
 */
class QueuedModbusInputDevice : public QueuedModbusDevice, public IModbusAnalogInput {
public:
    explicit QueuedModbusInputDevice(uint8_t serverAddr) : QueuedModbusDevice(serverAddr) {}
    
    // IModbusInput implementation
    ModbusResult<void> update() override {
        // Process any queued packets
        processQueue();
        
        // Trigger new read if needed
        return triggerUpdate();
    }
    
    bool hasValidData() const override {
        return lastUpdateTime > 0 && getInitPhase() == InitPhase::READY;
    }
    
    uint32_t getLastUpdateTime() const override { return lastUpdateTime; }
    
    uint32_t getDataAge() const override {
        if (lastUpdateTime == 0) return UINT32_MAX;
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        return (now >= lastUpdateTime) ? (now - lastUpdateTime) : 0;
    }
    
protected:
    /**
     * @brief Trigger data update
     * 
     * Override to send Modbus requests for data refresh
     */
    virtual ModbusResult<void> triggerUpdate() = 0;
    
    uint32_t lastUpdateTime = 0;
};

} // namespace modbus

#endif // QUEUEDMODBUSDEVICE_H