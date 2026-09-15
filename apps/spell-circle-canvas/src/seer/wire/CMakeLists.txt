# The wires themselves: the feeds a session has open and what each of
# them is doing, the log of one wire's messages, the way out through a
# peer, the recording a wire is written down to, and the four readings a
# message is shown through.
#
# PUBLIC is what the headers spell: SigilIO's feeds and its byte
# vocabulary, and nothing else. The transports are PRIVATE because no
# header of this library names one — a scheme is a string here — and the
# JSON parser and the OSC codec are PRIVATE for the same reason: a
# reading answers a string and a spelled message answers bytes, so
# nobody who links this inherits a document type.
sigil_library(SigilSeerWire
  SOURCES Log.cpp Recorder.cpp Rendering.cpp Sender.cpp Wires.cpp
  HEADERS Log.h Recorder.h Rendering.h Sender.h Wires.h
  PUBLIC SigilIO
  PRIVATE SigilIOTransport SigilDataDecode)

# What a wire promises: a URI nobody can open is still a row that says
# why, two wires on the loopback carry bytes between them in order, a
# message is kept with the sender it was delivered with, a tick reads a
# rate off a wire without a clock of its own, a recording is a file that
# reads back and a wire that plays it as time moves forward, and each
# reading of a message is empty exactly when the message is not that.
# No toolkit: every one of them runs with no application.
#
# The codec is named here and not in the library: what the OSC cases
# assert is that a reading and a spelling agree with the bytes the codec
# itself writes, which is a claim about both and cannot be made against
# one of them alone.
sigil_test(seer_test
  SOURCES test/OscTest.cpp test/WiresTest.cpp
  LIBRARIES SigilSeerWire SigilDataDecode)
