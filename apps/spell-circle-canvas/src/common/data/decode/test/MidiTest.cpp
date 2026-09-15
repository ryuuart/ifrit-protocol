/** The MIDI codec: what a message off the wire reads as, what a value
 *  goes back out as, and what is no message at all.
 *
 *  A codec that stands on its own is judged against the wire and
 *  nothing else, so every message here is spelled out byte by byte, and
 *  every kind is read and then written back to the bytes it was read
 *  from.
 */

#include <gtest/gtest.h>
#include <sigildata/decode/Midi.h>

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

using namespace sigil::data;

namespace {

/** A message spelled out byte by byte. */
std::vector<std::byte> wireOf(std::initializer_list<int> bytes) {
  std::vector<std::byte> message;
  message.reserve(bytes.size());
  for (const int one : bytes) message.push_back(static_cast<std::byte>(one));
  return message;
}

/** The list of numbers a `data` or `bytes` member is. */
Json listOf(std::initializer_list<int> numbers) {
  Json::Array listed;
  for (const int one : numbers) listed.push_back(Json(one));
  return Json(std::move(listed));
}

/** What @p wire reads as, and the bytes that reading goes back out as:
 *  every kind is read and written in one breath, because a codec that
 *  reads what it cannot write is half a codec. */
Json readAndWritten(std::initializer_list<int> wire) {
  const std::vector<std::byte> bytes = wireOf(wire);
  const std::optional<Json> value = decodeMidi(bytes);
  EXPECT_TRUE(value.has_value());
  if (!value) return Json();
  EXPECT_EQ(encodeMidi(*value), bytes);
  return *value;
}

TEST(DataMidi, ANoteOnIsItsChannelItsNoteAndItsVelocity) {
  // Middle C on the first channel, struck hard. The whole record is
  // compared, because what the fields are CALLED is what a scene reads
  // and is as much the codec's promise as the numbers are.
  EXPECT_EQ(readAndWritten({0x90, 60, 100}),
            Json(Json::Object{{"kind", Json("NoteOn")},
                              {"channel", Json(1)},
                              {"note", Json(60)},
                              {"velocity", Json(100)},
                              {"status", Json(0x90)},
                              {"data", listOf({60, 100})}}));

  // The channel is the low half of the status byte, counted the way
  // every desk in the world prints it: 1 to 16, not 0 to 15.
  EXPECT_EQ(readAndWritten({0x9F, 36, 127})["channel"], Json(16));
}

TEST(DataMidi, EveryOtherKindPlayedOnAChannelReadsAsItsOwnFields) {
  const Json off = readAndWritten({0x82, 60, 40});
  EXPECT_EQ(off["kind"].text(), "NoteOff");
  EXPECT_EQ(off["channel"], Json(3));
  EXPECT_EQ(off["note"], Json(60));
  EXPECT_EQ(off["velocity"], Json(40));

  // One key leaned on, and the whole keyboard leaned on: the same word
  // for two different messages, so each says which it is by its name.
  const Json key = readAndWritten({0xA0, 60, 90});
  EXPECT_EQ(key["kind"].text(), "PolyAftertouch");
  EXPECT_EQ(key["note"], Json(60));
  EXPECT_EQ(key["pressure"], Json(90));

  const Json board = readAndWritten({0xD0, 70});
  EXPECT_EQ(board["kind"].text(), "Aftertouch");
  EXPECT_EQ(board["pressure"], Json(70));
  EXPECT_TRUE(board["note"].null());

  // A knob.
  const Json knob = readAndWritten({0xB0, 74, 32});
  EXPECT_EQ(knob["kind"].text(), "ControlChange");
  EXPECT_EQ(knob["controller"], Json(74));
  EXPECT_EQ(knob["value"], Json(32));
  EXPECT_EQ(knob["data"], listOf({74, 32}));

  const Json program = readAndWritten({0xC5, 12});
  EXPECT_EQ(program["kind"].text(), "ProgramChange");
  EXPECT_EQ(program["channel"], Json(6));
  EXPECT_EQ(program["program"], Json(12));
  EXPECT_EQ(program["data"], listOf({12}));
}

TEST(DataMidi, TheWheelIsOneNumberEitherSideOfWhereItRests) {
  // The two sevens the wire carries a wheel in are put back together,
  // and the middle of its travel is nothing rather than a number a
  // reader has to know to subtract.
  EXPECT_EQ(readAndWritten({0xE0, 0x00, 0x40})["bend"], Json(0));
  EXPECT_EQ(readAndWritten({0xE0, 0x00, 0x00})["bend"], Json(-8192));
  EXPECT_EQ(readAndWritten({0xE0, 0x7F, 0x7F})["bend"], Json(8191));
  // The smaller half arrives first, so a wheel a little above centre is
  // the one byte that moved.
  EXPECT_EQ(readAndWritten({0xE3, 0x01, 0x40})["bend"], Json(1));
  EXPECT_EQ(readAndWritten({0xE3, 0x01, 0x40})["channel"], Json(4));
}

TEST(DataMidi, ANoteOnAtNoVelocityIsANoteOff) {
  const std::vector<std::byte> released = wireOf({0x90, 60, 0});
  const std::optional<Json> value = decodeMidi(released);
  ASSERT_TRUE(value);
  // It is how a keyboard says the key came back up, so it is read as
  // the release it is rather than as a note struck infinitely softly.
  EXPECT_EQ((*value)["kind"].text(), "NoteOff");
  EXPECT_EQ((*value)["velocity"], Json(0));
  // The status byte it arrived under is still on it, and it is the ONE
  // form that does not go back out as the bytes it came in as: a note
  // off writes the status byte a note off carries.
  EXPECT_EQ((*value)["status"], Json(0x90));
  EXPECT_EQ(encodeMidi(*value), wireOf({0x80, 60, 0}));
}

TEST(DataMidi, AMessageAddressedToTheWholeRoomCarriesNoChannel) {
  const Json clock = readAndWritten({0xF8});
  EXPECT_EQ(clock["kind"].text(), "Clock");
  EXPECT_TRUE(clock["channel"].null());
  EXPECT_EQ(clock["status"], Json(0xF8));
  EXPECT_EQ(clock["data"], listOf({}));

  EXPECT_EQ(readAndWritten({0xFA})["kind"].text(), "Start");
  EXPECT_EQ(readAndWritten({0xFB})["kind"].text(), "Continue");
  EXPECT_EQ(readAndWritten({0xFC})["kind"].text(), "Stop");
  EXPECT_EQ(readAndWritten({0xFE})["kind"].text(), "ActiveSensing");
  EXPECT_EQ(readAndWritten({0xFF})["kind"].text(), "Reset");
}

TEST(DataMidi, AMessageThisCodecHasNoNameForIsCarriedWhole) {
  // A device asking another device who it is: the whole message is on
  // the record, which is what writes it back out verbatim, and the
  // payload alone is the data.
  const Json asked = readAndWritten({0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7});
  EXPECT_EQ(asked["kind"].text(), "SystemExclusive");
  EXPECT_EQ(asked["bytes"], listOf({0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7}));
  EXPECT_EQ(asked["data"], listOf({0x7E, 0x7F, 0x06, 0x01}));
  EXPECT_TRUE(asked["channel"].null());

  // Where a song stands, which is a system message this codec has no
  // name of its own for: it is carried rather than read, so it goes
  // back out the way it came in.
  const Json where = readAndWritten({0xF2, 0x20, 0x01});
  EXPECT_EQ(where["kind"].text(), "System");
  EXPECT_EQ(where["bytes"], listOf({0xF2, 0x20, 0x01}));
  EXPECT_EQ(where["data"], listOf({0x20, 0x01}));

  // Bytes are what a record is written from, whatever else it says: a
  // sender that holds one of these and hands it back sends what it was
  // given.
  EXPECT_EQ(encodeMidi(Json(Json::Object{{"kind", Json("System")},
                                         {"bytes", listOf({0xF6})}})),
            wireOf({0xF6}));
}

TEST(DataMidi, BytesThatAreNoMessageReadAsNothing) {
  // Nothing at all.
  EXPECT_FALSE(decodeMidi({}).has_value());
  // Data bytes with no status byte in front of them: they belong to
  // whatever message came before, which is a message this was not
  // given.
  EXPECT_FALSE(decodeMidi(wireOf({60, 100})).has_value());
  // A note on missing its velocity, and one carrying a byte too many.
  EXPECT_FALSE(decodeMidi(wireOf({0x90, 60})).has_value());
  EXPECT_FALSE(decodeMidi(wireOf({0x90, 60, 100, 7})).has_value());
  // A status byte where a data byte should be is another message
  // beginning, and a message is read whole or not at all.
  EXPECT_FALSE(decodeMidi(wireOf({0x90, 0xF8, 100})).has_value());
  // A message the room acts on the instant it arrives carries nothing.
  EXPECT_FALSE(decodeMidi(wireOf({0xF8, 0x00})).has_value());
  // And a system exclusive message with another message inside it.
  EXPECT_FALSE(decodeMidi(wireOf({0xF0, 0x7E, 0x90, 0xF7})).has_value());
}

TEST(DataMidi, AValueWithNoSpellingOnTheWireWritesNoBytes) {
  EXPECT_TRUE(encodeMidi(Json()).empty());
  EXPECT_TRUE(
      encodeMidi(Json(Json::Object{{"kind", Json("Thunder")}})).empty());
  // A message that names no channel is on the first one, which is where
  // a controller with no channel of its own speaks.
  EXPECT_EQ(encodeMidi(Json(Json::Object{{"kind", Json("NoteOn")},
                                         {"note", Json(60)},
                                         {"velocity", Json(100)}})),
            wireOf({0x90, 60, 100}));
  // A value outside what its place on the wire holds is written at the
  // nearer end of it: wrapping it would put a note nobody played on the
  // cable.
  EXPECT_EQ(encodeMidi(Json(Json::Object{{"kind", Json("ControlChange")},
                                         {"channel", Json(99)},
                                         {"controller", Json(74)},
                                         {"value", Json(400)}})),
            wireOf({0xBF, 74, 127}));
}

}  // namespace
