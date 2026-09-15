#pragma once

/** @file
 * ONE NUMBER OF A WIRE AS A MOTION OUTPUT: the channel a fader reaches a
 * bound property through, with no handler standing between the two.
 */

#include <choreograph/Choreograph.h>

#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <variant>

namespace sigil::data {
/** A feed read as values — the door a message arrives at. Its own words
 *  are `<sigildata/connection/Connection.h>`, which this names and does
 *  not include. */
class Connection;
}  // namespace sigil::data

namespace sigil::io {
/** What opens that door and drives the frame that drains it. Its own
 *  words are `<sigilio/hub/Hub.h>`. */
class Hub;
}  // namespace sigil::io

namespace sigil::sketch::kit {

/** A CONNECTION'S NUMBER, AS THE OUTPUT A BINDING CHAIN READS.
 *
 *  A desk sends a fader reading and a property has to move by it. What
 *  usually stands between the two is a handler: read the message, write
 *  a member, scale it into the units the property wants, describe again.
 *  Every step of that but the first is arithmetic the binding chain
 *  already spells, so this is the piece that takes the handler out — the
 *  channel follows one number of one message, and the chain does the
 *  rest:
 *
 *      sketch::kit::Channel wind{hub, desk, "/sky/wind", 0};
 *      …
 *      compose::box().scaleY(motion::bind(&wind.output())
 *                                .source(0, 127)
 *                                .target(0.2f, 1.0f))
 *
 *  TWO READINGS, because a number stands in one of two places. An
 *  argument INDEX takes it out of a message whose arguments are a list,
 *  which is what an OSC message is — `Channel(hub, desk, "/sky/wind", 0)`
 *  is `latest("/sky/wind")["arguments"][0]`. A field NAME takes it out of
 *  a message that is a record — `Channel(hub, phone, "Wind", "value")` is
 *  `latest("Wind")["value"]`. The name is the one a handler would have
 *  been registered under: an address on an OSC wire, a kind on a JSON
 *  one.
 *
 *  IT MOVES ONLY WHEN THE MESSAGE MOVED. A dispatch that delivered
 *  nothing, a message under another name, and a message carrying the same
 *  reading as the last all leave the output exactly where it stood, so a
 *  still fader does not rewrite a bound property once a frame. A number
 *  that is not there — no message of that name yet, an argument short of
 *  the index, a field the record does not carry, a value that is not a
 *  number — leaves it where it stood too: a sender that went quiet holds
 *  its last reading rather than snapping to zero.
 *
 *  THE HUB IS NAMED because a connection does not carry the one it was
 *  opened on, and it is the hub's dispatch that drives this: the callback
 *  runs after every recording has been advanced and before the frame is
 *  described, in the order the callbacks were registered. So a channel
 *  built AFTER its connection was opened reads what that same dispatch
 *  delivered, and a frame's every reading agrees with every other.
 *
 *  THE CONNECTION AND THE HUB OUTLIVE THE CHANNEL. Neither is owned: the
 *  channel holds the connection to read it and the hub to be driven by
 *  it, and a channel standing after either is gone reads freed memory.
 *
 *  Movable and not copyable. The state lives behind a pointer, so the
 *  output keeps its address through a move — which is what a binding
 *  holding a pointer to it needs — and what the dispatch runs goes on
 *  reading the same state. */
class Channel {
 public:
  /** WHERE THE NUMBER STANDS IN THE MESSAGE: the INDEX of an argument,
   *  for a wire whose message is an address and a list, or the NAME of a
   *  field, for a wire whose message is a record.
   *
   *  Each is written as itself at the call site — `0` and `"value"` —
   *  because which of the two a reading is, the value already says. It is
   *  a type rather than the variant alone so that it can be: a whole
   *  number written as a literal is an `int`, and an `int` reaches no
   *  alternative of a variant over a `size_t`. */
  struct Reading {
    std::variant<size_t, std::string> at;

    /** AN ARGUMENT'S INDEX. Every whole-number type spells one, so a
     *  literal and a loop's own counter are the same reading; a boolean
     *  names no place and is refused rather than read as the index 1. */
    template <std::integral Index>
      requires(!std::same_as<Index, bool>)
    Reading(Index index)  // NOLINT: implicit by design
        : at(static_cast<size_t>(index)) {}
    /** A FIELD'S NAME. A literal is the name and never a pointer to be
     *  measured as a number. */
    Reading(std::string field) : at(std::move(field)) {}  // NOLINT: implicit
    Reading(const char* field)                            // NOLINT: implicit
        : at(field != nullptr ? std::string(field) : std::string()) {}
  };

  /** A channel onto nothing: no connection, no dispatch, and an output
   *  standing still at zero. It is what a scene's member is before there
   *  is a hub to open a door on, and a binding written against it reads a
   *  number rather than nothing at all. */
  Channel() = default;

  /** Follows the number @p reading names in the newest message called
   *  @p name on @p connection, and registers on @p hub's dispatch to do
   *  it. */
  Channel(io::Hub& hub, data::Connection& connection, std::string name,
          Reading reading);

  Channel(Channel&&) noexcept = default;
  Channel& operator=(Channel&&) noexcept = default;
  Channel(const Channel&) = delete;
  Channel& operator=(const Channel&) = delete;

  /** THE OUTPUT A BINDING READS — `motion::bind(&channel.output())`. It
   *  keeps its address for the life of the channel, moves included, so a
   *  description that bound it once goes on reading it. */
  [[nodiscard]] const choreograph::Output<float>& output() const;

  /** The number that output stands at, for a caller that wants the
   *  reading rather than a binding onto it. */
  [[nodiscard]] float value() const;

  /** THE NUMBER AS THE WIRE SPELLED IT, at the width the message carried
   *  it at; nothing until one has arrived that this reading could take a
   *  number out of. It is what a readout says about the door — the output
   *  is the same reading narrowed to what a property is driven by. */
  [[nodiscard]] std::optional<double> lastRead() const;

  /** The message name this follows; empty for a channel onto nothing. */
  [[nodiscard]] const std::string& name() const;

 private:
  /** Everything a channel is, behind one pointer. What the dispatch runs
   *  reaches this rather than the channel. */
  struct State;
  std::shared_ptr<State> m_state;
};

}  // namespace sigil::sketch::kit
