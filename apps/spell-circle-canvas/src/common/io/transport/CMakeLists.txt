# The transports a feed is opened through: UDP over Boost.Asio, one
# socket per feed and one thread every socket of a registration shares;
# WebSocket over uWebSockets, one listener per feed on a loop of its own;
# the WebSocket client over libcurl, one session per feed on a thread
# of its own, which is what ws:// and wss:// open when the URI names a
# server to call rather than a port to hold; the shared memory
# reader, which carries no stack at all — a region another process on
# this machine mapped, looked at on a timer that shares the UDP
# transport's kind of thread. POSIX is what a region is made of here,
# so it needs nothing found beside it; Windows would carry a file of its
# own behind the same door. And MIDI over RtMidi, which is the platform's
# own MIDI stack behind one class: no thread is started for it at all,
# the driver running the callback every arriving message is delivered
# from. And the serial port, over Boost.Asio again: a device file read
# on the UDP transport's kind of thread, with the lines a board prints
# cut out of the stream that carries them.
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
find_package(rtmidi CONFIG REQUIRED)
sigil_library(SigilIOTransport
  SOURCES Udp.cpp WebSocket.cpp WebSocketClient.cpp SharedMemory.cpp Midi.cpp
          Serial.cpp IoThread.cpp IoThread.h
  HEADERS Transport.h
  PUBLIC SigilIOHub
  PRIVATE Boost::asio unofficial::uwebsockets::uwebsockets CURL::libcurl
          RtMidi::rtmidi)

# What the transports promise: the port a URI binds, the server a URI
# calls, the region a URI maps, the bytes that arrive through any of
# them, who sent them, the peer or peers a sending feed reaches, and
# what a URI nobody can open leaves on its feed. The cases send their
# own datagrams, hold their own ports and speak the websocket handshake
# by hand through raw Asio sockets, which is the test's own use of Boost
# and not one it inherits; the client's cases call a listener this same
# process is holding and the region's cases write the region they read,
# and the MIDI cases make a port of their own and open it back, so both
# ends of a session, of a region and of a cable stand in one binary. The
# serial cases make a pseudo-terminal pair and write the readings into
# the master, the feed opening the slave by the path the system named
# it, so a board is needed for none of them.
sigil_test(io_test
  SOURCES test/UdpTest.cpp test/WebSocketTest.cpp test/WebSocketClientTest.cpp
          test/SharedMemoryTest.cpp test/MidiTest.cpp test/SerialTest.cpp
  LIBRARIES SigilIOTransport Boost::asio)
