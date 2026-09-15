/** The OSC codec: what a packet off the wire reads as, what a value
 *  goes back out as, and the resource name that says which of the two
 *  readings a hub's bytes get.
 *
 *  A codec that stands on its own is judged against the wire and
 *  nothing else, so every packet here is spelled out byte by byte. */

#include <gtest/gtest.h>
#include <sigildata/decode/Decoders.h>
#include <sigildata/decode/Osc.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

using namespace sigil::data;
using sigil::io::Bytes;

namespace {

/** The packet a desk sends when one fader moves, byte for byte: the
 *  address and a null padded out to four, the type tag string ",f"
 *  padded the same way, and the float's four bytes most significant
 *  first. Every other fixture is built from the pieces below, and this
 *  one is what says those pieces put bytes down in the order the wire
 *  wants them. */
constexpr unsigned char deskWire[] = {'/', 's', 'k',  'y',  '/',  'w', 'i',
                                      'n', 'd', 0,    0,    0,    ',', 'f',
                                      0,   0,   0x3F, 0x00, 0x00, 0x00};

/** A packet under construction: every number written most significant
 *  byte first, every string padded with nulls to a multiple of four. */
struct Wire {
  std::vector<std::byte> bytes;

  Wire& put(unsigned value) {
    bytes.push_back(std::byte{static_cast<unsigned char>(value & 0xFFu)});
    return *this;
  }
  Wire& word(uint32_t value) {
    return put(value >> 24).put(value >> 16).put(value >> 8).put(value);
  }
  Wire& giant(uint64_t value) {
    return word(static_cast<uint32_t>(value >> 32))
        .word(static_cast<uint32_t>(value));
  }
  Wire& real(float value) { return word(std::bit_cast<uint32_t>(value)); }
  Wire& real(double value) { return giant(std::bit_cast<uint64_t>(value)); }
  Wire& text(std::string_view value) {
    for (const char letter : value) put(static_cast<unsigned char>(letter));
    do {
      put(0);
    } while (bytes.size() % 4 != 0);
    return *this;
  }
  Wire& blob(std::initializer_list<unsigned> values) {
    word(static_cast<uint32_t>(values.size()));
    for (const unsigned value : values) put(value);
    while (bytes.size() % 4 != 0) put(0);
    return *this;
  }
};

/** One bundle: the mark it is named by, the time its elements are for,
 *  and every element behind its own length. */
std::vector<std::byte> bundleOf(
    uint64_t timeTag, std::initializer_list<std::vector<std::byte>> elements) {
  Wire wire;
  wire.text("#bundle").giant(timeTag);
  for (const std::vector<std::byte>& element : elements) {
    wire.word(static_cast<uint32_t>(element.size()));
    wire.bytes.insert(wire.bytes.end(), element.begin(), element.end());
  }
  return wire.bytes;
}

/** A run of bytes spelled out as numbers. */
template <size_t N>
std::vector<std::byte> bytesOf(const unsigned char (&wire)[N]) {
  std::vector<std::byte> packet;
  packet.reserve(N);
  for (const unsigned char value : wire) packet.push_back(std::byte{value});
  return packet;
}

/** @p text as the bytes a hub would hand a decoder. */
Bytes bytesOfText(std::string_view text) {
  Bytes bytes;
  for (const char value : text)
    bytes.bytes.push_back(static_cast<std::byte>(value));
  return bytes;
}

TEST(DataOsc, AMessageIsItsAddressAndAnArgumentOfEveryTypeTheWireCarries) {
  Wire wire;
  wire.text("/sky/every").text(",ihfdsSTFNIbtcrm[ii]");
  wire.word(0xFFFFFFF9u);            // a 32-bit whole number, negative
  wire.giant(uint64_t{5000000000});  // a 64-bit whole number
  wire.real(0.5f);
  wire.real(2.5);
  wire.text("gust");
  wire.text("sym");
  // True, false, nil and impulse carry no content at all.
  wire.blob({0x01, 0x02, 0xFF});
  wire.giant(uint64_t{3600} << 32);  // a timetag, an hour past the era
  wire.word('A');
  wire.word(0x10203040);  // a colour
  wire.word(0x01903C40);  // a MIDI message
  wire.word(1).word(2);   // the two members of the array

  const std::optional<Json> value = decodeOsc(wire.bytes);
  ASSERT_TRUE(value);
  EXPECT_EQ("/sky/every", (*value)["address"].text());
  const Json& arguments = (*value)["arguments"];
  ASSERT_EQ(16u, arguments.size());

  // Every number the wire carries is a number, whatever width it
  // arrived at, because a number is what a drawing asks it for.
  EXPECT_DOUBLE_EQ(-7.0, arguments[size_t{0}].number());
  EXPECT_DOUBLE_EQ(5000000000.0, arguments[size_t{1}].number());
  EXPECT_DOUBLE_EQ(0.5, arguments[size_t{2}].number());
  EXPECT_DOUBLE_EQ(2.5, arguments[size_t{3}].number());
  // A string and a symbol are both text.
  EXPECT_EQ("gust", arguments[size_t{4}].text());
  EXPECT_EQ("sym", arguments[size_t{5}].text());
  EXPECT_EQ(Json(true), arguments[size_t{6}]);
  EXPECT_EQ(Json(false), arguments[size_t{7}]);
  EXPECT_TRUE(arguments[size_t{8}].null());
  // An impulse carries nothing but the fact that it arrived.
  EXPECT_EQ(Json(true), arguments[size_t{9}]);
  // The three arguments that are bytes with meanings of their own keep
  // them, in the order the wire writes them.
  EXPECT_EQ(Json(Json::Object{
                {"blob", Json(Json::Array{Json(1), Json(2), Json(255)})}}),
            arguments[size_t{10}]);
  // A timetag is seconds since the era the protocol counts from.
  EXPECT_DOUBLE_EQ(3600.0, arguments[size_t{11}].number());
  EXPECT_DOUBLE_EQ(65.0, arguments[size_t{12}].number());
  EXPECT_EQ(Json(Json::Object{{"rgba", Json(Json::Array{Json(16), Json(32),
                                                        Json(48), Json(64)})}}),
            arguments[size_t{13}]);
  EXPECT_EQ(Json(Json::Object{{"midi", Json(Json::Array{Json(1), Json(144),
                                                        Json(60), Json(64)})}}),
            arguments[size_t{14}]);
  // Arguments the sender bracketed are a list of their own.
  EXPECT_EQ(Json(Json::Array{Json(1), Json(2)}), arguments[size_t{15}]);
}

TEST(DataOsc, ABundleIsItsElementsAndTheTimeTheyAreFor) {
  const std::vector<std::byte> wind =
      Wire().text("/sky/wind").text(",f").real(0.5f).bytes;
  const std::vector<std::byte> rain =
      Wire().text("/sky/rain").text(",i").word(3).bytes;

  const std::optional<Json> value =
      decodeOsc(bundleOf(uint64_t{3600} << 32, {wind, rain}));
  ASSERT_TRUE(value);
  const Json& elements = (*value)["bundle"];
  ASSERT_EQ(2u, elements.size());
  EXPECT_EQ("/sky/wind", elements[size_t{0}]["address"].text());
  EXPECT_DOUBLE_EQ(0.5, elements[size_t{0}]["arguments"][size_t{0}].number());
  EXPECT_EQ("/sky/rain", elements[size_t{1}]["address"].text());
  EXPECT_DOUBLE_EQ(3.0, elements[size_t{1}]["arguments"][size_t{0}].number());
  EXPECT_DOUBLE_EQ(3600.0, (*value)["at"].number());

  // A bundle for now carries the one timetag that is a word rather than
  // a time, and reads as no time at all.
  const std::vector<std::byte> bare = Wire().text("/sky/wind").text(",").bytes;
  const std::optional<Json> immediate = decodeOsc(bundleOf(1, {bare}));
  ASSERT_TRUE(immediate);
  EXPECT_TRUE((*immediate)["at"].null());
  ASSERT_EQ(1u, (*immediate)["bundle"].size());
  EXPECT_EQ(0u, (*immediate)["bundle"][size_t{0}]["arguments"].size());

  // A bundle inside a bundle is nested where it stood, with a time of
  // its own.
  const std::optional<Json> inside =
      decodeOsc(bundleOf(1, {bundleOf(uint64_t{7200} << 32, {bare})}));
  ASSERT_TRUE(inside);
  ASSERT_EQ(1u, (*inside)["bundle"].size());
  const Json& within = (*inside)["bundle"][size_t{0}];
  EXPECT_DOUBLE_EQ(7200.0, within["at"].number());
  EXPECT_EQ("/sky/wind", within["bundle"][size_t{0}]["address"].text());
}

TEST(DataOsc, BytesThatAreNotAPacketAreReadAsNothing) {
  EXPECT_FALSE(decodeOsc(std::span<const std::byte>{}));

  // A document is not a packet however long it is: its address pattern
  // never ends.
  const unsigned char document[] = {'{', '"', 'a', '"', ':', ' ',
                                    '1', '}', ' ', ' ', ' ', ' '};
  EXPECT_FALSE(decodeOsc(bytesOf(document)));

  // A packet's length is a multiple of four, and a row of fields is
  // not.
  const unsigned char row[] = {'m', 'o', 'n', 't', 'h', ',', 'v'};
  EXPECT_FALSE(decodeOsc(bytesOf(row)));

  // A packet cut short has a type tag with no argument under it.
  const std::vector<std::byte> whole = bytesOf(deskWire);
  EXPECT_FALSE(decodeOsc(std::span<const std::byte>(whole).first(16)));

  // An address pattern opens with a slash, which is what tells a
  // message from a run of bytes that happens to hold a null.
  EXPECT_FALSE(decodeOsc(Wire().text("sky").text(",").bytes));

  // And a type tag string opens with a comma.
  EXPECT_FALSE(decodeOsc(Wire().text("/sky").text("f").real(0.5f).bytes));

  // A tag this cannot measure leaves every argument after it standing
  // somewhere unknown, so the message is not read at all.
  EXPECT_FALSE(decodeOsc(Wire().text("/sky").text(",zf").real(0.5f).bytes));
}

TEST(DataOsc, APacketClaimingMoreThanItHoldsIsReadAsNothing) {
  // A blob's length is the packet's own claim about itself.
  EXPECT_FALSE(decodeOsc(Wire().text("/b").text(",b").word(64).word(0).bytes));

  // So is a bundle element's, which the protocol also aligns to four.
  EXPECT_FALSE(
      decodeOsc(Wire().text("#bundle").giant(1).word(64).word(0).bytes));
  EXPECT_FALSE(
      decodeOsc(Wire().text("#bundle").giant(1).word(3).word(0).bytes));

  // And so is the null a string argument ends at.
  EXPECT_FALSE(decodeOsc(Wire().text("/s").text(",s").word(0x61626364).bytes));

  // A bundle with no room for its own timetag is no bundle.
  EXPECT_FALSE(decodeOsc(Wire().text("#bundle").word(0).bytes));
}

TEST(DataOsc, WritingTheFormAPacketReadsAsAnswersThatPacketBack) {
  // Every type a value writes as a member of its own kind, so what goes
  // out is what came in: a float, a string, the three arguments that
  // carry no content, a blob, a colour, a MIDI message and an array.
  Wire wire;
  wire.text("/sky/wind").text(",fsTFNbrm[ff]");
  wire.real(0.5f).text("gust");
  wire.blob({0x01, 0x02, 0xFF});
  wire.word(0x10203040).word(0x01903C40);
  wire.real(1.0f).real(-2.25f);

  const std::optional<Json> value = decodeOsc(wire.bytes);
  ASSERT_TRUE(value);
  EXPECT_EQ(wire.bytes, encodeOsc(*value));
  EXPECT_EQ(wire.bytes, encodeOsc("/sky/wind", (*value)["arguments"]));

  // And the other way round, which is what a reply down the same wire
  // is: the value goes out and the same value comes back.
  const std::optional<Json> again = decodeOsc(encodeOsc(*value));
  ASSERT_TRUE(again);
  EXPECT_EQ(*value, *again);

  // A message with no arguments member is a message with no arguments.
  EXPECT_EQ(Wire().text("/sky/wind").text(",").bytes,
            encodeOsc(Json(Json::Object{{"address", Json("/sky/wind")}})));

  // A record with none of the five keys has no type on the wire and
  // keeps its place as nil, so a reader's positions do not shift.
  EXPECT_EQ(Wire().text("/n").text(",N").bytes,
            encodeOsc("/n", Json(Json::Object{{"shape", Json("round")}})));

  // A value with no address is addressed to nothing and goes nowhere.
  EXPECT_TRUE(
      encodeOsc(Json(Json::Object{{"arguments", Json(Json::Array{Json(1)})}}))
          .empty());
  EXPECT_TRUE(encodeOsc("sky", Json(1)).empty());
}

TEST(DataOsc, ANumberIsAFloatUnlessARecordAsksForAnotherWidth) {
  // A number carries no memory of the width it arrived at, and what a
  // performance tool expects under a continuous control is a float, so
  // seven goes out as one.
  EXPECT_EQ(Wire().text("/n").text(",f").word(0x40E00000).bytes,
            encodeOsc("/n", Json(7)));

  // The two records that ask for another width by name.
  EXPECT_EQ(Wire().text("/n").text(",i").word(7).bytes,
            encodeOsc("/n", Json(Json::Object{{"int", Json(7)}})));
  EXPECT_EQ(Wire().text("/n").text(",d").real(1e10).bytes,
            encodeOsc("/n", Json(Json::Object{{"double", Json(1e10)}})));

  // A number outside what its place on the wire holds is written at the
  // nearer end of it rather than wrapped around to the other.
  EXPECT_EQ(Wire().text("/n").text(",i").word(0x7FFFFFFF).bytes,
            encodeOsc("/n", Json(Json::Object{{"int", Json(1e18)}})));

  // And this is the round trip the rule gives up: a whole number that
  // arrived as one reads as a plain number and goes back out a float.
  const std::optional<Json> read =
      decodeOsc(Wire().text("/n").text(",i").word(7).bytes);
  ASSERT_TRUE(read);
  EXPECT_DOUBLE_EQ(7.0, (*read)["arguments"][size_t{0}].number());
  EXPECT_EQ(Wire().text("/n").text(",f").word(0x40E00000).bytes,
            encodeOsc(*read));
}

TEST(DataOsc, APacketSpelledTheWayADeskSendsOneIsItsAddressAndItsFloat) {
  const std::vector<std::byte> packet = bytesOf(deskWire);
  const std::optional<Json> value = decodeOsc(packet);
  ASSERT_TRUE(value);
  EXPECT_EQ("/sky/wind", (*value)["address"].text());
  ASSERT_EQ(1u, (*value)["arguments"].size());
  EXPECT_DOUBLE_EQ(0.5, (*value)["arguments"][size_t{0}].number());
  // And the same bytes go back out, which is what a reply is.
  EXPECT_EQ(packet, encodeOsc(*value));
}

TEST(DataOsc, TheNameAResourceCarriesSaysWhetherItsBytesAreOscOrJson) {
  const JsonDecoder decoder;
  const Bytes packet{bytesOf(deskWire)};
  const Bytes document = bytesOfText(R"({"root": {"depth": 4}})");

  const std::optional<Json> fromDesk =
      decoder.decode(packet, "osc://desk:9000");
  ASSERT_TRUE(fromDesk);
  EXPECT_EQ("/sky/wind", (*fromDesk)["address"].text());

  // The same decoder and the same type: a name that is a file still
  // reads text, exactly as it did before a desk had a name of its own.
  const std::optional<Json> fromFile =
      decoder.decode(document, "res://data.json");
  ASSERT_TRUE(fromFile);
  EXPECT_DOUBLE_EQ(4.0, (*fromFile)["root"]["depth"].number());

  // A recorded packet is filed under its own extension.
  EXPECT_TRUE(decoder.decode(packet, "res://take.osc"));

  // And a name is a claim about the bytes, not a reading of them: text
  // under a desk's name is no packet.
  EXPECT_FALSE(decoder.decode(document, "osc://desk:9000"));
}

}  // namespace
