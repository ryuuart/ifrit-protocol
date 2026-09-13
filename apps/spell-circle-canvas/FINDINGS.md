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
