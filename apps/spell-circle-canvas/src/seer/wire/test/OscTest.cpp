/** @file
 * OSC on a wire: the packet a reading shows, and the message a form
 * spells for a peer that speaks it.
 *
 * Both are judged against the codec's own bytes rather than against a
 * packet written out here. A reading that agreed with a fixture and not
 * with what a sender writes would be a tool that reads its own idea of
 * the protocol, which is the one thing a tool for looking at somebody
 * else's wire may not do.
 */

#include <gtest/gtest.h>
#include <sigildata/decode/Json.h>
#include <sigildata/decode/Dialect.h>
#include <sigilseer/wire/Rendering.h>
#include <sigilseer/wire/Sender.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

using sigil::io::Bytes;

namespace {

Bytes bytesOf(std::string_view text) {
  const auto* const first = reinterpret_cast<const std::byte*>(text.data());
  return Bytes(std::span(first, text.size()));
}

/** The bytes of @p packet as a message off a wire. */
Bytes messageOf(std::vector<std::byte> packet) {
  return Bytes(std::move(packet));
}

}  // namespace

TEST(SeerOsc, APacketIsReadAsTheAddressAndTheArgumentsUnderIt) {
  const Bytes packet = messageOf(sigil::data::encode(sigil::data::oscMessage("/sky/wind", sigil::data::Json::Array{0.5, std::string("gust")}), sigil::data::Dialect::Osc));
  EXPECT_EQ(sigil::seer::oscReading(packet),
            "{\n"
            "  \"address\": \"/sky/wind\",\n"
            "  \"arguments\": [\n"
            "    0.5,\n"
            "    \"gust\"\n"
            "  ]\n"
            "}");
}

TEST(SeerOsc, ADocumentIsNoPacketAndAPacketIsNoDocument) {
  // Each reading is empty exactly when the message is not that, so the
  // pair of them says which of the two a wire is carrying.
  const Bytes document = bytesOf(R"({"live": true})");
  EXPECT_TRUE(sigil::seer::oscReading(document).empty());
  EXPECT_FALSE(sigil::seer::indentedJson(document).empty());

  const Bytes packet = messageOf(
      sigil::data::encode(sigil::data::oscMessage("/sky/wind", sigil::data::Json::Array{}), sigil::data::Dialect::Osc));
  EXPECT_FALSE(sigil::seer::oscReading(packet).empty());
  EXPECT_TRUE(sigil::seer::indentedJson(packet).empty());

  EXPECT_TRUE(sigil::seer::oscReading(bytesOf("")).empty());
}

TEST(SeerOsc, AMessageIsSpelledFromAnAddressAndTheArgumentsAsADocument) {
  EXPECT_EQ(sigil::seer::oscMessage("/sky/wind", R"([0.5, "gust"])"),
            sigil::io::Bytes(sigil::data::encode(sigil::data::oscMessage("/sky/wind", sigil::data::Json::Array{
                                                    0.5, std::string("gust")}), sigil::data::Dialect::Osc)));

  // An argument that is not a list is the one argument it is, and an
  // editor with nothing in it is a message carrying none.
  EXPECT_EQ(sigil::seer::oscMessage("/sky/wind", "0.5"),
            sigil::io::Bytes(sigil::data::encode(sigil::data::oscMessage("/sky/wind", sigil::data::Json(0.5)), sigil::data::Dialect::Osc)));
  EXPECT_EQ(sigil::seer::oscMessage("/sky/wind", "  \n"),
            sigil::io::Bytes(sigil::data::encode(sigil::data::oscMessage("/sky/wind", sigil::data::Json::Array{}), sigil::data::Dialect::Osc)));

  // Arguments that are no document, and a message with no address, are
  // no message: half a message spelled is not a shorter one.
  EXPECT_TRUE(sigil::seer::oscMessage("/sky/wind", "gust").empty());
  EXPECT_TRUE(sigil::seer::oscMessage("", "[0.5]").empty());
}
