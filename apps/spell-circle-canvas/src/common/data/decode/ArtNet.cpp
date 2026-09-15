/** @file
 * The Art-Net codec: one packet off the wire read into the one dynamic
 * value, and that same value written back on to it.
 *
 * THE WIRE IS A PAGE OF ARITHMETIC. Every packet opens with the same
 * eight bytes of name and then a two-byte operation code written LOW
 * byte first, which is the one number on this wire written that way;
 * every other number it carries — the protocol version, the count of
 * dimmers — is written high byte first. A reading that is bounded and a
 * packet that cannot be made sense of answering nothing is the whole of
 * what a receiver needs, which is why this is written here rather than
 * taken from a package.
 */

#include <sigildata/decode/ArtNet.h>

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <utility>

namespace sigil::data {

namespace {

/** THE NAME EVERY PACKET OPENS WITH, and the null after it: eight bytes
 *  that say the datagram is addressed to a lighting wire and not to
 *  whatever else is listening on the port. */
constexpr std::string_view kName = "Art-Net";

/** Which operation a packet is. A universe of dimmers is the one nearly
 *  every packet carries; a poll asks who is out there and its answer
 *  says. */
constexpr uint16_t kPollCode = 0x2000;
constexpr uint16_t kPollReplyCode = 0x2100;
constexpr uint16_t kDmxCode = 0x5000;

/** Where the operation code and the protocol version stand. A poll's
 *  answer carries no version at all, so the version is read only under
 *  the codes that take one. */
constexpr size_t kOpCodeAt = 8;
constexpr size_t kVersionAt = 10;

/** The version this reads and writes. A sender writing an older one
 *  writes other fields where these are read, so its packet is no packet
 *  here rather than a reading of whatever those bytes happen to say. */
constexpr int kVersion = 14;

/** The bytes of a dimmer packet before its levels: the name, the code,
 *  the version, the sequence and the physical input, the two halves of
 *  the port address, and the count. */
constexpr size_t kSequenceAt = 12;
constexpr size_t kPhysicalAt = 13;
constexpr size_t kSubUniverseAt = 14;
constexpr size_t kNetAt = 15;
constexpr size_t kCountAt = 16;
constexpr size_t kDmxHeaderBytes = 18;

/** The bytes of a poll: the name, the code, the version, what its
 *  sender wants told back, and how urgent a diagnostic has to be to be
 *  worth sending. */
constexpr size_t kPollBytes = 14;

/** HOW MANY DIMMERS ONE PACKET CARRIES. The wire counts them in pairs,
 *  and a universe is 512 of them. */
constexpr size_t kFewestChannels = 2;
constexpr size_t kMostChannels = 512;

/** The most a dimmer holds, and the most a port address does: the
 *  address is fifteen bits, the low byte the sub-universe a desk names
 *  and the seven bits above it the net that sub-universe stands in. */
constexpr double kLevelCeiling = 255;
constexpr double kUniverseCeiling = 0x7FFF;
constexpr int kNetMask = 0x7F;

/** @p value held inside [@p low, @p high]. A level outside what its
 *  place on the wire holds is written at the nearer end, since wrapping
 *  it would put a fixture at nothing that was meant to stand at full; a
 *  value that is not a number lands at the low end, having no side to
 *  be on. */
double clamped(double value, double low, double high) {
  if (!(value > low)) return low;
  return value > high ? high : value;
}

uint8_t byteAt(std::span<const std::byte> packet, size_t at) {
  return std::to_integer<uint8_t>(packet[at]);
}

/** The two bytes at @p at, HIGH byte first, which is the order this
 *  wire writes every number in but the one that says what a packet
 *  is. */
int bigAt(std::span<const std::byte> packet, size_t at) {
  return byteAt(packet, at) << 8 | byteAt(packet, at + 1);
}

/** Whether @p packet opens with the name every Art-Net packet opens
 *  with. It is read only where the whole name and the code behind it
 *  arrived. */
bool namesArtNet(std::span<const std::byte> packet) {
  for (size_t step = 0; step != kName.size(); ++step)
    if (std::to_integer<char>(packet[step]) != kName[step]) return false;
  return packet[kName.size()] == std::byte{0};
}

/** @p bytes as a list of the numbers they are. */
Json listed(std::span<const std::byte> bytes) {
  Json::Array numbers;
  numbers.reserve(bytes.size());
  for (const std::byte one : bytes)
    numbers.push_back(Json(std::to_integer<int>(one)));
  return Json(std::move(numbers));
}

/** The bytes every packet this writes opens with: the name, the null
 *  that ends it, @p opcode low byte first, and the version high byte
 *  first. */
void appendHeader(std::vector<std::byte>& packet, uint16_t opcode) {
  for (const char letter : kName)
    packet.push_back(static_cast<std::byte>(letter));
  packet.push_back(std::byte{0});
  packet.push_back(static_cast<std::byte>(opcode & 0xFFu));
  packet.push_back(static_cast<std::byte>(opcode >> 8));
  packet.push_back(static_cast<std::byte>(kVersion >> 8));
  packet.push_back(static_cast<std::byte>(kVersion & 0xFF));
}

std::byte levelByte(double value) {
  return static_cast<std::byte>(
      static_cast<int>(clamped(value, 0, kLevelCeiling)));
}

/** HOW MANY DIMMERS @p channels GOES OUT AS: what it holds, padded up
 *  to the pair the wire counts in and held inside the one universe a
 *  packet carries. A list with nothing in it still goes out as two
 *  dimmers at nothing, every fixture dark being a thing a desk says. */
size_t countOf(const Json& channels) {
  size_t count = std::min(channels.size(), kMostChannels);
  if (count < kFewestChannels) return kFewestChannels;
  return count % 2 == 0 ? count : count + 1;
}

}  // namespace

std::optional<Json> decodeArtNet(std::span<const std::byte> packet) {
  // A packet is its name and the code that says what it is; bytes too
  // few for those two cannot say either.
  if (packet.size() < kOpCodeAt + 2 || !namesArtNet(packet))
    return std::nullopt;
  const uint16_t opcode = static_cast<uint16_t>(
      byteAt(packet, kOpCodeAt) | (byteAt(packet, kOpCodeAt + 1) << 8));

  if (opcode == kDmxCode) {
    if (packet.size() < kDmxHeaderBytes || bigAt(packet, kVersionAt) < kVersion)
      return std::nullopt;
    const size_t count = static_cast<size_t>(bigAt(packet, kCountAt));
    // The count is the packet's own claim about itself: a desk writes
    // its dimmers in pairs and a universe holds 512 of them, so a count
    // that is odd, empty or larger is no count a desk meant — and one
    // reaching past what arrived is a claim these bytes do not hold.
    if (count < kFewestChannels || count > kMostChannels || count % 2 != 0 ||
        count > packet.size() - kDmxHeaderBytes)
      return std::nullopt;

    Json::Object fields;
    fields.emplace_back("kind", Json("Dmx"));
    // The port address arrives in two halves, the sub-universe first,
    // and is put back together here. Its top bit is no part of an
    // address and is not read.
    fields.emplace_back("universe",
                        Json(((byteAt(packet, kNetAt) & kNetMask) << 8) |
                             byteAt(packet, kSubUniverseAt)));
    fields.emplace_back("sequence", Json(byteAt(packet, kSequenceAt)));
    fields.emplace_back("physical", Json(byteAt(packet, kPhysicalAt)));
    fields.emplace_back("channels",
                        listed(packet.subspan(kDmxHeaderBytes, count)));
    return Json(std::move(fields));
  }

  if (opcode == kPollCode) {
    // What a poll carries past its version is what its sender wants
    // told back and how urgent a diagnostic must be to be worth
    // sending, neither of which a scene acts on: that a desk asked at
    // all is the whole of it here.
    if (packet.size() < kPollBytes || bigAt(packet, kVersionAt) < kVersion)
      return std::nullopt;
    return Json(Json::Object{{"kind", Json("Poll")}});
  }

  // A poll's answer is a node's name, its addresses and what it can do;
  // every other code carries whatever its sender means by it. Neither
  // has a reading here, so each is carried whole and goes back out the
  // way it came in.
  Json::Object fields;
  fields.emplace_back("kind",
                      Json(opcode == kPollReplyCode ? "PollReply" : "ArtNet"));
  if (opcode != kPollReplyCode)
    fields.emplace_back("opcode", Json(static_cast<int>(opcode)));
  fields.emplace_back("bytes", listed(packet));
  return Json(std::move(fields));
}

std::vector<std::byte> encodeArtNet(const Json& message) {
  const std::string_view kind = message["kind"].text();

  if (kind == "Dmx") {
    const Json& channels = message["channels"];
    const size_t count = countOf(channels);
    std::vector<std::byte> packet;
    packet.reserve(kDmxHeaderBytes + count);
    appendHeader(packet, kDmxCode);
    packet.push_back(levelByte(message["sequence"].number()));
    packet.push_back(levelByte(message["physical"].number()));
    const int universe = static_cast<int>(
        clamped(message["universe"].number(), 0, kUniverseCeiling));
    // The address goes out in the two halves the wire carries it in,
    // the sub-universe first.
    packet.push_back(static_cast<std::byte>(universe & 0xFF));
    packet.push_back(static_cast<std::byte>((universe >> 8) & kNetMask));
    packet.push_back(static_cast<std::byte>(count >> 8));
    packet.push_back(static_cast<std::byte>(count & 0xFF));
    // A list shorter than the count it goes out under is padded with
    // dimmers at nothing, a member that is not there reading as one.
    for (size_t at = 0; at != count; ++at)
      packet.push_back(levelByte(channels[at].number()));
    return packet;
  }

  if (kind == "Poll") {
    std::vector<std::byte> packet;
    packet.reserve(kPollBytes);
    appendHeader(packet, kPollCode);
    // What is wanted told back, and how urgent a diagnostic has to be:
    // both stand at nothing, which asks a node for its answer and for
    // nothing else.
    packet.push_back(std::byte{0});
    packet.push_back(std::byte{0});
    return packet;
  }

  // A record carrying its own bytes goes out as exactly those bytes,
  // whatever else it says: a packet this codec has no reading for is
  // written back the way it arrived.
  const Json& verbatim = message["bytes"];
  if (verbatim.kind() == Json::Kind::List) {
    std::vector<std::byte> packet;
    packet.reserve(verbatim.size());
    for (const Json& one : verbatim.items())
      packet.push_back(levelByte(one.number()));
    return packet;
  }

  // A value with neither a kind this knows nor bytes of its own has no
  // spelling on the wire: an empty datagram is no dimmer at all.
  return {};
}

}  // namespace sigil::data
