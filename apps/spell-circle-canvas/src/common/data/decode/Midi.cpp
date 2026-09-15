/** @file
 * The MIDI codec: one message off the wire read into the one dynamic
 * value, and that same value written back on to it.
 *
 * THE WIRE IS A PAGE OF ARITHMETIC. A message opens with a byte that
 * has its top bit set — which is what tells a status byte from a data
 * byte — and that byte says both what the message is and, for the six
 * kinds that are played on a channel, which channel it was played on.
 * Everything after it is seven bits wide. A reading that is bounded and
 * a message that cannot be made sense of answering nothing is the whole
 * of what a receiver needs, which is why this is written here rather
 * than taken from a package.
 */

#include <sigildata/decode/Midi.h>

#include <cstdint>
#include <string_view>
#include <utility>

namespace sigil::data {

namespace {

/** WHAT TELLS A STATUS BYTE FROM A DATA BYTE: the top bit. A byte below
 *  this opens no message and stands in no message but as data. */
constexpr uint8_t kStatusMark = 0x80;
/** The two halves of a channel message's status byte: what it is, and
 *  which of the sixteen channels it was played on. */
constexpr uint8_t kKindMask = 0xF0;
constexpr uint8_t kChannelMask = 0x0F;

constexpr uint8_t kNoteOff = 0x80;
constexpr uint8_t kNoteOn = 0x90;
constexpr uint8_t kPolyAftertouch = 0xA0;
constexpr uint8_t kControlChange = 0xB0;
constexpr uint8_t kProgramChange = 0xC0;
constexpr uint8_t kAftertouch = 0xD0;
constexpr uint8_t kPitchBend = 0xE0;
/** Where the channel messages stop and the messages addressed to the
 *  whole room begin. */
constexpr uint8_t kSystemFloor = 0xF0;
constexpr uint8_t kSystemExclusive = 0xF0;
constexpr uint8_t kExclusiveEnd = 0xF7;
constexpr uint8_t kClock = 0xF8;
constexpr uint8_t kStart = 0xFA;
constexpr uint8_t kContinue = 0xFB;
constexpr uint8_t kStop = 0xFC;
constexpr uint8_t kActiveSensing = 0xFE;
constexpr uint8_t kReset = 0xFF;

/** The most a data byte holds, which is also the mask that takes one
 *  half of a value the wire carries in two: a data byte is seven bits,
 *  the eighth being what would make it a status byte. */
constexpr int kDataCeiling = 127;

/** How many channels there are, counted the way a person counts them:
 *  the wire writes a channel as 0 to 15 and every desk in the world
 *  prints it as 1 to 16. */
constexpr int kChannels = 16;

/** Where the wheel stands when nobody is touching it, as the wire
 *  counts: the two halves put together make 0 to 16383, and `bend` is
 *  measured from the centre of that travel. */
constexpr int kBendCentre = 8192;

/** @p value held inside [@p low, @p high]. A value outside what its
 *  place on the wire holds is written at the nearer end, since wrapping
 *  it would put a note nobody played on the cable; a value that is not
 *  a number lands at the low end, having no side to be on. */
double clamped(double value, double low, double high) {
  if (!(value > low)) return low;
  return value > high ? high : value;
}

uint8_t byteAt(std::span<const std::byte> message, size_t at) {
  return std::to_integer<uint8_t>(message[at]);
}

/** Whether every byte of @p bytes is a data byte. A status byte inside
 *  a message is another message beginning, and a message is read whole
 *  or not at all. */
bool allData(std::span<const std::byte> bytes) {
  for (std::byte one : bytes)
    if (std::to_integer<uint8_t>(one) >= kStatusMark) return false;
  return true;
}

/** @p bytes as a list of the numbers they are. */
Json listed(std::span<const std::byte> bytes) {
  Json::Array numbers;
  numbers.reserve(bytes.size());
  for (std::byte one : bytes)
    numbers.push_back(Json(std::to_integer<int>(one)));
  return Json(std::move(numbers));
}

/** The two fields every message carries, added last: the byte it opens
 *  with, and the data bytes after it. So a reader that knows the wire
 *  reads the wire, and a reader that does not reads the names above
 *  these. */
void addWire(Json::Object& fields, uint8_t status,
             std::span<const std::byte> data) {
  fields.emplace_back("status", Json(static_cast<int>(status)));
  fields.emplace_back("data", listed(data));
}

/** The name of the message @p status opens, for the kinds played on a
 *  channel. A note on at no velocity at all is how a keyboard says the
 *  key came back up, so it is a note off: a scene acting on the kind
 *  alone would otherwise hold every note it was ever played. */
const char* channelKind(uint8_t kind, int velocity) {
  switch (kind) {
    case kNoteOff:
      return "NoteOff";
    case kNoteOn:
      return velocity == 0 ? "NoteOff" : "NoteOn";
    case kPolyAftertouch:
      return "PolyAftertouch";
    case kControlChange:
      return "ControlChange";
    case kProgramChange:
      return "ProgramChange";
    case kAftertouch:
      return "Aftertouch";
    default:
      return "PitchBend";
  }
}

/** The name of a message addressed to the room, or nothing where this
 *  codec has none of its own for it. Each of these is one byte and
 *  nothing else. */
const char* systemKind(uint8_t status) {
  switch (status) {
    case kClock:
      return "Clock";
    case kStart:
      return "Start";
    case kContinue:
      return "Continue";
    case kStop:
      return "Stop";
    case kActiveSensing:
      return "ActiveSensing";
    case kReset:
      return "Reset";
    default:
      return nullptr;
  }
}

std::byte dataByte(double value) {
  return static_cast<std::byte>(
      static_cast<int>(clamped(value, 0, kDataCeiling)));
}

std::vector<std::byte> messageOf(int status) {
  return {static_cast<std::byte>(status)};
}

std::vector<std::byte> messageOf(int status, double first) {
  return {static_cast<std::byte>(status), dataByte(first)};
}

std::vector<std::byte> messageOf(int status, double first, double second) {
  return {static_cast<std::byte>(status), dataByte(first), dataByte(second)};
}

}  // namespace

std::optional<Json> decodeMidi(std::span<const std::byte> message) {
  if (message.empty()) return std::nullopt;
  const uint8_t status = byteAt(message, 0);
  // A message opens with a status byte. Bytes that arrived without one
  // belong to whatever message came before them, which is a message
  // this was not given.
  if (status < kStatusMark) return std::nullopt;

  if (status < kSystemFloor) {
    const uint8_t kind = status & kKindMask;
    // Two of the seven kinds carry one byte; the rest carry two. A
    // message that carries another number of them is no message of that
    // kind rather than that kind with something after it.
    const size_t takes =
        kind == kProgramChange || kind == kAftertouch ? 1u : 2u;
    if (message.size() != takes + 1) return std::nullopt;
    const std::span<const std::byte> data = message.subspan(1);
    if (!allData(data)) return std::nullopt;
    const int first = byteAt(message, 1);
    const int second = takes == 2 ? byteAt(message, 2) : 0;

    Json::Object fields;
    fields.emplace_back("kind", Json(channelKind(kind, second)));
    fields.emplace_back("channel",
                        Json(static_cast<int>(status & kChannelMask) + 1));
    switch (kind) {
      case kNoteOff:
      case kNoteOn:
        fields.emplace_back("note", Json(first));
        fields.emplace_back("velocity", Json(second));
        break;
      case kPolyAftertouch:
        fields.emplace_back("note", Json(first));
        fields.emplace_back("pressure", Json(second));
        break;
      case kControlChange:
        fields.emplace_back("controller", Json(first));
        fields.emplace_back("value", Json(second));
        break;
      case kProgramChange:
        fields.emplace_back("program", Json(first));
        break;
      case kAftertouch:
        fields.emplace_back("pressure", Json(first));
        break;
      default:
        // The wheel arrives in two sevens, the smaller half first, and
        // is put back together here: nothing reading this value has to
        // know that it travelled in halves.
        fields.emplace_back("bend", Json((second << 7 | first) - kBendCentre));
        break;
    }
    addWire(fields, status, data);
    return Json(std::move(fields));
  }

  if (const char* const named = systemKind(status)) {
    // One byte and nothing else: a message the room is meant to act on
    // the instant it arrives carries nothing it would have to wait for.
    if (message.size() != 1) return std::nullopt;
    Json::Object fields;
    fields.emplace_back("kind", Json(named));
    addWire(fields, status, message.subspan(1));
    return Json(std::move(fields));
  }

  if (status == kSystemExclusive) {
    // The payload is everything between the byte the message opens with
    // and the byte that ends it — which may be missing altogether,
    // where a driver handed on a message it did not see the end of.
    size_t payload = message.size() - 1;
    if (payload != 0 && byteAt(message, message.size() - 1) >= kStatusMark) {
      if (byteAt(message, message.size() - 1) != kExclusiveEnd)
        return std::nullopt;
      --payload;
    }
    if (!allData(message.subspan(1, payload))) return std::nullopt;
    Json::Object fields;
    fields.emplace_back("kind", Json("SystemExclusive"));
    fields.emplace_back("bytes", listed(message));
    addWire(fields, status, message.subspan(1, payload));
    return Json(std::move(fields));
  }

  // Every other message addressed to the room. What it says is the
  // sender's own business, so it is carried whole rather than named,
  // and goes back out the way it came in.
  const std::span<const std::byte> data = message.subspan(1);
  if (!allData(data)) return std::nullopt;
  Json::Object fields;
  fields.emplace_back("kind", Json("System"));
  fields.emplace_back("bytes", listed(message));
  addWire(fields, status, data);
  return Json(std::move(fields));
}

std::vector<std::byte> encodeMidi(const Json& message) {
  // A record carrying its own bytes goes out as exactly those bytes,
  // whatever else it says: a message this codec has no name for is
  // written back the way it arrived.
  const Json& verbatim = message["bytes"];
  if (verbatim.kind() == Json::Kind::List) {
    std::vector<std::byte> bytes;
    bytes.reserve(verbatim.size());
    for (const Json& one : verbatim.items())
      bytes.push_back(static_cast<std::byte>(
          static_cast<int>(clamped(one.number(), 0, 255))));
    return bytes;
  }

  const std::string_view kind = message["kind"].text();
  // A message that names no channel is on the first one, which is where
  // a controller with no channel of its own speaks.
  const int channel =
      static_cast<int>(clamped(message["channel"].number(1), 1, kChannels)) - 1;

  if (kind == "NoteOn")
    return messageOf(kNoteOn | channel, message["note"].number(),
                     message["velocity"].number());
  if (kind == "NoteOff")
    return messageOf(kNoteOff | channel, message["note"].number(),
                     message["velocity"].number());
  if (kind == "PolyAftertouch")
    return messageOf(kPolyAftertouch | channel, message["note"].number(),
                     message["pressure"].number());
  if (kind == "ControlChange")
    return messageOf(kControlChange | channel, message["controller"].number(),
                     message["value"].number());
  if (kind == "ProgramChange")
    return messageOf(kProgramChange | channel, message["program"].number());
  if (kind == "Aftertouch")
    return messageOf(kAftertouch | channel, message["pressure"].number());
  if (kind == "PitchBend") {
    const int bend = static_cast<int>(clamped(message["bend"].number(),
                                              -kBendCentre, kBendCentre - 1)) +
                     kBendCentre;
    return messageOf(kPitchBend | channel, bend & kDataCeiling, bend >> 7);
  }
  if (kind == "Clock") return messageOf(kClock);
  if (kind == "Start") return messageOf(kStart);
  if (kind == "Continue") return messageOf(kContinue);
  if (kind == "Stop") return messageOf(kStop);
  if (kind == "ActiveSensing") return messageOf(kActiveSensing);
  if (kind == "Reset") return messageOf(kReset);

  // A value with neither a kind this knows nor bytes of its own has no
  // spelling on the wire: an empty message is no shorter message.
  return {};
}

}  // namespace sigil::data
