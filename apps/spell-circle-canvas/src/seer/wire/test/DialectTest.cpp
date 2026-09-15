/** @file
 * The dialects a wire speaks: what a MIDI message and a universe of
 * dimmers read as, the messages a form spells for a peer that speaks
 * one of them, and the word a wire says before anything has arrived on
 * it.
 *
 * The readings and the spellings are judged against the codecs' own
 * bytes rather than against a message written out here. A tool that
 * agreed with a fixture and not with what an instrument writes would be
 * a tool reading its own idea of the protocol, which is the one thing a
 * tool for looking at somebody else's wire may not do.
 */

#include <gtest/gtest.h>
#include <sigildata/decode/ArtNet.h>
#include <sigildata/decode/Json.h>
#include <sigildata/decode/Midi.h>
#include <sigilseer/wire/Rendering.h>
#include <sigilseer/wire/Sender.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using sigil::data::Json;
using sigil::io::Bytes;

namespace {

Bytes bytesOf(std::string_view text) {
  const auto* const first = reinterpret_cast<const std::byte*>(text.data());
  Bytes out;
  out.bytes.assign(first, first + text.size());
  return out;
}

/** The bytes of @p message as a message off a wire. */
Bytes messageOf(std::vector<std::byte> message) {
  Bytes out;
  out.bytes = std::move(message);
  return out;
}

/** A note struck on a channel, as the codec writes it. */
Json noteOn(int channel, int note, int velocity) {
  return Json(Json::Object{{"kind", Json("NoteOn")},
                           {"channel", Json(channel)},
                           {"note", Json(note)},
                           {"velocity", Json(velocity)}});
}

/** A universe of @p levels, as the codec writes it. */
Json universe(int address, const std::vector<int>& levels) {
  Json::Array listed;
  for (const int level : levels) listed.push_back(Json(level));
  return Json(Json::Object{{"kind", Json("Dmx")},
                           {"universe", Json(address)},
                           {"channels", Json(std::move(listed))}});
}

}  // namespace

TEST(SeerMidi, AMessageIsReadAsItsKindItsChannelAndWhatThatKindCarries) {
  const Bytes played = messageOf(sigil::data::encodeMidi(noteOn(1, 60, 100)));
  // The fields the codec names, in the codec's own order, and the
  // message's own bytes across one line rather than down a column: a run
  // along a cable is counted through and not compared member by member.
  EXPECT_EQ(sigil::seer::midiReading(played),
            "{\n"
            "  \"kind\": \"NoteOn\",\n"
            "  \"channel\": 1,\n"
            "  \"note\": 60,\n"
            "  \"velocity\": 100,\n"
            "  \"status\": 144,\n"
            "  \"data\": [60, 100]\n"
            "}");

  // A knob on the sixteenth channel, which is the number every desk in
  // the world prints rather than the one on the wire.
  const Bytes turned = messageOf(
      sigil::data::encodeMidi(Json(Json::Object{{"kind", Json("ControlChange")},
                                                {"channel", Json(16)},
                                                {"controller", Json(74)},
                                                {"value", Json(32)}})));
  EXPECT_EQ(sigil::seer::midiReading(turned),
            "{\n"
            "  \"kind\": \"ControlChange\",\n"
            "  \"channel\": 16,\n"
            "  \"controller\": 74,\n"
            "  \"value\": 32,\n"
            "  \"status\": 191,\n"
            "  \"data\": [74, 32]\n"
            "}");
}

TEST(SeerMidi, ADocumentIsNoMidiMessageAndTheReadingSaysSoByBeingEmpty) {
  // Each reading is empty exactly when the message is not that, which is
  // how the pane says what a message is not. A document opens with a
  // byte no message on the wire can open with.
  const Bytes document = bytesOf(R"({"live": true})");
  EXPECT_TRUE(sigil::seer::midiReading(document).empty());
  EXPECT_FALSE(sigil::seer::indentedJson(document).empty());

  EXPECT_TRUE(sigil::seer::midiReading(bytesOf("")).empty());
  EXPECT_TRUE(sigil::seer::midiReading(bytesOf("a scene arrives")).empty());
}

