# /// script
# dependencies = ["pyserial", "typer"]
# ///
"""Watch the optocoupler edges streamed by intercom/ring_capture, print one line per ring burst, and optionally save the raw edges.

Usage: uv run intercom/tools/record_ring.py [--output FILE] [--seconds N] [--port PORT]
"""

import glob
import sys
import time
from pathlib import Path
from typing import Annotated

import serial
import typer

BAUD = 460800
BURST_GAP_US = 300_000


def find_port() -> str:
    ports = glob.glob("/dev/cu.usbserial-*") + glob.glob("/dev/cu.SLAB*") + glob.glob("/dev/cu.wchusbserial*")
    if not ports:
        sys.exit("No ESP32 serial port found.")
    return ports[0]


def main(
    output: Annotated[
        Path | None, typer.Option("--output", "-o", help="File to save the raw edges to.")
    ] = None,
    seconds: Annotated[float, typer.Option("--seconds", "-s")] = 90,
    port_name: Annotated[str | None, typer.Option("--port", "-p")] = None,
) -> None:
    port_name = port_name or find_port()
    port = serial.Serial(port_name, BAUD, timeout=0.2)
    port.dtr = False
    port.rts = True
    time.sleep(0.2)
    port.rts = False
    port.reset_input_buffer()

    target = f" to {output}" if output else ""
    print(f"Listening {seconds:.0f} s on {port_name}{target}. Ring now.", flush=True)
    clock_us = 0
    burst_start = None
    burst_edges = 0
    last_edge_us = 0
    end = time.time() + seconds
    out = output.open("w") if output else None
    try:
        buffer = b""
        while time.time() < end:
            buffer += port.read(4096)
            *lines, buffer = buffer.split(b"\n")
            for raw in lines:
                line = raw.decode("ascii", "replace").strip()
                if not line:
                    continue
                if out:
                    out.write(line + "\n")
                if line.startswith("#"):
                    print(line, flush=True)
                    continue
                try:
                    delta = int(line.split()[0])
                except ValueError:
                    continue
                clock_us += delta
                if burst_start is not None and clock_us - last_edge_us > BURST_GAP_US:
                    print(
                        f"  burst at {burst_start / 1e6:6.2f} s: {(last_edge_us - burst_start) / 1e3:6.0f} ms, {burst_edges} edges",
                        flush=True,
                    )
                    burst_start = None
                if burst_start is None:
                    burst_start = clock_us
                    burst_edges = 0
                burst_edges += 1
                last_edge_us = clock_us
            if out:
                out.flush()
    finally:
        if out:
            out.close()
    if burst_start is not None:
        print(
            f"  burst at {burst_start / 1e6:6.2f} s: {(last_edge_us - burst_start) / 1e3:6.0f} ms, {burst_edges} edges"
        )
    print("Done.")


if __name__ == "__main__":
    typer.run(main)
