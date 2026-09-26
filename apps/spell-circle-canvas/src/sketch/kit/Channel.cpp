/** @file
 * The channel: the advance it registers on, the number it takes out of
 * the newest message of its name, and the one write it makes when that
 * number moved.
 */

#include "sigilsketch/kit/Channel.h"

#include <sigildata/connection/Connection.h>
#include <sigildata/decode/Json.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/advanced/Time.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>

namespace sigil::sketch::kit {

namespace {

/** The value a channel onto nothing answers: one, still at zero, so a
 *  binding written against a member that was never opened reads a number
 *  rather than nothing at all. */
const motion::Animatable<float>& stillValue() {
  static const motion::Animatable<float> still = 0.0f;
  return still;
}

const std::string& noName() {
  static const std::string empty;
  return empty;
}

}  // namespace

/** WHAT A CHANNEL IS. The advance reaches it weakly and every reading
 *  below reaches it through the channel's pointer, so the two agree
 *  however the channel is moved about — and the live value keeps its cell,
 *  which is what a description that bound it needs. */
struct Channel::State {
  /** The door the number arrives at. Not owned: a connection outlives the
   *  channels that follow it. */
  data::Connection* connection = nullptr;
  std::string name;
  /** The reading itself, which a channel onto nothing holds as the index
   *  of a message it will never be given. */
  std::variant<size_t, std::string> reading;
  motion::Animatable<float> live = motion::animatable(0.0f);
  /** How many arrivals the connection had counted when this last looked.
   *  Nothing has to be read until it moves. */
  uint64_t seen = 0;
  /** The number as the wire spelled it; nothing until one has arrived. */
  std::optional<double> read;
  /** What the hub's advance runs. It is released with this state, so a
   *  channel that is gone leaves nothing to run. */
  io::Lease lease;

  /** THE NUMBER THE READING NAMES in the newest message of this name, or
   *  nothing where it names none: no message under the name yet, an
   *  argument short of the index, a field the record does not carry, or a
   *  value that is not a number. A value of another kind is not a quiet
   *  zero, because a fader that reported text would then read as a fader
   *  at rest. */
  std::optional<double> take() const {
    const data::Json& message = connection->latest(name);
    const data::Json& at = std::holds_alternative<size_t>(reading)
                               ? message["arguments"][std::get<size_t>(reading)]
                               : message[std::get<std::string>(reading)];
    if (at.kind() != data::Json::Kind::Number) return std::nullopt;
    return at.number();
  }

  /** ONE DISPATCH'S WORK: look only when something arrived, and write
   *  only when what arrived moved this reading. */
  void follow() {
    if (!connection) return;
    const uint64_t arrived = connection->revision();
    if (arrived == seen) return;
    seen = arrived;
    const std::optional<double> number = take();
    if (!number || number == read) return;
    read = number;
    live = (float)*number;
  }
};

Channel::Channel(io::Hub& hub, data::Connection& connection, std::string name,
                 Reading reading) {
  auto state = std::make_shared<State>();
  state->connection = &connection;
  state->name = std::move(name);
  state->reading = std::move(reading.at);
  // The advance knows the state weakly: the state owns the lease, and a
  // lease owning the state back would keep both standing after the last
  // channel onto them was gone.
  state->lease = io::onAdvance(hub, [held = std::weak_ptr<State>(state)](std::chrono::duration<double>) {
    if (const std::shared_ptr<State> living = held.lock()) living->follow();
  });
  m_state = std::move(state);
}

const motion::Animatable<float>& Channel::live() const {
  return m_state ? m_state->live : stillValue();
}

float Channel::value() const { return live().value(); }

std::optional<double> Channel::lastRead() const {
  return m_state ? m_state->read : std::optional<double>{};
}

const std::string& Channel::name() const {
  return m_state ? m_state->name : noName();
}

}  // namespace sigil::sketch::kit
