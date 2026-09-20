#include "QuicPeer.h"

#include <msquic.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "QuicAddress.h"
#include "QuicLibrary.h"
#include "QuicSession.h"
#include "sigilio/hub/Feed.h"

namespace sigil::io::quic {
namespace {

/** A stream this end opened, from the send that started it to the end
 *  the library indicates when it is over. */
QUIC_STATUS QUIC_API onSentStream(HQUIC stream, void* context,
                                  QUIC_STREAM_EVENT* event);

/** A stream the other end opened, which is one message arriving in
 *  however many pieces. */
QUIC_STATUS QUIC_API onReceivedStream(HQUIC stream, void* context,
                                      QUIC_STREAM_EVENT* event);

QUIC_STATUS QUIC_API onSentStream(HQUIC stream, void* context,
                                  QUIC_STREAM_EVENT* event) {
  const InCallback callback;
  auto* const sending = static_cast<Sending*>(context);
  if (event->Type == QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE) {
    // The one place a stream this end opened may be given back, and the
    // one place the bytes it wrote from may be freed.
    library().api->StreamClose(stream);
    delete sending;
  }
  return QUIC_STATUS_SUCCESS;
}

QUIC_STATUS QUIC_API onReceivedStream(HQUIC stream, void* context,
                                      QUIC_STREAM_EVENT* event) {
  const InCallback callback;
  auto* const incoming = static_cast<Incoming*>(context);
  switch (event->Type) {
    case QUIC_STREAM_EVENT_RECEIVE: {
      if (incoming->abandoned) break;
      std::vector<std::byte>& gathered = incoming->message.bytes;
      if (gathered.size() + event->RECEIVE.TotalBufferLength >
          kMessageCeiling) {
        // A message is one whole thing, so a stream that will not be one
        // is dropped rather than held: the bytes go, and the other end is
        // told to stop writing this one.
        incoming->abandoned = true;
        gathered.clear();
        gathered.shrink_to_fit();
        library().api->StreamShutdown(
            stream, QUIC_STREAM_SHUTDOWN_FLAG_ABORT_RECEIVE, kNothingToSay);
        break;
      }
      for (uint32_t at = 0; at < event->RECEIVE.BufferCount; ++at) {
        const QUIC_BUFFER& piece = event->RECEIVE.Buffers[at];
        const auto* const first =
            reinterpret_cast<const std::byte*>(piece.Buffer);
        gathered.insert(gathered.end(), first, first + piece.Length);
      }
      // THE END OF THE STREAM IS THE END OF THE MESSAGE, and it arrives
      // on the last piece as often as it arrives on its own.
      if ((event->RECEIVE.Flags & QUIC_RECEIVE_FLAG_FIN) != 0) {
        incoming->delivered = true;
        deliver(*incoming->session, incoming->named,
                std::move(incoming->message));
      }
      break;
    }
    case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
      if (!incoming->abandoned && !incoming->delivered) {
        incoming->delivered = true;
        deliver(*incoming->session, incoming->named,
                std::move(incoming->message));
      }
      // Nothing is shut down from here. A stream the other end opened
      // one way has no send side at this end, so the end of theirs is
      // the end of the whole stream and the library says so next.
      break;
    case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
      // Half a message is no message: what was gathered is dropped.
      incoming->abandoned = true;
      break;
    case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
      library().api->StreamClose(stream);
      delete incoming;
      break;
    default:
      break;
  }
  return QUIC_STATUS_SUCCESS;
}

}  // namespace

void Peer::adopt(HQUIC connection, const std::shared_ptr<Peer>& self) {
  m_self = self;
  {
    const std::lock_guard<std::mutex> lock(m_gate);
    m_connection = connection;
  }
  if (m_dialled) return;
  const std::lock_guard<std::mutex> lock(m_session->gate);
  m_session->peers[m_named] = self;
}

void Peer::abandon() {
  m_stopped.store(true, std::memory_order_release);
  {
    const std::lock_guard<std::mutex> lock(m_gate);
    m_connection = nullptr;
    m_ending = true;
  }
  forget();
  const std::shared_ptr<Peer> last = std::move(m_self);
}

void Peer::discard() {
  m_stopped.store(true, std::memory_order_release);
  HQUIC connection = nullptr;
  {
    const std::lock_guard<std::mutex> lock(m_gate);
    connection = m_connection;
    m_connection = nullptr;
    m_ending = true;
  }
  if (connection) library().api->ConnectionClose(connection);
  forget();
  const std::shared_ptr<Peer> last = std::move(m_self);
}

bool Peer::write(const Bytes& message) {
  const Library& lib = library();
  const std::lock_guard<std::mutex> lock(m_gate);
  if (m_ending || !m_connection) return false;
  return m_session->datagrams ? sendDatagram(lib, message)
                              : sendStream(lib, message);
}

void Peer::shutdown() {
  if (m_stopped.exchange(true)) return;
  const std::lock_guard<std::mutex> lock(m_gate);
  if (m_connection)
    library().api->ConnectionShutdown(
        m_connection, QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, kNothingToSay);
}

void Peer::takeStream(HQUIC stream) {
  auto* const incoming = new Incoming{m_session, m_named, {}, false, false};
  library().api->SetCallbackHandler(
      stream, reinterpret_cast<void*>(&onReceivedStream), incoming);
}

void Peer::takeDatagram(const QUIC_BUFFER& buffer) {
  Bytes message;
  const auto* const first = reinterpret_cast<const std::byte*>(buffer.Buffer);
  message.bytes.assign(first, first + buffer.Length);
  deliver(*m_session, m_named, std::move(message));
}

void Peer::concluded() {
  // No shutdown touches the handle from here on, which is what lets
  // everything below run without one part waiting for another.
  m_stopped.store(true, std::memory_order_release);
  forget();
  if (m_dialled) tell();
  {
    const std::lock_guard<std::mutex> lock(m_gate);
    m_ending = true;
    if (m_connection) library().api->ConnectionClose(m_connection);
    m_connection = nullptr;
  }
  // The last reference goes here and nowhere earlier: this is the one
  // place a peer may be freed, and it is freed on the way out of this
  // function with nothing left to touch.
  const std::shared_ptr<Peer> last = std::move(m_self);
}

bool Peer::sendStream(const Library& lib, const Bytes& message) {
  auto* const sending = new Sending(message);
  HQUIC stream = nullptr;
  if (QUIC_FAILED(lib.api->StreamOpen(m_connection,
                                      QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL,
                                      &onSentStream, sending, &stream))) {
    delete sending;
    return false;
  }
  // The stream is started BY the send rather than before it, so one
  // message crosses as one act and there is no half-open stream to
  // take down when a second call is the one that fails.
  const QUIC_SEND_FLAGS flags = QUIC_SEND_FLAG_START | QUIC_SEND_FLAG_FIN;
  if (QUIC_FAILED(
          lib.api->StreamSend(stream, &sending->buffer, 1, flags, sending))) {
    // A stream that never started indicates nothing, so the handle is
    // given back here rather than waited for.
    lib.api->StreamClose(stream);
    delete sending;
    return false;
  }
  return true;
}

bool Peer::sendDatagram(const Library& lib, const Bytes& message) {
  const uint16_t most = m_datagramCeiling.load(std::memory_order_acquire);
  if (most == 0 || message.bytes.size() > most) return false;
  auto* const sending = new Sending(message);
  if (QUIC_FAILED(lib.api->DatagramSend(m_connection, &sending->buffer, 1,
                                        QUIC_SEND_FLAG_NONE, sending))) {
    delete sending;
    return false;
  }
  return true;
}

void Peer::forget() {
  std::shared_ptr<Peer> kept;
  const std::lock_guard<std::mutex> lock(m_session->gate);
  const auto found = m_session->peers.find(m_named);
  if (found == m_session->peers.end()) return;
  if (found->second.get() != this) return;
  kept = std::move(found->second);
  m_session->peers.erase(found);
}

void Peer::tell() {
  if (m_given.load(std::memory_order_acquire)) return;
  const std::shared_ptr<Feed> feed = m_session->feed.lock();
  if (!feed) return;
  const bool reached = m_reached.load(std::memory_order_acquire);
  if (m_byPeer || (reached && m_why == QUIC_STATUS_SUCCESS)) {
    feed->close();
    return;
  }
  if (reached) {
    feed->fail(m_named + " ended: " + reasonOf(m_why));
    return;
  }
  if (m_why == QUIC_STATUS_CONNECTION_TIMEOUT) {
    feed->fail("could not reach " + m_named + " within ten seconds");
    return;
  }
  feed->fail("could not reach " + m_named + ": " + reasonOf(m_why));
}

QUIC_STATUS QUIC_API onConnection(HQUIC /*connection*/, void* context,
                                  QUIC_CONNECTION_EVENT* event) {
  const InCallback callback;
  auto* const peer = static_cast<Peer*>(context);
  switch (event->Type) {
    case QUIC_CONNECTION_EVENT_CONNECTED:
      peer->reached();
      break;
    case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
      peer->reached();
      peer->takeStream(event->PEER_STREAM_STARTED.Stream);
      break;
    case QUIC_CONNECTION_EVENT_DATAGRAM_RECEIVED:
      peer->reached();
      peer->takeDatagram(*event->DATAGRAM_RECEIVED.Buffer);
      break;
    case QUIC_CONNECTION_EVENT_DATAGRAM_STATE_CHANGED:
      peer->datagramCeiling(event->DATAGRAM_STATE_CHANGED.SendEnabled
                                ? event->DATAGRAM_STATE_CHANGED.MaxSendLength
                                : 0);
      break;
    case QUIC_CONNECTION_EVENT_DATAGRAM_SEND_STATE_CHANGED:
      // A datagram that will not change state again is one whose bytes
      // nobody is reading any more, however it ended.
      if (QUIC_DATAGRAM_SEND_STATE_IS_FINAL(
              event->DATAGRAM_SEND_STATE_CHANGED.State))
        delete static_cast<Sending*>(
            event->DATAGRAM_SEND_STATE_CHANGED.ClientContext);
      break;
    case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
      peer->endingWith(event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status);
      break;
    case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
      peer->endedByPeer();
      break;
    case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
      peer->concluded();
      break;
    default:
      break;
  }
  return QUIC_STATUS_SUCCESS;
}

QUIC_STATUS QUIC_API onListener(HQUIC /*listener*/, void* context,
                                QUIC_LISTENER_EVENT* event) {
  const InCallback callback;
  auto* const standing = static_cast<Session*>(context);
  if (event->Type != QUIC_LISTENER_EVENT_NEW_CONNECTION)
    return QUIC_STATUS_SUCCESS;

  const Library& lib = library();
  const std::shared_ptr<Session> session = standing->shared_from_this();
  uint64_t number = 0;
  {
    const std::lock_guard<std::mutex> lock(session->gate);
    number = ++session->taken;
  }
  // ONE MACHINE MAY HOLD SEVERAL CONNECTIONS at once, each of them a
  // conversation of its own with its own answers, so the address alone
  // would not tell two of them apart. The number is what does, and it
  // counts the connections one feed has taken.
  std::string named = peerAddress(*event->NEW_CONNECTION.Info->RemoteAddress) +
                      "#" + std::to_string(number);
  const auto peer =
      std::make_shared<Peer>(session, std::move(named), /*dialled=*/false);

  HQUIC connection = event->NEW_CONNECTION.Connection;
  // The handler and the peer stand before the handshake is let go, so no
  // event can arrive on a connection whose context is not yet this peer.
  lib.api->SetCallbackHandler(
      connection, reinterpret_cast<void*>(&onConnection), peer.get());
  peer->adopt(connection, peer);
  const QUIC_STATUS status =
      lib.api->ConnectionSetConfiguration(connection, session->configuration);
  if (QUIC_FAILED(status)) {
    // A refused connection is the library's again: nothing more is
    // indicated on it, so this end gives the handle up rather than
    // closing it.
    peer->abandon();
    return status;
  }
  return QUIC_STATUS_SUCCESS;
}

}  // namespace sigil::io::quic
