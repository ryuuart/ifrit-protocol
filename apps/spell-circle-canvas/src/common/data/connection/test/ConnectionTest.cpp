/** The connection: what a message reads as through the door it came in
 *  by, which handler it reaches and which one runs when no name did,
 *  what the newest of a name is, what receive() hands out once a first
 *  call has asked for a queue, what a message that cannot be read
 *  costs, what goes back down the wire — to the door or to the sender
 *  of one message — and what a recording replays through one.
 */

#include <gtest/gtest.h>
#include <sigildata/connection/Connection.h>
#include <sigildata/decode/ArtNet.h>
#include <sigildata/decode/Json.h>
#include <sigildata/decode/Osc.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/hub/Recording.h>

#include <cstddef>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ScratchDir.h"

using namespace sigil::data;
using sigil::io::Bytes;
using sigil::io::Hub;
using sigil::test::ScratchDir;

namespace {

/** What a case reads to say what went out: every message a connection
 *  sent, in order. */
using Sent = std::vector<std::vector<std::byte>>;

/** The same for what went back to ONE sender: the address each reply
 *  named, and the bytes it carried. */
using Answered = std::vector<std::pair<std::string, std::vector<std::byte>>>;

/** A TRANSPORT WITH NO SOCKET UNDER IT. It opens every URI it is given
 *  and takes what is sent into @p sent and what is answered into @p
 *  answered; what arrives a case delivers into the feed itself, so one
 *  thread runs a case from its first line to its last and no port has
 *  to be free for it to pass. A null @p answered is a door that cannot
 *  address one sender, which is what a case not about replies has. */
sigil::io::FeedTransport intoVector(std::shared_ptr<Sent> sent,
                                    std::shared_ptr<Answered> answered = {}) {
  return
      [sent, answered](std::string_view uri, std::weak_ptr<sigil::io::Feed>) {
        sigil::io::OpenedFeed opened;
        // It binds nothing, so the local end it names is the URI it was
        // asked for: enough for a case to see that the end reaches through.
        opened.address = std::string(uri);
        opened.send = [sent](const Bytes& bytes) {
          sent->push_back(bytes.bytes);
          return true;
        };
        if (answered)
          opened.sendTo = [answered](std::string_view to, const Bytes& bytes) {
            answered->emplace_back(std::string(to), bytes.bytes);
            return true;
          };
        return opened;
      };
}

Bytes bytesOf(std::string_view text) {
  const auto* first = reinterpret_cast<const std::byte*>(text.data());
  Bytes bytes;
  bytes.bytes.assign(first, first + text.size());
  return bytes;
}

Bytes bytesOf(std::vector<std::byte> packet) {
  Bytes bytes;
  bytes.bytes = std::move(packet);
  return bytes;
}

std::string textOf(const std::vector<std::byte>& raw) {
  return std::string(reinterpret_cast<const char*>(raw.data()), raw.size());
}

TEST(DataConnection, AJsonMessageIsTheLatestAndReachesEveryHandlerNamingIt) {
  Hub hub;
  hub.setFeedTransport("ws", intoVector(std::make_shared<Sent>()));

  Connection scene(hub, "ws://:8848/scene");
  int every = 0;
  std::vector<double> gusts;
  scene.on("*", [&every](const Json&) { ++every; });
  scene.on("gust", [&gusts](const Json& message) {
    gusts.push_back(message["strength"].number());
  });
  scene.on("calm", [&gusts](const Json&) { gusts.push_back(-1); });

  scene.feed()->deliver(bytesOf(R"({"kind":"gust","strength":0.5})"));
  // Nothing has been read yet: the frame is what reads it.
  EXPECT_TRUE(scene.latest().null());

  hub.dispatch(0.0);
  EXPECT_EQ(every, 1);
  ASSERT_EQ(gusts.size(), 1u);
  EXPECT_DOUBLE_EQ(gusts.front(), 0.5);
  EXPECT_EQ(scene.latest()["kind"].text(), "gust");
  EXPECT_EQ(scene.generation(), 1u);
  EXPECT_EQ(scene.undecodable(), 0u);
  EXPECT_EQ(scene.uri(), "ws://:8848/scene");
  EXPECT_EQ(scene.address(), "ws://:8848/scene");
  EXPECT_TRUE(scene.error().empty());
  EXPECT_FALSE(scene.closed());
}

TEST(DataConnection, AnOscPacketIsItsAddressAndReachesTheHandlerOnThatAddress) {
  Hub hub;
  hub.setFeedTransport("osc", intoVector(std::make_shared<Sent>()));

  Connection desk(hub, "osc://:9000");
  std::vector<double> faders;
  desk.on("/sky/gust", [&faders](const Json& message) {
    faders.push_back(message["arguments"][0].number());
  });
  desk.on("/sky/calm", [&faders](const Json&) { faders.push_back(-1); });

  desk.feed()->deliver(
      bytesOf(encodeOsc("/sky/gust", Json(Json::Array{Json(0.5)}))));
  hub.dispatch(0.0);

  ASSERT_EQ(faders.size(), 1u);
  EXPECT_DOUBLE_EQ(faders.front(), 0.5);
  EXPECT_EQ(desk.latest()["address"].text(), "/sky/gust");
  ASSERT_EQ(desk.latest()["arguments"].size(), 1u);
  EXPECT_DOUBLE_EQ(desk.latest()["arguments"][0].number(), 0.5);
}

/** THE OTHER WIRE OF THE PERFORMANCE ROOM, standing beside the OSC one:
 *  a `midi://` door reads each message as its kind and the fields that
 *  kind carries, and writes one back as the bytes a cable carries. The
 *  stub transport under it is what lets both ends be judged with no
 *  controller in the room. */
TEST(DataMidi, AMidiDoorReadsAMessageAsItsKindAndWritesOneBack) {
  Hub hub;
  const auto sent = std::make_shared<Sent>();
  hub.setFeedTransport("midi", intoVector(sent));

  // One message, spelled out byte by byte the way a cable carries it.
  const auto wire = [](std::initializer_list<int> bytes) {
    std::vector<std::byte> message;
    for (const int one : bytes) message.push_back(static_cast<std::byte>(one));
    return message;
  };

  Connection pads(hub, "midi://in/Launchpad");
  std::vector<double> struck;
  pads.on("NoteOn", [&struck](const Json& message) {
    struck.push_back(message["note"].number());
  });
  pads.on("NoteOff", [&struck](const Json&) { struck.push_back(-1); });

  // A pad struck, and the same pad coming back up — which a keyboard
  // says with a note on at no velocity, and which the codec reads as
  // the release it is, so the two handlers above are the whole of it.
  pads.feed()->deliver(bytesOf(wire({0x90, 0x3C, 0x64})));
  pads.feed()->deliver(bytesOf(wire({0x90, 0x3C, 0x00})));
  hub.dispatch(0.0);

  ASSERT_EQ(struck.size(), 2u);
  EXPECT_DOUBLE_EQ(struck.front(), 60.0);
  EXPECT_DOUBLE_EQ(struck.back(), -1.0);
  // A message's KIND is the name a handler is registered under and the
  // name it latches under, there being no address on this wire.
  EXPECT_EQ(pads.latest()["kind"].text(), "NoteOff");
  EXPECT_DOUBLE_EQ(pads.latest("NoteOn")["velocity"].number(), 100.0);
  EXPECT_EQ(pads.undecodable(), 0u);

  // And back out the same door as the bytes a cable carries: the light
  // under the pad that was struck.
  EXPECT_TRUE(pads.send(Json(Json::Object{{"kind", Json("NoteOn")},
                                          {"channel", Json(1)},
                                          {"note", Json(60)},
                                          {"velocity", Json(127)}})));
  ASSERT_EQ(sent->size(), 1u);
  EXPECT_EQ(sent->front(), wire({0x90, 0x3C, 0x7F}));

  // A value this wire cannot spell does not go out as an empty message,
  // and an address with arguments under it is one of those: naming a
  // message is OSC's way and not a cable's.
  EXPECT_FALSE(pads.send(Json(Json::Object{{"kind", Json("Thunder")}})));
  EXPECT_FALSE(pads.send("/sky/gust", Json(Json::Array{Json(0.5)})));
  EXPECT_EQ(sent->size(), 1u);

  // Bytes that are no message at all reach no reader and are counted.
  pads.feed()->deliver(bytesOf(wire({0x3C, 0x64})));
  hub.dispatch(1.0);
  EXPECT_EQ(pads.undecodable(), 1u);
  EXPECT_EQ(struck.size(), 2u);
}

/** THE WIRE THE LIGHTING DESKS SPEAK, standing beside the other two of
 *  the performance room: an `artnet://` door reads each packet as its
 *  universe, its sequence and its dimmers, and writes one back as the
 *  datagram a desk sends. The stub transport under it is what lets both
 *  ends be judged with no desk in the room. */
TEST(DataArtNet, AnArtNetDoorReadsAUniverseAndWritesOneBack) {
  Hub hub;
  const auto sent = std::make_shared<Sent>();
  hub.setFeedTransport("artnet", intoVector(sent));

  // One packet, spelled out byte by byte the way a desk sends it: the
  // name and the null that ends it, the code for a universe of dimmers
  // low byte first, the version, the sequence and the physical input,
  // the two halves of the port address with the sub-universe first, the
  // count of dimmers, and the levels.
  const auto wire = [](std::initializer_list<int> bytes) {
    std::vector<std::byte> packet;
    for (const int one : bytes) packet.push_back(static_cast<std::byte>(one));
    return packet;
  };

  Connection desk(hub, "artnet://:6454");
  std::vector<double> washes;
  desk.on("Dmx", [&washes](const Json& message) {
    washes.push_back(message["channels"][0].number());
  });

  desk.feed()->deliver(bytesOf(
      wire({'A',  'r', 't', '-',  'N',  'e',  't',  0,   0x00, 0x50, 0x00,
            0x0E, 7,   0,   0x02, 0x00, 0x00, 0x04, 255, 128,  0,    0})));
  hub.dispatch(0.0);

  ASSERT_EQ(washes.size(), 1u);
  EXPECT_DOUBLE_EQ(washes.front(), 255.0);
  // A packet's KIND is the name a handler is registered under and the
  // name it latches under, there being no address on this wire.
  EXPECT_EQ(desk.latest()["kind"].text(), "Dmx");
  EXPECT_EQ(desk.latest()["universe"], Json(2));
  EXPECT_EQ(desk.latest("Dmx")["sequence"], Json(7));
  EXPECT_EQ(desk.latest()["channels"].size(), 4u);
  EXPECT_EQ(desk.undecodable(), 0u);

  // And back out the same door as the datagram a desk sends: a universe
  // of this scene's own, which is the bytes the codec writes for it.
  const Json lit(Json::Object{
      {"kind", Json("Dmx")},
      {"universe", Json(1)},
      {"sequence", Json(1)},
      {"channels", Json(Json::Array{Json(255), Json(128), Json(64)})}});
  EXPECT_TRUE(desk.send(lit));
  ASSERT_EQ(sent->size(), 1u);
  EXPECT_EQ(sent->front(), encodeArtNet(lit));

  // A value this wire cannot spell does not go out as an empty
  // datagram, and an address with arguments under it is one of those:
  // naming a message is OSC's way and not a lighting wire's.
  EXPECT_FALSE(desk.send(Json(Json::Object{{"kind", Json("Blackout")}})));
  EXPECT_FALSE(desk.send("/sky/gust", Json(Json::Array{Json(0.5)})));
  EXPECT_EQ(sent->size(), 1u);

  // Bytes that are no packet at all reach no reader and are counted.
  desk.feed()->deliver(bytesOf(wire({'A', 'r', 't', 0})));
  hub.dispatch(1.0);
  EXPECT_EQ(desk.undecodable(), 1u);
  EXPECT_EQ(washes.size(), 1u);
}

TEST(DataConnection, EachNameLatchesItsOwnNewestBesideTheNewestOfAll) {
  Hub hub;
  hub.setFeedTransport("osc", intoVector(std::make_shared<Sent>()));

  Connection desk(hub, "osc://:9000");
  desk.feed()->deliver(
      bytesOf(encodeOsc("/sky/wind", Json(Json::Array{Json(0.25)}))));
  desk.feed()->deliver(
      bytesOf(encodeOsc("/sky/gust", Json(Json::Array{Json(0.5)}))));
  desk.feed()->deliver(
      bytesOf(encodeOsc("/sky/wind", Json(Json::Array{Json(0.75)}))));
  hub.dispatch(0.0);

  // One fader read off the wire with no handler at all, and the one
  // beside it standing where it was left: a name is its own latch.
  EXPECT_DOUBLE_EQ(desk.latest("/sky/wind")["arguments"][0].number(), 0.75);
  EXPECT_DOUBLE_EQ(desk.latest("/sky/gust")["arguments"][0].number(), 0.5);
  // The reading that names nothing is the newest of every name.
  EXPECT_EQ(desk.latest()["address"].text(), "/sky/wind");
  EXPECT_DOUBLE_EQ(desk.latest()["arguments"][0].number(), 0.75);
}

TEST(DataConnection, ANameNothingIsLatchedUnderAnswersNothing) {
  Hub hub;
  hub.setFeedTransport("ws", intoVector(std::make_shared<Sent>()));

  // Two of anything is all this door holds, latches and queue alike, so
  // each message arrives on a frame of its own: one left on the feed
  // would fall off the front before the connection read it.
  Connection scene(hub, "ws://:8848/scene", {.capacity = 2});
  const auto arrives = [&scene, &hub](std::string_view text) {
    scene.feed()->deliver(bytesOf(text));
    hub.dispatch(0.0);
  };
  arrives(R"({"kind":"gust","strength":1})");
  arrives(R"({"kind":"calm","strength":2})");
  arrives(R"({"kind":"hail","strength":3})");
  arrives(R"({"strength":4})");

  EXPECT_TRUE(scene.latest("squall").null());  // no such message ever came
  // The third name was one too many, so the name written longest ago
  // reads as if nothing had ever arrived under it.
  EXPECT_TRUE(scene.latest("gust").null());
  EXPECT_DOUBLE_EQ(scene.latest("calm")["strength"].number(), 2);
  EXPECT_DOUBLE_EQ(scene.latest("hail")["strength"].number(), 3);
  EXPECT_TRUE(scene.latest("*").null());  // a handler's word, not a name
  EXPECT_TRUE(scene.latest("").null());
  // The last message named nothing, so it latched under nothing and is
  // the newest all the same.
  EXPECT_DOUBLE_EQ(scene.latest()["strength"].number(), 4);
  EXPECT_TRUE(Connection().latest("gust").null());
}

TEST(DataConnection, ReceiveHandsOutEveryMessageInOrderAndThenNothing) {
  Hub hub;
  hub.setFeedTransport("ws", intoVector(std::make_shared<Sent>()));

  Connection scene(hub, "ws://:8848/scene");
  // The queue opens on the first ask, so that ask comes before the
  // messages it expects to be held.
  EXPECT_FALSE(scene.receive().has_value());
  for (int number = 0; number != 3; ++number)
    scene.feed()->deliver(
        bytesOf(R"({"kind":"step","n":)" + std::to_string(number) + "}"));
  hub.dispatch(0.0);

  for (int number = 0; number != 3; ++number) {
    const std::optional<Json> message = scene.receive();
    ASSERT_TRUE(message.has_value());
    EXPECT_DOUBLE_EQ((*message)["n"].number(), number);
  }
  EXPECT_FALSE(scene.receive().has_value());
  // Taking is not forgetting: the newest is still the newest.
  EXPECT_DOUBLE_EQ(scene.latest()["n"].number(), 2);
}

TEST(DataConnection, TheUnreadQueueFillsFromTheFirstReceiveOn) {
  Hub hub;
  hub.setFeedTransport("ws", intoVector(std::make_shared<Sent>()));

  Connection scene(hub, "ws://:8848/scene");
  int handled = 0;
  scene.on("*", [&handled](const Json&) { ++handled; });
  const auto arrives = [&scene, &hub](std::string_view text) {
    scene.feed()->deliver(bytesOf(text));
    hub.dispatch(0.0);
  };

  // Nobody has asked for a queue, so these are read, handled and not
  // held: a reader that only registers handlers keeps no backlog.
  arrives(R"({"kind":"step","n":1})");
  arrives(R"({"kind":"step","n":2})");
  EXPECT_EQ(handled, 2);
  EXPECT_EQ(scene.generation(), 2u);
  EXPECT_DOUBLE_EQ(scene.latest()["n"].number(), 2);

  // This ask is the one that opens it, and it comes back empty because
  // what came before it was never held.
  EXPECT_FALSE(scene.receive().has_value());
  arrives(R"({"kind":"step","n":3})");
  const std::optional<Json> message = scene.receive();
  ASSERT_TRUE(message.has_value());
  EXPECT_DOUBLE_EQ((*message)["n"].number(), 3);
  EXPECT_FALSE(scene.receive().has_value());
  EXPECT_EQ(scene.dropped(), 0u);  // the feed dropped nothing either
}

TEST(DataConnection, OtherwiseRunsForEveryMessageNoNameMatched) {
  Hub hub;
  hub.setFeedTransport("osc", intoVector(std::make_shared<Sent>()));

  Connection desk(hub, "osc://:9000");
  std::vector<std::string> ran;
  desk.on("/sky/wind", [&ran](const Json&) { ran.push_back("wind"); });
  desk.on("*", [&ran](const Json&) { ran.push_back("every"); });
  desk.otherwise([&ran](const Json& message) {
    ran.push_back("otherwise " + std::string(message["address"].text()));
  });
  desk.otherwise([&ran](const Json&) { ran.push_back("otherwise again"); });

  desk.feed()->deliver(bytesOf(encodeOsc("/sky/thunder", Json(Json::Array{}))));
  desk.feed()->deliver(
      bytesOf(encodeOsc("/sky/wind", Json(Json::Array{Json(0.5)}))));
  hub.dispatch(0.0);

  // A name nothing was registered under reaches "*" and then, after it,
  // both otherwise handlers in the order they were registered — "*"
  // being every message rather than a name, so one standing does not
  // make a message matched. A name a handler WAS registered under
  // reaches that one and "*" and no otherwise at all.
  EXPECT_EQ(ran,
            (std::vector<std::string>{"every", "otherwise /sky/thunder",
                                      "otherwise again", "wind", "every"}));
}

TEST(DataConnection, AMessageThatCannotBeReadLeavesTheLatestStanding) {
  Hub hub;
  hub.setFeedTransport("ws", intoVector(std::make_shared<Sent>()));

  Connection scene(hub, "ws://:8848/scene");
  int handled = 0;
  scene.on("*", [&handled](const Json&) { ++handled; });
  EXPECT_FALSE(scene.receive().has_value());  // the queue opens here

  scene.feed()->deliver(bytesOf(R"({"kind":"gust","strength":0.5})"));
  scene.feed()->deliver(bytesOf("this is no document at all"));
  hub.dispatch(0.0);

  EXPECT_EQ(scene.undecodable(), 1u);
  EXPECT_EQ(handled, 1);
  EXPECT_EQ(scene.generation(), 2u);  // the feed took it; no reader saw it
  EXPECT_EQ(scene.latest()["kind"].text(), "gust");
  ASSERT_TRUE(scene.receive().has_value());
  EXPECT_FALSE(scene.receive().has_value());
}

TEST(DataConnection, SendWritesThePacketOnOscAndTheTextOnEveryOtherDoor) {
  Hub hub;
  const auto desked = std::make_shared<Sent>();
  const auto browsed = std::make_shared<Sent>();
  hub.setFeedTransport("osc", intoVector(desked));
  hub.setFeedTransport("ws", intoVector(browsed));

  const Connection desk(hub, "osc://:9000");
  const Connection browser(hub, "ws://:8848/scene");
  const Json arguments = Json(Json::Array{Json(0.5), Json("gust")});

  EXPECT_TRUE(desk.send("/sky/gust", arguments));
  ASSERT_EQ(desked->size(), 1u);
  EXPECT_EQ(desked->front(), encodeOsc("/sky/gust", arguments));

  EXPECT_TRUE(browser.send("/sky/gust", arguments));
  ASSERT_EQ(browsed->size(), 1u);
  const std::optional<Json> readBack = decodeJson(textOf(browsed->front()));
  ASSERT_TRUE(readBack.has_value());
  EXPECT_EQ(textOf(browsed->front()), encodeJson(*readBack));
  EXPECT_EQ((*readBack)["address"].text(), "/sky/gust");
  EXPECT_TRUE((*readBack)["arguments"] == arguments);

  // The message form goes out the same way on either door: the record a
  // packet reads as is the record a packet is written from.
  EXPECT_TRUE(desk.send(*readBack));
  ASSERT_EQ(desked->size(), 2u);
  EXPECT_EQ(desked->back(), desked->front());
}

TEST(DataConnection, AReplyInAHandlerAnswersTheSenderOfTheMessage) {
  Hub hub;
  const auto answered = std::make_shared<Answered>();
  hub.setFeedTransport("osc", intoVector(std::make_shared<Sent>(), answered));

  Connection desk(hub, "osc://:9000");
  desk.on("/sky/wind", [&desk](const Json& message) {
    desk.reply("/sky/state", Json(Json::Array{message["arguments"][0]}));
  });

  desk.feed()->deliver(
      bytesOf(encodeOsc("/sky/wind", Json(Json::Array{Json(0.5)}))),
      "osc://127.0.0.1:52341");
  hub.dispatch(0.0);

  ASSERT_EQ(answered->size(), 1u);
  // It went to the address the arrival named, and it is written exactly
  // as a send on that door writes one.
  EXPECT_EQ(answered->front().first, "osc://127.0.0.1:52341");
  EXPECT_EQ(answered->front().second,
            encodeOsc("/sky/state", Json(Json::Array{Json(0.5)})));
}

TEST(DataConnection, AReplyOutsideAHandlerAnswersTheNewestSender) {
  Hub hub;
  const auto answered = std::make_shared<Answered>();
  hub.setFeedTransport("osc", intoVector(std::make_shared<Sent>(), answered));

  Connection desk(hub, "osc://:9000");
  // Nothing has arrived, so there is nobody to answer.
  EXPECT_FALSE(desk.reply("/sky/state", Json(Json::Array{})));

  desk.feed()->deliver(
      bytesOf(encodeOsc("/sky/wind", Json(Json::Array{Json(0.25)}))),
      "osc://127.0.0.1:52341");
  desk.feed()->deliver(
      bytesOf(encodeOsc("/sky/wind", Json(Json::Array{Json(0.75)}))),
      "osc://127.0.0.1:52342");
  hub.dispatch(0.0);

  EXPECT_TRUE(desk.reply("/sky/state", Json(Json::Array{Json(0.75)})));
  ASSERT_EQ(answered->size(), 1u);
  // The newest message is the one an answer outside a handler answers,
  // as the newest message is what latest() reads.
  EXPECT_EQ(answered->front().first, "osc://127.0.0.1:52342");
  // The message form goes back the same way the address form does.
  EXPECT_TRUE(desk.reply(desk.latest()));
  ASSERT_EQ(answered->size(), 2u);
  EXPECT_EQ(answered->back().first, "osc://127.0.0.1:52342");
  EXPECT_EQ(answered->back().second,
            encodeOsc("/sky/wind", Json(Json::Array{Json(0.75)})));
}

TEST(DataConnection, AMessageThatNamedNoSenderIsNobodyToAnswer) {
  Hub hub;
  const auto sent = std::make_shared<Sent>();
  const auto answered = std::make_shared<Answered>();
  hub.setFeedTransport("ws", intoVector(sent, answered));

  Connection scene(hub, "ws://:8848/scene");
  // The door answers one sender, and a message arrived; what is missing
  // is who sent it, which a transport that cannot say leaves empty.
  scene.feed()->deliver(bytesOf(R"({"kind":"gust"})"));
  hub.dispatch(0.0);
  EXPECT_EQ(scene.latest()["kind"].text(), "gust");
  EXPECT_FALSE(scene.reply(scene.latest()));
  EXPECT_FALSE(scene.reply("/sky/state", Json(Json::Array{})));
  EXPECT_TRUE(answered->empty());
  // The door itself is no less open for it: what goes out to everybody
  // still goes.
  EXPECT_TRUE(scene.send(scene.latest()));
  EXPECT_EQ(sent->size(), 1u);
}

TEST(DataConnection, ARecordingHasNobodyToReplyTo) {
  const ScratchDir scratch("data_connection_reply_replay");
  const std::filesystem::path path = scratch.path / "scene.feed";
  {
    sigil::io::RecordingWriter writer(path);
    ASSERT_TRUE(writer.good());
    writer.append({1, 0.0,
                   std::make_shared<const Bytes>(
                       bytesOf(R"({"kind":"gust","strength":1})"))});
  }

  Hub hub;
  hub.mount("ws://:8848/scene", path);
  Connection scene(hub, "ws://:8848/scene");
  hub.dispatch(0.0);

  EXPECT_DOUBLE_EQ(scene.latest()["strength"].number(), 1.0);
  // A recording holds the messages and not who sent them, so what runs
  // again against a file answers nobody rather than answering wrongly.
  EXPECT_FALSE(scene.reply(scene.latest()));
  EXPECT_FALSE(scene.reply("/sky/state", Json(Json::Array{})));
}

TEST(DataConnection, AMovedConnectionGoesOnDispatchingToItsHandlers) {
  Hub hub;
  hub.setFeedTransport("ws", intoVector(std::make_shared<Sent>()));

  int seen = 0;
  Connection opened(hub, "ws://:8848/scene");
  opened.on("*", [&seen](const Json&) { ++seen; });
  Connection moved = std::move(opened);

  moved.feed()->deliver(bytesOf(R"({"kind":"gust"})"));
  hub.dispatch(0.0);

  EXPECT_EQ(seen, 1);
  EXPECT_EQ(moved.uri(), "ws://:8848/scene");
  EXPECT_EQ(moved.latest()["kind"].text(), "gust");
}

TEST(DataConnection, AConnectionOntoNothingAnswersNothing) {
  Connection none;
  EXPECT_TRUE(none.latest().null());
  EXPECT_TRUE(none.uri().empty());
  EXPECT_TRUE(none.address().empty());
  EXPECT_TRUE(none.error().empty());
  EXPECT_EQ(none.generation(), 0u);
  EXPECT_EQ(none.dropped(), 0u);
  EXPECT_EQ(none.undecodable(), 0u);
  EXPECT_TRUE(none.closed());
  EXPECT_EQ(none.feed(), nullptr);
  EXPECT_FALSE(none.receive().has_value());
  EXPECT_FALSE(none.send(Json(Json::Object{{"kind", Json("gust")}})));
  EXPECT_FALSE(none.send("/sky/gust", Json(Json::Array{})));
  EXPECT_FALSE(none.reply(Json(Json::Object{{"kind", Json("gust")}})));
  EXPECT_FALSE(none.reply("/sky/gust", Json(Json::Array{})));
  none.on("*", [](const Json&) { FAIL() << "no message arrives at no door"; });
  none.otherwise(
      [](const Json&) { FAIL() << "no message arrives at no door"; });
}

TEST(DataConnection, ARecordingReplaysThroughAConnectionByTheTimeDispatched) {
  const ScratchDir scratch("data_connection_replay");
  const std::filesystem::path path = scratch.path / "scene.feed";
  {
    sigil::io::RecordingWriter writer(path);
    ASSERT_TRUE(writer.good());
    writer.append({1, 0.0,
                   std::make_shared<const Bytes>(
                       bytesOf(R"({"kind":"gust","strength":1})"))});
    writer.append({2, 1.0,
                   std::make_shared<const Bytes>(
                       bytesOf(R"({"kind":"gust","strength":2})"))});
  }

  Hub hub;
  // The URI is mounted straight onto the file: nothing is beneath it,
  // and no transport is needed for a door that reads a recording.
  hub.mount("ws://:8848/scene", path);

  Connection scene(hub, "ws://:8848/scene");
  std::vector<double> strengths;
  scene.on("gust", [&strengths](const Json& message) {
    strengths.push_back(message["strength"].number());
  });
  EXPECT_TRUE(scene.error().empty());
  EXPECT_EQ(scene.generation(), 0u);  // nothing arrives until time moves

  hub.dispatch(0.0);
  ASSERT_EQ(strengths.size(), 1u);
  EXPECT_DOUBLE_EQ(strengths.front(), 1.0);
  EXPECT_FALSE(scene.closed());

  hub.dispatch(1.0);
  ASSERT_EQ(strengths.size(), 2u);
  EXPECT_DOUBLE_EQ(strengths.back(), 2.0);
  // The recording ran out: the door is shut and what it delivered
  // stands.
  EXPECT_TRUE(scene.closed());
  EXPECT_DOUBLE_EQ(scene.latest()["strength"].number(), 2.0);
}

}  // namespace
