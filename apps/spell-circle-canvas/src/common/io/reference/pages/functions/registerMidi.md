---
kind: function
library: SigilIO
name: registerMidi
qualified: sigil::io::registerMidi
group: Transports
status: stable
---

# registerMidi

## Description

Installs the MIDI transport on the hub for the scheme "midi". A URI of
the form midi://in/NAME opens the first input port whose own name holds
NAME, letter for letter with case left out of it, and midi://out/NAME the
first output port that does; NAME left out altogether takes the first
port there is, which is the one controller on a desk that has one. A PORT
IS NAMED AND NEVER NUMBERED here, because which port a machine calls its
second depends on what else was plugged in this morning. A name nobody
answers to opens nothing and leaves on the feed both the name that was
looked for and the ports that do exist.

### A port made rather than found

midi://in/virtual:NAME and midi://out/virtual:NAME MAKE a port of that
name instead of looking for one, so other software on this machine can
reach the scene as it reaches a controller. A system that does not offer
ports made rather than found opens nothing and says so.

### An input delivers every message the wire carries

One arrival per message, status byte first and the bytes exactly as they
arrived — system exclusive included, since that is what a controller
answers a question with. The two a scene cannot use are left out: a clock
beats twenty-four times a quarter note and a sensing byte arrives several
times a second whether or not anybody played anything, so a feed taking
both would be a feed of heartbeat with the performance somewhere inside
it. Every arrival names the port as its sender, spelled midi://in/NAME
with the port's whole name, exactly as the `Feed::address` such a feed
reports is; an output reports midi://out/NAME the same way.

An input is ONE WAY — what comes back down a cable is the other cable,
which is a door of its own — so `Feed::send` and `Feed::sendTo` are both
false on one, while an output's send writes the bytes as one message and
is false where the driver refused it.

### No thread is started

The driver runs a callback of its own on every arriving message, which is
the thread a message is delivered from, and an output is written on the
thread that asked.
