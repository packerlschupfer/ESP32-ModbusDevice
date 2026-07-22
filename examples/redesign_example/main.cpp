/**
 * @file main.cpp
 * @brief Redesigned-architecture example for ESP32-ModbusDevice
 *
 * Highlights the post-redesign API:
 *  - Everything lives in `namespace modbus`
 *  - Operations are synchronous and return ModbusResult<T> instead of
 *    using async callbacks
 *  - Reads return the data directly (e.g. ModbusResult<std::vector<uint16_t>>)
 *  - Writes return ModbusResult<void>
 *
 * The example wires an esp32ModbusRTU master through the ModbusRegistry,
 * then performs representative read and write transactions.
 */

#include <Arduino.h>
#include <esp32ModbusRTU.h>
#include <ModbusDevice.h>   // namespace modbus, ModbusRegistry, ModbusResult

static constexpr int MODBUS_RX_PIN = 16;
static constexpr int MODBUS_TX_PIN = 17;
static constexpr uint8_t DEVICE_ADDRESS = 1;

esp32ModbusRTU modbusMaster(&Serial1);

// Bridge the single-argument RTU error callback to the library router.
static void modbusErrorHandler(uint16_t serverAddress, esp32Modbus::Error error) {
    handleError(serverAddress, error);
}

/**
 * @brief A thin device that exposes a couple of typed accessors built on the
 *        synchronous ModbusResult<T> primitives.
 */
class RelayBoard : public modbus::ModbusDevice {
public:
    explicit RelayBoard(uint8_t addr) : modbus::ModbusDevice(addr) {}

    /// Read N holding registers starting at `address`.
    modbus::ModbusResult<std::vector<uint16_t>> readBlock(uint16_t address,
                                                          uint16_t count) {
        return readHoldingRegisters(address, count);
    }

    /// Toggle a single coil (relay) at `address`.
    modbus::ModbusResult<void> setRelay(uint16_t address, bool on) {
        return writeSingleCoil(address, on);
    }
};

static RelayBoard board(DEVICE_ADDRESS);

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("ESP32-ModbusDevice :: redesigned-architecture example");

    // Bring up RS485 and the Modbus master.
    Serial1.begin(MODBUS_BAUD_RATE, SERIAL_8N1, MODBUS_RX_PIN, MODBUS_TX_PIN);

    modbus::ModbusRegistry::getInstance().setModbusRTU(&modbusMaster);
    modbusMaster.onData(mainHandleData);
    modbusMaster.onError(modbusErrorHandler);
    modbusMaster.begin();

    board.setInitPhase(modbus::ModbusDevice::InitPhase::READY);

    // Synchronous write: returns ModbusResult<void>.
    auto wr = board.setRelay(0x0000, true);
    if (wr.isOk()) {
        Serial.println("Relay 0 energized");
    } else {
        Serial.printf("Write failed: %s\n", getModbusErrorString(wr.error()));
    }
}

void loop() {
    // Synchronous read: the data comes straight back in the result value.
    auto block = board.readBlock(0x0000, 4);
    if (block.isOk()) {
        Serial.print("Registers:");
        for (uint16_t v : block.value()) {
            Serial.printf(" 0x%04X", v);
        }
        Serial.println();
    } else {
        Serial.printf("Read failed: %s\n", getModbusErrorString(block.error()));
    }

    delay(2000);
}
