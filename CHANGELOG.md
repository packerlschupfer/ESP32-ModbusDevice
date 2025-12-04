# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2025-12-04

### Added
- Initial public release
- ModbusDevice base class for Modbus RTU device implementations
- QueuedModbusDevice for async queue-based operations
- ModbusRegistry for device callback registration
- Result<T> based error handling using LibraryCommon
- Modbus function support (read/write holding registers, input registers, coils)
- Initialization phases (UNINITIALIZED, CONFIGURING, READY, ERROR)
- Response callback system via ModbusRegistry
- Thread-safe operations with mutex protection
- Integration with IDeviceInstance interface
- Automatic retry and error recovery
- CRC validation and timeout handling

Platform: ESP32 (Arduino/ESP-IDF)
Hardware: RS485 transceiver for Modbus RTU
License: GPL-3
Dependencies: MutexGuard, LibraryCommon, ESP32-IDeviceInstance, esp32ModbusRTU (external)

### Notes
- Production-tested as foundation for MB8ART, RYN4, ANDRTF3 drivers
- Stable Modbus communication in industrial environment
- Previous internal versions (v1.x-v2.x) not publicly released
- Reset to v0.1.0 for clean public release start
