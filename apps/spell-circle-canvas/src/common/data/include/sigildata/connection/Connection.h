#pragma once

/** @file
 * @ingroup data-connection
 * A CONNECTION: a feed read as values — IO's `hub.listen`, one level
 * up. `data::connect(hub, uri)` opens the door and hands back a value
 * handle whose verbs are the feed's own — `latest`, `receive`, `send`,
 * `record`, `state`, `close` — answering a `data::Message` whose payload
 * is a `Json` where the feed answers bytes. The dialect is read off the
 * URI's scheme — `osc://` an OSC packet, `midi://` a MIDI message,
 * `artnet://` an Art-Net packet, and JSON text on every other — or
 * named in the options, and a door opened with a schema reads every
 * message through it. NOTHING DRIVES IT BUT THE FRAME: a connection
 * registers on the hub's advance as it opens, and between two advances
 * it answers exactly what the last one left it. ONE THREAD, the
 * advancing one, so a connection holds no lock of its own.
 */

#include <sigildata/connection/Message.h>
#include <sigildata/decode/Dialect.h>
#include <sigildata/decode/Json.h>
#include <sigildata/decode/Schema.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/source/State.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace sigil::io {
// The door is opened on a hub, which a consumer of this header names
// through its own; nothing here reads one.
class Hub;
}  // namespace sigil::io

namespace sigil::data {

/** HOW A DOOR IS OPENED: `connect(hub, uri, {.capacity = 64})`. */
struct ConnectOptions {
  /** IO's: messages the door holds between two advances, and what
   *  receive()'s queue and the latches per name hold. */
  size_t capacity = 256;
  /** IO's: where send() goes from a door that did not open to one,
   *  `10.0.0.4:9001`. */
  std::string peer;
  /** How bytes read; empty reads it off the scheme. */
  std::optional<Dialect> dialect;
  /** Read and write every message through this FlatBuffers schema,
   *  `schema<Root>()`: the buffer and the schema's JSON form both
   *  arrive, and a message that does not fit is undecodable.
   *  @trap AN `osc://`, A `midi://` OR AN `artnet://` DOOR TAKES NO
   *  SCHEMA and is refused as it opens: nothing is bound, nothing
   *  arrives, and `state().error` says so. */
  Schema schema;
  /** Hold messages for receive() from the first arrival, rather than
   *  from the first receive(). */
  bool queue = false;
};

/** WHERE A CONNECTION STANDS, as one comparable value: IO's `FeedState`
 *  — readiness, revision, dropped, the local end, the error — and the
 *  arrivals that were no message in the door's dialect. A reader that
 *  keeps the one it last showed asks `now != shown`. */
struct ConnectionState : io::FeedState {
  /** Arrivals that were no message in this door's dialect, or did not
   *  fit its schema. They reach no reader, so a sender speaking the
   *  wrong language is seen here rather than in the drawing. */
  uint64_t undecodable = 0;

  bool operator==(const ConnectionState&) const = default;
};

/** ONE DOOR, ITS MESSAGES, AND THE HANDLERS OVER THEM. A copyable handle:
 *  every copy reads the same door, and the door closes when close() is
 *  called or the last handle goes. */
class Connection {
 public:
  /** What runs for one message. */
  using Handler = std::function<void(const Message& message)>;

  /** A connection onto nothing: no door, no message, a closed state. It
   *  is what a member stands at before there is a hub to open it on. */
  Connection() = default;

  /** THE NEWEST MESSAGE; the empty message until one has arrived and
   *  been read. A message that cannot be read leaves it standing, so a
   *  sender speaking the wrong language cannot blank a scene. Inside a
   *  handler it is the message that handler was given. */
  const Message& latest() const;

  /** THE NEWEST MESSAGE NAMED @p address — by the rule on() names by, an
   *  OSC address pattern included — so a reader takes one fader off the
   *  wire with no handler at all: `sky.latest("/sky/wind").number()`.
   *  The empty message until one of that name has arrived.
   *  @trap One latch per name, bounded by the options' capacity: a name
   *  one too many drops the name written longest ago. */
  const Message& latest(std::string_view address) const;

