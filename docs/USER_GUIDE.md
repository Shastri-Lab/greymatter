# greymatter DAC controller - User Guide

This guide provides comprehensive documentation for operating the greymatter DAC Controller, including all available commands, addressing schemes, calibration procedures, and detailed implementation information.

## Table of Contents

1. [System Overview](#system-overview)
2. [Getting Started](#getting-started)
3. [Command Reference](#command-reference)
4. [Addressing Scheme](#addressing-scheme)
5. [Span Configuration](#span-configuration)
6. [Calibration](#calibration)
7. [Code Flow and Implementation Details](#code-flow-and-implementation-details)
8. [Hardware Architecture](#hardware-architecture)
9. [Troubleshooting](#troubleshooting)

---

## System Overview

The greymatter DAC Controller manages 24 Digital-to-Analog Converters distributed across 8 daughter boards. Each board contains:

- **2x LTC2662**: 5-channel current DACs (DAC 0 and DAC 1)
- **1x LTC2664**: 4-channel voltage DAC (DAC 2)

**Total outputs**: 80 current channels + 32 voltage channels = 112 independent analog outputs

### DAC Specifications

| DAC Type | Channels | Output Type | Range | Resolution |
|----------|----------|-------------|-------|------------|
| LTC2662 | 5 | Current | 3.125 mA - 300 mA | 12 or 16-bit |
| LTC2664 | 4 | Voltage | ±10V / 0-10V | 12 or 16-bit |

---

## Getting Started

### Connecting

1. Flash the firmware (see [README.md](../README.md))
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
5. Loads calibration data from flash (if present)
6. Reports any detected faults
7. Enters command loop

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
| `*IDN?` | Query device identification | `greymatter,DAC Controller,001,0.1` |
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
| Set Channel Span | `BOARD<n>:DAC2:CH<c>:SPAN <code>` | `BOARD0:DAC2:CH0:SPAN 3` |
| Set All Spans | `BOARD<n>:DAC2:SPAN:ALL <code>` | `BOARD0:DAC2:SPAN:ALL 3` |

### Current Commands (LTC2662 - DAC 0, DAC 1)

| Command | Format | Example |
|---------|--------|---------|
| Set Current | `BOARD<n>:DAC<0-1>:CH<c>:CURR <value>` | `BOARD0:DAC0:CH0:CURR 50.0` |
| Set Channel Span | `BOARD<n>:DAC<0-1>:CH<c>:SPAN <code>` | `BOARD0:DAC0:CH0:SPAN 6` |
| Set All Spans | `BOARD<n>:DAC<0-1>:SPAN:ALL <code>` | `BOARD0:DAC0:SPAN:ALL 6` |

### Raw Code Commands (All DACs)

| Command | Format | Example |
|---------|--------|---------|
| Set Code | `BOARD<n>:DAC<m>:CH<c>:CODE <value>` | `BOARD0:DAC0:CH0:CODE 32767` |
| Update DAC | `BOARD<n>:DAC<m>:UPDATE` | `BOARD0:DAC0:UPDATE` |

### Resolution Commands

| Command | Format | Description |
|---------|--------|-------------|
| Query Resolution | `BOARD<n>:DAC<m>:RES?` | Returns `12` or `16` |
| Set Resolution | `BOARD<n>:DAC<m>:RES <12\|16>` | Re-initializes DAC with new resolution |

### Power Management

| Command | Format | Description |
|---------|--------|-------------|
| Power Down Channel | `BOARD<n>:DAC<m>:CH<c>:PDOWN` | Powers down single channel |
| Power Down Chip | `BOARD<n>:DAC<m>:PDOWN` | Powers down entire DAC |

### Calibration Commands

| Command | Description | Response |
|---------|-------------|----------|
| `BOARD<n>:SN <string>` | Set board serial number | `OK` |
| `BOARD<n>:SN?` | Query board serial number | Serial number or `(not set)` |
| `BOARD<n>:DAC<m>:CH<c>:CAL:GAIN <value>` | Set gain factor | `OK` |
| `BOARD<n>:DAC<m>:CH<c>:CAL:GAIN?` | Query gain factor | Gain value |
| `BOARD<n>:DAC<m>:CH<c>:CAL:OFFS <value>` | Set offset | `OK` |
| `BOARD<n>:DAC<m>:CH<c>:CAL:OFFS?` | Query offset | Offset value |
| `BOARD<n>:DAC<m>:CH<c>:CAL:EN <0\|1>` | Enable/disable calibration | `OK` |
| `BOARD<n>:DAC<m>:CH<c>:CAL:EN?` | Query calibration enable | `0` or `1` |
| `CAL:DATA?` | Export all calibration data | Formatted data string |
| `CAL:SAVE` | Save calibration to flash | `OK` or error |
| `CAL:LOAD` | Load calibration from flash | `OK` or error |
| `CAL:CLEAR` | Clear all calibration data | `OK` |

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

# Set mid-scale (32767 = 50% of full scale for 16-bit)
BOARD0:DAC0:CH0:CODE 32767

# Set maximum (65535 = 100% of full scale for 16-bit)
BOARD0:DAC0:CH0:CODE 65535


# === Resolution Examples ===

# Query current resolution
BOARD0:DAC0:RES?

# Set to 12-bit resolution
BOARD0:DAC0:RES 12


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
I_out = (code / max_code) × I_fullscale
```

**Voltage Output (LTC2664)**:
```
Unipolar: V_out = (code / max_code) × V_max
Bipolar:  V_out = V_min + (code / max_code) × (V_max - V_min)
```

Where `max_code` is 4095 for 12-bit or 65535 for 16-bit resolution.

---

## Calibration

The greymatter firmware supports two-point linear calibration for each DAC channel to correct for gain and offset errors.

### Calibration Model

```
calibrated_output = (ideal_output × gain) + offset
```

Where:
- `ideal_output` is the requested voltage (V) or current (mA)
- `gain` is the gain correction factor (nominally 1.0)
- `offset` is the offset correction in physical units (nominally 0.0)

### Equipment Required

- Keithley Source-Measure Unit (SMU) or equivalent high-precision meter
  - For voltage DACs (LTC2664): Voltage measurement with at least 6-digit resolution
  - For current DACs (LTC2662): Current measurement with at least 6-digit resolution
- Test leads appropriate for the output being measured
- Temperature-stable environment (allow 30 minutes warm-up)

### Two-Point Calibration Procedure

The goal is to characterize the transfer function of each DAC channel by measuring the actual output at two known setpoints, then computing correction factors.

Given:
- Point 1: Set `V_set_low`, measure `V_meas_low`
- Point 2: Set `V_set_high`, measure `V_meas_high`

**Calibration factor calculation:**

```
gain_cal = (V_set_high - V_set_low) / (V_meas_high - V_meas_low)
offset_cal = V_set_low - (gain_cal × V_meas_low)
```

### Procedure for LTC2664 (Voltage DAC)

1. **Setup**
   - Connect SMU to the voltage output under test
   - Configure SMU for voltage measurement (high-Z input)
   - Ensure appropriate span is set for the channel

2. **Set calibration points**
   - For bipolar spans (±10V, ±5V, ±2.5V): Use 10% and 90% of range
   - For unipolar spans (0-5V, 0-10V): Use 10% and 90% of range

   Example for ±10V span:
   ```
   Low point:  -8.0V (10% from -10V)
   High point: +8.0V (90% toward +10V)
   ```

3. **Measure low point**
   ```
   BOARD0:DAC2:CH0:VOLT -8.0
   OK
   ```
   Record SMU reading: `V_meas_low = -8.0123` (example)

4. **Measure high point**
   ```
   BOARD0:DAC2:CH0:VOLT 8.0
   OK
   ```
   Record SMU reading: `V_meas_high = +7.9987` (example)

5. **Calculate calibration factors**
   ```
   gain_cal = (8.0 - (-8.0)) / (7.9987 - (-8.0123))
           = 16.0 / 16.011
           = 0.999313

   offset_cal = -8.0 - (0.999313 × (-8.0123))
             = -8.0 - (-8.0068)
             = 0.0068
   ```

6. **Apply calibration**
   ```
   BOARD0:DAC2:CH0:CAL:GAIN 0.999313
   OK
   BOARD0:DAC2:CH0:CAL:OFFS 0.0068
   OK
   BOARD0:DAC2:CH0:CAL:EN 1
   OK
   ```

7. **Verify calibration**
   ```
   BOARD0:DAC2:CH0:VOLT -8.0
   ```
   SMU should now read closer to -8.000V

### Procedure for LTC2662 (Current DAC)

1. **Setup**
   - Connect SMU to the current output under test
   - Configure SMU for current measurement (low burden voltage)
   - Set appropriate span for expected current range

2. **Set calibration points**
   - Use 10% and 90% of the configured span

   Example for 100mA span:
   ```
   Low point:  10.0 mA
   High point: 90.0 mA
   ```

3. **Measure low point**
   ```
   BOARD0:DAC0:CH0:CURR 10.0
   OK
   ```
   Record SMU reading: `I_meas_low = 10.015` mA (example)

4. **Measure high point**
   ```
   BOARD0:DAC0:CH0:CURR 90.0
   OK
   ```
   Record SMU reading: `I_meas_high = 89.985` mA (example)

5. **Calculate calibration factors**
   ```
   gain_cal = (90.0 - 10.0) / (89.985 - 10.015)
           = 80.0 / 79.970
           = 1.000375

   offset_cal = 10.0 - (1.000375 × 10.015)
             = 10.0 - 10.0188
             = -0.0188
   ```

6. **Apply calibration**
   ```
   BOARD0:DAC0:CH0:CAL:GAIN 1.000375
   OK
   BOARD0:DAC0:CH0:CAL:OFFS -0.0188
   OK
   BOARD0:DAC0:CH0:CAL:EN 1
   OK
   ```

7. **Verify calibration**
   ```
   BOARD0:DAC0:CH0:CURR 50.0
   ```
   SMU should now read closer to 50.000 mA

### Storing Calibration Data

Calibration data is stored in the RP2350's onboard flash memory, in the last 4KB sector (offset 0x1FF000).

**Workflow**:
1. Perform calibration using the procedures above
2. Save to flash: `CAL:SAVE`
3. Calibration persists across power cycles

**Automatic Loading**: On power-up, the firmware automatically loads calibration data from flash if valid data is found.

**Example session**:
```
# Set calibration for a channel
BOARD0:DAC2:CH0:CAL:GAIN 0.999313
OK
BOARD0:DAC2:CH0:CAL:OFFS 0.0068
OK
BOARD0:DAC2:CH0:CAL:EN 1
OK

# Save to flash
CAL:SAVE
OK

# Power cycle the device...

# After power-up, verify calibration was loaded
BOARD0:DAC2:CH0:CAL:GAIN?
0.999313
BOARD0:DAC2:CH0:CAL:EN?
1
```

### Export Format

The `CAL:DATA?` command returns calibration data in a human-readable format:

```
BOARD0:SN=GM-2024-001
  DAC2:CH0:G=0.999313,O=0.006800,E=1
  DAC2:CH1:G=1.000125,O=-0.003200,E=1
BOARD1:SN=GM-2024-002
  DAC0:CH0:G=1.000375,O=-0.018800,E=1
```

### Flash Storage Details

- **Location**: Last 4KB sector of 2MB flash (offset 0x1FF000)
- **Data integrity**: CRC-16 checksum validates stored data
- **Magic number**: 0x47524D43 ("GRMC") identifies valid calibration data
- **Wear leveling**: Flash is only written when `CAL:SAVE` is explicitly called
- **Sector erase**: Each save erases and rewrites the entire sector (typical flash endurance: 100,000 cycles)

### Calibration Best Practices

1. **Temperature stability**: Allow 30 minutes warm-up before calibrating
2. **Calibration points**: Use 10% and 90% of range to capture the full transfer function
3. **Verification**: Always verify calibration by measuring at the midpoint
4. **Documentation**: Record serial numbers and calibration dates
5. **Periodic recalibration**: Recalibrate annually or after significant temperature excursions
6. **Per-span calibration**: If using multiple spans, consider calibrating each span separately

### Automated Calibration Script

Here's a Python example for automated calibration using PyVISA:

```python
import pyvisa
import time

# Initialize instruments
rm = pyvisa.ResourceManager()
keithley = rm.open_resource('GPIB0::24::INSTR')  # Adjust address
greymatter = rm.open_resource('ASRL/dev/tty.usbmodem1101::INSTR')

def calibrate_voltage_channel(board, dac, channel, span_min, span_max):
    """Calibrate a voltage DAC channel."""

    # Calculate calibration points (10% and 90% of range)
    v_low = span_min + 0.1 * (span_max - span_min)
    v_high = span_min + 0.9 * (span_max - span_min)

    # Measure low point
    greymatter.write(f'BOARD{board}:DAC{dac}:CH{channel}:VOLT {v_low}')
    time.sleep(0.5)  # Allow settling
    v_meas_low = float(keithley.query(':MEAS:VOLT?'))

    # Measure high point
    greymatter.write(f'BOARD{board}:DAC{dac}:CH{channel}:VOLT {v_high}')
    time.sleep(0.5)
    v_meas_high = float(keithley.query(':MEAS:VOLT?'))

    # Calculate calibration factors
    gain = (v_high - v_low) / (v_meas_high - v_meas_low)
    offset = v_low - (gain * v_meas_low)

    # Apply calibration
    greymatter.write(f'BOARD{board}:DAC{dac}:CH{channel}:CAL:GAIN {gain:.6f}')
    greymatter.write(f'BOARD{board}:DAC{dac}:CH{channel}:CAL:OFFS {offset:.6f}')
    greymatter.write(f'BOARD{board}:DAC{dac}:CH{channel}:CAL:EN 1')

    print(f'Board {board}, DAC {dac}, CH {channel}:')
    print(f'  Gain: {gain:.6f}, Offset: {offset:.6f} V')

    return gain, offset

# Example: Calibrate Board 0, DAC 2 (voltage), all channels
for ch in range(4):
    calibrate_voltage_channel(0, 2, ch, -10.0, 10.0)

# Save calibration to flash
greymatter.write('CAL:SAVE')
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
│   │   ├── spi_init(spi0, SPI_BAUDRATE)  // Configurable, default 10 MHz
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
│   ├─► For each board (0-7):
│   │   ├── Create LTC2662 instances (DAC 0, 1)
│   │   │   └── init(): set_span_all(0x6), update_all()
│   │   │
│   │   └── Create LTC2664 instance (DAC 2)
│   │       └── init(): set_span_all(0x3), update_all()
│   │
│   └─► CalStorage::load_from_flash()
│       └── Load calibration data if valid
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
│   │   └── Match FAULT?, SYST:ERR?, LDAC, UPDATE:ALL, CAL:*
│   │
│   └─► Try parse_board_command()
│       ├── Extract BOARD<n>
│       ├── Extract :DAC<m> or :SN
│       └── Parse subcommand:
│           ├── CH<c>:VOLT, CH<c>:CURR, CH<c>:CODE
│           ├── CH<c>:SPAN, CH<c>:PDOWN, CH<c>:CAL:*
│           ├── SPAN:ALL, RES
│           ├── UPDATE
│           └── PDOWN
│
├─► BoardManager::execute(cmd)
│   │
│   └─► Switch on command type:
│       │
│       ├─► SET_VOLTAGE
│       │   ├── Validate: DAC must be 2 (LTC2664)
│       │   ├── Apply calibration if enabled
│       │   ├── Get span range from LTC2664_SPAN_INFO[]
│       │   ├── Clamp voltage to range
│       │   ├── Calculate: code = normalize(voltage) × max_code
│       │   └── dac->send_command(WRITE_UPDATE_N, channel, code)
│       │
│       ├─► SET_CURRENT
│       │   ├── Validate: DAC must be 0 or 1 (LTC2662)
│       │   ├── Apply calibration if enabled
│       │   ├── Get full-scale from span setting
│       │   ├── Clamp current to 0..full_scale
│       │   ├── Calculate: code = (current / full_scale) × max_code
│       │   └── dac->send_command(WRITE_UPDATE_N, channel, code)
│       │
│       └─► ... (other commands)
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
│   ├── [4:0] CS0-CS4: 5-bit DAC address (bit-reversed)
│   └── [5]   D_EN: Decoder enable
└── Port B [7:0]:
    ├── [0]   LDAC: Load DAC (active-low)
    └── [7]   CLR: Clear (active-low)

EXPANDER_1 (Address 0x1): Fault inputs 0-15
├── Port A [7:0]: Boards 0-3, DACs 0-1 (active-low)
└── Port B [7:0]: Boards 4-7, DACs 0-1 (active-low)

EXPANDER_2 (Address 0x2): Temperature fault inputs
└── Port A [7:0]: All 8 boards, DAC 2 (active-low)
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
│       ├── Read EXPANDER_1 GPIO (faults 0-15)
│       ├── Read EXPANDER_2 Port A (faults 16-23)
│       ├── Invert bits (active-low → active-high)
│       ├── Reorganize to DAC index order
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

### Calibration Not Applied

- Verify calibration is enabled: `BOARD<n>:DAC<m>:CH<c>:CAL:EN?` should return `1`
- Check gain and offset values are reasonable (gain near 1.0, offset near 0)

### Calibration Lost After Power Cycle

- Ensure `CAL:SAVE` was called after setting calibration
- Verify calibration was loaded: `BOARD<n>:DAC<m>:CH<c>:CAL:EN?` should return `1`
- Check for valid data in flash: `CAL:LOAD` should return `OK`
- If flash is corrupted, recalibrate and save again

### Build Errors

1. **Check PICO_SDK_PATH**: Must point to valid Pico SDK installation
2. **Verify compiler**: ARM cross-compiler required
3. **Clean build**: Delete `build/` directory and rebuild

```bash
rm -rf build
mkdir build
cd build
cmake .. -DPICO_BOARD=pico2
make picotoolBuild
C_INCLUDE_PATH= CPLUS_INCLUDE_PATH= make
```

---

## Appendix: Source File Reference

| File | Purpose |
|------|---------|
| `main.cpp` | Entry point, USB serial loop |
| `scpi_parser.cpp/hpp` | SCPI command parsing |
| `board_manager.cpp/hpp` | DAC routing and execution |
| `spi_manager.cpp/hpp` | SPI peripheral control |
| `io_expander.cpp/hpp` | MCP23S17 driver |
| `dac_device.cpp/hpp` | Abstract DAC base class |
| `ltc2662.cpp/hpp` | Current DAC driver |
| `ltc2664.cpp/hpp` | Voltage DAC driver |
| `cal_storage.cpp/hpp` | Flash-based calibration persistence |
| `utils.cpp/hpp` | String utilities |
