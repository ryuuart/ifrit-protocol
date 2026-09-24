# THE ENDPOINT: the dispatcher behind SigilData's connection on SigilIO's
# ws:// listener — loopback unless peers beyond it are stated, the
# address written under the state root before the first frame, the
# definition served at /protocol on the same port, and every command
# answered inside the hub's dispatch. Its own target, so a consumer that
# attaches in process links no socket. Asio's address reading holds a
# stated peer to a bare IP address before it rides in the listener's
# query.
find_package(boost_asio CONFIG REQUIRED)
sigil_library(SigilProtocolEndpoint
  SOURCES Endpoint.cpp
  HEADERS Endpoint.h
  PUBLIC SigilProtocolDispatch
  PRIVATE SigilDataConnection SigilIOHub SigilIOTransport Boost::asio)

# THE ENDPOINT'S SELF-CHECKS, under the same label: a real socket on
# loopback, the address file written before the first frame and taken
# back after, the definition served, a command answered over the wire, a
# peer that leaves detached, and a peer beyond loopback refused — the
# last only where this machine has an address that is not loopback.
sigil_test(protocol_test
  SOURCES test/EndpointTest.cpp
  LIBRARIES SigilProtocolEndpoint SigilDataConnection SigilIOHub
            SigilIOTransport
  LABELS protocol)
