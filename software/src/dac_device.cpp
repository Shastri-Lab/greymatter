#include "dac_device.hpp"
#include "spi_manager.hpp"

void DacDevice::send_command(uint8_t command, uint8_t address, uint16_t data) {
    // Build 24-bit command: [Command(4) | Address(4) | Data(16)]
    uint8_t tx_buf[3] = {
        static_cast<uint8_t>(((command & 0x0F) << 4) | (address & 0x0F)),
        static_cast<uint8_t>((data >> 8) & 0xFF),
        static_cast<uint8_t>(data & 0xFF)
    };

    spi_->transaction(board_id_, device_id_, tx_buf, nullptr, 3);
}

void DacDevice::send_command_read24(uint8_t command, uint8_t address, uint16_t data, uint8_t rx[3]) {
    uint8_t tx_buf[3] = {
        static_cast<uint8_t>(((command & 0x0F) << 4) | (address & 0x0F)),
        static_cast<uint8_t>((data >> 8) & 0xFF),
        static_cast<uint8_t>(data & 0xFF)
    };
    spi_->transaction(board_id_, device_id_, tx_buf, rx, 3);
}

void DacDevice::send_command_read32(uint8_t command, uint8_t address, uint16_t data, uint8_t rx[4]) {
    uint8_t tx_buf[4] = {
        0x00,  // Leading 8 zero bits for 32-bit mode
        static_cast<uint8_t>(((command & 0x0F) << 4) | (address & 0x0F)),
        static_cast<uint8_t>((data >> 8) & 0xFF),
        static_cast<uint8_t>(data & 0xFF)
    };
    spi_->transaction(board_id_, device_id_, tx_buf, rx, 4);
}