  /** The next message this reader has not taken, in order; nothing when
   *  none is waiting, and never a wait. The handlers see every message
   *  whether or not it is taken here.
   *  @trap THE FIRST CALL OPENS THE QUEUE unless the door was opened
   *  with `.queue`: messages read before it are not held. */
  std::optional<Message> receive();

  /** Runs @p handler for every message @p pattern names, from now on. A
   *  MESSAGE'S NAME is its `address` where it carries one as a string,
   *  and otherwise the first of `type`, `message_type` and `kind` it
   *  carries as one. @p pattern is an OSC 1.0 address pattern — `?` one
   *  character, `*` any run of them, `[a-c]` and `[!a-c]` one of or none
   *  of a set, `{wind,gust}` one of a list, none of them crossing a `/` —
   *  so a plain name matches itself alone; `"*"` names every message, one
   *  with no name included. Handlers run on advance, in the order the
   *  messages arrived and then in the order they were registered.
   *  @trap A handler registered after a message arrived does not see it;
   *  latest() is how a late reader catches up. */
  void on(std::string_view pattern, Handler handler);

  /** Runs @p handler for every message NO on() pattern other than `"*"`
   *  matched, after the handlers that did run for it. */
  void otherwise(Handler handler);

  /** Sends @p message back through the same door, written in its
   *  dialect — `oscMessage(address, arguments)` on an `osc://` door —
   *  to the door's peer, or with `.to` to ONE sender as
   *  `Message::sender()` names it. False when the door is one-way,
   *  closed or never opened, and when the value has no spelling in the
   *  dialect. */
  bool send(const Json& message, const io::SendOptions& options = {}) const;

  /** Sends @p message TO THE SENDER OF ONE: inside a handler, the sender
   *  of the message being handled; outside one, of the newest. False
   *  where there is nobody to answer — a recording names no sender. */
  bool reply(const Json& message) const;

  /** Writes every message from now on to the file at @p path, IO's
   *  recording, until the handle this returns stops or goes. */
  [[nodiscard]] io::Recording record(std::filesystem::path path) const;

  /** Where the door stands, as of the last advance. A connection onto
   *  nothing answers a closed state. */
  ConnectionState state() const;

  /** No more messages are taken, for every handle; what was read stays
   *  readable. */
  void close() const;

  /** The URI this was opened on; empty for a connection onto nothing. */
  const std::string& uri() const;

  /** The schema every message is read and written through; none where
   *  the door was opened without one. */
  const Schema& schema() const;

  /** THE FLOOR BELOW: the feed itself, bytes and all. Empty for a
   *  connection onto nothing. */
  io::Feed feed() const;

  /** Whether this handle holds a door. */
  explicit operator bool() const { return m_state != nullptr; }

 private:
  friend Connection connect(io::Hub& hub, std::string_view uri,
                            ConnectOptions options);
  friend Connection replay(io::Hub& hub, std::string_view uri,
                           std::string_view recording, ConnectOptions options);

  /** Opens @p uri, played from @p recording where one is named. */
  static Connection opened(io::Hub& hub, std::string_view uri,
                           const std::string_view* recording,
                           ConnectOptions options);

  /** Everything a connection is, behind one pointer. What the hub
   *  advances reaches this rather than the handle, so a copy or a move
   *  goes on reading the same state. */
  struct State;
  std::shared_ptr<State> m_state;
};

/** OPENS @p uri ON @p hub — IO's `hub.listen`, read as values — and
 *  registers the connection on the hub's advance. */
Connection connect(io::Hub& hub, std::string_view uri,
                   ConnectOptions options = {});

/** THE SAME DOOR, PLAYED FROM @p recording — IO's `hub.replay`, read as
 *  values: every later listen on @p uri plays that file, so a reader
 *  written against the live wire reads the recording without knowing
 *  it. The recording names no sender, so reply() answers false. */
Connection replay(io::Hub& hub, std::string_view uri,
                  std::string_view recording, ConnectOptions options = {});

/** Whether @p name matches the OSC 1.0 address @p pattern, the rule
 *  `Connection::on()` registers by. */
bool matchesAddress(std::string_view pattern, std::string_view name);

}  // namespace sigil::data
