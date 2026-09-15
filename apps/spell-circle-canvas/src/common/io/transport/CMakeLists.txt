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
# cut out of the stream that carries them. And gRPC, at both ends of one
# generic method: a server holding a port and a call reaching one, with
# no generated stub anywhere in the transport and no thread of this
# library's at all, gRPC running its own and calling back onto them. And
# WebRTC over libdatachannel: one connection per peer and one channel on
# each, introduced over a websocket door this same feature opens, with
# no thread of this library's either.
#
# Every stack is PRIVATE: the header names a Hub and a scheme and
# nothing of the network behind them, so a consumer that opens a feed
# inherits no executor, no socket, no Boost and no loop. uWebSockets
# carries the sockets it stands on, and this port of it is built without
# TLS and without zlib, so there is no OpenSSL or ZLIB to find beside it
# and nothing it could listen for wss:// on; libcurl carries the TLS the
# client end speaks it over, and the hub beneath this feature already
# requires it. gRPC brings its own stack the same way — its transport,
# its threads and the protobuf, abseil and OpenSSL beneath them are
# found by its own package and reach nothing above this target.
# libdatachannel brings the same way what a connection through a router
# is made of — the routes, the encryption and the stream — and its
# introductions cross a door this feature already opens, so there is no
# signalling server to find beside it. And QUIC over msquic: one
# encrypted connection per peer on one UDP port, a unidirectional stream
# per message and datagrams beside them, with no thread of this
# library's either — msquic runs its own workers and calls back onto
# them. It carries the TLS that door is made of inside its own shared
# library, so there is no OpenSSL to find beside it, and it is linked
# privately like every other stack here.
find_package(boost_asio CONFIG REQUIRED)
find_package(unofficial-uwebsockets CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(rtmidi CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(LibDataChannel CONFIG REQUIRED)
find_package(msquic CONFIG REQUIRED)
sigil_library(SigilIOTransport
  SOURCES Udp.cpp WebSocket.cpp WebSocketClient.cpp SharedMemory.cpp Midi.cpp
          Serial.cpp Grpc.cpp Quic.cpp WebRtc.cpp Introduction.cpp
          Introduction.h IoThread.cpp IoThread.h
  HEADERS Transport.h
  PUBLIC SigilIOHub
  PRIVATE Boost::asio unofficial::uwebsockets::uwebsockets CURL::libcurl
          RtMidi::rtmidi gRPC::grpc++ LibDataChannel::LibDataChannel msquic)

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
# it, so a board is needed for none of them. The gRPC cases call a
# server this same process is holding, so both ends of a method stand in
# one binary and no stub is generated for either. The WebRTC cases give
# each end a hub of its own — which is what two machines are in one
# process — and dispatch every one of them on every look, an
# introduction crossing on the frame. The QUIC cases call a port this
# same process is holding, so both ends of a connection stand in one
# binary; that port has to answer for itself, so they WRITE A CERTIFICATE
# AND A KEY into a scratch directory of their own with OpenSSL — which
# is why this binary and no other target here finds it — and the call
# reaches the self-signed pair the way a machine on a stage does.
find_package(OpenSSL REQUIRED)
sigil_test(io_test
  SOURCES test/UdpTest.cpp test/WebSocketTest.cpp test/WebSocketClientTest.cpp
          test/SharedMemoryTest.cpp test/MidiTest.cpp test/SerialTest.cpp
          test/GrpcTest.cpp test/QuicTest.cpp test/WebRtcTest.cpp
  LIBRARIES SigilIOTransport Boost::asio OpenSSL::SSL OpenSSL::Crypto)
