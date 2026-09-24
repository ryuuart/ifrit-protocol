# THE DISPATCHER: every handler a host mounts filed by method, the
# clients attached one session each, which domain's events each has
# enabled, and the refusals no agent is asked for — with the host domain
# this library answers itself, and the in-process form a test attaches
# to. No socket stands here, so a test of a domain's agent links this
# and nothing of the network.
sigil_library(SigilProtocolDispatch
  SOURCES Dispatcher.cpp Declared.cpp Declared.h HostDomain.cpp
          InProcess.cpp Parameters.cpp Parameters.h
  HEADERS Dispatcher.h HostDomain.h InProcess.h Program.h
  PUBLIC SigilProtocolDefinition SigilDataDecode)

# THE DISPATCHER'S SELF-CHECKS, under the seams' one label: host.describe
# answers first on every dispatcher; a method the definition does not
# declare, one no agent is mounted for, parameters their table cannot
# hold and a message that is no request are each refused naming what
# was refused, the agent never asked; events reach a client only between
# its enable and its disable; and a client's detaching takes what it
# alone set with it.
sigil_test(protocol_test
  SOURCES test/DescribeTest.cpp test/RefusalsTest.cpp test/EventsTest.cpp
          test/SessionsTest.cpp
  LIBRARIES SigilProtocolDispatch
  LABELS protocol)
