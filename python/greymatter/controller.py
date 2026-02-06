import threading

import serial

from .errors import GreyMatterError


# Sentinel bytes for detecting the prompt
_PROMPT = b"> "

# How long to wait for the startup banner to finish (seconds)
_CONNECT_TIMEOUT = 5.0

# Per-command timeout (seconds)
_CMD_TIMEOUT = 2.0


class GreyMatter:
    """Top-level driver for the greymatter DAC controller.

    Communicates with the firmware over USB serial using SCPI commands.

    Usage::

        gm = GreyMatter("/dev/tty.usbmodem1101")
        gm.board(0).dac(0).channel(0).set_current(50.0)
        gm.close()

    Or as a context manager::

        with GreyMatter("/dev/tty.usbmodem1101") as gm:
            gm.board(0).dac(0).channel(0).set_current(50.0)
    """

    def __init__(self, port: str, baudrate: int = 115200,
                 num_boards: int = 8, timeout: float = _CMD_TIMEOUT):
        self._lock = threading.Lock()
        self._timeout = timeout
        self._num_boards = num_boards
        self._ser = serial.Serial(port, baudrate, timeout=timeout)

        # Drain startup banner — read until we see the "> " prompt
        self._drain_until_prompt(_CONNECT_TIMEOUT)

        # Lazily-constructed board cache
        self._boards: dict[int, "Board"] = {}

    # -- Context manager --

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def close(self):
        """Close the serial connection."""
        if self._ser and self._ser.is_open:
            self._ser.close()

    # -- Navigation --

    def board(self, board_id: int) -> "Board":
        """Return a Board object for the given board index (0-7)."""
        if board_id < 0 or board_id >= self._num_boards:
            raise ValueError(f"board_id must be 0-{self._num_boards - 1}")
        if board_id not in self._boards:
            from .components import Board
            self._boards[board_id] = Board(self, board_id)
        return self._boards[board_id]

    @property
    def channels(self) -> list:
        """Flat list of all Channel objects across all boards."""
        chs = []
        for b in range(self._num_boards):
            board = self.board(b)
            for d in range(3):
                dac = board.dac(d)
                for c in range(dac.num_channels):
                    chs.append(dac.channel(c))
        return chs

    # -- Global commands --

    def identify(self) -> str:
        """Send *IDN? and return the identification string."""
        return self.query("*IDN?")

    def reset(self) -> None:
        """Send *RST to reset all DACs."""
        self.command("*RST")

    def fault_status(self) -> str:
        """Query FAULT? and return the result."""
        return self.query("FAULT?")

    def update_all(self) -> None:
        """Send UPDATE:ALL to update all DAC outputs."""
        self.command("UPDATE:ALL")

    def pulse_ldac(self) -> None:
        """Send LDAC to pulse the load-DAC line."""
        self.command("LDAC")

    # -- Raw SCPI --

    def command(self, cmd: str) -> str:
        """Send a SCPI command and return the response.

        Raises GreyMatterError if the firmware returns an ERROR: response.
        """
        resp = self._transact(cmd)
        if resp.startswith("ERROR:"):
            raise GreyMatterError(resp)
        return resp

    def query(self, cmd: str) -> str:
        """Send a SCPI query and return the response string.

        Raises GreyMatterError if the firmware returns an ERROR: response.
        """
        return self.command(cmd)

    # -- Internal serial I/O --

    def _drain_until_prompt(self, timeout: float) -> None:
        """Read and discard bytes until the '> ' prompt is seen."""
        old_timeout = self._ser.timeout
        self._ser.timeout = timeout
        buf = b""
        try:
            while True:
                byte = self._ser.read(1)
                if not byte:
                    break  # timeout
                buf += byte
                if buf.endswith(_PROMPT):
                    break
        finally:
            self._ser.timeout = old_timeout

    def _transact(self, cmd: str) -> str:
        """Send a command and return the response body, thread-safe.

        Protocol:
        1. Flush input buffer
        2. Send cmd + newline
        3. Read until prompt '> ' appears
        4. Strip echo (first line) and prompt, return response body
        """
        with self._lock:
            self._ser.reset_input_buffer()
            self._ser.write((cmd + "\n").encode("ascii"))

            # Read until we see the prompt after the response
            buf = b""
            while True:
                byte = self._ser.read(1)
                if not byte:
                    raise GreyMatterError("Timeout waiting for response")
                buf += byte
                if buf.endswith(_PROMPT):
                    break

            # Parse: the buffer contains "ECHO\r\nRESPONSE\r\n> "
            text = buf.decode("ascii", errors="replace")

            # Remove trailing prompt
            text = text[:-len("> ")]

            # Split into lines and strip
            lines = text.split("\r\n")

            # Remove empty trailing entries
            while lines and lines[-1] == "":
                lines.pop()

            # First line is the echo, rest is the response
            if len(lines) <= 1:
                return ""

            # Response is everything after the echo line
            return "\r\n".join(lines[1:])
