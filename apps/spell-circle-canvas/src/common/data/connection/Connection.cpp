/** @file
 * The connection: the door it opened, the scheme or the schema its
 * messages are read by, the value and the latch per name and the queue
 * and the handlers one dispatch fills, and the ways a message goes back
 * out — to the door, or to the sender of the message being answered.
 */

#include "sigildata/connection/Connection.h"

#include <sigildata/decode/ArtNet.h>
#include <sigildata/decode/FlatBuffer.h>
#include <sigildata/decode/Midi.h>
#include <sigildata/decode/Osc.h>
#include <sigilio/hub/Hub.h>

#include <cstddef>
#include <deque>
#include <map>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace sigil::data {

namespace {

/** The value a reader gets where there is no message: one, shared, so
 *  answering it costs nothing and a chain of lookups through it stays
 *  standing. */
const Json& nothing() {
  static const Json empty;
  return empty;
}

const std::string& noUri() {
  static const std::string empty;
  return empty;
}

/** The part of @p uri before "://", which is what says how a message is
 *  read. Empty when the URI names no scheme. */
std::string_view schemeOf(std::string_view uri) {
  const size_t mark = uri.find("://");
  return mark == 0 || mark == std::string_view::npos ? std::string_view{}
                                                     : uri.substr(0, mark);
}

/** THE NAME OF A MESSAGE: the address it carries, and otherwise the
 *  first of the three keys a sender says its kind with. Empty when it
 *  says neither, which only a "*" handler matches. A key whose value is
 *  not text names nothing and the lookup goes on: a name is what a
 *  handler was registered under, and that is a string. */
std::string_view nameOf(const Json& message) {
  static constexpr std::string_view keys[] = {"address", "type", "message_type",
                                              "kind"};
  for (std::string_view key : keys) {
    const Json& named = message[key];
    if (named.kind() == Json::Kind::Text) return named.text();
  }
  return {};
}

io::Bytes textBytes(std::string_view text) {
  const auto* first = reinterpret_cast<const std::byte*>(text.data());
  io::Bytes bytes;
  bytes.bytes.assign(first, first + text.size());
  return bytes;
}

}  // namespace

/** What a connection IS. The dispatch reaches it weakly and every
 *  reading below reaches it through the connection's pointer, so the
 *  two agree however the connection is moved about. */
struct Connection::State {
  std::string uri;
  /** Whether a message is an OSC packet rather than JSON text, read off
   *  the scheme once, when the door is opened. */
  bool osc = false;
  /** Whether a message is a MIDI message, read off the scheme the same
   *  way and at the same moment. */
  bool midi = false;
  /** Whether a message is an Art-Net packet, read off the scheme the
   *  same way and at the same moment. */
  bool artnet = false;
  /** THE ONE DYNAMIC VALUE ON THIS WIRE, where the door was opened with
   *  one: every message in and out goes through it, and the form a
   *  reader sees is the schema's own. None for a door read as the
   *  scheme alone says. */
  Schema schema;
  /** Why this door was never opened, where it was refused. It stands in
   *  front of whatever the feed would say, there being no feed. */
  std::string trouble;
  /** What the undelivered queue holds before its oldest falls off. */
  size_t capacity = 0;
  std::shared_ptr<io::Feed> feed;
  Json latest;
  /** WHO SENT THE MESSAGE A REPLY ANSWERS: the address the newest
   *  message that could be read arrived from, which inside a handler is
   *  the address of the message that handler was given. Empty where
   *  there is nobody to answer — nothing has arrived, or the arrival
   *  named no sender, as a recording's frames do not. */
  std::string sender;
  std::deque<Json> unread;
  /** Whether the queue behind receive() is being filled. The first
   *  receive() raises it and nothing lowers it: a reader that only
   *  registers handlers holds no backlog it never looks at, and one
   *  that asks for a message is asking for the ones after it too. */
  bool queuing = false;

