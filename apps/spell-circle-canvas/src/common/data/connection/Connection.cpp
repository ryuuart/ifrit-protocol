/** @file
 * The connection: the door it opened, the scheme its messages are read
 * by, the value and the latch per name and the queue and the handlers
 * one dispatch fills, and the two ways a message goes back out.
 */

#include "sigildata/connection/Connection.h"

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
  /** What the undelivered queue holds before its oldest falls off. */
  size_t capacity = 0;
  std::shared_ptr<io::Feed> feed;
  Json latest;
  std::deque<Json> unread;

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
  uint64_t undecodable = 0;
  /** What the hub's dispatch runs. It is released with this state, so a
   *  connection that is gone leaves nothing to run. */
  io::DispatchLease lease;

  /** One arrival as a value, or nothing when the bytes are no message
   *  in this connection's scheme. */
  std::optional<Json> read(const io::Bytes& bytes) const {
    if (osc) return decodeOsc(std::span<const std::byte>(bytes.bytes));
    return decodeJson(bytes.asText());
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

  /** ONE FRAME'S MESSAGES. The feed is drained in order, and each
   *  message that reads is latched — under nothing and under its own
   *  name — queued, and handed to every handler that names it before
   *  the next one is taken, so a handler asking for the latest reads
   *  the message it was given. */
  void dispatch() {
    if (!feed) return;
    while (const std::optional<io::Arrival> arrival = feed->receive()) {
      std::optional<Json> message = read(*arrival->bytes);
      if (!message) {
        ++undecodable;
        continue;
      }
      latest = *message;
      const std::string_view name = nameOf(latest);
      // A message that says what it is is latched under that name as
      // well as under none, so a reader asks for the newest of one name
      // without registering a handler for it. A message that says
      // nothing latches under nothing: no name is not a name.
      if (!name.empty()) latch(name, latest);
      unread.push_back(std::move(*message));
      while (capacity != 0 && unread.size() > capacity) unread.pop_front();

      // The count is taken first and the handler is held rather than
      // referred to: a handler may register another, which moves the
      // list it is standing in, and one registered from inside a
      // handler runs from the next message.
      const size_t registered = handlers.size();
      for (size_t index = 0; index != registered; ++index) {
        const std::string_view what = handlers[index].first;
        if (what != "*" && what != name) continue;
        const Handler handler = handlers[index].second;
        handler(latest);
      }
    }
  }
};

Connection::Connection(io::Hub& hub, std::string_view uri,
                       io::FeedPolicy policy) {
  auto state = std::make_shared<State>();
  state->uri = std::string(uri);
  state->osc = schemeOf(state->uri) == "osc";
  state->capacity = policy.capacity;
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
  if (!m_state || m_state->unread.empty()) return std::nullopt;
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

bool Connection::send(const Json& message) const {
  if (!m_state || !m_state->feed) return false;
  if (!m_state->osc) return m_state->feed->send(textBytes(encodeJson(message)));
  std::vector<std::byte> packet = encodeOsc(message);
  // No bytes is no message: a value the wire has no spelling for does
  // not go out as an empty datagram.
  if (packet.empty()) return false;
  io::Bytes bytes;
  bytes.bytes = std::move(packet);
  return m_state->feed->send(bytes);
}

bool Connection::send(std::string_view address, const Json& arguments) const {
  if (!m_state || !m_state->feed) return false;
  if (!m_state->osc) {
    // Off the OSC wire the same message is the record a packet reads
    // as, which is the form a name is read out of at the other end.
    return send(Json(Json::Object{{"address", Json(std::string(address))},
                                  {"arguments", arguments}}));
  }
  std::vector<std::byte> packet = encodeOsc(address, arguments);
  if (packet.empty()) return false;
  io::Bytes bytes;
  bytes.bytes = std::move(packet);
  return m_state->feed->send(bytes);
}

const std::string& Connection::uri() const {
  return m_state ? m_state->uri : noUri();
}

std::string Connection::address() const {
  return m_state && m_state->feed ? m_state->feed->address() : std::string();
}

std::string Connection::error() const {
  return m_state && m_state->feed ? m_state->feed->error() : std::string();
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
