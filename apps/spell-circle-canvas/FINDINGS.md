# Findings

## Six scenes render a different plate after an unrelated rebuild

codec_roundtrip, contour_poses, formation_bands, pop_billboards, pop_math
and usd_roundtrip render byte-identically across repeated runs of one
Sketchbook binary and differently after a rebuild of the libraries whose
changes touch no code they run: the same six moved across the 11
September build, this morning's cascade build and this afternoon's
kernel build, while the other 191 scenes stayed identical across all
three, and by the evening build pop_math had returned to its 11
September digest, so a scene alternates between a few stable pictures.
The plate compare confines every difference to sub-pixel shifts of small
marks, with nothing cleared and nothing new. Rendering with every heap
allocation pre-filled (libmalloc's scribble) reproduces the standing
digest exactly, so an uninitialised heap read is ruled out; the
libraries on the path hold no pointer-keyed container or pointer
comparison. The six share a capture moment of 0.05 seconds; four run
the point operators, two the geometry paths, two load models — all of
them numerically heavy inline code in the sketch's own translation unit,
where the compiler may fuse a multiply and an add or not depending on
which calls it inlined, and what it inlines shifts with unrelated code
in the same unit. The material library already compiles its shaders
with contraction off for this reason. A plate is meant to be a function
of the sources alone, which is what the ledger's byte identity across
builds assumes. A test should build twice from identical sources with
one unrelated object changed between the builds and require the same
digest for one of the six; the fix to try is contraction off for the
tree's own targets, re-verified by that test.
