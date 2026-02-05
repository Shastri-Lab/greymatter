# greymatter DAC controller

Embedded firmware for the Raspberry Pi Pico 2 (RP2350) controlling a multi-board DAC system for precision analog output generation. Designed for photonic neural network research applications.

## Features

- **24 DAC Control**: 8 daughter boards, each with 3 DACs (2x current, 1x voltage)
- **High Channel Count**: 112 current outputs + 32 voltage outputs = 144 total channels
- **SCPI Interface**: Standard instrument control commands over USB serial
- **Precision Output**:
  - Current: 3.125 mA to 300 mA full-scale (LTC2662)
  - Voltage: ±2.5V to ±10V bipolar, 0-10V unipolar (LTC2664)
- **16-bit Resolution**: 65,536 steps per channel
- **Fault Monitoring**: Per-DAC fault detection with interrupt support

## Hardware Requirements

- Raspberry Pi Pico 2 (RP2350)
- Greymatter motherboard with:
  - 3x MCP23S17 I/O expanders
  - TXB0106 level shifter
  - 8 daughter board slots
- Microcyte daughter boards (per slot):
  - 2x LTC2662 current DAC (5 channels each)
  - 1x LTC2664 voltage DAC (4 channels)

## Quick Start

### Prerequisites

```bash
# Set Pico SDK path
export PICO_SDK_PATH=/path/to/pico-sdk

# Required tools
# - CMake 3.13+
# - GCC ARM cross-compiler (gcc-15 recommended)
```

### Build

```bash
cd build
cmake .. -DPICO_BOARD=pico2
make picotoolBuild
C_INCLUDE_PATH= CPLUS_INCLUDE_PATH= make

# Optional: Custom SPI clock rate (default 10 MHz)
cmake .. -DPICO_BOARD=pico2 -DSPI_BAUDRATE=1000000  # 1 MHz for logic analyzer
make picotoolBuild && C_INCLUDE_PATH= CPLUS_INCLUDE_PATH= make
```

### Flash

1. Hold the **BOOTSEL** button on the Pico 2
2. Connect USB cable to computer
3. Copy `build/greymatter.uf2` to the `RPI-RP2` drive that appears
4. The Pico will reset and start running automatically

### Connect

```bash
# macOS/Linux
screen /dev/tty.usbmodem1101 115200

# Exit screen: Ctrl-A, K, Y
```

### Verify

```
*IDN?
```
Response: `GreyMatter,DAC Controller,001,0.1`

## Basic Usage

```bash
# Set voltage output (Board 0, DAC 2, Channel 0 to 5V)
BOARD0:DAC2:CH0:VOLT 5.0

# Set current output (Board 0, DAC 0, Channel 1 to 50mA)
BOARD0:DAC0:CH1:CURR 50.0

# Check for faults
FAULT?

# Reset all outputs
*RST
```

## Documentation

The **[user guide](docs/USER_GUIDE.md)** for the firmware contains a complete command reference, implementation details, calibration routines, and more details.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     USB Serial (115200 baud)                │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                     SCPI Parser                             │
│              Command tokenization & validation              │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                     Board Manager                           │
│              Routes commands to 24 DACs                     │
└─────────────────────────────────────────────────────────────┘
                              │
              ┌───────────────┴───────────────┐
              ▼                               ▼
┌──────────────────────┐         ┌──────────────────────┐
│      LTC2662         │         │      LTC2664         │
│  Current DAC Driver  │         │  Voltage DAC Driver  │
│  (5 ch, 3-300 mA)    │         │  (4 ch, ±10V)        │
└──────────────────────┘         └──────────────────────┘
              │                               │
              └───────────────┬───────────────┘
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                     SPI Manager                             │
│              Transaction coordination                       │
└─────────────────────────────────────────────────────────────┘
                              │
              ┌───────────────┴───────────────┐
              ▼                               ▼
┌──────────────────────┐         ┌──────────────────────┐
│    I/O Expanders     │         │    DAC Decoder       │
│  (MCP23S17 × 3)      │         │    (CS routing)      │
└──────────────────────┘         └──────────────────────┘
```

## Pin Assignments

| GPIO | Function | Description |
|------|----------|-------------|
| GP16 | SPI_MISO | Data from DACs/expanders |
| GP17 | SPI_CS | Chip select (expanders only) |
| GP18 | SPI_CLK | SPI clock (configurable, default 10 MHz) |
| GP19 | SPI_MOSI | Data to DACs/expanders |
| GP20 | FAULT | Combined fault input |
| GP21 | LEVEL_SHIFT_OE | Level shifter enable |
| GP22 | EXPANDER_RESET | I/O expander reset |

## Project Structure

```
greymatter/
├── main.cpp              # Entry point, USB serial loop
├── scpi_parser.*         # SCPI command parsing
├── board_manager.*       # DAC routing and management
├── spi_manager.*         # SPI peripheral control
├── io_expander.*         # MCP23S17 driver
├── dac_device.*          # Abstract DAC interface
├── ltc2662.*             # Current DAC driver
├── ltc2664.*             # Voltage DAC driver
├── utils.*               # String parsing utilities
├── CMakeLists.txt        # Build configuration
├── docs/                 # Hardware programming guides
├── README.md             # This file
└── USER_GUIDE.md         # Detailed user documentation
```


## License
This software is licensed under the MIT license found at [LICENSE](LICENSE).
