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

## web_script moves under a four-job sweep and holds when rendered alone

In a sweep of the whole registry at four jobs, web_script's plate came
back with a different digest, and two renders of that one scene alone,
straight afterwards, matched the baseline byte for byte. The scene loads
a page through the shared web engine, whose frame arrives on a thread of
its own, so what the capture reads evidently depends on how far that
thread has run when the sweep is contended, which is a settle the sketch
does not wait for. A test should render the scene under load — the other
scenes of the sweep running beside it — and require the digest a solo
render gives; the fix is in the sketch or the settled-page door it
should be using, so the capture waits for the frame it describes.
