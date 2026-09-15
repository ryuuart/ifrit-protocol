# The transports a feed is opened through: UDP over Boost.Asio, one
# socket per feed and one thread every socket of a registration shares;
# WebSocket over uWebSockets, one listener per feed on a loop of its own;
# and the WebSocket client over libcurl, one session per feed on a thread
# of its own, which is what ws:// and wss:// open when the URI names a
# server to call rather than a port to hold.
#
# Every stack is PRIVATE: the header names a Hub and a scheme and
# nothing of the network behind them, so a consumer that opens a feed
# inherits no executor, no socket, no Boost and no loop. uWebSockets
# carries the sockets it stands on, and this port of it is built without
# TLS and without zlib, so there is no OpenSSL or ZLIB to find beside it
# and nothing it could listen for wss:// on; libcurl carries the TLS the
# client end speaks it over, and the hub beneath this feature already
# requires it.
find_package(boost_asio CONFIG REQUIRED)
find_package(unofficial-uwebsockets CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
sigil_library(SigilIOTransport
  SOURCES Udp.cpp WebSocket.cpp WebSocketClient.cpp IoThread.cpp IoThread.h
  HEADERS Transport.h
  PUBLIC SigilIOHub
  PRIVATE Boost::asio unofficial::uwebsockets::uwebsockets CURL::libcurl)

# What the transports promise: the port a URI binds, the server a URI
# calls, the bytes that arrive through either, who sent them, the peer or
# peers a sending feed reaches, and what a URI nobody can open leaves on
# its feed. The cases send their own datagrams, hold their own ports and
# speak the websocket handshake by hand through raw Asio sockets, which
# is the test's own use of Boost and not one it inherits; the client's
# cases call a listener this same process is holding, so both ends of a
# session stand in one binary.
sigil_test(io_test
  SOURCES test/UdpTest.cpp test/WebSocketTest.cpp test/WebSocketClientTest.cpp
  LIBRARIES SigilIOTransport Boost::asio)
