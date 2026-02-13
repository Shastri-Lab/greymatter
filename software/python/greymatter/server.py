#!/usr/bin/env python3
"""greymatter ZMQ server.

Runs on the host Raspberry Pi that has Pico boards connected via USB.
Accepts SCPI commands over ZMQ and routes them to the appropriate Pico.

Usage::

    python -m greymatter.server
    python -m greymatter.server --port 5556 --scan '/dev/ttyACM*'

The server auto-discovers Pico boards by scanning serial ports and
sending ``*IDN?``.  Each board is assigned a name (``pico_0``, ``pico_1``,
etc.) in the order they are discovered.

Clients connect with::

    gm = GreyMatter(address="<host-ip>", pico="pico_0")
"""

from __future__ import annotations

import argparse
import json
import glob as glob_module
from datetime import datetime

from .transport import SerialTransport
from .errors import GreyMatterError


def _log(msg: str) -> None:
    print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] {msg}", flush=True)


class PicoConnection:
    """Tracks a single Pico board connection."""

    def __init__(self, name: str, port: str, transport: SerialTransport,
                 idn: str):
        self.name = name
        self.port = port
        self.transport = transport
        self.idn = idn


def discover_picos(
    patterns: list[str] | None = None,
    baudrate: int = 115200,
    timeout: float = 2.0,
) -> dict[str, PicoConnection]:
    """Scan serial ports for greymatter Pico boards.

    Returns:
        dict mapping name -> PicoConnection for each board that responds
        to ``*IDN?``.
    """
    if patterns is None:
        patterns = ["/dev/ttyACM*", "/dev/ttyUSB*", "/dev/tty.usbmodem*"]

    ports: list[str] = []
    for pattern in patterns:
        ports.extend(sorted(glob_module.glob(pattern)))

    picos: dict[str, PicoConnection] = {}
    idx = 0
    for port in ports:
        try:
            _log(f"Trying {port} ...")
            transport = SerialTransport(port, baudrate, timeout)
            idn = transport.send_command("*IDN?")
            name = f"pico_{idx}"
            picos[name] = PicoConnection(name, port, transport, idn)
            _log(f"  -> {name}: {idn}")
            idx += 1
        except Exception as exc:
            _log(f"  -> skip: {exc}")

    return picos


def _handle_meta(cmd: str, picos: dict[str, PicoConnection],
                 scan_kw: dict) -> str:
    """Handle server meta-commands (prefixed with ``__``)."""
    if cmd == "__list__":
        info = [
            {"name": p.name, "port": p.port, "idn": p.idn}
            for p in picos.values()
        ]
        return json.dumps({"ok": True, "data": info})

    if cmd == "__rescan__":
        for p in picos.values():
            try:
                p.transport.close()
            except Exception:
                pass
        picos.clear()
        picos.update(discover_picos(**scan_kw))
        return json.dumps(
            {"ok": True, "data": f"Found {len(picos)} pico(s)"}
        )

    return json.dumps({"ok": False, "error": f"Unknown meta command: {cmd}"})


def run_server(
    port: int = 5556,
    scan_patterns: list[str] | None = None,
    baudrate: int = 115200,
) -> None:
    """Start the ZMQ server loop.

    Args:
        port: TCP port for the ZMQ REP socket.
        scan_patterns: Glob patterns for serial port discovery.
        baudrate: Baud rate for Pico serial connections.
    """
    import zmq

    _log("Starting greymatter server")

    scan_kw = {"patterns": scan_patterns, "baudrate": baudrate}
    picos = discover_picos(**scan_kw)

    if not picos:
        _log("WARNING: No Pico boards found. Server will still start "
             "(use __rescan__ after connecting boards).")

    _log(f"Managing {len(picos)} Pico board(s)")

    context = zmq.Context()
    socket = context.socket(zmq.REP)
    socket.bind(f"tcp://*:{port}")
    _log(f"Listening on tcp://*:{port}")

    try:
        while True:
            raw = socket.recv_string()

            # Parse request
            try:
                request = json.loads(raw)
            except json.JSONDecodeError:
                socket.send_string(
                    json.dumps({"ok": False, "error": "Invalid JSON"})
                )
                continue

            cmd = request.get("cmd", "")
            pico_name = request.get("pico")

            # Meta commands
            if cmd.startswith("__") and cmd.endswith("__"):
                reply = _handle_meta(cmd, picos, scan_kw)
                socket.send_string(reply)
                continue

            # Resolve which Pico to route to
            if pico_name is None:
                if len(picos) == 1:
                    pico = next(iter(picos.values()))
                elif len(picos) == 0:
                    socket.send_string(json.dumps({
                        "ok": False,
                        "error": "No Pico boards connected"
                    }))
                    continue
                else:
                    socket.send_string(json.dumps({
                        "ok": False,
                        "error": (
                            f"Multiple picos connected, specify one of: "
                            f"{list(picos.keys())}"
                        ),
                    }))
                    continue
            else:
                pico = picos.get(pico_name)
                if pico is None:
                    socket.send_string(json.dumps({
                        "ok": False,
                        "error": (
                            f"Unknown pico '{pico_name}'. "
                            f"Available: {list(picos.keys())}"
                        ),
                    }))
                    continue

            # Forward SCPI command to the Pico
            try:
                _log(f"[{pico.name}] {cmd}")
                result = pico.transport.send_command(cmd)
                socket.send_string(json.dumps({"ok": True, "data": result}))
            except GreyMatterError as exc:
                _log(f"[{pico.name}] ERROR: {exc}")
                socket.send_string(
                    json.dumps({"ok": False, "error": str(exc)})
                )
            except Exception as exc:
                _log(f"[{pico.name}] COMM ERROR: {exc}")
                socket.send_string(json.dumps({
                    "ok": False,
                    "error": f"Communication error: {exc}",
                }))

    except KeyboardInterrupt:
        _log("Shutting down")
    finally:
        for p in picos.values():
            try:
                p.transport.close()
            except Exception:
                pass
        socket.close()
        context.destroy()


def main() -> None:
    parser = argparse.ArgumentParser(
        description="greymatter ZMQ server — routes SCPI commands to Pico boards"
    )
    parser.add_argument(
        "--port", type=int, default=5556,
        help="ZMQ listen port (default: 5556)",
    )
    parser.add_argument(
        "--scan", nargs="*", metavar="PATTERN",
        help="Serial port glob patterns (default: /dev/ttyACM* /dev/ttyUSB* "
             "/dev/tty.usbmodem*)",
    )
    parser.add_argument(
        "--baudrate", type=int, default=115200,
        help="Serial baud rate (default: 115200)",
    )
    args = parser.parse_args()
    run_server(port=args.port, scan_patterns=args.scan, baudrate=args.baudrate)


if __name__ == "__main__":
    main()
