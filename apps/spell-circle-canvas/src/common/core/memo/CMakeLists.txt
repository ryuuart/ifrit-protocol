# SigilCoreMemo — the keyed memo: the "compare the inputs, rebuild when
# they differ" guard every animated consumer otherwise hand-rolls, the
# value held together with its guard, and the snap a drifting input is
# keyed through. It knows nothing of what it holds and nothing of a tree,
# so a text kit, a gallery scene and a retained host all reach it without
# acquiring a kernel. The standard library is the whole of its
# dependencies. Header-only, hence an INTERFACE target.
sigil_library(SigilCoreMemo
  HEADERS Rebuild.h)

# When a build fires, what a throw leaves behind, the held value, and the
# snap.
sigil_test(core_test
  SOURCES test/RebuildTest.cpp
  LIBRARIES SigilCoreMemo)
