/**
 * @file new-architecture-demo.ino
 * @brief New-architecture demo: SimpleModbusDevice + QueuedModbusDevice
 *
 * Demonstrates the two device flavours offered by the redesigned library,
 * both living in `namespace modbus` and both built on the synchronous
 * ModbusResult<T> API:
 *
 *  1. modbus::SimpleModbusDevice  - periodic synchronous polling with a
 *     channel/value model (IModbusAnalogInput).
 *  2. modbus::QueuedModbusDevice - async-capable device whose onAsyncResponse()
 *     consumes responses dequeued by processQueue().
 *
 * The esp32ModbusRTU master is registered with the ModbusRegistry singleton.
 */

#include <Arduino.h>
#include <esp32ModbusRTU.h>
#include <SimpleModbusDevice.h>
#include <QueuedModbusDevice.h>

static constexpr int MODBUS_RX_PIN = 16;
static constexpr int MODBUS_TX_PIN = 17;
static constexpr uint8_t SIMPLE_ADDRESS = 1;
static constexpr uint8_t QUEUED_ADDRESS = 2;

esp32ModbusRTU modbusMaster(&Serial1);

// Bridge the single-argument RTU error callback to the library router.
static void modbusErrorHandler(esp32Modbus::Error error) {
    handleError(0, error);
}

// ---- 1. Simple (synchronous, polled) device -------------------------------
class PressureSensor : public modbus::SimpleModbusDevice {
public:
    explicit PressureSensor(uint8_t addr) : modbus::SimpleModbusDevice(addr) {}

protected:
    bool configure() override {
        if (!readHoldingRegisters(0x0000, 1).isOk()) {
            return false;
        }
        addChannel("Pressure", "kPa", 0x0010);
        return true;
    }

    bool readChannelData() override {
        auto regs = readHoldingRegisters(0x0010, 1);
        if (!regs.isOk()) {
            return false;
        }
        values.clear();
        values.push_back(static_cast<int32_t>(regs.value()[0]));
        return true;
    }

public:
    modbus::ModbusResult<float> getFloat(size_t channel = 0) const override {
        if (channel >= values.size()) {
            return modbus::ModbusResult<float>::error(
                modbus::ModbusError::INVALID_PARAMETER);
        }
        return modbus::ModbusResult<float>::ok(values[channel]);
    }
};

// ---- 2. Queued (async-capable) device -------------------------------------
class FlowMeter : public modbus::QueuedModbusDevice {
public:
    explicit FlowMeter(uint8_t addr) : modbus::QueuedModbusDevice(addr) {}

    float lastFlow() const { return flow_; }

    /// Issue a synchronous read and cache the latest flow value.
    modbus::ModbusResult<void> poll() {
        auto regs = readHoldingRegisters(0x0020, 1);
        if (!regs.isOk()) {
            return modbus::ModbusResult<void>::error(regs.error());
        }
        flow_ = regs.value()[0] / 100.0f;
        return modbus::ModbusResult<void>::ok();
    }

protected:
    // Called by processQueue() for each dequeued async packet.
    void onAsyncResponse(uint8_t functionCode, uint16_t address,
                         const uint8_t* data, size_t length) override {
        if (length >= 2) {
            uint16_t raw = (static_cast<uint16_t>(data[0]) << 8) | data[1];
            flow_ = raw / 100.0f;
        }
    }

private:
    float flow_ = 0.0f;
};

static PressureSensor pressure(SIMPLE_ADDRESS);
static FlowMeter flow(QUEUED_ADDRESS);

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("ESP32-ModbusDevice :: new-architecture demo");

    Serial1.begin(MODBUS_BAUD_RATE, SERIAL_8N1, MODBUS_RX_PIN, MODBUS_TX_PIN);

    modbus::ModbusRegistry::getInstance().setModbusRTU(&modbusMaster);
    modbusMaster.onData(mainHandleData);
    modbusMaster.onError(modbusErrorHandler);
    modbusMaster.begin();

    if (pressure.initialize()) {
        Serial.println("Pressure sensor (simple) ready");
    } else {
        Serial.printf("Pressure init failed: %s\n",
                      getModbusErrorString(pressure.getLastError()));
    }

    // Enable async queuing on the flow meter.
    flow.setInitPhase(modbus::ModbusDevice::InitPhase::READY);
    if (flow.enableAsync(8)) {
        Serial.println("Flow meter (queued) async mode enabled");
    }
}

void loop() {
    // Simple device: synchronous poll.
    if (pressure.update().isOk()) {
        auto p = pressure.getFloat(0);
        if (p.isOk()) {
            Serial.printf("Pressure: %.0f kPa\n", p.value());
        }
    }

    // Queued device: drain any queued async responses, then poll synchronously.
    flow.processQueue();
    if (flow.poll().isOk()) {
        Serial.printf("Flow: %.2f (queue depth %u)\n",
                      flow.lastFlow(),
                      static_cast<unsigned>(flow.getQueueDepth()));
    }

    delay(2000);
}
