/** @file
 * The WebRTC transport: the room a feed's URI names, the WebSocket door
 * the introductions cross, the peer connection made for each peer that
 * takes the room up, and the data channel every message arrives on and
 * goes out through.
 *
 * TWO MACHINES THAT HAVE NO ADDRESS FOR EACH OTHER. A phone on a mobile
 * network and a scene behind a router are both behind something that
 * rewrites their addresses, so neither can be dialled. What they can do
 * is say what addresses they might be reachable at, hear the other's,
 * and try every pair until one answers — and from then on the messages
 * go straight between them, over no server at all. Only the saying is
 * carried by somebody else, and what carries it here is a WebSocket
 * door this tree already opens.
 *
 * THE SIGNAL IS A FEED LIKE ANY OTHER. `?signal=` names a ws:// or
 * wss:// URI, and the shape of that URI says which end of the
 * introduction this door is: a port to hold WAITS to be taken up, and a
 * server to call TAKES UP a room somebody else is waiting in. The
 * introductions are JSON text on that socket — an offer, an answer, and
 * the candidate addresses either end finds — each naming the room it
 * belongs to, so one socket carries as many conversations as there are
 * rooms on it and every door reads only its own.
 *
 * THE FRAME CARRIES THE INTRODUCTION. A feed is read by the call a host
 * already makes once a frame, so that is when the signalling socket is
 * drained and what this door has to say goes back out of it: a
 * handshake takes a few frames rather than a few microseconds, and a
 * host that never dispatches never finishes one. What arrives on a
 * channel is NOT frame-paced — the library underneath delivers it from
 * a thread of its own, the moment it lands.
 *
 * A SIGNAL THAT ENDS ENDS NO CONVERSATION. Two ends that have found
 * each other speak through nothing else, so a door whose signalling
 * socket has closed keeps every peer it has and only takes no new one.
 */

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <rtc/candidate.hpp>
#include <rtc/configuration.hpp>
#include <rtc/datachannel.hpp>
#include <rtc/description.hpp>
#include <rtc/peerconnection.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "Introduction.h"
#include "sigilio/hub/Feed.h"
#include "sigilio/hub/Hub.h"
#include "sigilio/source/Source.h"
#include "sigilio/transport/Transport.h"

