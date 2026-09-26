/** The Art-Net codec: what a packet off the wire reads as, what a value
 *  goes back out as, and what is no packet at all.
 *
 *  A codec that stands on its own is judged against the wire and
 *  nothing else, so every packet here is spelled out byte by byte, and
 *  what is read is written back to the bytes it was read from.
 */

#include <gtest/gtest.h>
#include <sigildata/decode/Dialect.h>

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <vector>

using namespace sigil::data;

namespace {

/** THE PACKET A DESK SENDS WHEN ONE FIXTURE IS LIT, byte for byte: the
 *  name and the null that ends it, the code for a universe of dimmers
 *  LOW byte first, the version high byte first as every other number on
 *  this wire is, the sequence, the physical input, the two halves of
 *  the port address with the sub-universe first, the count of dimmers,
 *  and the levels themselves.
 *
 *  The desk is lighting one three-colour fixture on universe 0, and the
 *  wire counts its dimmers in pairs, so a fourth at nothing rides along
 *  behind the three. Every other packet here is built from these same
 *  pieces, and this one is what says they go down in the order the wire
 *  wants them. */
constexpr unsigned char deskWire[] = {
    'A',  'r', 't', '-',  'N',  'e',  't',  0,   0x00, 0x50, 0x00,
    0x0E, 7,   0,   0x00, 0x00, 0x00, 0x04, 255, 128,  0,    0};

/** A run of bytes spelled out as numbers. */
std::vector<std::byte> bytesOf(std::initializer_list<int> wire) {
  std::vector<std::byte> packet;
  packet.reserve(wire.size());
  for (const int one : wire) packet.push_back(static_cast<std::byte>(one));
  return packet;
}

template <size_t N>
std::vector<std::byte> bytesOf(const unsigned char (&wire)[N]) {
  std::vector<std::byte> packet;
  packet.reserve(N);
  for (const unsigned char one : wire)
    packet.push_back(static_cast<std::byte>(one));
  return packet;
}

/** The desk's packet with the bytes at @p at replaced, which is how one
 *  field at a time is put wrong. */
std::vector<std::byte> deskWith(size_t at, std::initializer_list<int> bytes) {
  std::vector<std::byte> packet = bytesOf(deskWire);
  for (const int one : bytes) packet[at++] = static_cast<std::byte>(one);
  return packet;
}

/** The list of numbers a `channels` or a `bytes` member is. */
Json listOf(std::initializer_list<int> numbers) {
  Json::Array listed;
  for (const int one : numbers) listed.push_back(Json(one));
  return Json(std::move(listed));
}

TEST(DataArtNet, AUniverseIsItsAddressItsSequenceAndItsDimmers) {
  const std::optional<Json> value = decode(bytesOf(deskWire), Dialect::ArtNet);
  ASSERT_TRUE(value.has_value());
  // The whole record is compared, because what the fields are CALLED is
  // what a scene reads and is as much the codec's promise as the levels
  // are.
  EXPECT_EQ(*value, Json(Json::Object{{"kind", Json("Dmx")},
                                      {"universe", Json(0)},
                                      {"sequence", Json(7)},
                                      {"physical", Json(0)},
                                      {"channels", listOf({255, 128, 0, 0})}}));

  // And back out as the bytes it was read from: a codec that reads what
  // it cannot write is half a codec.
  EXPECT_EQ(encode(*value, Dialect::ArtNet), bytesOf(deskWire));
}

TEST(DataArtNet, ThePortAddressIsItsTwoHalvesPutBackTogether) {
  // The sub-universe is the low byte and the net the seven bits above
  // it, so a desk on net 2 at sub-universe 5 is universe 517 — and
  // nothing reading that number has to know it travelled in halves.
  const std::optional<Json> value = decode(deskWith(14, {0x05, 0x02}), Dialect::ArtNet);
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ((*value)["universe"], Json(0x205));
  EXPECT_EQ(encode(*value, Dialect::ArtNet), deskWith(14, {0x05, 0x02}));

  // The eighth bit of the net byte is no part of an address, so it is
  // not read and the address is the same one.
  const std::optional<Json> topped = decode(deskWith(14, {0x05, 0x82}), Dialect::ArtNet);
  ASSERT_TRUE(topped.has_value());
  EXPECT_EQ((*topped)["universe"], Json(0x205));
}

TEST(DataArtNet, APollIsTheAskingAndNothingElse) {
  // A poll asks who is out there. What it carries past its version is
  // what its sender wants told back and how urgent a diagnostic has to
  // be to be worth sending, neither of which a scene acts on.
  const std::vector<std::byte> asked = bytesOf(
      {'A', 'r', 't', '-', 'N', 'e', 't', 0, 0x00, 0x20, 0x00, 0x0E, 0, 0});
  const std::optional<Json> value = decode(asked, Dialect::ArtNet);
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(*value, Json(Json::Object{{"kind", Json("Poll")}}));
  EXPECT_EQ(encode(*value, Dialect::ArtNet), asked);
}

TEST(DataArtNet, APacketWithNoReadingIsCarriedWholeAndGoesBackOutTheSame) {
  // A poll's ANSWER is a node's name, its addresses and what it can do,
  // in a layout this has no reading for — and it carries no version at
  // all, the code standing where every other packet's version does.
  const std::vector<std::byte> answered =
      bytesOf({'A', 'r', 't', '-', 'N', 'e', 't', 0, 0x00, 0x21, 2, 0, 0, 1});
  const std::optional<Json> reply = decode(answered, Dialect::ArtNet);
  ASSERT_TRUE(reply.has_value());
  EXPECT_EQ((*reply)["kind"].string(), "PollReply");
  EXPECT_EQ((*reply)["bytes"].size(), answered.size());
  EXPECT_EQ(encode(*reply, Dialect::ArtNet), answered);

  // Every other code says which one it was, so a reader that knows it
  // reads the bytes under it.
  const std::vector<std::byte> synchronised = bytesOf(
      {'A', 'r', 't', '-', 'N', 'e', 't', 0, 0x00, 0x52, 0x00, 0x0E, 0, 0});
  const std::optional<Json> sync = decode(synchronised, Dialect::ArtNet);
  ASSERT_TRUE(sync.has_value());
  EXPECT_EQ((*sync)["kind"].string(), "ArtNet");
  EXPECT_EQ((*sync)["opcode"], Json(0x5200));
  EXPECT_EQ(encode(*sync, Dialect::ArtNet), synchronised);
}

TEST(DataArtNet, BytesThatAreNoPacketReadAsNothing) {
  // Not this wire's name at all, which is the one mark that tells a
  // lighting packet from whatever else reached the port.
  EXPECT_FALSE(decode(deskWith(0, {'B'}), Dialect::ArtNet).has_value());
  // Too few bytes to say even what it is.
  EXPECT_FALSE(
      decode(bytesOf({'A', 'r', 't', '-', 'N', 'e', 't', 0, 0x00}), Dialect::ArtNet)
          .has_value());
  // A count reaching past what arrived: every length on the wire is a
  // claim the bytes make about themselves, and this one they do not
  // hold.
  EXPECT_FALSE(decode(deskWith(16, {0x02, 0x00}), Dialect::ArtNet).has_value());
  // A count the wire cannot mean: dimmers are counted in pairs, and
  // there is no packet of none.
  EXPECT_FALSE(decode(deskWith(16, {0x00, 0x03}), Dialect::ArtNet).has_value());
  EXPECT_FALSE(decode(deskWith(16, {0x00, 0x00}), Dialect::ArtNet).has_value());
  // A sender writing an older version of the protocol writes other
  // fields where these are read.
  EXPECT_FALSE(decode(deskWith(10, {0x00, 0x0D}), Dialect::ArtNet).has_value());
  // A poll too short to be one.
  EXPECT_FALSE(decode(bytesOf({'A', 'r', 't', '-', 'N', 'e', 't', 0, 0x00,
                                     0x20, 0x00, 0x0E}), Dialect::ArtNet)
                   .has_value());
}

TEST(DataArtNet, ADimmerListGoesOutInThePairsTheWireCounts) {
  // Three levels for one fixture go out as four, the fourth at nothing:
  // the wire counts its dimmers in pairs.
  EXPECT_EQ(
      encode(Json(Json::Object{{"kind", Json("Dmx")},
                                     {"sequence", Json(7)},
                                     {"channels", listOf({255, 128, 0})}}), Dialect::ArtNet),
      bytesOf(deskWire));

  // A sequence and a physical input nobody named stand at 0, which is
  // what a sender that does not count its packets writes. And a level
  // outside what a dimmer holds is written at the nearer end of it,
  // since wrapping it would put a fixture at nothing that was meant to
  // stand at full.
  EXPECT_EQ(encode(Json(Json::Object{{"kind", Json("Dmx")},
                                           {"channels", listOf({300, -20})}}), Dialect::ArtNet),
            bytesOf({'A',  'r',  't', '-', 'N',  'e',  't',  0,    0x00, 0x50,
                     0x00, 0x0E, 0,   0,   0x00, 0x00, 0x00, 0x02, 255,  0}));

  // A universe of dimmers is the most one packet carries, so a list
  // longer than one stops at the end of it.
  Json::Array many;
  for (int at = 0; at != 600; ++at) many.push_back(Json(at % 256));
  const std::vector<std::byte> full = encode(Json(Json::Object{
      {"kind", Json("Dmx")}, {"channels", Json(std::move(many))}}), Dialect::ArtNet);
  ASSERT_EQ(full.size(), 18u + 512u);
  EXPECT_EQ(std::to_integer<int>(full[16]), 0x02);
  EXPECT_EQ(std::to_integer<int>(full[17]), 0x00);
}

TEST(DataArtNet, AValueWithNoSpellingOnTheWireWritesNoBytes) {
  // A kind this has no reading for and no bytes of its own: an empty
  // datagram is no dimmer at all rather than a shorter message.
  EXPECT_TRUE(
      encode(Json(Json::Object{{"kind", Json("Blackout")}}), Dialect::ArtNet).empty());
  EXPECT_TRUE(encode(Json(), Dialect::ArtNet).empty());
  EXPECT_TRUE(
      encode(Json(Json::Object{{"address", Json("/sky/wind")}}), Dialect::ArtNet).empty());
}

}  // namespace