TEST(SeerDmx, AUniverseIsReadAsItsAddressAndItsLevelsSixteenToARow) {
  // A rig of one three-colour fixture, the wire's own pair of dimmers
  // behind it: a run that fits a row stands whole beside its key.
  const Bytes small =
      messageOf(sigil::data::encodeArtNet(universe(0, {255, 128, 0})));
  EXPECT_EQ(sigil::seer::dmxReading(small),
            "{\n"
            "  \"kind\": \"Dmx\",\n"
            "  \"universe\": 0,\n"
            "  \"sequence\": 0,\n"
            "  \"physical\": 0,\n"
            "  \"channels\": [255, 128, 0, 0]\n"
            "}");

  const std::vector<int> levels{255, 128, 0,   0,   16,  32,  48,  64,
                                80,  96,  112, 128, 144, 160, 176, 192};
  const Bytes lit = messageOf(sigil::data::encodeArtNet(universe(0, levels)));
  EXPECT_EQ(sigil::seer::dmxReading(lit),
            "{\n"
            "  \"kind\": \"Dmx\",\n"
            "  \"universe\": 0,\n"
            "  \"sequence\": 0,\n"
            "  \"physical\": 0,\n"
            "  \"channels\": [255, 128, 0, 0, 16, 32, 48, 64, 80, 96, 112, "
            "128, 144, 160, 176, 192]\n"
            "}");

  // A universe past the sixteenth channel is rows of sixteen, so a
  // reader counts a level's place off the row it stands in rather than
  // down four hundred lines.
  std::vector<int> wider = levels;
  wider.push_back(200);
  wider.push_back(255);
  const Bytes more = messageOf(sigil::data::encodeArtNet(universe(3, wider)));
  EXPECT_EQ(sigil::seer::dmxReading(more),
            "{\n"
            "  \"kind\": \"Dmx\",\n"
            "  \"universe\": 3,\n"
            "  \"sequence\": 0,\n"
            "  \"physical\": 0,\n"
            "  \"channels\": [\n"
            "    255, 128, 0, 0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, "
            "176, 192,\n"
            "    200, 255\n"
            "  ]\n"
            "}");
}

TEST(SeerDmx, APacketIsNoDocumentAndADocumentIsNoPacket) {
  const Bytes document = bytesOf(R"({"kind": "Dmx"})");
  EXPECT_TRUE(sigil::seer::dmxReading(document).empty());

  const Bytes lit = messageOf(sigil::data::encodeArtNet(universe(0, {255})));
  EXPECT_FALSE(sigil::seer::dmxReading(lit).empty());
  EXPECT_TRUE(sigil::seer::indentedJson(lit).empty());

  EXPECT_TRUE(sigil::seer::dmxReading(bytesOf("")).empty());
}

