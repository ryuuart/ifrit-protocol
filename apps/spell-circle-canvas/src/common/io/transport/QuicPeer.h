/** @file
 * ONE CONNECTION OF A FEED, and the events the library indicates on a
 * connection, on a stream and on a listener: every message that crosses
 * a quic:// door goes out through one of these and arrives through one
 * of these.
 */

#pragma once

#include <msquic.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "QuicLibrary.h"
#include "QuicSession.h"
#include "sigilio/source/Source.h"

namespace sigil::io::quic {

/** ONE CONNECTION: the handle, the name its messages arrive under, and
 *  how much of a datagram the path will take.
 *
 *  IT HOLDS ITSELF UNTIL THE CONNECTION IS DONE. The library may call
 *  back onto it until the shutdown is complete, so the reference the
 *  session's table keeps is not what its life stands on: the connection
 *  keeps one of its own and lets that one go where the handle is given
 *  back, which is the one place it may be freed. */
class Peer {
 public:
  Peer(std::shared_ptr<Session> session, std::string named, bool dialled)
      : m_session(std::move(session)),
        m_named(std::move(named)),
        m_dialled(dialled) {}

  /** Takes @p connection over: the handle first, then the table, so a
   *  message cannot arrive naming a sender nobody can answer, and then
   *  the reference this connection's life stands on. A call holds the
   *  one connection it made and stands in no table. */
  void adopt(HQUIC connection, const std::shared_ptr<Peer>& self);

  /** Gives the handle up WITHOUT closing it, for a connection the
   *  library takes back because this end refused it: nothing more is
   *  indicated on it, so there is nothing left to hold. */
  void abandon();

  /** Closes a connection that was opened and never started, which
   *  indicates nothing at all and so would otherwise never come back. */
  void discard();

  /** ONE MESSAGE OUT: a stream of its own, or a datagram where the door
   *  was opened for them. False where the connection is ending, where
   *  the library refused the send, and — for a datagram — where the
   *  message is larger than one packet on this path carries, or where
   *  the path has not yet said how large that is. */
  bool write(const Bytes& message);

  /** Ends the connection. The other end is told it is over rather than
   *  left to find a connection that stopped answering, and what the
   *  library indicates afterwards is what gives the handle back. It
   *  waits for nothing. */
  void shutdown();

  /** The feed gave this connection up on purpose, so however it ends
   *  there is nobody it has to be explained to. */
  void givenUp() { m_given.store(true, std::memory_order_release); }

  /** Takes over a stream the other end opened: every message of theirs
   *  arrives on one of these. */
  void takeStream(HQUIC stream);

  /** ONE DATAGRAM IN, which is one arrival like any other. */
  void takeDatagram(const QUIC_BUFFER& buffer);

  /** How much of a datagram this path carries now; zero while it carries
   *  none. */
  void datagramCeiling(uint16_t most) {
    m_datagramCeiling.store(most, std::memory_order_release);
  }

  /** The connection was reached: the handshake finished, or something
   *  crossed it. Raised once and never lowered — an end that answered
   *  and then went away is not an end that was never there. */
  void reached() { m_reached.store(true, std::memory_order_release); }

  /** What the library said as the connection began to end, kept for the
   *  sentence the feed is left with. */
  void endingWith(QUIC_STATUS status) { m_why = status; }

  /** The other end said it was over, on purpose, rather than the
   *  connection failing under it. */
  void endedByPeer() { m_byPeer = true; }

  /** THE CONNECTION IS DONE: the table loses this peer, a feed whose
   *  life follows one connection is told how it ended, and the handle
   *  comes back. */
  void concluded();

 private:
  /** A message as a stream of its own: opened, written whole, and ended
   *  by the write, so the other end reads the whole stream as the
   *  message it is. */
  bool sendStream(const Library& lib, const Bytes& message);

  /** A message as one datagram: unreliable, unordered, and no larger
   *  than one packet on this path. */
  bool sendDatagram(const Library& lib, const Bytes& message);

  /** Takes this connection out of its session's table. The reference
   *  that comes out is let go after the lock, since dropping it under
   *  one would be a peer freed while another peer waits for the table.
   */
  void forget();

  /** WHAT A CALL'S FEED IS LEFT WITH. A connection the other end ended
   *  on purpose closes the feed, there being nothing more to come and
   *  nothing to explain. Any other ending is a sentence, and WHICH
   *  ending it was is the difference between an end that was never there
   *  and one that went away mid-conversation, which a reader of the
   *  sentence has to be able to tell apart. */
  void tell();

  const std::shared_ptr<Session> m_session;
  /** The address every message of this connection's arrives under,
   *  written once so the sender a message names, the key an answer is
   *  found under, and the key that is cleared cannot drift apart. */
  const std::string m_named;
  /** Whether this end opened the connection, which is also whether the
   *  feed's life follows it: a call holds the one connection it made,
   *  and a port holds as many as reached it. */
  const bool m_dialled;
  /** This connection's own reference to itself, let go where the handle
   *  is given back. */
  std::shared_ptr<Peer> m_self;

  /** Guards the handle and the flag beside it: a send may not be inside
   *  the library while the handle it is using is being given back. */
  std::mutex m_gate;
  HQUIC m_connection = nullptr;
  bool m_ending = false;

  std::atomic<bool> m_stopped{false};
  std::atomic<bool> m_reached{false};
  std::atomic<bool> m_given{false};
  std::atomic<uint16_t> m_datagramCeiling{0};
  /** How the connection began to end. Written as the library says so and
   *  read where the connection is concluded, which the library indicates
   *  after it, on the same worker. */
  QUIC_STATUS m_why = QUIC_STATUS_SUCCESS;
  bool m_byPeer = false;
};

/** Everything the library indicates on one connection, which is what
 *  moves a peer through its life: reached, a stream or a datagram
 *  arriving, how much of a datagram the path takes, how the connection
 *  began to end, and that it is done. */
QUIC_STATUS QUIC_API onConnection(HQUIC connection, void* context,
                                  QUIC_CONNECTION_EVENT* event);

/** A connection reaching a port this feed holds: it is numbered, named
 *  for the end it came from, given a peer of its own and handed the
 *  session's arrangement, or given back where this end refuses it. */
QUIC_STATUS QUIC_API onListener(HQUIC listener, void* context,
                                QUIC_LISTENER_EVENT* event);

}  // namespace sigil::io::quic