  /** ONE MESSAGE UNDER EACH NAME, and the write that put it there: the
   *  name written longest ago is the one carrying the smallest count,
   *  which is the one that goes when there is no room for another. The
   *  comparator is transparent so a reader's name is looked up as the
   *  view it already is, without a string being built for it. */
  struct Named {
    Json message;
    uint64_t written = 0;
  };
  std::map<std::string, Named, std::less<>> named;
  /** How many latches have been written, which is where a name's own
   *  count comes from. */
  uint64_t writes = 0;
  std::vector<std::pair<std::string, Handler>> handlers;
  /** What runs for a message no name above matched, in the order these
   *  were registered. */
  std::vector<Handler> otherwise;
  uint64_t undecodable = 0;
  /** What the hub's dispatch runs. It is released with this state, so a
   *  connection that is gone leaves nothing to run. */
  io::DispatchLease lease;

  /** One arrival as a value, or nothing when the bytes are no message
   *  in this connection's scheme, or no message its schema holds. */
  std::optional<Json> read(const io::Bytes& bytes) const {
    if (osc) return decodeOsc(std::span<const std::byte>(bytes.bytes));
    if (midi) return decodeMidi(std::span<const std::byte>(bytes.bytes));
    if (artnet) return decodeArtNet(std::span<const std::byte>(bytes.bytes));
    if (!schema) return decodeJson(bytes.asText());
    return readThroughSchema(bytes);
  }

  /** ONE ARRIVAL THROUGH THE SCHEMA, in whichever form it came: the
   *  schema's JSON form is parsed to a buffer first, a buffer is taken
   *  as it stands, and either is rendered back out of the schema. So
   *  what a reader sees is the schema's own form both ways, and an
   *  arrival that does not fit the schema is no message rather than a
   *  value carrying whichever fields it happened to have.
   *
   *  Which form an arrival is in is read the way a resource's is: from
   *  the door's NAME where it ends `.json`, and otherwise from the
   *  first byte that is not a space, the JSON form opening with a brace
   *  or a bracket. */
  std::optional<Json> readThroughSchema(const io::Bytes& bytes) const {
    std::optional<std::string> form;
    if (flatBufferLooksLikeJson(bytes.asText(), uri)) {
      const std::optional<std::vector<std::byte>> buffer =
          schema.binary(bytes.asText());
      if (!buffer) return std::nullopt;
      form = schema.text(*buffer);
    } else {
      form = schema.text(bytes.bytes);
    }
    if (!form) return std::nullopt;
    return decodeJson(*form);
  }

  /** Puts @p message under @p name, as the newest message of that name.
   *
   *  The names are bounded as the queue is, by the one capacity: a
   *  sender that writes an address it never writes again would
   *  otherwise grow this for as long as the door is open. When there is
   *  no room for one more, the name written longest ago goes, so the
   *  names a scene keeps hearing are the names it keeps. */
  void latch(std::string_view name, const Json& message) {
    const auto found = named.find(name);
    if (found != named.end()) {
      found->second = Named{message, ++writes};
      return;
    }
    if (capacity != 0 && named.size() >= capacity) {
      auto oldest = named.begin();
      for (auto latched = named.begin(); latched != named.end(); ++latched)
        if (latched->second.written < oldest->second.written) oldest = latched;
      named.erase(oldest);
    }
    named.emplace(std::string(name), Named{message, ++writes});
  }

  /** ONE MESSAGE ON THIS DOOR'S WIRE: the message a MIDI door carries,
   *  the universe an Art-Net door does, the packet an OSC door is read
   *  by, the buffer a door with a schema is, the JSON text every other
   *  door is. Nothing where the value has no spelling there — no bytes
   *  is no message, and a value the wire cannot hold does not go out as
   *  an empty datagram. */
  std::optional<io::Bytes> write(const Json& message) const {
    if (midi) {
      std::vector<std::byte> played = encodeMidi(message);
      if (played.empty()) return std::nullopt;
      io::Bytes bytes;
      bytes.bytes = std::move(played);
      return bytes;
    }
    if (artnet) {
      std::vector<std::byte> universe = encodeArtNet(message);
      if (universe.empty()) return std::nullopt;
      io::Bytes bytes;
      bytes.bytes = std::move(universe);
      return bytes;
    }
    if (!osc) {
      if (!schema) return textBytes(encodeJson(message));
      // Through the schema where the door has one: what goes out is the
      // buffer the message makes, and a message the schema cannot hold
      // is no message rather than text nobody at the far end reads.
      std::optional<std::vector<std::byte>> buffer =
          schema.binary(encodeJson(message));
      if (!buffer) return std::nullopt;
      io::Bytes bytes;
      bytes.bytes = std::move(*buffer);
      return bytes;
    }
    std::vector<std::byte> packet = encodeOsc(message);
    if (packet.empty()) return std::nullopt;
    io::Bytes bytes;
    bytes.bytes = std::move(packet);
    return bytes;
  }

