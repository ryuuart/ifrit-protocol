# Findings

## Six scenes render a different plate after an unrelated rebuild

codec_roundtrip, contour_poses, formation_bands, pop_billboards, pop_math
and usd_roundtrip render byte-identically across repeated runs of one
Sketchbook binary and differently after any rebuild of the libraries,
including rebuilds that touch no code they run: the same six moved across
the 11 September build, this morning's cascade build and this afternoon's
kernel build, while the other 191 scenes stayed identical across all
three. The six share a capture moment of 0.05 seconds and the page
furniture; four run the point operators, two the geometry paths, two load
models. The plate compare confines every difference to sub-pixel shifts
of small marks, with nothing cleared and nothing new. The point kit's
lanes are vectors with fill values and its scatter takes a fixed seed, so
the drift is not in what those declare. A plate is meant to be a function
of the sources alone, which is what the ledger's byte identity across
builds assumes. A test should render one of the six from two binaries
built from identical sources with one unrelated object changed between
them and require the same digest; the diagnosis that fits a picture
stable per binary and moving per build is an uninitialised read, which a
memory-sanitizer run over these six scenes' path would name.
