# Findings

A work queue: each entry states what the code does, what it was evidently
intended to do, and what a test should assert once intent is restored.
Entries are deleted as they are fixed, and the file is deleted when it is
empty.

## live_settling's plate moves under a full sweep and holds alone

**What the code does.** In one whole-registry sweep, five jobs at once,
`live_settling` rendered with a different hash from its baseline; rendered
alone twice afterwards it was byte-identical to that baseline both times.
The sketch settles a web page through the settle machine, whose quiet
stage is a stretch of time with no repaint in it, and under the load of a
sweep the engine's repaints land later, so the frame the settle stops on
is not always the same frame.

**What it was intended to do.** A capture is a function of the
declaration alone; the settle's stages are judged on the page's own
events, and the quiet window was meant to be the one clock, long enough
that load does not move the frame it stops on.

**What a test should assert.** A settle on the same page, run under a
parallel load, stops on the same frame; the plate ledger's full sweep
and a single render of `live_settling` agree.

## libdatachannel ends the process from its own threads

**What the code does.** Two paths in libdatachannel 0.24.3 throw a
`std::runtime_error` across a C frame on threads the library owns: the
ICE-state change that starts the DTLS transport, when the handshake's
first write fails because two agents completed their candidates on one
poll sweep, and the SCTP transport's write callback during teardown,
when the protocol underneath is already shut down. No frame of this
tree's is on either stack, so no guard here catches them, and the
process ends. The WebRTC door guards every call it makes, and its suite
keeps one end of a conversation per process, which is the topology that
does not provoke the first path; a phone reconnecting while another
connects could still provoke it in a window.

**What it was intended to do.** A failed handshake or a write on a
transport being torn down is one peer's ending, reported through the
connection's state, never the process's.

**What a test should assert.** With the port carrying a patch that
wraps the DTLS start in the ICE-state lambda and the SCTP write
callback in the library's own error handling, two peers completing
their handshakes on one poll sweep in one process end with both
connections open or one reported failed, and the process running;
the patch belongs in the sigil-vcpkg-registry port with the baseline
bumped.
