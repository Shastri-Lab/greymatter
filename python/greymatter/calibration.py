from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .controller import GreyMatter


class GreyMatterCalibration:
    """Calibration interface for the greymatter DAC controller.

    Usage::

        from greymatter.calibration import GreyMatterCalibration

        cal = GreyMatterCalibration(gm)
        cal.set_gain(board=0, dac=0, channel=0, gain=0.999)
        cal.save()
    """

    def __init__(self, gm: GreyMatter):
        self._gm = gm

    def _ch_prefix(self, board: int, dac: int, channel: int) -> str:
        return f"BOARD{board}:DAC{dac}:CH{channel}"

    def get_gain(self, board: int, dac: int, channel: int) -> float:
        return float(self._gm.query(f"{self._ch_prefix(board, dac, channel)}:CAL:GAIN?"))

    def set_gain(self, board: int, dac: int, channel: int, gain: float) -> None:
        self._gm.command(f"{self._ch_prefix(board, dac, channel)}:CAL:GAIN {gain}")

    def get_offset(self, board: int, dac: int, channel: int) -> float:
        return float(self._gm.query(f"{self._ch_prefix(board, dac, channel)}:CAL:OFFS?"))

    def set_offset(self, board: int, dac: int, channel: int, offset: float) -> None:
        self._gm.command(f"{self._ch_prefix(board, dac, channel)}:CAL:OFFS {offset}")

    def get_enabled(self, board: int, dac: int, channel: int) -> bool:
        return self._gm.query(f"{self._ch_prefix(board, dac, channel)}:CAL:EN?") == "1"

    def set_enabled(self, board: int, dac: int, channel: int, enabled: bool) -> None:
        self._gm.command(f"{self._ch_prefix(board, dac, channel)}:CAL:EN {1 if enabled else 0}")

    def save(self) -> None:
        """Save calibration data to flash."""
        self._gm.command("CAL:SAVE")

    def load(self) -> None:
        """Load calibration data from flash."""
        self._gm.command("CAL:LOAD")

    def clear(self) -> None:
        """Clear all calibration data (RAM and flash)."""
        self._gm.command("CAL:CLEAR")

    def export_data(self) -> str:
        """Export all calibration data as a formatted string."""
        return self._gm.query("CAL:DATA?")
