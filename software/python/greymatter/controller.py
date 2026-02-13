from __future__ import annotations

from .errors import GreyMatterError
from .transport import SerialTransport, ZmqTransport, _CMD_TIMEOUT


class GreyMatter:
    """Top-level driver for the greymatter DAC controller.

    Communicates with the firmware either directly over USB serial
    or remotely via a ZMQ server running on the host machine.

    Direct serial connection::

        gm = GreyMatter("/dev/tty.usbmodem1101")
        gm.board(0).dac(0).channel(0).set_current(50.0)
        gm.close()

    Remote connection via server::

        gm = GreyMatter(address="192.168.1.100", pico="pico_0")
        gm.board(0).dac(0).channel(0).set_current(50.0)
        gm.close()

    As a context manager::

        with GreyMatter("/dev/tty.usbmodem1101") as gm:
            gm.board(0).dac(0).channel(0).set_current(50.0)
    """

    def __init__(
        self,
        port: str | None = None,
        *,
        address: str | None = None,
        pico: str | None = None,
        zmq_port: int = 5556,
        baudrate: int = 115200,
        num_boards: int = 8,
        timeout: float = _CMD_TIMEOUT,
    ):
        self._num_boards = num_boards

        if port is not None:
            self._transport = SerialTransport(port, baudrate, timeout)
        elif address is not None:
            self._transport = ZmqTransport(
                address, pico, zmq_port, timeout=max(timeout, 10.0)
            )
        else:
            raise ValueError(
                "Provide 'port' for direct serial or 'address' for remote"
            )

        # Lazily-constructed board cache
        self._boards: dict[int, "Board"] = {}

    # -- Context manager --

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def close(self):
        """Close the underlying connection."""
        self._transport.close()

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
        resp = self._transport.send_command(cmd)
        if resp.startswith("ERROR:"):
            raise GreyMatterError(resp)
        return resp

    def query(self, cmd: str) -> str:
        """Send a SCPI query and return the response string.

        Raises GreyMatterError if the firmware returns an ERROR: response.
        """
        return self.command(cmd)

    # -- Server utilities --

    @staticmethod
    def list_picos(
        address: str, zmq_port: int = 5556, timeout: float = 5.0
    ) -> list[dict]:
        """Query the server for a list of connected Pico boards.

        Returns a list of dicts with keys: name, port, idn.

        Example::

            picos = GreyMatter.list_picos("192.168.1.100")
            for p in picos:
                print(f"{p['name']} on {p['port']}: {p['idn']}")
        """
        import json
        import zmq

        context = zmq.Context()
        socket = context.socket(zmq.REQ)
        socket.setsockopt(zmq.RCVTIMEO, int(timeout * 1000))
        socket.setsockopt(zmq.LINGER, 1000)
        socket.connect(f"tcp://{address}:{zmq_port}")
        try:
            socket.send_string(json.dumps({"cmd": "__list__"}))
            reply = json.loads(socket.recv_string())
            if reply.get("ok"):
                return reply["data"]
            raise GreyMatterError(reply.get("error", "Unknown error"))
        finally:
            socket.close()
            context.destroy()
