# The transports a feed is opened through: UDP over Boost.Asio, one
# socket per feed and one thread every socket of a registration shares;
# WebSocket over uWebSockets, one listener per feed on a loop of its own.
#
# Both stacks are PRIVATE: the header names a Hub and a scheme and
# nothing of the network behind them, so a consumer that opens a feed
# inherits no executor, no socket, no Boost and no loop. uWebSockets
# carries the sockets it stands on, and this port of it is built without
# TLS and without zlib, so there is no OpenSSL or ZLIB to find beside it
# and no wss:// scheme to register.
find_package(boost_asio CONFIG REQUIRED)
find_package(unofficial-uwebsockets CONFIG REQUIRED)
sigil_library(SigilIOTransport
  SOURCES Udp.cpp WebSocket.cpp IoThread.cpp IoThread.h
  HEADERS Transport.h
  PUBLIC SigilIOHub
  PRIVATE Boost::asio unofficial::uwebsockets::uwebsockets)

# What the transports promise: the port a URI binds, the bytes that
# arrive through it, who sent them, the peer or peers a sending feed
# reaches, and what a URI nobody can open leaves on its feed. The cases
# send their own datagrams, hold their own ports and speak the websocket
# handshake by hand through raw Asio sockets, which is the test's own use
# of Boost and not one it inherits.
sigil_test(io_test
  SOURCES test/UdpTest.cpp test/WebSocketTest.cpp
  LIBRARIES SigilIOTransport Boost::asio)
