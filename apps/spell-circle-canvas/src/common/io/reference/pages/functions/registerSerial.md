---
kind: function
library: SigilIO
name: registerSerial
qualified: sigil::io::registerSerial
group: Transports
status: stable
---

# registerSerial

## Description

Installs the SERIAL transport on the hub for the scheme "serial". A URI
of the form serial://DEVICE?baud=RATE opens the device file of that path
— whole and absolute, as in serial:///dev/tty.usbmodem1101?baud=115200 —
at that rate. THE RATE IS REQUIRED and there is none to fall back on: two
ends that disagree about it read each other as noise, so a URI carrying
none opens nothing and says so. The rest of the wire's settings stand in
the same query and have the defaults a board is wired for —
bits=5|6|7|8 (8), parity=none|odd|even (none), stop=1|2 (1),
flow=none|software|hardware (none) — and a port that will not take one of
them is a door that does not open, since a port read at a setting nobody
asked for answers bytes that are not the ones on the wire.

### A message is a line, both ways

What reaches a serial port is a run of bytes with no message boundary in
it, so the boundary is the one the sender writes: every arrival is the
bytes up to a newline, with a carriage return before it left off and a
blank line delivered to nobody, and half a line is no arrival at all
until the rest of it comes. A FEED THAT OPENS ONTO A WIRE ALREADY IN
MID-LINE takes the tail of that line as its first arrival, there being
nothing in the bytes that says where the line began. A run of 64 KiB with
no newline among them is handed over as the line it stands as and the
reading begins again, so a sender that frames nothing still reaches a
reader. `Feed::send` writes the bytes and a newline after them, which is
where the reader on the board stops.

### A cable holds one peer

`Feed::send` reaches what is at the other end and there is no sender to
pick out by name, so `Feed::sendTo` is false on such a feed. Every
arrival names the port as its sender, spelled serial://DEVICE with the
settings left off — what the board IS, and not how this end was told to
read it — exactly as the `Feed::address` the feed reports is. The ports
every registration opens share ONE thread, made when the first of them
opens, so a hub taught the scheme and never asked for a port starts
nothing.
