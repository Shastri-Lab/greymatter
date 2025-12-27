#ifndef DEBUG_SPI_HPP
#define DEBUG_SPI_HPP

#include <cstdint>
#include <cstddef>

#ifdef DEBUG_SPI_MODE

// Debug SPI Configuration
namespace DEBUG_SPI_CONFIG {
    // Bit period for 1 Hz clock (500ms high, 500ms low per bit)
    constexpr uint32_t BIT_PERIOD_MS = 1000;
    constexpr uint32_t HALF_PERIOD_MS = 500;

    // Loopback verification pins (directly mirrored from SPI pins)
    constexpr unsigned int LOOPBACK_MOSI = 0;  // GP0 mirrors GP19 (MOSI)
    constexpr unsigned int LOOPBACK_MISO = 1;  // GP1 mirrors GP16 (MISO)
    constexpr unsigned int LOOPBACK_CLK  = 2;  // GP2 mirrors GP18 (CLK)
    constexpr unsigned int LOOPBACK_CS   = 3;  // GP3 mirrors GP17 (CS)

    // Trace output verbosity levels
    constexpr uint8_t TRACE_NONE     = 0;
    constexpr uint8_t TRACE_SUMMARY  = 1;
    constexpr uint8_t TRACE_BIT      = 2;
    constexpr uint8_t TRACE_EDGE     = 3;  // Both rising and falling edges
}

// Debug SPI state machine states
enum class DebugSpiState {
    IDLE,
    CS_ASSERT,
    CLK_LOW,
    CLK_HIGH,
    CS_RELEASE,
    WAITING_FOR_STEP
};

// Debug SPI Controller - bit-banged 1 Hz SPI with tracing
class DebugSpi {
public:
    // Initialize debug SPI pins (converts SPI pins to GPIO mode)
    void init();

    // Configure trace verbosity (0=none, 1=summary, 2=bit, 3=edge)
    void set_trace_level(uint8_t level);
    uint8_t get_trace_level() const { return trace_level_; }

    // Enable/disable step mode (pause between each bit)
    void set_step_mode(bool enabled);
    bool get_step_mode() const { return step_mode_; }

    // Advance one step in step mode (called from SCPI command)
    void step();

    // Enable/disable loopback pin mirroring
    void set_loopback_enabled(bool enabled);
    bool get_loopback_enabled() const { return loopback_enabled_; }

    // Perform a debug SPI transaction (bit-banged at 1 Hz)
    // Returns received data, prints trace to serial
    void transaction(const uint8_t* tx_data, uint8_t* rx_data, size_t len);

    // Assert/release chip select
    void cs_assert();
    void cs_release();

    // Get current state (for status queries)
    DebugSpiState get_state() const { return state_; }

    // Check if step mode is waiting for input
    bool is_waiting() const { return state_ == DebugSpiState::WAITING_FOR_STEP; }

    // Flag to signal step advance (set by main loop when DEBUG:STEP received)
    volatile bool step_pending_ = false;

private:
    DebugSpiState state_ = DebugSpiState::IDLE;
    uint8_t trace_level_ = DEBUG_SPI_CONFIG::TRACE_BIT;
    bool step_mode_ = false;
    bool loopback_enabled_ = true;

    // Bit-bang helpers
    void set_mosi(bool value);
    bool read_miso();
    void set_clk(bool value);
    void set_cs(bool value);

    // Mirror current SPI state to loopback pins
    void update_loopback();

    // Print trace output
    void trace_bit(uint8_t bit_num, bool mosi_out, bool miso_in, bool clk, bool cs);
    void trace_byte_summary(uint8_t tx_byte, uint8_t rx_byte);
    void trace_edge(const char* edge, bool mosi, bool miso, bool clk, bool cs);

    // Wait for step in step mode
    void wait_for_step();
};

// Global debug SPI instance (only exists in debug mode)
extern DebugSpi g_debug_spi;

#endif // DEBUG_SPI_MODE

#endif // DEBUG_SPI_HPP
