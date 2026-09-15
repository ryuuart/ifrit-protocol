#!/usr/bin/env python3
"""The board serial_sensor reads: a port that prints lines, or a recording.

Two modes, one sensor:

    python3 sensor.py
        makes a pseudo-terminal pair — a port with a path and nothing
        soldered to it — prints the path to open the sketch on, and
        writes one reading into it ten times a second until interrupted.
        That is what makes the sketch move on a desk with no board on
        it. Open the sketch on the path it prints, and START THIS FIRST:
        the pair stands as long as this program does.

    python3 sensor.py --record data/readings.feed --seconds 8
        writes those same readings to a file in the feed recording
        format, which is what the sketch's plate replays.

A READING IS ONE LINE, and the line is compact JSON:

    {"lux": 412, "tilt": -3.2}

`lux` is what the light sensor reads, a whole number the way a board
prints an analogue count; `tilt` is degrees off level, one decimal. A
board that has only one of them prints only that field, and the sketch
leaves the other reading where it was.

WHAT THE RECORDING HOLDS is the lines WITHOUT their newlines. The
newline is the frame on the wire — it is what says one reading ended and
the next began — and an arrival is what is inside one, so a recording of
this feed carries the readings and not the framing.

The readings are a function of the message's own time and of nothing
else, so the file and the live writing carry the same lines at the same
seconds and neither holds any randomness.

Standard library only: this runs wherever python3 does, with nothing
installed.
"""

import argparse
import json
import math
import os
import pty
import struct
import sys
import termios
import time

# The line every feed recording opens with, and the frame one arrival is
# written as: the time as a double, the length as a 32-bit unsigned, then
# that many bytes. Both numbers are written little-endian, which is the
# byte order of every machine that reads one of these.
HEADER = b"sigil-feed-recording 1\n"
FRAME = struct.Struct("<dI")

# EDIT THESE FIRST: what the sensor reads, and how it swings.
LUX_LEVEL = 430.0  # the light a room sits at
LUX_SWING = 380.0  # how far the light climbs and falls either way
LUX_PERIOD = 11.0  # how long one of those climbs takes, in seconds
FLICKER = 26.0  # the small unsteadiness a real photocell has
FLICKER_PERIOD = 1.3
TILT_SWING = 22.0  # how far the board leans either way, in degrees
TILT_PERIOD = 6.5  # how long one lean takes, in seconds

# The rate a board of this kind prints at, and the rate the recording is
# written at: one reading every hundred milliseconds.
RATE = 10.0
BAUD = 115200


def reading_at(seconds):
    """The reading at `seconds`, as the record the board prints."""
    lux = (
        LUX_LEVEL
        + LUX_SWING * math.sin(2.0 * math.pi * seconds / LUX_PERIOD)
        + FLICKER * math.sin(2.0 * math.pi * seconds / FLICKER_PERIOD)
    )
    tilt = TILT_SWING * math.sin(2.0 * math.pi * seconds / TILT_PERIOD + 0.7)
    # A board prints what its converter read: a whole count for the
    # light, and one decimal of a degree for the lean.
    return {"lux": max(0, round(lux)), "tilt": round(tilt, 1)}


def line_at(seconds):
    """The bytes of one reading: the record, as compact JSON."""
    return json.dumps(reading_at(seconds), separators=(",", ":")).encode("utf-8")


def frame_times(seconds, rate):
    """The time of every reading in a run of `seconds` at `rate` a second.

    A reading stands at (i + 0.5) / rate rather than at i / rate so that
    none of them falls on a whole second, where a reader stepping in
    whole frames could take one a frame early or a frame late.
    """
    return [(index + 0.5) / rate for index in range(int(seconds * rate))]


def write_recording(path, seconds, rate):
    """Every reading of a run of `seconds`, as a recording file."""
    written = 0
    with open(path, "wb") as recording:
        recording.write(HEADER)
        for at in frame_times(seconds, rate):
            payload = line_at(at)
            recording.write(FRAME.pack(at, len(payload)))
            recording.write(payload)
            written += 1
    return written


def make_port():
    """A port with a path and nothing soldered to it.

    The pair is a master this program writes into and a slave the sketch
    opens by name, which is every bit of what a board is to the reader:
    a device file that answers lines. The slave is put in raw mode so
    that nothing written here is echoed back or translated on its way
    through, and the slave descriptor is held open for as long as this
    program runs — a pair nobody holds is a path that stops existing.
    """
    master, slave = pty.openpty()
    iflag, oflag, cflag, lflag, ispeed, ospeed, control = termios.tcgetattr(slave)
    iflag &= ~(
        termios.IGNBRK
        | termios.BRKINT
        | termios.PARMRK
        | termios.ISTRIP
        | termios.INLCR
        | termios.IGNCR
        | termios.ICRNL
        | termios.IXON
    )
    oflag &= ~termios.OPOST
    lflag &= ~(
        termios.ECHO | termios.ECHONL | termios.ICANON | termios.ISIG | termios.IEXTEN
    )
    cflag &= ~(termios.CSIZE | termios.PARENB)
    cflag |= termios.CS8
    termios.tcsetattr(
        slave,
        termios.TCSANOW,
        [iflag, oflag, cflag, lflag, ispeed, ospeed, control],
    )
    # Nothing is read back out of the master, so a write that has no room
    # left answers rather than waits: a reading nobody is reading is one
    # the sketch was not there for, and a sensor drops it rather than
    # holding a queue of the past.
    os.set_blocking(master, False)
    return master, slave, os.ttyname(slave)


def print_readings(rate):
    """The same readings, onto the port, until interrupted."""
    master, slave, path = make_port()
    # Flushed as it is printed: the path is the one thing a person
    # cannot work out for themselves, and a run whose output is piped
    # somewhere would otherwise hold it until the program ended.
    print(f"the board stands at {path} · a reading every {1000.0 / rate:g} ms",
          flush=True)
    print(f"open serial://{path}?baud={BAUD}", flush=True)
    try:
        started = time.monotonic()
        index = 0
        while True:
            at = (index + 0.5) / rate
            pause = started + at - time.monotonic()
            if pause > 0:
                time.sleep(pause)
            try:
                os.write(master, line_at(at) + b"\n")
            except BlockingIOError:
                pass
            index += 1
    finally:
        os.close(slave)
        os.close(master)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--record",
        metavar="PATH",
        help="write a recording at PATH instead of printing to a port",
    )
    parser.add_argument(
        "--seconds", type=float, default=8.0, help="how long a recording runs"
    )
    parser.add_argument("--rate", type=float, default=RATE, help="readings a second")
    arguments = parser.parse_args(argv)

    if arguments.record:
        written = write_recording(arguments.record, arguments.seconds, arguments.rate)
        print(f"{arguments.record}: {written} readings over {arguments.seconds:g}s")
        return 0

    try:
        print_readings(arguments.rate)
    except KeyboardInterrupt:
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
