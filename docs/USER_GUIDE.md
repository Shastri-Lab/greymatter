# GreyMatter DAC Controller - User Guide

This guide provides comprehensive documentation for operating the GreyMatter DAC Controller, including all available commands, addressing schemes, and detailed implementation information.

## Table of Contents

1. [System Overview](#system-overview)
2. [Getting Started](#getting-started)
3. [Command Reference](#command-reference)
4. [Addressing Scheme](#addressing-scheme)
5. [Span Configuration](#span-configuration)
6. [Code Flow and Implementation Details](#code-flow-and-implementation-details)
7. [Hardware Architecture](#hardware-architecture)
8. [Troubleshooting](#troubleshooting)

---

## System Overview

The GreyMatter DAC Controller manages 24 Digital-to-Analog Converters distributed across 8 daughter boards. Each board contains:

- **2x LTC2662**: 5-channel current DACs (DAC 0 and DAC 1)
- **1x LTC2664**: 4-channel voltage DAC (DAC 2)

**Total outputs**: 80 current channels + 32 voltage channels = 112 independent analog outputs

### DAC Specifications

| DAC Type | Channels | Output Type | Range | Resolution |
|----------|----------|-------------|-------|------------|
| LTC2662 | 5 | Current | 3.125 mA - 300 mA | 16-bit |
| LTC2664 | 4 | Voltage | ±10V / 0-10V | 16-bit |

---

## Getting Started

### Connecting

1. Flash the firmware (see [README.md](README.md))
2. Connect via USB serial at **115200 baud**
3. Wait for the startup banner

```bash
# macOS/Linux
screen /dev/tty.usbmodem1101 115200

# Windows (use PuTTY or similar)
# Select appropriate COM port, 115200 baud
```

### Startup Sequence

On power-up, the controller:
1. Initializes USB CDC serial interface
2. Waits for serial connection
3. Configures SPI and GPIO peripherals
4. Initializes all 24 DACs with default spans
5. Reports any detected faults
6. Enters command loop

### First Commands

```
*IDN?                              # Verify connection
FAULT?                             # Check for hardware faults
BOARD0:DAC2:CH0:VOLT 1.0           # Set a voltage output
BOARD0:DAC0:CH0:CURR 10.0          # Set a current output
```

---

## Command Reference

All commands follow the SCPI (Standard Commands for Programmable Instruments) standard. Commands are case-insensitive and terminated with newline (`\n`).

### IEEE 488.2 Common Commands

| Command | Description | Response |
|---------|-------------|----------|
| `*IDN?` | Query device identification | `GreyMatter,DAC Controller,001,0.1` |
| `*RST` | Reset all DACs to power-on state | `OK` |

### System Commands

| Command | Description | Response |
|---------|-------------|----------|
| `FAULT?` | Query 24-bit fault status | `OK` or `FAULT:0xNNNNNN` |
| `SYST:ERR?` | Query error queue | `0,No error` |
| `LDAC` | Pulse LDAC to update all outputs | `OK` |
| `UPDATE:ALL` | Update all DAC outputs | `OK` |

### Voltage Commands (LTC2664 - DAC 2 only)

| Command | Format | Example |
|---------|--------|---------|
| Set Voltage | `BOARD<n>:DAC2:CH<c>:VOLT <value>` | `BOARD0:DAC2:CH0:VOLT 5.0` |
| Set Span | `BOARD<n>:DAC2:SPAN <code>` | `BOARD0:DAC2:SPAN 3` |
| Set All Spans | `BOARD<n>:DAC2:SPAN:ALL <code>` | `BOARD0:DAC2:SPAN:ALL 3` |

### Current Commands (LTC2662 - DAC 0, DAC 1)

| Command | Format | Example |
|---------|--------|---------|
| Set Current | `BOARD<n>:DAC<0-1>:CH<c>:CURR <value>` | `BOARD0:DAC0:CH0:CURR 50.0` |
| Set Span | `BOARD<n>:DAC<0-1>:SPAN <code>` | `BOARD0:DAC0:SPAN 6` |
| Set All Spans | `BOARD<n>:DAC<0-1>:SPAN:ALL <code>` | `BOARD0:DAC0:SPAN:ALL 6` |

### Raw Code Commands (All DACs)

| Command | Format | Example |
|---------|--------|---------|
| Set Code | `BOARD<n>:DAC<m>:CH<c>:CODE <value>` | `BOARD0:DAC0:CH0:CODE 32767` |
| Update DAC | `BOARD<n>:DAC<m>:UPDATE` | `BOARD0:DAC0:UPDATE` |

### Power Management

| Command | Format | Description |
|---------|--------|-------------|
| Power Down Channel | `BOARD<n>:DAC<m>:CH<c>:PDOWN` | Powers down single channel |
| Power Down Chip | `BOARD<n>:DAC<m>:PDOWN` | Powers down entire DAC |

### Command Examples

```bash
# === Voltage Output Examples ===

# Set Board 0, Voltage DAC, Channel 0 to +5V
BOARD0:DAC2:CH0:VOLT 5.0

# Set Board 3, Voltage DAC, Channel 2 to -3.3V (bipolar mode)
BOARD3:DAC2:CH2:VOLT -3.3

# Configure all voltage channels on Board 0 to ±10V range
BOARD0:DAC2:SPAN:ALL 3


# === Current Output Examples ===

# Set Board 0, Current DAC 0, Channel 1 to 50 mA
BOARD0:DAC0:CH1:CURR 50.0

# Set Board 5, Current DAC 1, Channel 4 to 100 mA
BOARD5:DAC1:CH4:CURR 100.0

# Configure all current channels on Board 2, DAC 0 to 200 mA range
BOARD2:DAC0:SPAN:ALL 7


# === Raw Code Examples ===

# Set mid-scale (32767 = 50% of full scale)
BOARD0:DAC0:CH0:CODE 32767

# Set maximum (65535 = 100% of full scale)
BOARD0:DAC0:CH0:CODE 65535

# Set minimum (0 = 0% of full scale)
BOARD0:DAC0:CH0:CODE 0


# === System Commands ===

# Check device identity
*IDN?

# Reset all DACs
*RST

# Check for faults
FAULT?

# Global LDAC pulse (update all pending outputs)
LDAC

# Power down a channel
BOARD0:DAC0:CH0:PDOWN

# Power down entire DAC chip
BOARD0:DAC0:PDOWN
```

---

## Addressing Scheme

### Board-DAC-Channel Hierarchy

```
BOARD<n>:DAC<m>:CH<c>
  │       │      │
  │       │      └── Channel: 0-4 (LTC2662) or 0-3 (LTC2664)
  │       │
  │       └── DAC: 0, 1 (current DAC - LTC2662)
  │                 2 (voltage DAC - LTC2664)
  │
  └── Board: 0-7 (8 daughter boards)
```

### DAC Index Calculation

Internally, DACs are indexed 0-23:

```
dac_index = (board_id × 3) + device_id
```

| Board | DAC 0 (LTC2662) | DAC 1 (LTC2662) | DAC 2 (LTC2664) |
|-------|-----------------|-----------------|-----------------|
| 0 | Index 0 | Index 1 | Index 2 |
| 1 | Index 3 | Index 4 | Index 5 |
| 2 | Index 6 | Index 7 | Index 8 |
| 3 | Index 9 | Index 10 | Index 11 |
| 4 | Index 12 | Index 13 | Index 14 |
| 5 | Index 15 | Index 16 | Index 17 |
| 6 | Index 18 | Index 19 | Index 20 |
| 7 | Index 21 | Index 22 | Index 23 |

### Channel Mapping

**LTC2662 (Current DAC)**:
- 5 channels: CH0, CH1, CH2, CH3, CH4
- Total per board: 10 current channels

**LTC2664 (Voltage DAC)**:
- 4 channels: CH0, CH1, CH2, CH3
- Total per board: 4 voltage channels

---

## Span Configuration

### LTC2662 Current DAC Spans

| Code | Full-Scale Current | Notes |
|------|-------------------|-------|
| 0x0 | Hi-Z (disabled) | Output disabled |
| 0x1 | 3.125 mA | |
| 0x2 | 6.25 mA | |
| 0x3 | 12.5 mA | |
| 0x4 | 25 mA | |
| 0x5 | 50 mA | |
| 0x6 | 100 mA | **Default** |
| 0x7 | 200 mA | |
| 0x8 | Switch to V- | Special mode |
| 0xF | 300 mA | Maximum |

**Example**: Set 200 mA range on Board 0, DAC 0:
```
BOARD0:DAC0:SPAN:ALL 7
```

### LTC2664 Voltage DAC Spans

| Code | Range | Type | Min | Max |
|------|-------|------|-----|-----|
| 0 | 0V to 5V | Unipolar | 0V | +5V |
| 1 | 0V to 10V | Unipolar | 0V | +10V |
| 2 | ±5V | Bipolar | -5V | +5V |
| 3 | ±10V | Bipolar | -10V | +10V |
| 4 | ±2.5V | Bipolar | -2.5V | +2.5V |

**Default**: ±10V (code 3)

**Example**: Set ±5V range on Board 0, DAC 2:
```
BOARD0:DAC2:SPAN:ALL 2
```

### Output Calculations

**Current Output (LTC2662)**:
```
I_out = (code / 65535) × I_fullscale
```

**Voltage Output (LTC2664)**:
```
Unipolar: V_out = (code / 65535) × V_max
Bipolar:  V_out = V_min + (code / 65535) × (V_max - V_min)
```

---

## Code Flow and Implementation Details

This section describes the internal implementation for developers and advanced users.

### 1. System Initialization

When the firmware starts, the following initialization sequence occurs:

```
main()
│
├─► stdio_init_all()
│   Initialize USB CDC for serial communication
│
├─► Wait for tud_cdc_connected()
│   Block until a serial terminal connects
│
├─► SpiManager::init()
│   │
│   ├─► init_gpio()
│   │   ├── GP21 = HIGH (enable TXB0106 level shifter) [CRITICAL FIRST]
│   │   ├── GP22 = output (expander reset)
│   │   ├── GP20 = input + pull-up (fault detection)
│   │   └── GP17 = output, HIGH (SPI chip select)
│   │
│   ├─► init_spi()
│   │   ├── spi_init(spi0, 10_000_000)  // 10 MHz
│   │   ├── Configure GP16 (MISO), GP18 (CLK), GP19 (MOSI)
│   │   └── SPI Mode 0 (CPOL=0, CPHA=0)
│   │
│   ├─► reset_io_expanders()
│   │   ├── GP22 = LOW for 10µs
│   │   └── GP22 = HIGH, wait 100µs
│   │
│   └─► IoExpander::init(spi0)
│       ├── Enable HAEN on all expanders (address broadcast)
│       ├── Configure EXPANDER_0 (CS bits, D_EN, LDAC, CLR)
│       ├── Configure EXPANDER_1 (FAULT0-15 inputs)
│       └── Configure EXPANDER_2 (FAULT16-23 inputs)
│
├─► BoardManager::init_all()
│   │
│   └─► For each board (0-7):
│       ├── Create LTC2662 instances (DAC 0, 1)
│       │   └── init(): set_span_all(0x6), update_all()
│       │
│       └── Create LTC2664 instance (DAC 2)
│           └── init(): set_span_all(0x3), update_all()
│
├─► Check initial fault status
│   └── If GP20 is LOW, read and report fault mask
│
└─► Enter main command loop
```

### 2. Command Processing Flow

```
main loop
│
├─► Read line from USB serial
│   └── Accumulate characters until '\n' or '\r'
│
├─► ScpiParser::parse(line)
│   │
│   ├─► Skip leading whitespace
│   │
│   ├─► Try parse_common_command()
│   │   └── Match *IDN?, *RST
│   │
│   ├─► Try parse_system_command()
│   │   └── Match FAULT?, SYST:ERR?, LDAC, UPDATE:ALL
│   │
│   └─► Try parse_board_command()
│       ├── Extract BOARD<n>
│       ├── Extract :DAC<m>
│       └── Parse subcommand:
│           ├── CH<c>:VOLT, CH<c>:CURR, CH<c>:CODE
│           ├── CH<c>:PDOWN
│           ├── SPAN, SPAN:ALL
│           ├── UPDATE
│           └── PDOWN
│
├─► BoardManager::execute(cmd)
│   │
│   └─► Switch on command type:
│       │
│       ├─► SET_VOLTAGE
│       │   ├── Validate: DAC must be 2 (LTC2664)
│       │   ├── Get span range from LTC2664_SPAN_INFO[]
│       │   ├── Clamp voltage to range
│       │   ├── Calculate: code = normalize(voltage) × 65535
│       │   └── dac->send_command(WRITE_UPDATE_N, channel, code)
│       │
│       ├─► SET_CURRENT
│       │   ├── Validate: DAC must be 0 or 1 (LTC2662)
│       │   ├── Get full-scale from span setting
│       │   ├── Clamp current to 0..full_scale
│       │   ├── Calculate: code = (current / full_scale) × 65535
│       │   └── dac->send_command(WRITE_UPDATE_N, channel, code)
│       │
│       ├─► SET_CODE
│       │   └── dac->send_command(WRITE_UPDATE_N, channel, code)
│       │
│       ├─► SET_SPAN / SET_ALL_SPAN
│       │   ├── Cache span in dac->span_[channel]
│       │   └── dac->send_command(WRITE_SPAN_N, channel, span_code)
│       │
│       ├─► UPDATE / UPDATE_ALL
│       │   ├── dac->send_command(UPDATE_ALL, 0, 0)
│       │   └── io_expander.pulse_ldac()
│       │
│       ├─► FAULT_QUERY
│       │   ├── Read EXPANDER_1 GPIO (faults 0-15)
│       │   ├── Read EXPANDER_2 GPIO (faults 16-23)
│       │   └── Invert and combine (active-low → active-high)
│       │
│       └─► POWER_DOWN / POWER_DOWN_CHIP
│           └── dac->send_command(POWER_DOWN_N/CHIP, ...)
│
└─► Send response via USB serial
```

### 3. SPI Transaction Flow

Every DAC command involves a multi-step SPI transaction:

```
SpiManager::transaction(board_id, device_id, tx_buf, rx_buf, len)
│
├─► Step 1: Select DAC via I/O Expander
│   │
│   └─► IoExpander::set_dac_select(board_id, device_id)
│       ├── Calculate dac_index = board_id × 3 + device_id
│       ├── Set CS0-CS4 bits (5-bit address)
│       ├── Set D_EN bit HIGH (enable decoder tree)
│       ├── Write to EXPANDER_0 Port A
│       └── sleep_us(1)  // Decoder settling
│
├─► Step 2: SPI Transfer to DAC
│   │
│   ├── NOTE: CS (GP17) is NOT asserted here
│   │   The decoder tree provides chip select to the DAC
│   │
│   └── spi_write_read_blocking(spi0, tx_buf, rx_buf, len)
│
├─► sleep_us(1)  // Data latch time
│
└─► Step 3: Deselect DAC
    │
    └─► IoExpander::deselect_dac()
        ├── Clear D_EN bit (disable decoder)
        └── Write to EXPANDER_0 Port A
```

### 4. DAC Command Format (24-bit SPI)

```
┌─────────────────────────────────────────────────────────────┐
│  Byte 0 (MSB)      │  Byte 1          │  Byte 2 (LSB)      │
├────────┬───────────┼──────────────────┼────────────────────┤
│ Command│  Address  │    Data[15:8]    │    Data[7:0]       │
│  [7:4] │   [3:0]   │                  │                    │
└────────┴───────────┴──────────────────┴────────────────────┘

DacDevice::send_command(command, address, data):
    tx_buf[0] = ((command & 0x0F) << 4) | (address & 0x0F)
    tx_buf[1] = (data >> 8) & 0xFF
    tx_buf[2] = data & 0xFF
```

**Command Codes**:

| Code | Name | Description |
|------|------|-------------|
| 0x0 | WRITE_N | Write code to input register |
| 0x1 | UPDATE_N | Copy input to DAC register |
| 0x2 | WRITE_ALL_UPDATE_ALL | Write all, update all |
| 0x3 | WRITE_UPDATE_N | Write and update single channel |
| 0x4 | POWER_DOWN_N | Power down channel |
| 0x5 | POWER_DOWN_CHIP | Power down entire chip |
| 0x6 | WRITE_SPAN_N | Set span for channel |
| 0x7 | CONFIG | Configuration register |
| 0x8 | WRITE_ALL | Write to all channels |
| 0x9 | UPDATE_ALL | Update all channels |
| 0xA | WRITE_ALL_UPDATE_ALL | Write all, update all |
| 0xB | MUX | Monitor/analog mux |
| 0xE | WRITE_SPAN_ALL | Set span for all channels |
| 0xF | NOP | No operation |

### 5. I/O Expander Operations

The three MCP23S17 expanders are addressed via hardware pins:

```
EXPANDER_0 (Address 0x0): Control signals
├── Port A [7:0]:
│   ├── [4:0] CS0-CS4: 5-bit DAC address
│   ├── [5]   D_EN: Decoder enable
│   ├── [6]   LDAC: Load DAC (active-low)
│   └── [7]   CLR: Clear (active-low)
└── Port B [7:0]: Reserved

EXPANDER_1 (Address 0x1): Fault inputs 0-15
├── Port A [7:0]: FAULT0-7 (active-low)
└── Port B [7:0]: FAULT8-15 (active-low)

EXPANDER_2 (Address 0x2): Fault inputs 16-23
├── Port A [7:0]: FAULT16-23 (active-low)
└── Port B [7:0]: Reserved
```

**MCP23S17 SPI Protocol**:

```
Write: [0x40 | (addr << 1) | 0] [register] [data]
Read:  [0x40 | (addr << 1) | 1] [register] → [data]
```

### 6. Fault Detection

```
Fault Monitoring:
│
├─► Hardware: GP20 connected to OR'd FAULT outputs
│   └── Active-low: LOW = at least one fault
│
├─► FAULT? Command:
│   ├── Check GP20 state
│   │
│   ├── If HIGH (no fault):
│   │   └── Return "OK"
│   │
│   └── If LOW (fault present):
│       ├── Read EXPANDER_1 Port A (faults 0-7)
│       ├── Read EXPANDER_1 Port B (faults 8-15)
│       ├── Read EXPANDER_2 Port A (faults 16-23)
│       ├── Invert bits (active-low → active-high)
│       ├── Mask to 24 bits
│       └── Return "FAULT:0xNNNNNN"
│
└─► Bit Position Mapping:
    Bit 0  → Board 0, DAC 0
    Bit 1  → Board 0, DAC 1
    Bit 2  → Board 0, DAC 2
    ...
    Bit 23 → Board 7, DAC 2
```

---

## Hardware Architecture

### Block Diagram

```
                                    ┌─────────────────────────────────────┐
                                    │         Daughter Board 0            │
                                    │  ┌─────────┐ ┌─────────┐ ┌────────┐ │
                              ┌────►│  │ LTC2662 │ │ LTC2662 │ │LTC2664 │ │
                              │     │  │ DAC 0   │ │ DAC 1   │ │ DAC 2  │ │
                              │     │  │ 5ch I   │ │ 5ch I   │ │ 4ch V  │ │
                              │     │  └─────────┘ └─────────┘ └────────┘ │
┌──────────────┐              │     └─────────────────────────────────────┘
│              │              │                        ⋮
│   RP2350     │    SPI       │     ┌─────────────────────────────────────┐
│  (Pico 2)    │◄────────────►│     │         Daughter Board 7            │
│              │              │     │  ┌─────────┐ ┌─────────┐ ┌────────┐ │
│  GP16 MISO   │              └────►│  │ LTC2662 │ │ LTC2662 │ │LTC2664 │ │
│  GP17 CS     │                    │  │ DAC 21  │ │ DAC 22  │ │ DAC 23 │ │
│  GP18 CLK    │     ┌──────────┐   │  │ 5ch I   │ │ 5ch I   │ │ 4ch V  │ │
│  GP19 MOSI   │────►│ TXB0106  │   │  └─────────┘ └─────────┘ └────────┘ │
│              │     │ Level    │   └─────────────────────────────────────┘
│  GP20 FAULT◄─┼─────│ Shifter  │
│  GP21 OE────►│     └──────────┘
│  GP22 RST───►│            │
│              │            ▼
└──────────────┘   ┌──────────────────────────────────┐
                   │     MCP23S17 I/O Expanders       │
                   │ ┌────────┐ ┌────────┐ ┌────────┐ │
                   │ │ EXP 0  │ │ EXP 1  │ │ EXP 2  │ │
                   │ │CS,LDAC │ │FAULT   │ │FAULT   │ │
                   │ │CLR,DEN │ │ 0-15   │ │ 16-23  │ │
                   │ └────────┘ └────────┘ └────────┘ │
                   └──────────────────────────────────┘
                              │
                              ▼
                   ┌──────────────────┐
                   │  Decoder Tree    │
                   │  (5-to-24 CS)    │
                   └──────────────────┘
```

### Signal Flow

1. **Commands** arrive via USB serial (115200 baud)
2. **SCPI Parser** tokenizes and validates the command
3. **Board Manager** routes to the appropriate DAC driver
4. **DAC Driver** constructs the 24-bit SPI command
5. **SPI Manager** orchestrates the multi-step transaction:
   - Sets decoder address via I/O expander
   - Sends SPI data to DAC
   - Deselects DAC
6. **Response** sent back via USB serial

### Critical Timing

| Operation | Minimum Time | Code Implementation |
|-----------|--------------|---------------------|
| Level shifter enable to SPI | 0 µs | Set GP21 first thing in init |
| Expander reset pulse | 10 µs low | `sleep_us(10)` |
| Expander reset recovery | 100 µs | `sleep_us(100)` |
| Decoder settling | 1 µs | `sleep_us(1)` |
| DAC data latch | 1 µs | `sleep_us(1)` |
| LDAC pulse width | 20 ns min | `sleep_us(1)` |

---

## Troubleshooting

### No Response from Device

1. **Check USB connection**: Device should enumerate as USB CDC
2. **Verify baud rate**: Must be 115200
3. **Check terminal settings**: No hardware flow control
4. **Try `*IDN?`**: Should return identification string

### FAULT? Returns Non-Zero

The fault mask indicates which DACs have faults:

```
FAULT:0x000001  → DAC 0 (Board 0, DAC 0) has fault
FAULT:0x000004  → DAC 2 (Board 0, DAC 2) has fault
FAULT:0x800000  → DAC 23 (Board 7, DAC 2) has fault
```

**Common causes**:
- Overcurrent condition (LTC2662)
- Thermal shutdown
- Output shorted
- Power supply issue

### Output Not Changing

1. **Check span setting**: Output must be within configured span
2. **Verify addressing**: Board (0-7), DAC (0-2), Channel varies by DAC type
3. **Try raw code**: `BOARD0:DAC0:CH0:CODE 32767` (mid-scale)
4. **Issue LDAC**: `LDAC` to update all outputs

### Current/Voltage Out of Range

The firmware clamps values to the configured span:

```bash
# If span is 100 mA (code 6):
BOARD0:DAC0:CH0:CURR 200.0  # Clamped to 100 mA

# If span is ±5V (code 2):
BOARD0:DAC2:CH0:VOLT 8.0    # Clamped to +5V
```

**Solution**: Set appropriate span first:
```bash
BOARD0:DAC0:SPAN:ALL 7      # 200 mA range
BOARD0:DAC2:SPAN:ALL 3      # ±10V range
```

### Build Errors

1. **Check PICO_SDK_PATH**: Must point to valid Pico SDK installation
2. **Verify compiler**: GCC ARM cross-compiler required
3. **Clean build**: Delete `build/` directory and rebuild

```bash
rm -rf build
mkdir build
cd build
cmake .. -DPICO_BOARD=pico2
make
```

---

## Appendix: Source File Reference

| File | Lines | Purpose |
|------|-------|---------|
| `main.cpp` | 113 | Entry point, USB serial loop |
| `scpi_parser.cpp` | 210 | SCPI command parsing |
| `scpi_parser.hpp` | 94 | Parser types and interface |
| `board_manager.cpp` | 188 | DAC routing and execution |
| `board_manager.hpp` | 82 | Manager interface |
| `spi_manager.cpp` | 140 | SPI peripheral control |
| `spi_manager.hpp` | 57 | SPI interface |
| `io_expander.cpp` | 300 | MCP23S17 driver |
| `io_expander.hpp` | 135 | Expander interface |
| `dac_device.cpp` | 36 | Abstract DAC base class |
| `dac_device.hpp` | 39 | DAC interface |
| `ltc2662.cpp` | 128 | Current DAC driver |
| `ltc2662.hpp` | 58 | LTC2662 interface |
| `ltc2664.cpp` | 130 | Voltage DAC driver |
| `ltc2664.hpp` | 57 | LTC2664 interface |
| `utils.cpp` | 86 | String utilities |
| `utils.hpp` | 49 | Utility interface |

**Total**: ~2,100 lines of C++ code
