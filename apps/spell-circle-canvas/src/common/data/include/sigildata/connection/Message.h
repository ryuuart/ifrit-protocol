#pragma once

/** @file
 * @ingroup data-connection
 * ONE MESSAGE OFF A CONNECTION: the value its bytes decoded to, and the
 * arrival they came in — who sent it, when, and its place in the count.
 * It reads as its payload, so a handler's body reads a field the way it
 * reads a `Json`: `message["colors"]`, and on an OSC message
 * `message[0]`, the first argument.
 */

#include <sigildata/decode/FlatBuffer.h>
#include <sigildata/decode/Json.h>
#include <sigildata/decode/Schema.h>
#include <sigildata/values/Values.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/source/Source.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::data {

/** A MESSAGE, READ. An empty one — before anything arrived, or under a
 *  name nothing arrived under — has a null payload, which reads through
 *  as the default of whatever is asked of it. */
class Message {
 public:
  /** The empty message. */
  Message() = default;

  /** @p payload, read from @p arrival, named @p name, read through
   *  @p schema where its door has one. */
  Message(Json payload, io::Message arrival, std::string name = {},
          Schema schema = {})
      : payload(std::move(payload)),
        m_arrival(std::move(arrival)),
        m_name(std::move(name)),
        m_schema(std::move(schema)) {}

  /** THE DECODED VALUE, whole. */
  Json payload;

  /** The member @p key of the payload, or a shared null. */
  const Json& operator[](std::string_view key) const { return payload[key]; }

  /** ON AN OSC MESSAGE, THE @p index-TH ARGUMENT; on any other, the
   *  payload's own @p index-th member. A null where it is not there. */
  const Json& operator[](size_t index) const { return value()[index]; }

  /** THE MESSAGE'S VALUE as one number, string or boolean: an OSC
   *  message's first argument, and any other message's payload itself.
   *  @p fallback where it is another kind. */
  double number(double fallback = 0.0) const {
    return scalar().number(fallback);
  }
  /** The same, as a string. */
  std::string_view string(std::string_view fallback = {}) const {
    return scalar().string(fallback);
  }
  /** The same, as a boolean. */
  bool boolean(bool fallback = false) const {
    return scalar().boolean(fallback);
  }

  /** THE OSC PATH the message is addressed to, `/sky/wind` — its
   *  `address` where it carries one as a string; empty otherwise. */
  std::string_view address() const { return payload["address"].string(); }

  /** What `Connection::on()` matched it by: its address, or the first of
   *  `type`, `message_type` and `kind` it carries as a string. */
  const std::string& name() const { return m_name; }

  /** Who sent it, `udp://10.0.0.4:9001`; empty for a recording and for
   *  a transport that cannot know. What `send(…, {.to})` answers. */
  const std::string& sender() const { return m_arrival.sender(); }

  /** When it arrived, counted from when its feed opened. */
  std::chrono::duration<double> arrivedAt() const {
    return m_arrival.arrivedAt();
  }

  /** When it arrived on the steady clock. */
  std::chrono::steady_clock::time_point receivedAt() const {
    return m_arrival.receivedAt();
  }

  /** 1 for the first message on its feed, counting up from there; 0 on
   *  the empty message. */
  uint64_t revision() const { return m_arrival.revision(); }

  /** The bytes as they arrived, unread; null on the empty message. */
  const std::shared_ptr<const io::Bytes>& bytes() const {
    return m_arrival.payload;
  }

  /** Whether this is the empty message. */
  bool empty() const { return !m_arrival.payload; }

  /** THE MESSAGE AS A VALUE OF ITS OWN: the bytes read through the
   *  reading a schema's generated value header wrote for Value — the
   *  schema's JSON form converted through the door's schema first.
   *  Nothing on the empty message and where the bytes are not that
   *  value.
   *  @trap A reading and not a cache: the bytes are decoded on each ask. */
  template <values::Readable Value>
  std::optional<Value> as() const;

 private:
  /** What an index and a scalar read: the arguments of an OSC message,
   *  the payload otherwise. */
  const Json& value() const {
    const Json& arguments = payload["arguments"];
    return arguments.kind() == Json::Kind::Array && !address().empty()
               ? arguments
               : payload;
  }
  const Json& scalar() const {
    const Json& held = value();
    return &held == &payload ? payload : held[0];
  }

  io::Message m_arrival;
  std::string m_name;
  Schema m_schema;
};

template <values::Readable Value>
std::optional<Value> Message::as() const {
  if (!m_arrival.payload) return std::nullopt;
  const io::Bytes& arrived = *m_arrival.payload;
  // The schema's JSON form is parsed to a buffer first, so a sender that
  // speaks it hands out the same value as one that sends the buffer.
  if (m_schema && flatBufferLooksLikeJson(arrived.asText(), {})) {
    const std::optional<std::vector<std::byte>> buffer =
        m_schema.binary(arrived.asText());
    if (!buffer) return std::nullopt;
    return values::Read<Value>::from(*buffer);
  }
  return values::Read<Value>::from(arrived.span());
}

}  // namespace sigil::data