namespace sigil::io {
namespace {

constexpr std::string_view kScheme = "webrtc://";

/** The label both ends open their channel under. One channel per peer,
 *  carrying this feed's messages and nothing else. */
constexpr std::string_view kChannelLabel = "feed";

/** WHAT A webrtc:// URI NAMES: the conversation, the door its
 *  introductions cross, and the servers either end asks what address it
 *  has to the world. */
struct Conversation {
  std::string room;
  std::string signal;
  std::vector<std::string> ice;
};

/** A conversation, or the sentence saying why a URI named none: exactly
 *  one of the two stands. */
struct ReadConversation {
  Conversation conversation;
  std::string trouble;
};

/** The conversation @p uri names, or the sentence saying why it names
 *  none. Everything between the scheme and the query is the room's own
 *  name; the query is the arrangement the introductions are made under,
 *  ampersand-separated, and A SETTING'S VALUE RUNS TO THE NEXT
 *  AMPERSAND — so a signal URI carrying a query of its own stands here
 *  whole. */
ReadConversation readConversation(std::string_view uri) {
  const std::string opening =
      " is not a webrtc address: a feed opens on "
      "webrtc://ROOM?signal=ws://:PORT/PATH to be taken up, and on "
      "webrtc://ROOM?signal=ws://HOST:PORT/PATH to take a room up";
  if (!uri.starts_with(kScheme)) return {.trouble = std::string(uri) + opening};
  const std::string_view rest = uri.substr(kScheme.size());
  const size_t question = rest.find('?');
  const std::string_view room = rest.substr(0, question);
  if (room.empty() || room.find('/') != std::string_view::npos)
    return {.trouble = std::string(uri) +
                       " names no room: the conversation's own name stands "
                       "whole between the scheme and the query, as in "
                       "webrtc://sky?signal=ws://:8849/signal"};

  Conversation conversation;
  conversation.room = std::string(room);
  std::string_view query = question == std::string_view::npos
                               ? std::string_view()
                               : rest.substr(question + 1);
  while (!query.empty()) {
    const size_t ampersand = query.find('&');
    const std::string_view setting = query.substr(0, ampersand);
    query = ampersand == std::string_view::npos ? std::string_view()
                                                : query.substr(ampersand + 1);
    if (setting.empty()) continue;
    const size_t equals = setting.find('=');
    if (equals == std::string_view::npos)
      return {.trouble = std::string(uri) + ": " + std::string(setting) +
                         " is not a setting, which is written name=value"};
    const std::string_view name = setting.substr(0, equals);
    const std::string_view value = setting.substr(equals + 1);
    if (name == "signal")
      conversation.signal = std::string(value);
    else if (name == "ice")
      conversation.ice.emplace_back(value);
    else
      return {.trouble = std::string(uri) + ": " + std::string(name) +
                         " is no setting of a webrtc address: they are "
                         "signal and ice"};
  }
  if (conversation.signal.empty())
    return {.trouble = std::string(uri) +
                       " names no signal: two ends that cannot dial each "
                       "other are introduced over a door that is already "
                       "reachable, which is written ?signal=ws://:PORT/PATH"};
  if (!conversation.signal.starts_with("ws://") &&
      !conversation.signal.starts_with("wss://"))
    return {.trouble = conversation.signal +
                       " is no signal for a webrtc address: the "
                       "introductions cross a websocket door, which is what "
                       "a browser can open back and what carries text both "
                       "ways"};
  return {.conversation = std::move(conversation)};
}

/** Whether @p signal names a server to CALL rather than a port to hold:
 *  whether anything stands between the scheme and the path. It is the
 *  same shape that decides which end of a websocket a feed is, so one
 *  URI says which end of the introduction this door is as well. */
bool callsOut(std::string_view signal) {
  const size_t scheme = signal.find("://");
  if (scheme == std::string_view::npos) return false;
  std::string_view authority = signal.substr(scheme + 3);
  if (const size_t slash = authority.find('/'); slash != std::string_view::npos)
    authority = authority.substr(0, slash);
  return !authority.empty() && !authority.starts_with(':');
}

/** The bytes of one channel message, whichever of the two forms a
 *  channel carries it in. A feed answers bytes, so text and binary are
 *  one arrival here and what a message MEANS is read off the URI. */
Bytes bytesOf(const rtc::message_variant& data) {
  Bytes out;
  if (const auto* const raw = std::get_if<rtc::binary>(&data)) {
    out.bytes.assign(raw->begin(), raw->end());
  } else if (const auto* const text = std::get_if<std::string>(&data)) {
    const auto* const first = reinterpret_cast<const std::byte*>(text->data());
    out.bytes.assign(first, first + text->size());
  }
  return out;
}

struct Signal;

/** ONE PEER OF A CONVERSATION: the connection to it, the channel its
 *  messages cross, the address every one of those messages is named by,
 *  and the signalling sender its introductions come from and go back
 *  to. */
struct Peer {
  /** webrtc://ROOM#NUMBER, counting from one over the door's whole
   *  life: a number is never handed out twice, so an address a reader
   *  kept never comes to mean another peer. */
  std::string name;
  /** The sender on the signalling door to answer, spelled the way an
   *  arrival's `from` is; empty where the door has one peer and
   *  nothing to name. */
  std::string through;
  std::shared_ptr<rtc::PeerConnection> connection;
  std::shared_ptr<rtc::DataChannel> channel;
  /** Raised from the library's own thread when the channel ends. The
   *  peer is let go on the next frame instead of there, a connection
   *  torn down inside its own callback being one that waits for
   *  itself. */
  std::atomic<bool> gone{false};
};

/** THE TRANSPORT'S END OF ONE FEED: the room, the peers in it, the
 *  introductions waiting to go out, and the signalling door they cross.
 *
 *  Every callback holds this weakly and locks what it needs, so a feed
 *  let go while the library is mid-callback leaves nothing dangling and
 *  the message stops there. */
struct Door : std::enable_shared_from_this<Door> {
  /** Takes one introduction addressed to this room, from @p from. */
  void take(const detail::Introduction& message, const std::string& from);
  /** One frame: the peers that have gone are let go, and what this door
   *  has to say goes out through the signalling feed. */
  void carry();
  void close();
  bool send(const Bytes& message);
  bool sendTo(std::string_view to, const Bytes& message);

