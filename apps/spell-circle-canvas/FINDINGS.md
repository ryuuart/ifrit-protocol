# Findings

## psx_doom_fire renders a different plate after an unrelated rebuild

With floating-point contraction pinned off for every target of the tree,
the six scenes that used to move with every rebuild held byte-identical
across a rebuild whose only change was a comment line in a header every
sketch includes, and psx_doom_fire moved instead — a scene that had held
across every rebuild of the day before the flag. It steps a fire
simulation on a kept canvas for six seconds before its capture, seeded
from a fixed xorshift state, so any one-bit difference anywhere in the
step compounds over three hundred and sixty frames. What differs between
two builds of identical sources is not contraction now; the remaining
candidates are a read of a cell the step has not yet written, and code
in the sketch's own translation unit whose result the compiler may order
differently once inlining shifts. A test should build twice from
identical sources with one unrelated object changed between the builds
and require the same digest for this scene; the fix is in the sketch
once the read is found.

## sketch_test dies on WebCore's resource-usage thread after the shared engine shuts down

`shutdownSharedEngine()` destroys the one Ultralight renderer the process
booted, and `WebEngine::~WebEngine` releases it on the web thread once
the last view is gone. WebCore's `ResourceUsageThread`, started when a
page first loads, is not joined by that release: it keeps polling after
the renderer is freed and reads freed state, so about one run in three
of `sketch_test` dies with SIGSEGV or SIGBUS in whichever kit suite
follows the web-engine suites, and the crash report names that thread
rather than the test that was running. What the release evidently
intends is a process left as it was before the engine booted — which
the settled-page suite also assumes when it boots a second engine, and
finds it cannot. A test should shut the shared engine down, then boot a
view again and read a settled frame from it, with no thread of the
first engine left running; the fix is scry's — keep the one renderer
for the process's lifetime and hand it back to a later engine, or stop
that thread before the renderer goes.
