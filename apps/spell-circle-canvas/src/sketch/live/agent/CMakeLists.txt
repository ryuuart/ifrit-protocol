# The session domain's agent and the clock domain's: the one running
# sketch a protocol host holds, opened for its clock and photographed as
# a plate is, and SigilMotion's policy clock it is drawn at — with the
# bundle that mounts them beside the registry's agent. A leaf of its own,
# so the host tier a window or a sweep stands on links no protocol.
sigil_library(SigilSketchSessionAgent
  SOURCES SessionAgent.cpp ClockAgent.cpp HostAgents.cpp
  HEADERS SessionAgent.h ClockAgent.h HostAgents.h
  PUBLIC SigilSketch SigilSketchRegistryAgent SigilProtocolDispatch
         SigilMotionClock
  PRIVATE SigilImageEncode SigilIOSource)

# Their self-checks, under the protocol's one label: every command each
# answers, asked in process, and the clock's claims as cases — among
# them that an endpoint mounted with no client costs a run nothing.
sigil_test(sketch_test
  SOURCES test/HostAgentsTest.cpp test/QuietEndpointTest.cpp
  LIBRARIES SigilSketchSessionAgent SigilSketchTesting SigilProtocolEndpoint
            SigilIOHub
  SUITES SketchSessionAgent SketchClockAgent SketchQuietEndpoint
  LABELS protocol)