TEST(SeerMidi, AMessageIsSpelledFromItsKindItsChannelAndItsNumbers) {
  EXPECT_EQ(sigil::seer::midiMessage("NoteOn", 1, 60, 100).bytes,
            sigil::data::encodeMidi(noteOn(1, 60, 100)));
  EXPECT_EQ(
      sigil::seer::midiMessage("ControlChange", 16, 74, 32).bytes,
      sigil::data::encodeMidi(Json(Json::Object{{"kind", Json("ControlChange")},
                                                {"channel", Json(16)},
                                                {"controller", Json(74)},
                                                {"value", Json(32)}})));

  // A kind that takes one number takes the first and leaves the second
  // off the wire, so what a form shows for it is one field.
  EXPECT_EQ(
      sigil::seer::midiMessage("ProgramChange", 2, 7, 99).bytes,
      sigil::data::encodeMidi(Json(Json::Object{{"kind", Json("ProgramChange")},
                                                {"channel", Json(2)},
                                                {"program", Json(7)}})));
  EXPECT_EQ(
      sigil::seer::midiMessage("PitchBend", 1, -8192, 0).bytes,
      sigil::data::encodeMidi(Json(Json::Object{{"kind", Json("PitchBend")},
                                                {"channel", Json(1)},
                                                {"bend", Json(-8192)}})));

  // And what a reader spells goes back down the wire as what it says: a
  // message spelled and then read is the message that was meant.
  const Bytes played =
      messageOf(sigil::seer::midiMessage("NoteOff", 3, 60, 40).bytes);
  const std::optional<Json> read = sigil::data::decodeMidi(played.bytes);
  ASSERT_TRUE(read.has_value());
  EXPECT_EQ((*read)["kind"].text(), "NoteOff");
  EXPECT_EQ((*read)["channel"], Json(3));
  EXPECT_EQ((*read)["note"], Json(60));
  EXPECT_EQ((*read)["velocity"], Json(40));

  // A kind the wire has no status byte for is no message at all.
  EXPECT_TRUE(sigil::seer::midiMessage("Applause", 1, 60, 100).bytes.empty());
  EXPECT_TRUE(sigil::seer::midiMessage("", 1, 0, 0).bytes.empty());
}

TEST(SeerDmx, AUniverseIsSpelledFromItsAddressAndItsLevelsAsADocument) {
  EXPECT_EQ(sigil::seer::dmxMessage(3, "[255, 128, 0]").bytes,
            sigil::data::encodeArtNet(universe(3, {255, 128, 0})));

  // A list with nothing in it is every fixture dark, which a desk says
  // and a reader may mean.
  EXPECT_EQ(sigil::seer::dmxMessage(0, "[]").bytes,
            sigil::data::encodeArtNet(universe(0, {})));

  // Levels that are not a list are no universe: one number where a list
  // was meant would light one fixture and darken the rig.
  EXPECT_TRUE(sigil::seer::dmxMessage(0, "255, 128").bytes.empty());
  EXPECT_TRUE(sigil::seer::dmxMessage(0, "255").bytes.empty());
  EXPECT_TRUE(sigil::seer::dmxMessage(0, R"({"1": 255})").bytes.empty());
  EXPECT_TRUE(sigil::seer::dmxMessage(0, "").bytes.empty());
}

TEST(SeerDialect, AWireSaysWhatItSpeaksBeforeAnythingArrivesOnIt) {
  // A reader picks the wire to watch before a message has come down any
  // of them, so the word is read off the URI and off nothing else.
  EXPECT_EQ(sigil::seer::dialect("osc://:27050"), "osc");
  EXPECT_EQ(sigil::seer::dialect("midi://in/virtual:seer"), "midi");
  EXPECT_EQ(sigil::seer::dialect("artnet://:6454"), "dmx");
  EXPECT_EQ(sigil::seer::dialect("serial:///dev/tty.usbmodem1101?baud=115200"),
            "lines");
  EXPECT_EQ(sigil::seer::dialect("ws://:27060/sky"), "json");
  EXPECT_EQ(sigil::seer::dialect("wss://sky.example:443/feed"), "json");
  // Two schemes carry whatever a sender writes, and say so rather than
  // naming a format they do not hold the sender to.
  EXPECT_EQ(sigil::seer::dialect("udp://:27020"), "bytes");
  EXPECT_EQ(sigil::seer::dialect("shm://sky"), "bytes");

  // A scheme there is no word for claims none: the wire opens and
  // carries messages all the same, and a word invented for it would be
  // a claim about a wire that does not exist.
  EXPECT_TRUE(sigil::seer::dialect("pigeon://the.desk").empty());
  EXPECT_TRUE(sigil::seer::dialect("the.desk").empty());
  EXPECT_TRUE(sigil::seer::dialect("").empty());
}