  /** THE SAME, spelled as @p arguments under @p address. */
  std::optional<io::Bytes> write(std::string_view address,
                                 const Json& arguments) const {
    // Off the OSC wire the same message is the record a packet reads
    // as, which is the form a name is read out of at the other end — and
    // a door with a schema writes that record through it, so it goes out
    // only where the schema declares those two fields.
    if (!osc)
      return write(Json(Json::Object{{"address", Json(std::string(address))},
                                     {"arguments", arguments}}));
    std::vector<std::byte> packet = encodeOsc(address, arguments);
    if (packet.empty()) return std::nullopt;
    io::Bytes bytes;
    bytes.bytes = std::move(packet);
    return bytes;
  }

  /** ONE FRAME'S MESSAGES. The feed is drained in order, and each
   *  message that reads is latched — under nothing and under its own
   *  name — queued where a queue was asked for, and handed to every
   *  handler that names it before the next one is taken, so a handler
   *  asking for the latest reads the message it was given and a handler
   *  replying answers the sender of it. */
  void dispatch() {
    if (!feed) return;
    while (const std::optional<io::Arrival> arrival = feed->receive()) {
      std::optional<Json> message = read(*arrival->bytes);
      if (!message) {
        ++undecodable;
        continue;
      }
      latest = *message;
      // Who sent it moves with what it says: a message that cannot be
      // read is no message, so it leaves the sender standing exactly as
      // it leaves the latest standing.
      sender = arrival->from;
      const std::string_view name = nameOf(latest);
      // A message that says what it is is latched under that name as
      // well as under none, so a reader asks for the newest of one name
      // without registering a handler for it. A message that says
      // nothing latches under nothing: no name is not a name.
      if (!name.empty()) latch(name, latest);
      if (queuing) {
        unread.push_back(std::move(*message));
        while (capacity != 0 && unread.size() > capacity) unread.pop_front();
      }

      // The count is taken first and the handler is held rather than
      // referred to: a handler may register another, which moves the
      // list it is standing in, and one registered from inside a
      // handler runs from the next message.
      const size_t registered = handlers.size();
      bool matched = false;
      for (size_t index = 0; index != registered; ++index) {
        const std::string_view what = handlers[index].first;
        if (what != "*" && what != name) continue;
        // "*" is every message rather than a name a message carries, so
        // one standing leaves a message no NAME reached still unnamed.
        if (what != "*") matched = true;
        const Handler handler = handlers[index].second;
        handler(latest);
      }
      if (matched) continue;
      const size_t unmatched = otherwise.size();
      for (size_t index = 0; index != unmatched; ++index) {
        const Handler handler = otherwise[index];
        handler(latest);
      }
    }
  }
};

Connection::Connection(io::Hub& hub, std::string_view uri,
                       io::FeedPolicy policy)
    : Connection(hub, uri, Schema{}, policy) {}