  /** Makes the connection this door offers through, wires it to @p
   *  through, and takes the room up. Only a door that calls out. */
  void callOut();
  /** Answers @p sdp, which @p from offered, with a connection of this
   *  door's own. Only a door that waits. */
  void answer(const std::string& sdp, const std::string& from);
  /** The peer @p from's introductions belong to, or null where none
   *  does. */
  std::shared_ptr<Peer> peerThrough(const std::string& from);
  /** Wires @p connection's own sentences onto the outgoing queue, and
   *  @p peer's channel when one arrives. */
  void wire(const std::shared_ptr<Peer>& peer);
  /** Holds @p channel as @p peer's, and hands every message on it to
   *  the feed. */
  void hold(const std::shared_ptr<Peer>& peer,
            std::shared_ptr<rtc::DataChannel> channel);
  /** Puts @p message on the queue the frame empties. */
  void say(detail::Introduction message, std::string to);

  std::string room;
  /** What the feed reports and every peer is named from: the room,
   *  spelled as a URI of this scheme, without the signal — which is
   *  this door's own arrangement and no part of what the conversation
   *  is called. */
  std::string address;
  rtc::Configuration configuration;
  /** Whether this end takes a room up rather than waiting to be taken
   *  up: it offers, and reads answers; a door that waits answers
   *  offers. */
  bool calling = false;
  std::weak_ptr<Feed> feed;
  /** The signalling door, held: the last door of one is what closes
   *  it. */
  std::shared_ptr<Signal> signal;

  /** Guards the peers below, and is never held across a call that could
   *  deliver into the feed. */
  std::mutex peerGate;
  std::vector<std::shared_ptr<Peer>> peers;
  std::uint64_t numbered = 0;

  /** Guards the queue below. Taken after peerGate where both are
   *  wanted, and never before it. */
  std::mutex sayingGate;
  std::deque<std::pair<detail::Introduction, std::string>> saying;

