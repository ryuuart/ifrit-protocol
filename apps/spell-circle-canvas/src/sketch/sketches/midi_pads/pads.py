#!/usr/bin/env python3
"""The messages midi_pads reads: a controller's own bytes, as a recording.

    python3 pads.py                       # writes data/pads.feed
    python3 pads.py --record elsewhere.feed --seconds 8

THERE IS NO MIDI IN PYTHON'S STANDARD LIBRARY — no port to open and
nothing to send one through — so this writes the recording and nothing
else. Playing the sketch live is a controller plugged in, or any
application offering a virtual MIDI output for the sketch's `midi://in/`
door to open; either way the sketch reads the cable and this file is
only what a capture replays instead.

WHAT A MESSAGE IS. The raw bytes of one MIDI message, exactly as a
controller puts them on the wire: a status byte whose top half says what
the message is and whose bottom half says which channel it was played
on, then the data bytes that kind carries.

    0x90 note velocity      a pad struck
    0x80 note 0             the same pad coming back up
    0x90 note 0             which is how a keyboard usually says that
    0xB0 controller value   a knob standing somewhere new

Both spellings of a release are written here, alternately, because both
are what arrives off a real controller — and the sketch reads them as
one thing, a note on at no velocity being a release.

The sequence is a function of its own time and of nothing else, so the
file holds no randomness and a plate taken from it is the same picture
every run. Every message stands a little off the whole frames a reader
steps in, so no arrival is taken a frame early or a frame late.

Standard library only: this runs wherever python3 does, with nothing
installed.
"""

import argparse
import math
import struct
import sys

# The line every feed recording opens with, and the frame one arrival is
# written as: the time as a double, the length as a 32-bit unsigned, then
# that many bytes. Both numbers are written little-endian, which is the
# byte order of every machine that reads one of these.
HEADER = b"sigil-feed-recording 1\n"
FRAME = struct.Struct("<dI")

# EDIT THESE FIRST: what is played, and when.
CHANNEL = 0  # the wire counts channels from 0; every desk prints them from 1
FIRST_NOTE = 36  # the note the first pad sends, the sketch's own first cell
KNOB = 74  # the controller number the knob turns on

# WHEN EACH PAD IS STRUCK, which one, how long it is held, and how hard.
# Three of them are down at the second the plate is taken — the pads at
# 1.30, 2.20 and 2.85, none of which comes up before 3.60 — and they are
# three notes apart, so the picture shows one cell of each colour and
# both rows of the grid. The pad at 2.30 is up and faded well before it.
STRIKES = (
    (0.35, 36, 0.90, 104),
    (0.80, 41, 0.55, 88),
    (1.30, 38, 2.30, 120),
    (1.75, 36, 0.45, 72),
    (2.20, 42, 1.55, 112),
    (2.30, 39, 0.30, 80),
    (2.85, 40, 1.05, 96),
    (3.40, 37, 0.90, 116),
    (4.10, 43, 0.70, 84),
    (4.55, 39, 1.00, 108),
    (5.20, 36, 0.50, 76),
    (5.70, 43, 1.40, 124),
    (6.30, 41, 0.80, 92),
    (6.85, 38, 0.60, 100),
    (7.20, 40, 0.55, 118),
)

# HOW THE KNOB TURNS: one whole sweep over the run, read at this rate and
# sent only where the reading it stands at has changed, which is what a
# knob does. KNOB_MIDWAY is the second at which it stands in the middle
# of its travel, and it is the one the plate is taken at.
KNOB_PERIOD = 8.0
KNOB_RATE = 5.0
KNOB_MIDWAY = 2.9

# HOW FAR OFF THE FRAME GRID a message stands. A reader steps in whole
# frames and delivers every message due by the one it is on, so a
# message landing exactly on a frame is a message that may be taken on
# either side of it; this is less than half a frame at sixty a second,
# which puts every one of them between two frames and nowhere else.
OFFSET = 0.007


def knob_at(seconds):
    """Where the knob stands at `seconds`, 0 to 127."""
    turn = math.sin(2.0 * math.pi * (seconds - KNOB_MIDWAY) / KNOB_PERIOD)
    return max(0, min(127, int(round(63.5 + 63.5 * turn))))


def note_on(note, velocity):
    """A pad struck, this hard."""
    return bytes((0x90 | CHANNEL, note, velocity))


def note_off(note, index):
    """A pad coming back up, spelled the way this one is spelled: every
    other release is a note off, and the ones between are the note on at
    no velocity a keyboard sends instead."""
    if index % 2 == 0:
        return bytes((0x80 | CHANNEL, note, 0))
    return bytes((0x90 | CHANNEL, note, 0))


def control_change(controller, value):
    """A knob standing somewhere new."""
    return bytes((0xB0 | CHANNEL, controller, value))


def sequence(seconds):
    """Every message of a run of `seconds`, as (at, bytes), in order."""
    messages = []
    for index, (at, note, held, velocity) in enumerate(STRIKES):
        if at >= seconds:
            continue
        messages.append((round(at + OFFSET, 5), note_on(note, velocity)))
        release = at + held
        if release < seconds:
            messages.append((round(release + OFFSET, 5), note_off(note, index)))
    standing = None
    for index in range(int(seconds * KNOB_RATE)):
        # Half a step off the grid the readings are taken on, so a
        # reading and a strike never land at the same moment.
        at = (index + 0.5) / KNOB_RATE + OFFSET
        value = knob_at(at)
        if value == standing:
            continue
        standing = value
        messages.append((round(at, 5), control_change(KNOB, value)))
    messages.sort(key=lambda message: message[0])
    return messages


def write_recording(path, seconds):
    """Every message of a run of `seconds`, as a recording file."""
    written = 0
    with open(path, "wb") as recording:
        recording.write(HEADER)
        for at, payload in sequence(seconds):
            recording.write(FRAME.pack(at, len(payload)))
            recording.write(payload)
            written += 1
    return written


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--record",
        metavar="PATH",
        default="data/pads.feed",
        help="where to write the recording",
    )
    parser.add_argument(
        "--seconds", type=float, default=8.0, help="how long the recording runs"
    )
    arguments = parser.parse_args(argv)

    written = write_recording(arguments.record, arguments.seconds)
    print(f"{arguments.record}: {written} messages over {arguments.seconds:g}s")
    return 0


if __name__ == "__main__":
    sys.exit(main())
