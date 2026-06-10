/**
 * @file modbus-namespace-demo.ino
 * @brief Namespace-usage demo for ESP32-ModbusDevice
 *
 * After the redesign every class lives in `namespace modbus`. This demo shows
 * the idiomatic ways to reference those types:
 *  - fully qualified (modbus::SimpleModbusDevice)
 *  - via a `using namespace modbus;` shortcut inside a translation unit
 *
 * It also shows that the old global named `modbus` is gone: the master
 * instance here is called `modbusMaster` so it cannot collide with the
 * `modbus` namespace.
 *
 * The concrete device subclasses modbus::SimpleModbusDevice, which provides
 * a channel-oriented IModbusAnalogInput on top of the synchronous
 * ModbusResult<T> read API.
 */

#include <Arduino.h>
#include <esp32ModbusRTU.h>
#include <SimpleModbusDevice.h>   // pulls in ModbusDevice.h + namespace modbus

static constexpr int MODBUS_RX_PIN = 16;
static constexpr int MODBUS_TX_PIN = 17;
static constexpr uint8_t SENSOR_ADDRESS = 1;

esp32ModbusRTU modbusMaster(&Serial1);

// Bridge the single-argument RTU error callback to the library router.
static void modbusErrorHandler(esp32Modbus::Error error) {
    handleError(0, error);
}

// Demonstrate the using-directive form of namespace access.
using namespace modbus;

/**
 * @brief A simple two-channel analog sensor.
 *
 * SimpleModbusDevice requires configure() (declare channels) and
 * readChannelData() (refresh values). Both build on the synchronous
 * readHoldingRegisters() -> ModbusResult<std::vector<uint16_t>> API.
 */
class EnvironmentSensor : public SimpleModbusDevice {
public:
    explicit EnvironmentSensor(uint8_t addr) : SimpleModbusDevice(addr) {}

protected:
    bool configure() override {
        // Probe presence with a synchronous read.
        auto id = readHoldingRegisters(0x0000, 1);
        if (!id.isOk()) {
            return false;
        }
        addChannel("Temperature", "C", 0x0001);
        addChannel("Humidity", "%RH", 0x0002);
        return true;
    }

    bool readChannelData() override {
        // Read both channel registers in one transaction.
        auto regs = readHoldingRegisters(0x0001, 2);
        if (!regs.isOk()) {
            return false;
        }
        values.clear();
        for (uint16_t raw : regs.value()) {
            values.push_back(static_cast<int32_t>(raw));
        }
        return true;
    }

public:
    // Scale raw counts (0.1 per LSB) into engineering units.
    ModbusResult<float> getFloat(size_t channel = 0) const override {
        if (channel >= values.size()) {
            return ModbusResult<float>::error(ModbusError::INVALID_PARAMETER);
        }
        return ModbusResult<float>::ok(values[channel] / 10.0f);
    }
};

static EnvironmentSensor sensor(SENSOR_ADDRESS);

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("ESP32-ModbusDevice :: namespace demo");

    Serial1.begin(MODBUS_BAUD_RATE, SERIAL_8N1, MODBUS_RX_PIN, MODBUS_TX_PIN);

    // Fully-qualified access to the registry singleton.
    modbus::ModbusRegistry::getInstance().setModbusRTU(&modbusMaster);
    modbusMaster.onData(mainHandleData);
    modbusMaster.onError(modbusErrorHandler);
    modbusMaster.begin();

    if (sensor.initialize()) {
        Serial.printf("Sensor ready with %u channels\n",
                      static_cast<unsigned>(sensor.getChannelCount()));
    } else {
        Serial.printf("Init failed: %s\n",
                      getModbusErrorString(sensor.getLastError()));
    }
}

void loop() {
    // update() refreshes all channels via the synchronous read path.
    auto upd = sensor.update();
    if (upd.isOk()) {
        for (size_t ch = 0; ch < sensor.getChannelCount(); ++ch) {
            auto val = sensor.getFloat(ch);
            if (val.isOk()) {
                Serial.printf("%s: %.1f %s\n",
                              sensor.getChannelName(ch),
                              val.value(),
                              sensor.getChannelUnits(ch));
            }
        }
    } else {
        Serial.printf("Update failed: %s\n", getModbusErrorString(upd.error()));
    }

    delay(2000);
}
