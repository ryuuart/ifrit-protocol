/** @file
 * The connection: the door it opened, the dialect or the schema its
 * messages are read by, the message and the latch per name and the queue
 * and the handlers one advance fills, and the ways a message goes back
 * out — to the door, to one sender, or to the sender of the message
 * being answered.
 */

#include "sigildata/connection/Connection.h"

#include <sigilio/advanced/Time.h>
#include <sigilio/hub/Hub.h>

#include <deque>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace sigil::data {

namespace {

/** The message a reader gets where there is none: one, shared. */
const Message& nothing() {
  static const Message empty;
  return empty;
}

const std::string& noUri() {
  static const std::string empty;
  return empty;
}

const Schema& noSchema() {
  static const Schema none;
  return none;
}

/** The part of @p uri before "://"; empty when it names no scheme. */
std::string_view schemeOf(std::string_view uri) {
  const size_t mark = uri.find("://");
  return mark == 0 || mark == std::string_view::npos ? std::string_view{}
                                                     : uri.substr(0, mark);
}

/** The dialect a scheme names: each wire of the performance room its
 *  own, JSON text for every other. */
Dialect dialectOf(std::string_view uri) {
  const std::string_view scheme = schemeOf(uri);
  if (scheme == "osc") return Dialect::Osc;
  if (scheme == "midi") return Dialect::Midi;
  if (scheme == "artnet") return Dialect::ArtNet;
  return Dialect::Json;
}

/** THE NAME OF A MESSAGE: the address it carries, and otherwise the
 *  first of the three keys a sender says its kind with. Empty when it
 *  says neither, which only "*" names. */
std::string_view nameOf(const Json& message) {
  static constexpr std::string_view keys[] = {"address", "type", "message_type",
                                              "kind"};
  for (std::string_view key : keys) {
    const Json& named = message[key];
    if (named.kind() == Json::Kind::String) return named.string();
  }
  return {};
}

bool isPattern(std::string_view text) {
  return text.find_first_of("*?[{") != std::string_view::npos;
}

/** One character against a bracketed set, @p set without its brackets:
 *  `!` first negates, `a-c` is a range. */
bool inSet(std::string_view set, char character) {
  bool negated = false;
  if (!set.empty() && set.front() == '!') {
    negated = true;
    set.remove_prefix(1);
  }
  bool found = false;
  for (size_t index = 0; index < set.size(); ++index) {
    if (index + 2 < set.size() && set[index + 1] == '-') {
      if (set[index] <= character && character <= set[index + 2]) found = true;
      index += 2;
    } else if (set[index] == character) {
      found = true;
    }
  }
  return found != negated;
}

}  // namespace

bool matchesAddress(std::string_view pattern, std::string_view name) {
  if (pattern.empty()) return name.empty();
  const char head = pattern.front();
  if (head == '*') {
    // Any run of characters within one part of the address.
    for (size_t taken = 0;; ++taken) {
      if (matchesAddress(pattern.substr(1), name.substr(taken))) return true;
      if (taken == name.size() || name[taken] == '/') return false;
    }
  }
  if (name.empty()) return false;
  if (head == '?')
    return name.front() != '/' && matchesAddress(pattern.substr(1), name.substr(1));
  if (head == '[') {
    const size_t close = pattern.find(']');
    if (close == std::string_view::npos) return false;
    return name.front() != '/' && inSet(pattern.substr(1, close - 1), name.front()) &&
           matchesAddress(pattern.substr(close + 1), name.substr(1));
  }
  if (head == '{') {
    const size_t close = pattern.find('}');
    if (close == std::string_view::npos) return false;
    const std::string_view choices = pattern.substr(1, close - 1);
    const std::string_view rest = pattern.substr(close + 1);
    size_t start = 0;
    while (start <= choices.size()) {
      size_t comma = choices.find(',', start);
      if (comma == std::string_view::npos) comma = choices.size();
      const std::string_view choice = choices.substr(start, comma - start);
      if (name.substr(0, choice.size()) == choice &&
          matchesAddress(rest, name.substr(choice.size())))
        return true;
      start = comma + 1;
    }
    return false;
  }
  return head == name.front() && matchesAddress(pattern.substr(1), name.substr(1));
}

