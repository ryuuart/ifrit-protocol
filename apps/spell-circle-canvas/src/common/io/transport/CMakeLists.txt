# The transports a feed is opened through: UDP over Boost.Asio, one
# socket per feed, and one thread every socket of a registration shares.
#
# Asio is PRIVATE: the header names a Hub and a scheme and nothing of the
# network stack behind them, so a consumer that opens a feed inherits no
# executor, no socket and no Boost.
find_package(boost_asio CONFIG REQUIRED)
sigil_library(SigilIOTransport
  SOURCES Udp.cpp IoThread.cpp IoThread.h
  HEADERS Transport.h
  PUBLIC SigilIOHub
  PRIVATE Boost::asio)

# What the transport promises: the port a URI binds, the bytes that
# arrive through it, the peer a sending feed reaches, and what a URI
# nobody can open leaves on its feed. The cases send their own datagrams
# and hold their own ports through raw Asio sockets, which is the test's
# own use of Boost and not one it inherits.
sigil_test(io_test
  SOURCES test/UdpTest.cpp
  LIBRARIES SigilIOTransport Boost::asio)