Connection::Connection(io::Hub& hub, std::string_view uri, Schema schema,
                       io::FeedPolicy policy) {
  auto state = std::make_shared<State>();
  state->uri = std::string(uri);
  state->osc = schemeOf(state->uri) == "osc";
  state->midi = schemeOf(state->uri) == "midi";
  state->artnet = schemeOf(state->uri) == "artnet";
  state->capacity = policy.capacity;
  state->schema = std::move(schema);
  if ((state->osc || state->midi || state->artnet) && state->schema) {
    // OSC, MIDI and Art-Net each spell every value themselves, down to
    // the width a number goes out at, and a buffer is not one of those
    // spellings. The door is not opened at all, so nothing is bound and
    // nothing arrives.
    state->trouble = std::string("a schema reads a FlatBuffer wire, not ") +
                     (state->osc    ? "OSC"
                      : state->midi ? "MIDI"
                                    : "Art-Net");
    m_state = std::move(state);
    return;
  }
  state->feed = hub.feed(state->uri, policy);
  // The dispatch knows the state weakly: the state owns the lease, and
  // a lease owning the state back would keep both standing after the
  // last connection onto them was gone.
  state->lease = hub.onDispatch([held = std::weak_ptr<State>(state)](double) {
    if (const std::shared_ptr<State> living = held.lock()) living->dispatch();
  });
  m_state = std::move(state);
}

const Json& Connection::latest() const {
  return m_state ? m_state->latest : nothing();
}

const Json& Connection::latest(std::string_view what) const {
  if (!m_state) return nothing();
  const auto found = m_state->named.find(what);
  return found == m_state->named.end() ? nothing() : found->second.message;
}

uint64_t Connection::generation() const {
  return m_state && m_state->feed ? m_state->feed->generation() : 0;
}

std::optional<Json> Connection::receive() {
  if (!m_state) return std::nullopt;
  // Asking for a message is what says this reader wants them held: what
  // arrived before the first ask was never queued, and everything after
  // it is.
  m_state->queuing = true;
  if (m_state->unread.empty()) return std::nullopt;
  Json message = std::move(m_state->unread.front());
  m_state->unread.pop_front();
  return message;
}

void Connection::on(std::string_view what, Handler handler) {
  // A connection onto nothing takes no handler: no message can arrive
  // for one to run on.
  if (!m_state || !handler) return;
  m_state->handlers.emplace_back(std::string(what), std::move(handler));
}

void Connection::otherwise(Handler handler) {
  if (!m_state || !handler) return;
  m_state->otherwise.push_back(std::move(handler));
}

bool Connection::send(const Json& message) const {
  if (!m_state || !m_state->feed) return false;
  const std::optional<io::Bytes> bytes = m_state->write(message);
  return bytes && m_state->feed->send(*bytes);
}

bool Connection::send(std::string_view address, const Json& arguments) const {
  if (!m_state || !m_state->feed) return false;
  const std::optional<io::Bytes> bytes = m_state->write(address, arguments);
  return bytes && m_state->feed->send(*bytes);
}

bool Connection::reply(const Json& message) const {
  // The same bytes as a send, out of a door that names whom they go to
  // instead of writing to whoever is on the other side. Nobody to
  // answer is not an error to report: it is what a recording, and a
  // door nothing has arrived at, has.
  if (!m_state || !m_state->feed || m_state->sender.empty()) return false;
  const std::optional<io::Bytes> bytes = m_state->write(message);
  return bytes && m_state->feed->sendTo(m_state->sender, *bytes);
}

bool Connection::reply(std::string_view address, const Json& arguments) const {
  if (!m_state || !m_state->feed || m_state->sender.empty()) return false;
  const std::optional<io::Bytes> bytes = m_state->write(address, arguments);
  return bytes && m_state->feed->sendTo(m_state->sender, *bytes);
}

const std::string& Connection::uri() const {
  return m_state ? m_state->uri : noUri();
}

std::string Connection::address() const {
  return m_state && m_state->feed ? m_state->feed->address() : std::string();
}

std::string Connection::error() const {
  if (!m_state) return {};
  if (!m_state->trouble.empty()) return m_state->trouble;
  return m_state->feed ? m_state->feed->error() : std::string();
}

uint64_t Connection::dropped() const {
  return m_state && m_state->feed ? m_state->feed->dropped() : 0;
}

uint64_t Connection::undecodable() const {
  return m_state ? m_state->undecodable : 0;
}

bool Connection::closed() const {
  return m_state && m_state->feed ? m_state->feed->closed() : true;
}

std::shared_ptr<io::Feed> Connection::feed() const {
  return m_state ? m_state->feed : nullptr;
}

}  // namespace sigil::data