/** What a connection IS. The advance reaches it weakly and every handle
 *  reaches it through its pointer, so they agree however the handles are
 *  copied and moved. */
struct Connection::State {
  std::string uri;
  Dialect dialect = Dialect::Json;
  Schema schema;
  /** Why this door was never opened, where it was refused. */
  std::string trouble;
  size_t capacity = 0;
  io::Feed feed;
  Message latest;
  /** The message a handler is running for, which a reply answers the
   *  sender of; null between handlers. */
  const Message* handling = nullptr;
  std::deque<Message> unread;
  /** Whether the queue behind receive() is being filled. */
  bool queuing = false;

  /** ONE MESSAGE UNDER EACH NAME, and the write that put it there, so
   *  the name written longest ago goes when there is no room. */
  struct Named {
    Message message;
    uint64_t written = 0;
  };
  std::map<std::string, Named, std::less<>> named;
  uint64_t writes = 0;
  std::vector<std::pair<std::string, Handler>> handlers;
  std::vector<Handler> otherwise;
  uint64_t undecodable = 0;
  io::Lease lease;

  void latch(std::string_view name, const Message& message) {
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

  /** ONE MESSAGE ON THIS DOOR'S WIRE; nothing where the value has no
   *  spelling there, a value the wire cannot hold not going out as an
   *  empty datagram. */
  std::optional<io::Bytes> write(const Json& message) const {
    std::vector<std::byte> bytes = encode(message, dialect, schema);
    if (bytes.empty()) return std::nullopt;
    return io::Bytes(std::move(bytes));
  }

  /** ONE FRAME'S MESSAGES, drained in order: each that reads is latched
   *  under nothing and under its own name, queued where a queue is
   *  open, and handed to every handler whose pattern names it before
   *  the next is taken. */
  void advance() {
    if (!feed) return;
    while (std::optional<io::Message> arrival = feed.receive()) {
      std::optional<Json> payload = decode(*arrival->payload, dialect, schema);
      if (!payload) {
        ++undecodable;
        continue;
      }
      const std::string name(nameOf(*payload));
      latest = Message(std::move(*payload), std::move(*arrival), name, schema);
      if (!name.empty()) latch(name, latest);
      if (queuing) {
        unread.push_back(latest);
        while (capacity != 0 && unread.size() > capacity) unread.pop_front();
      }

      // The message is held, not referred to, so a handler that asks for
      // the latest or replies reads the one it was given even when it
      // registers another handler; one registered from inside a handler
      // runs from the next message.
      const Message handled = latest;
      handling = &handled;
      const size_t registered = handlers.size();
      bool matched = false;
      for (size_t index = 0; index != registered; ++index) {
        const std::string_view pattern = handlers[index].first;
        const bool every = pattern == "*";
        if (!every && !matchesAddress(pattern, name)) continue;
        if (!every) matched = true;
        const Handler handler = handlers[index].second;
        handler(handled);
      }
      if (!matched) {
        const size_t unmatched = otherwise.size();
        for (size_t index = 0; index != unmatched; ++index) {
          const Handler handler = otherwise[index];
          handler(handled);
        }
      }
      handling = nullptr;
    }
  }
};

Connection Connection::opened(io::Hub& hub, std::string_view uri,
                              const std::string_view* recording,
                              ConnectOptions options) {
  Connection connection;
  auto state = std::make_shared<State>();
  state->uri = std::string(uri);
  state->dialect = options.dialect.value_or(dialectOf(uri));
  state->capacity = options.capacity;
  state->schema = std::move(options.schema);
  state->queuing = options.queue;
  connection.m_state = state;
  const bool ownWire = state->dialect == Dialect::Osc ||
                       state->dialect == Dialect::Midi ||
                       state->dialect == Dialect::ArtNet;
  if (state->schema && ownWire) {
    // OSC, MIDI and Art-Net each spell every value themselves, down to
    // the width a number goes out at, and a buffer is not one of those
    // spellings: nothing is bound, and nothing arrives.
    state->trouble = std::string("a schema reads a FlatBuffer wire, not ") +
                     (state->dialect == Dialect::Osc    ? "OSC"
                      : state->dialect == Dialect::Midi ? "MIDI"
                                                        : "Art-Net");
    return connection;
  }
  if (state->schema) state->dialect = Dialect::FlatBuffer;
  const io::ListenOptions listening{.capacity = options.capacity,
                                    .peer = std::move(options.peer)};
  state->feed = recording ? hub.replay(uri, *recording, listening)
                          : hub.listen(uri, listening);
  // The advance knows the state weakly: the state owns the lease, and a
  // lease owning the state back would keep both standing after the last
  // handle onto them was gone.
  state->lease = io::onAdvance(
      hub, [held = std::weak_ptr<State>(state)](std::chrono::duration<double>) {
        if (const std::shared_ptr<State> living = held.lock()) living->advance();
      });
  return connection;
}

Connection connect(io::Hub& hub, std::string_view uri, ConnectOptions options) {
  return Connection::opened(hub, uri, nullptr, std::move(options));
}

Connection replay(io::Hub& hub, std::string_view uri, std::string_view recording,
                  ConnectOptions options) {
  return Connection::opened(hub, uri, &recording, std::move(options));
}

const Message& Connection::latest() const {
  return m_state ? m_state->latest : nothing();
}

const Message& Connection::latest(std::string_view address) const {
  if (!m_state) return nothing();
  if (!isPattern(address)) {
    const auto found = m_state->named.find(address);
    return found == m_state->named.end() ? nothing() : found->second.message;
  }
  const State::Named* newest = nullptr;
  for (const auto& [name, latched] : m_state->named)
    if (matchesAddress(address, name) &&
        (!newest || latched.written > newest->written))
      newest = &latched;
  return newest ? newest->message : nothing();
}

std::optional<Message> Connection::receive() {
  if (!m_state) return std::nullopt;
  m_state->queuing = true;
  if (m_state->unread.empty()) return std::nullopt;
  Message message = std::move(m_state->unread.front());
  m_state->unread.pop_front();
  return message;
}

void Connection::on(std::string_view pattern, Handler handler) {
  if (!m_state || !handler) return;
  m_state->handlers.emplace_back(std::string(pattern), std::move(handler));
}

void Connection::otherwise(Handler handler) {
  if (!m_state || !handler) return;
  m_state->otherwise.push_back(std::move(handler));
}

bool Connection::send(const Json& message, const io::SendOptions& options) const {
  if (!m_state || !m_state->feed) return false;
  const std::optional<io::Bytes> bytes = m_state->write(message);
  return bytes && m_state->feed.send(*bytes, options);
}

bool Connection::reply(const Json& message) const {
  if (!m_state) return false;
  const Message& answered =
      m_state->handling ? *m_state->handling : m_state->latest;
  // Nobody to answer is not an error: it is what a recording, and a
  // door nothing has arrived at, has.
  if (answered.sender().empty()) return false;
  return send(message, {.to = answered.sender()});
}

io::Recording Connection::record(std::filesystem::path path) const {
  return m_state ? m_state->feed.record(std::move(path)) : io::Recording();
}

ConnectionState Connection::state() const {
  ConnectionState now;
  if (!m_state) {
    now.readiness = io::ReadyState::Closed;
    return now;
  }
  if (m_state->feed) static_cast<io::FeedState&>(now) = m_state->feed.state();
  else now.readiness = io::ReadyState::Closed;
  if (!m_state->trouble.empty()) now.error = m_state->trouble;
  now.undecodable = m_state->undecodable;
  return now;
}

void Connection::close() const {
  if (m_state && m_state->feed) m_state->feed.close();
}

const std::string& Connection::uri() const {
  return m_state ? m_state->uri : noUri();
}

const Schema& Connection::schema() const {
  return m_state ? m_state->schema : noSchema();
}

io::Feed Connection::feed() const {
  return m_state ? m_state->feed : io::Feed();
}

}  // namespace sigil::data
