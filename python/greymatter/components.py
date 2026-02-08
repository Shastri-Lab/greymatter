from __future__ import annotations

from typing import TYPE_CHECKING, Union

from .spans import CurrentSpan, VoltageSpan

if TYPE_CHECKING:
    from .controller import GreyMatter


class Board:
    """Represents one daughter board (index 0-7)."""

    def __init__(self, gm: GreyMatter, board_id: int):
        self._gm = gm
        self._id = board_id
        self._dacs: dict[int, Union[CurrentDAC, VoltageDAC]] = {}

    def dac(self, dac_id: int) -> Union["CurrentDAC", "VoltageDAC"]:
        """Return DAC 0-2. DAC 0,1 are CurrentDAC; DAC 2 is VoltageDAC."""
        if dac_id < 0 or dac_id > 2:
            raise ValueError("dac_id must be 0-2")
        if dac_id not in self._dacs:
            if dac_id < 2:
                self._dacs[dac_id] = CurrentDAC(self._gm, self._id, dac_id)
            else:
                self._dacs[dac_id] = VoltageDAC(self._gm, self._id, dac_id)
        return self._dacs[dac_id]

    @property
    def serial_number(self) -> str:
        return self._gm.query(f"BOARD{self._id}:SN?")

    @serial_number.setter
    def serial_number(self, value: str) -> None:
        self._gm.command(f"BOARD{self._id}:SN {value}")

    def power_down(self) -> None:
        """Power down all 3 DACs on this board."""
        for d in range(3):
            self.dac(d).power_down()


class CurrentDAC:
    """LTC2662 current DAC (5 channels)."""

    num_channels = 5

    def __init__(self, gm: GreyMatter, board_id: int, dac_id: int):
        self._gm = gm
        self._board = board_id
        self._dac = dac_id
        self._channels: dict[int, CurrentChannel] = {}

    def _prefix(self) -> str:
        return f"BOARD{self._board}:DAC{self._dac}"

    def channel(self, ch: int) -> "CurrentChannel":
        if ch < 0 or ch >= self.num_channels:
            raise ValueError(f"channel must be 0-{self.num_channels - 1}")
        if ch not in self._channels:
            self._channels[ch] = CurrentChannel(self._gm, self._board, self._dac, ch)
        return self._channels[ch]

    def set_span(self, span: CurrentSpan) -> None:
        """Set span for all channels."""
        self._gm.command(f"{self._prefix()}:SPAN:ALL {int(span)}")

    @property
    def resolution(self) -> int:
        return int(self._gm.query(f"{self._prefix()}:RES?"))

    @resolution.setter
    def resolution(self, bits: int) -> None:
        self._gm.command(f"{self._prefix()}:RES {bits}")

    def power_down(self) -> None:
        self._gm.command(f"{self._prefix()}:PDOWN")

    def update(self) -> None:
        self._gm.command(f"{self._prefix()}:UPDATE")

    def fault_status(self) -> str:
        """Read fault register from the LTC2662 via SPI readback.

        Returns "OK" if no faults, or a string like
        "FAULT:OC0,OVERTEMP" listing active fault conditions.

        Fault bits:
          OC0-OC4: Open-circuit on OUT0-OUT4
          OVERTEMP: Die temperature >175C
          POWER_LIMIT: VDDx-VOUTx >10V at >=200mA (auto-reduced to 100mA)
          INVALID_SPI: Bad SPI sequence length
        """
        return self._gm.query(f"{self._prefix()}:FAULT?")

    def echo_readback(self) -> str:
        """Echo readback test (32-bit NOP).

        Returns a string like "FR=0xNN ECHO=0xNNNNNN" where FR is the
        fault register and ECHO is the previous SPI command echoed back.
        """
        return self._gm.query(f"{self._prefix()}:ECHO?")


class VoltageDAC:
    """LTC2664 voltage DAC (4 channels)."""

    num_channels = 4

    def __init__(self, gm: GreyMatter, board_id: int, dac_id: int):
        self._gm = gm
        self._board = board_id
        self._dac = dac_id
        self._channels: dict[int, VoltageChannel] = {}

    def _prefix(self) -> str:
        return f"BOARD{self._board}:DAC{self._dac}"

    def channel(self, ch: int) -> "VoltageChannel":
        if ch < 0 or ch >= self.num_channels:
            raise ValueError(f"channel must be 0-{self.num_channels - 1}")
        if ch not in self._channels:
            self._channels[ch] = VoltageChannel(self._gm, self._board, self._dac, ch)
        return self._channels[ch]

    def set_span(self, span: VoltageSpan) -> None:
        """Set span for all channels."""
        self._gm.command(f"{self._prefix()}:SPAN:ALL {int(span)}")

    @property
    def resolution(self) -> int:
        return int(self._gm.query(f"{self._prefix()}:RES?"))

    @resolution.setter
    def resolution(self, bits: int) -> None:
        self._gm.command(f"{self._prefix()}:RES {bits}")

    def power_down(self) -> None:
        self._gm.command(f"{self._prefix()}:PDOWN")

    def update(self) -> None:
        self._gm.command(f"{self._prefix()}:UPDATE")

    def echo_readback(self) -> str:
        """Echo readback test (32-bit NOP).

        Returns a string like "ECHO=0xNNNNNNNN" — the previous SPI
        command echoed back. Only works in 32-bit mode.
        """
        return self._gm.query(f"{self._prefix()}:ECHO?")


class CurrentChannel:
    """A single channel on an LTC2662 current DAC."""

    def __init__(self, gm: GreyMatter, board_id: int, dac_id: int, ch: int):
        self._gm = gm
        self._prefix = f"BOARD{board_id}:DAC{dac_id}:CH{ch}"

    def set_current(self, milliamps: float) -> None:
        """Set output current in mA."""
        self._gm.command(f"{self._prefix}:CURR {milliamps}")

    def set_span(self, span: CurrentSpan) -> None:
        """Set span for this channel."""
        self._gm.command(f"{self._prefix}:SPAN {int(span)}")

    def set_code(self, code: int) -> None:
        """Set raw DAC code."""
        self._gm.command(f"{self._prefix}:CODE {code}")

    def power_down(self) -> None:
        self._gm.command(f"{self._prefix}:PDOWN")


class VoltageChannel:
    """A single channel on an LTC2664 voltage DAC."""

    def __init__(self, gm: GreyMatter, board_id: int, dac_id: int, ch: int):
        self._gm = gm
        self._prefix = f"BOARD{board_id}:DAC{dac_id}:CH{ch}"

    def set_voltage(self, volts: float) -> None:
        """Set output voltage in volts."""
        self._gm.command(f"{self._prefix}:VOLT {volts}")

    def set_span(self, span: VoltageSpan) -> None:
        """Set span for this channel."""
        self._gm.command(f"{self._prefix}:SPAN {int(span)}")

    def set_code(self, code: int) -> None:
        """Set raw DAC code."""
        self._gm.command(f"{self._prefix}:CODE {code}")

    def power_down(self) -> None:
        self._gm.command(f"{self._prefix}:PDOWN")
