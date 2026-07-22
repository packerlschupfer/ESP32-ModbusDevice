/**
 * @file main.cpp
 * @brief Event-group example for ESP32-ModbusDevice
 *
 * Demonstrates the current `namespace modbus` API:
 *  - Wiring an esp32ModbusRTU master into the ModbusRegistry singleton
 *  - A custom modbus::ModbusDevice that signals readiness through a
 *    FreeRTOS event group via setEventGroup()
 *  - Synchronous reads returning ModbusResult<std::vector<uint16_t>>
 *
 * All API calls are synchronous and return common::Result-based
 * ModbusResult<T>; errors are surfaced through isOk()/error().
 */

#include <Arduino.h>
#include <esp32ModbusRTU.h>
#include <ModbusDevice.h>      // brings in namespace modbus + ModbusRegistry
#include "freertos/event_groups.h"

// ----- RS485 / Modbus master wiring -----
static constexpr int MODBUS_RX_PIN = 16;
static constexpr int MODBUS_TX_PIN = 17;
static constexpr uint8_t SENSOR_ADDRESS = 1;

// mainHandleData / handleError are provided by ModbusDevice.h and route
// responses to the registered device.
esp32ModbusRTU modbusMaster(&Serial1);

// esp32ModbusRTU::onError expects a single-argument handler; bridge it to the
// library's global handleError(serverAddress, error) router.
static void modbusErrorHandler(uint16_t serverAddress, esp32Modbus::Error error) {
    handleError(serverAddress, error);
}

// ----- Event group bits -----
static EventGroupHandle_t deviceEvents = nullptr;
static constexpr EventBits_t DEVICE_READY_BIT = (1 << 0);
static constexpr EventBits_t DEVICE_ERROR_BIT = (1 << 1);

/**
 * @brief Minimal concrete device built on the current modbus::ModbusDevice API.
 */
class TemperatureDevice : public modbus::ModbusDevice {
public:
    explicit TemperatureDevice(uint8_t addr) : modbus::ModbusDevice(addr) {}

    /**
     * @brief Probe the device and report readiness through the event group.
     */
    bool begin() {
        setInitPhase(InitPhase::CONFIGURING);

        // Synchronous read of the holding register block (current API).
        auto probe = readHoldingRegisters(0x0000, 1);
        if (!probe.isOk()) {
            setInitPhase(InitPhase::ERROR);  // raises DEVICE_ERROR_BIT
            return false;
        }

        setInitPhase(InitPhase::READY);      // raises DEVICE_READY_BIT
        return true;
    }

    /**
     * @brief Read the current temperature (scaled 0.1 deg C per LSB).
     */
    modbus::ModbusResult<float> readTemperature() {
        auto regs = readHoldingRegisters(0x0001, 1);
        if (!regs.isOk()) {
            return modbus::ModbusResult<float>::error(regs.error());
        }
        return modbus::ModbusResult<float>::ok(regs.value()[0] / 10.0f);
    }
};

static TemperatureDevice sensor(SENSOR_ADDRESS);

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("ESP32-ModbusDevice :: event-group example");

    // 1. Bring up the RS485 UART.
    Serial1.begin(MODBUS_BAUD_RATE, SERIAL_8N1, MODBUS_RX_PIN, MODBUS_TX_PIN);

    // 2. Register the RTU master and wire its callbacks to the library.
    modbus::ModbusRegistry::getInstance().setModbusRTU(&modbusMaster);
    modbusMaster.onData(mainHandleData);
    modbusMaster.onError(modbusErrorHandler);
    modbusMaster.begin();

    // 3. Create an event group and let the device drive its bits.
    deviceEvents = xEventGroupCreate();
    sensor.setEventGroup(deviceEvents, DEVICE_READY_BIT, DEVICE_ERROR_BIT);

    // 4. Probe the device.
    if (sensor.begin()) {
        Serial.println("Sensor configured; waiting on DEVICE_READY_BIT");
    } else {
        Serial.printf("Sensor probe failed: %s\n",
                      getModbusErrorString(sensor.getLastError()));
    }
}

void loop() {
    // Block until the device signals ready (or error) through the event group.
    EventBits_t bits = xEventGroupWaitBits(
        deviceEvents,
        DEVICE_READY_BIT | DEVICE_ERROR_BIT,
        pdFALSE,  // do not clear on exit
        pdFALSE,  // wait for any bit
        pdMS_TO_TICKS(1000));

    if (bits & DEVICE_READY_BIT) {
        auto temp = sensor.readTemperature();
        if (temp.isOk()) {
            Serial.printf("Temperature: %.1f C\n", temp.value());
        } else {
            Serial.printf("Read error: %s\n",
                          getModbusErrorString(temp.error()));
        }
    } else if (bits & DEVICE_ERROR_BIT) {
        Serial.println("Device reported an initialization error");
    }

    delay(2000);
}
