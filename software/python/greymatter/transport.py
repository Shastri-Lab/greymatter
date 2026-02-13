from __future__ import annotations

import threading
from abc import ABC, abstractmethod

from .errors import GreyMatterError

# Sentinel bytes for detecting the firmware prompt
_PROMPT = b"> "

# How long to wait for the startup banner to finish (seconds)
_CONNECT_TIMEOUT = 5.0

# Per-command timeout (seconds)
_CMD_TIMEOUT = 2.0


class Transport(ABC):
    """Abstract transport for sending SCPI commands to a greymatter board."""

    @abstractmethod
    def send_command(self, cmd: str) -> str:
        """Send a SCPI command and return the response body.

        The response should already have the echo and prompt stripped.
        Raises GreyMatterError on communication failure.
        """
        ...

    @abstractmethod
    def close(self) -> None:
        """Release the underlying connection."""
        ...


class SerialTransport(Transport):
    """Direct USB serial connection to a Pico board.

    Handles the firmware's serial protocol: echo, ``\\r\\n`` delimiters,
    and the ``> `` prompt.
    """

    def __init__(self, port: str, baudrate: int = 115200,
                 timeout: float = _CMD_TIMEOUT):
        import serial
        self._lock = threading.Lock()
        self._ser = serial.Serial(port, baudrate, timeout=timeout)
        self._drain_until_prompt(_CONNECT_TIMEOUT)

    def send_command(self, cmd: str) -> str:
        with self._lock:
            self._ser.reset_input_buffer()
            self._ser.write((cmd + "\n").encode("ascii"))

            buf = b""
            while True:
                byte = self._ser.read(1)
                if not byte:
                    raise GreyMatterError("Timeout waiting for response")
                buf += byte
                if buf.endswith(_PROMPT):
                    break

            # Buffer contains "ECHO\r\nRESPONSE\r\n> "
            text = buf.decode("ascii", errors="replace")
            text = text[: -len("> ")]

            lines = text.split("\r\n")
            while lines and lines[-1] == "":
                lines.pop()

            # First line is the echo; the rest is the response
            if len(lines) <= 1:
                return ""
            return "\r\n".join(lines[1:])

    def close(self) -> None:
        if self._ser and self._ser.is_open:
            self._ser.close()

    def _drain_until_prompt(self, timeout: float) -> None:
        """Read and discard bytes until the '> ' prompt is seen."""
        old_timeout = self._ser.timeout
        self._ser.timeout = timeout
        buf = b""
        try:
            while True:
                byte = self._ser.read(1)
                if not byte:
                    break
                buf += byte
                if buf.endswith(_PROMPT):
                    break
        finally:
            self._ser.timeout = old_timeout


class ZmqTransport(Transport):
    """Remote connection to a greymatter board via a ZMQ server.

    The server manages the serial connections and routes commands
    to the correct Pico board.
    """

    def __init__(self, address: str, pico: str | None = None,
                 port: int = 5556, timeout: float = 10.0):
        import zmq
        self._pico = pico
        self._lock = threading.Lock()
        self._context = zmq.Context()
        self._socket = self._context.socket(zmq.REQ)
        self._socket.setsockopt(zmq.RCVTIMEO, int(timeout * 1000))
        self._socket.setsockopt(zmq.SNDTIMEO, int(timeout * 1000))
        self._socket.setsockopt(zmq.LINGER, 1000)
        self._socket.connect(f"tcp://{address}:{port}")

    def send_command(self, cmd: str) -> str:
        import json
        import zmq

        with self._lock:
            request = json.dumps({"pico": self._pico, "cmd": cmd})
            try:
                self._socket.send_string(request)
                reply = json.loads(self._socket.recv_string())
            except zmq.Again:
                raise GreyMatterError("Server timeout")

            if reply.get("ok"):
                return reply.get("data", "")
            else:
                raise GreyMatterError(
                    reply.get("error", "Unknown server error")
                )

    def close(self) -> None:
        self._socket.close()
        self._context.destroy()
