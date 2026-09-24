# The registry domain's agent: the registry's rows and the catalog's,
# answered as the protocol's tables. A leaf of its own, so the core a
# sketch is written against links no protocol, and a host that mounts no
# agent links none either.
sigil_library(SigilSketchRegistryAgent
  SOURCES RegistryAgent.cpp
  HEADERS RegistryAgent.h
  PUBLIC SigilSketch SigilProtocolDefinition)

# Its self-check, under the protocol's one label: mounted on a dispatcher
# and asked in process, it answers the registry's rows and the catalog's.
sigil_test(sketch_test
  SOURCES test/RegistryAgentTest.cpp
  LIBRARIES SigilSketchRegistryAgent SigilProtocolDispatch
  SUITES SketchRegistryAgent
  LABELS protocol)
