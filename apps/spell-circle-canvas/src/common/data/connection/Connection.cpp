/** @file
 * The connection: the door it opened, the scheme its messages are read
 * by, the value and the queue and the handlers one dispatch fills, and
 * the two ways a message goes back out.
 */

#include "sigildata/connection/Connection.h"

#include <sigildata/decode/Osc.h>
#include <sigilio/hub/Hub.h>

#include <cstddef>
#include <deque>
#include <span>
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

  /** ONE FRAME'S MESSAGES. The feed is drained in order, and each
   *  message that reads is latched, queued, and handed to every handler
   *  that names it before the next one is taken — so a handler asking
   *  for the latest reads the message it was given. */
  void dispatch() {
    if (!feed) return;
    while (const std::optional<io::Arrival> arrival = feed->receive()) {
      std::optional<Json> message = read(*arrival->bytes);
      if (!message) {
        ++undecodable;
        continue;
      }
      latest = *message;
      unread.push_back(std::move(*message));
      while (capacity != 0 && unread.size() > capacity) unread.pop_front();

      const std::string_view name = nameOf(latest);
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