  /** Raised before anything is taken down, so a callback the library is
   *  already inside stops rather than speaking to a door that is
   *  ending. */
  std::atomic<bool> closed{false};
};

/** ONE SIGNALLING DOOR AND EVERY CONVERSATION CROSSING IT: the feed the
 *  introductions arrive on, the doors waiting for the ones addressed to
 *  their room, and the lease that reads that feed on the frame.
 *
 *  THE DOORS OF ONE SIGNALLING URI SHARE ONE READING OF IT. Draining a
 *  feed is taking, so two doors each draining the same socket would
 *  take each other's introductions; here one reading hands each message
 *  to the room it names. The doors are held weakly and hold this, so
 *  the last of them to go is what lets the socket go. */
struct Signal {
  std::shared_ptr<Feed> feed;
  std::mutex gate;
  std::vector<std::weak_ptr<Door>> doors;
  DispatchLease lease;
};

void Door::say(detail::Introduction message, std::string to) {
  if (closed.load(std::memory_order_acquire)) return;
  const std::lock_guard<std::mutex> lock(sayingGate);
  // IN THE ORDER THEY WERE SAID, and no sooner than the door can take
  // them: an answer that overtook the candidates belonging to it would
  // reach a peer that has nowhere to put them yet.
  saying.emplace_back(std::move(message), std::move(to));
}

std::shared_ptr<Peer> Door::peerThrough(const std::string& from) {
  const std::lock_guard<std::mutex> lock(peerGate);
  for (const std::shared_ptr<Peer>& peer : peers) {
    // A peer whose conversation is over is nobody to hand an address
    // to, and it stands here until a frame lets it go — so a sender
    // that offered again is answered by the peer that offer made and
    // never by the one it replaced.
    if (peer->gone.load(std::memory_order_acquire)) continue;
    // A DOOR THAT TOOK A ROOM UP HOLDS ONE CONVERSATION, and there is
    // nothing to pick out by name: what answers such a door is the
    // server its signalling client dialled, which names itself and not
    // the peer the answer came from.
    if (calling || peer->through == from) return peer;
  }
  return nullptr;
}

void Door::hold(const std::shared_ptr<Peer>& peer,
                std::shared_ptr<rtc::DataChannel> channel) {
  const std::string name = peer->name;
  const std::weak_ptr<Door> door = weak_from_this();
  const std::weak_ptr<Peer> ending = peer;
  channel->onMessage([door, name](rtc::message_variant data) {
    const std::shared_ptr<Door> held = door.lock();
    if (!held || held->closed.load(std::memory_order_acquire)) return;
    const std::shared_ptr<Feed> into = held->feed.lock();
    if (!into) return;
    into->deliver(bytesOf(data), name);
  });
  channel->onClosed([ending] {
    // The peer is only MARKED here: this runs on the library's own
    // thread, and a connection taken down inside its own callback waits
    // for the callback it is inside.
    if (const std::shared_ptr<Peer> held = ending.lock())
      held->gone.store(true, std::memory_order_release);
  });
  const std::lock_guard<std::mutex> lock(peerGate);
  peer->channel = std::move(channel);
}

void Door::wire(const std::shared_ptr<Peer>& peer) {
  const std::weak_ptr<Door> door = weak_from_this();
  const std::weak_ptr<Peer> held = peer;
  const std::string to = peer->through;
  const std::string named = room;
  peer->connection->onLocalDescription(
      [door, to, named](rtc::Description description) {
        if (const std::shared_ptr<Door> standing = door.lock())
          standing->say({.kind = description.typeString(),
                         .room = named,
                         .sdp = std::string(description)},
                        to);
      });
  peer->connection->onLocalCandidate([door, to, named](rtc::Candidate found) {
    if (const std::shared_ptr<Door> standing = door.lock())
      standing->say({.kind = "candidate",
                     .room = named,
                     .candidate = found.candidate(),
                     .mid = found.mid()},
                    to);
  });
  // A door that waits is handed its channel by the peer that took the
  // room up; a door that calls out made its own before it offered.
  peer->connection->onDataChannel(
      [door, held](std::shared_ptr<rtc::DataChannel> channel) {
        const std::shared_ptr<Door> standing = door.lock();
        const std::shared_ptr<Peer> mine = held.lock();
        if (!standing || !mine) return;
        standing->hold(mine, std::move(channel));
      });
}

void Door::callOut() {
  const auto peer = std::make_shared<Peer>();
  {
    const std::lock_guard<std::mutex> lock(peerGate);
    peer->name = address + "#" + std::to_string(++numbered);
    peers.push_back(peer);
  }
  // THE LIBRARY UNDERNEATH ANSWERS MISUSE BY THROWING, and what a
  // configuration names — a server this machine cannot spell an address
  // for — is one caller's arrangement and not a reason to end a frame.
  try {
    peer->connection = std::make_shared<rtc::PeerConnection>(configuration);
    wire(peer);
    // Making the channel is what starts the offer: the description it
    // needs arrives through the callback above and goes out with the
    // frame.
    hold(peer, peer->connection->createDataChannel(std::string(kChannelLabel)));
  } catch (const std::exception& trouble) {
    if (const std::shared_ptr<Feed> into = feed.lock())
      into->fail(std::string("could not take up ") + address + ": " +
                 trouble.what());
  }
}

void Door::answer(const std::string& sdp, const std::string& from) {
  // ONE PEER PER SIGNALLING SENDER. A page that was reloaded offers
  // again from the same socket, and what it is offering is a new
  // conversation: the connection standing under that name is retired
  // rather than left with nobody at the other end of it.
  if (const std::shared_ptr<Peer> standing = peerThrough(from))
    standing->gone.store(true, std::memory_order_release);

  const auto peer = std::make_shared<Peer>();
  peer->through = from;
  {
    const std::lock_guard<std::mutex> lock(peerGate);
    peer->name = address + "#" + std::to_string(++numbered);
    peers.push_back(peer);
  }
  try {
    peer->connection = std::make_shared<rtc::PeerConnection>(configuration);
    wire(peer);
    // A DESCRIPTION A PEER WROTE IS NOT TRUSTED TO BE ONE: the library
    // reads a session by throwing at a text it cannot read, so a peer
    // that sends nonsense ends here rather than in the frame. Taking
    // the offer is what makes the answer, which goes out through the
    // callback above.
    peer->connection->setRemoteDescription(rtc::Description(sdp, "offer"));
  } catch (const std::exception&) {
    peer->gone.store(true, std::memory_order_release);
  }
}

void Door::take(const detail::Introduction& message, const std::string& from) {
  if (closed.load(std::memory_order_acquire)) return;
  if (message.kind == "offer") {
    // A door that took the room up has offered already, and two offers
    // crossing is one conversation neither end can finish.
    if (!calling) answer(message.sdp, from);
    return;
  }
  const std::shared_ptr<Peer> peer = peerThrough(from);
  if (!peer || !peer->connection) return;
  try {
    if (message.kind == "answer") {
      if (calling)
        peer->connection->setRemoteDescription(
            rtc::Description(message.sdp, "answer"));
      return;
    }
    if (message.kind == "candidate" && !message.candidate.empty())
      peer->connection->addRemoteCandidate(
          rtc::Candidate(message.candidate, message.mid));
  } catch (const std::exception&) {
    // One sentence this end could not read leaves the conversation
    // standing: a candidate is one address out of several, and an
    // answer that does not fit is a peer that will find no route
    // rather than a door that has to end.
  }
}

void Door::carry() {
  if (closed.load(std::memory_order_acquire)) return;
  {
    // THE PEERS THAT HAVE GONE ARE LET GO ON THE FRAME, which is a
    // thread of nobody's callback: the connection is taken down here
    // and destroyed with the last reference to it.
    std::vector<std::shared_ptr<Peer>> ending;
    {
      const std::lock_guard<std::mutex> lock(peerGate);
      for (auto entry = peers.begin(); entry != peers.end();) {
        const Peer& peer = **entry;
        // A channel that ended, and a connection that found no route at
        // all: a peer this door will hear nothing more from either way.
        const bool over =
            peer.gone.load(std::memory_order_acquire) ||
            (peer.connection &&
             (peer.connection->state() == rtc::PeerConnection::State::Closed ||
              peer.connection->state() == rtc::PeerConnection::State::Failed));
        if (over) {
          ending.push_back(std::move(*entry));
          entry = peers.erase(entry);
          continue;
        }
        ++entry;
      }
    }
    for (const std::shared_ptr<Peer>& peer : ending) {
      if (peer->channel) {
        peer->channel->resetCallbacks();
        peer->channel->close();
      }
      if (peer->connection) {
        peer->connection->resetCallbacks();
        peer->connection->close();
      }
    }
  }

  const std::shared_ptr<Feed> through = signal ? signal->feed : nullptr;
  if (!through) return;
  const std::lock_guard<std::mutex> lock(sayingGate);
  while (!saying.empty()) {
    const auto& [message, to] = saying.front();
    const std::string text = detail::writeIntroduction(message);
    const auto* const first = reinterpret_cast<const std::byte*>(text.data());
    Bytes payload;
    payload.bytes.assign(first, first + text.size());
    // TO THE ONE PEER THAT IS BEING INTRODUCED where the door can name
    // one, and out of the door where it holds a single peer of its own
    // — a client's server, which there is nothing to pick out of.
    const bool went =
        to.empty() ? through->send(payload)
                   : (through->sendTo(to, payload) || through->send(payload));
    // A DOOR STILL OPENING TAKES NOTHING YET, so what it would not take
    // waits for the next frame rather than being lost: a caller's offer
    // is written before its socket has finished its handshake.
    if (!went) return;
    saying.pop_front();
  }
}

bool Door::send(const Bytes& message) {
  if (closed.load(std::memory_order_acquire)) return false;
  std::vector<std::shared_ptr<rtc::DataChannel>> open;
  {
    const std::lock_guard<std::mutex> lock(peerGate);
    for (const std::shared_ptr<Peer>& peer : peers)
      if (peer->channel && peer->channel->isOpen())
        open.push_back(peer->channel);
  }
  for (const std::shared_ptr<rtc::DataChannel>& channel : open) {
    // A CHANNEL CARRIES WHOLE MESSAGES AND CUTS NONE IN HALF, so one
    // larger than a peer's channel takes is not written to that peer;
    // and a peer that left between the look and the write is misuse by
    // the time the write happens, which the library answers by
    // throwing.
    if (message.bytes.size() > channel->maxMessageSize()) continue;
    try {
      channel->send(message.bytes.data(), message.bytes.size());
    } catch (const std::exception&) {
    }
  }
  return true;
}

bool Door::sendTo(std::string_view to, const Bytes& message) {
  if (closed.load(std::memory_order_acquire)) return false;
  std::shared_ptr<rtc::DataChannel> channel;
  {
    const std::lock_guard<std::mutex> lock(peerGate);
    for (const std::shared_ptr<Peer>& peer : peers)
      if (peer->name == to && peer->channel && peer->channel->isOpen())
        channel = peer->channel;
  }
  if (!channel || message.bytes.size() > channel->maxMessageSize())
    return false;
  try {
    channel->send(message.bytes.data(), message.bytes.size());
  } catch (const std::exception&) {
    return false;
  }
  return true;
}

void Door::close() {
  if (closed.exchange(true)) return;
  std::vector<std::shared_ptr<Peer>> ending;
  {
    const std::lock_guard<std::mutex> lock(peerGate);
    ending.swap(peers);
  }
  // With no lock held: taking a connection down runs whatever the
  // library still has in flight for it, and what is in flight delivers
  // into this door.
  for (const std::shared_ptr<Peer>& peer : ending) {
    if (peer->channel) {
      peer->channel->resetCallbacks();
      peer->channel->close();
    }
    if (peer->connection) {
      peer->connection->resetCallbacks();
      peer->connection->close();
    }
  }
  if (signal) {
    const std::lock_guard<std::mutex> lock(signal->gate);
    for (auto entry = signal->doors.begin(); entry != signal->doors.end();) {
      const std::shared_ptr<Door> held = entry->lock();
      if (!held || held.get() == this) {
        entry = signal->doors.erase(entry);
        continue;
      }
      ++entry;
    }
  }
  // The last door of a signalling socket is what closes it.
  signal.reset();
}

/** Reads one frame of @p signal: every introduction that has arrived
 *  goes to the room it names, and every door then says what it has to
 *  say. */
void readSignal(const std::shared_ptr<Signal>& signal) {
  std::vector<std::shared_ptr<Door>> standing;
  {
    const std::lock_guard<std::mutex> lock(signal->gate);
    standing.reserve(signal->doors.size());
    for (auto entry = signal->doors.begin(); entry != signal->doors.end();) {
      std::shared_ptr<Door> held = entry->lock();
      if (!held) {
        entry = signal->doors.erase(entry);
        continue;
      }
      standing.push_back(std::move(held));
      ++entry;
    }
  }
  while (const std::optional<Arrival> arrival = signal->feed->receive()) {
    if (!arrival->bytes) continue;
    const std::optional<detail::Introduction> message =
        detail::readIntroduction(arrival->bytes->asText());
    // A MESSAGE THIS DOOR CANNOT READ IS SOMEBODY ELSE'S: a signalling
    // socket is an ordinary door, and what crosses it may be more than
    // the introductions.
    if (!message || message->kind.empty()) continue;
    for (const std::shared_ptr<Door>& door : standing)
      if (door->room == message->room) door->take(*message, arrival->from);
  }
  for (const std::shared_ptr<Door>& door : standing) door->carry();
}

/** EVERY SIGNALLING DOOR THIS REGISTRATION HAS OPENED, under the URI it
 *  was opened on. Weak: a socket stands for as long as a conversation
 *  crossing it does and no longer. */
struct Signals {
  std::mutex gate;
  std::vector<std::pair<std::string, std::weak_ptr<Signal>>> open;
};

/** The signalling door at @p uri — the one standing, or one opened now
 *  — with the reading of it registered on @p hub's dispatch. Null when
 *  the feed underneath could not be opened, with the reason in @p
 *  trouble. */
std::shared_ptr<Signal> signalFor(Signals& signals, Hub& hub,
                                  const std::string& uri,
                                  std::string& trouble) {
  {
    const std::lock_guard<std::mutex> lock(signals.gate);
    for (auto entry = signals.open.begin(); entry != signals.open.end();) {
      std::shared_ptr<Signal> held = entry->second.lock();
      if (!held) {
        entry = signals.open.erase(entry);
        continue;
      }
      if (entry->first == uri) return held;
      ++entry;
    }
  }
  const auto made = std::make_shared<Signal>();
  made->feed = hub.feed(uri);
  if (!made->feed->error().empty()) {
    trouble = made->feed->error();
    return nullptr;
  }
  made->lease = hub.onDispatch([held = std::weak_ptr<Signal>(made)](double) {
    if (const std::shared_ptr<Signal> standing = held.lock())
      readSignal(standing);
  });
  const std::lock_guard<std::mutex> lock(signals.gate);
  // Read again under the lock that adds it: two feeds asked for at once
  // are one socket, and the one that got there first is the one both of
  // them cross.
  for (const auto& [named, held] : signals.open)
    if (named == uri)
      if (const std::shared_ptr<Signal> standing = held.lock()) return standing;
  signals.open.emplace_back(uri, made);
  return made;
}

/** A feed whose transport could not open: the reason stands on the feed,
 *  and there is no door to close or to send through. */
OpenedFeed refuse(const std::weak_ptr<Feed>& into, std::string why) {
  if (const std::shared_ptr<Feed> feed = into.lock())
    feed->fail(std::move(why));
  return {};
}

/** Opens one feed's conversation: the room it names, the signalling
 *  door its introductions cross, and — where this end takes a room up
 *  rather than waiting to be taken up — the offer that starts one. */
OpenedFeed openFeed(Hub& hub, Signals& signals, std::string_view uri,
                    const std::weak_ptr<Feed>& into) {
  ReadConversation read = readConversation(uri);
  if (!read.trouble.empty()) return refuse(into, std::move(read.trouble));

  const auto door = std::make_shared<Door>();
  door->room = read.conversation.room;
  door->address = std::string(kScheme) + door->room;
  door->calling = callsOut(read.conversation.signal);
  door->feed = into;
  for (const std::string& server : read.conversation.ice) {
    // A server named here is asked what address this machine has to the
    // world, which is what two ends behind routers need and what two on
    // one network do not.
    try {
      door->configuration.iceServers.emplace_back(server);
    } catch (const std::exception& trouble) {
      return refuse(into, server + " is no ice server for " + std::string(uri) +
                              ": " + trouble.what());
    }
  }

  std::string trouble;
  door->signal = signalFor(signals, hub, read.conversation.signal, trouble);
  if (!door->signal)
    return refuse(into, "could not open the signal for " + std::string(uri) +
                            ": " + trouble);
  {
    const std::lock_guard<std::mutex> lock(door->signal->gate);
    door->signal->doors.emplace_back(door);
  }
  if (door->calling) door->callOut();

  OpenedFeed opened;
  opened.address = door->address;
  opened.close = [door] { door->close(); };
  opened.send = [door](const Bytes& message) { return door->send(message); };
  opened.sendTo = [door](std::string_view to, const Bytes& message) {
    return door->sendTo(to, message);
  };
  return opened;
}

}  // namespace

void registerWebRtc(Hub& hub) {
  // The signalling doors this registration opens are its own: the hub
  // is where they are asked for, as the pages a listener serves are
  // resolved through the hub that opened it, and one of them is shared
  // by every conversation on it. The reference is read at open and
  // nowhere else — a transport is registered ON a hub and held by it,
  // while the feed that call answers may outlive it.
  auto signals = std::make_shared<Signals>();
  hub.setFeedTransport("webrtc", [&hub, signals](std::string_view uri,
                                                 std::weak_ptr<Feed> into) {
    return openFeed(hub, *signals, uri, into);
  });
}

}  // namespace sigil::io
