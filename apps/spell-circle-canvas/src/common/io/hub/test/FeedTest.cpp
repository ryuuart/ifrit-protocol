/** @file
 * Feeds: what a reader sees of what a transport delivers — the newest
 * message whole, the ones it has not drained, the sender
 * each one names, and the ones a feed too full to hold them dropped —
 * the one door a hub opens per URI, closes when nobody holds it any
 * more and opens again when it could not be opened, what goes back out
 * of it to everybody or to one named sender, and the recording a feed
 * writes as it runs and plays back afterwards.
 */

#include <gtest/gtest.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/hub/Recording.h>
#include <sigilio/testing/Testing.h>

#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "MountedHub.h"

using namespace sigil::io;
namespace fs = std::filesystem;

namespace {

/** A message carrying @p text, for a case that says WHICH message
 *  arrived rather than how many bytes it was. */
Bytes message(std::string_view text) {
  const auto* first = reinterpret_cast<const std::byte*>(text.data());
  return Bytes(std::span(first, text.size()));
}

std::shared_ptr<const Bytes> shared(std::string_view text) {
  return std::make_shared<const Bytes>(message(text));
}

}  // namespace

/** A hub with one scratch directory mounted at res://, which is where
 *  the cases that record write their files. */
class IOFeed : public MountedHub {};

TEST_F(IOFeed, TheLatestIsTheNewestArrivalAndGenerationsCountFromOne) {
  const auto feed = std::make_shared<Feed>("udp://:27020");
  const Inlet inlet = sigil::io::testing::inletOf(feed);
  EXPECT_FALSE(feed->latest().has_value());
  EXPECT_EQ(feed->generation(), 0u);

  inlet.deliver(message("first"));
  EXPECT_EQ(feed->generation(), 1u);
  inlet.deliver(message("second"));
  ASSERT_TRUE(feed->latest().has_value());
  EXPECT_EQ(feed->latest()->bytes->asText(), "second");
  EXPECT_EQ(feed->generation(), 2u);
  EXPECT_EQ(feed->uri(), "udp://:27020");
  EXPECT_TRUE(feed->error().empty());
}

TEST_F(IOFeed, TheLatestIsTheWholeArrivalAndOutlastsDraining) {
  const auto feed = std::make_shared<Feed>("udp://:27020");
  const Inlet inlet = sigil::io::testing::inletOf(feed);
  EXPECT_FALSE(feed->latest().has_value());

  inlet.deliver(message("first"), "udp://127.0.0.1:52341");
  inlet.deliver(message("second"), "udp://127.0.0.1:52342");

  std::optional<Arrival> newest = feed->latest();
  ASSERT_TRUE(newest.has_value());
  EXPECT_EQ(newest->bytes->asText(), "second");
  EXPECT_EQ(newest->from, "udp://127.0.0.1:52342");
  EXPECT_EQ(newest->generation, 2u);
  EXPECT_GE(newest->at, 0.0);

  while (feed->receive().has_value()) {
  }
  // Draining is not taking it: the newest message stands whole after
  // the queue it was also put on is empty.
  newest = feed->latest();
  ASSERT_TRUE(newest.has_value());
  EXPECT_EQ(newest->generation, 2u);
  EXPECT_EQ(newest->from, "udp://127.0.0.1:52342");
}

TEST_F(IOFeed, AnArrivalCarriesTheSenderItWasDeliveredWithAndNoOther) {
  const auto feed = std::make_shared<Feed>("udp://:27020");
  const Inlet inlet = sigil::io::testing::inletOf(feed);
  inlet.deliver(message("from a peer"), "udp://127.0.0.1:52341");
  inlet.deliver(message("from nowhere named"));

  std::optional<Arrival> arrival = feed->receive();
  ASSERT_TRUE(arrival.has_value());
  EXPECT_EQ(arrival->from, "udp://127.0.0.1:52341");
  arrival = feed->receive();
  ASSERT_TRUE(arrival.has_value());
  // A transport with no way of knowing who sent a message names
  // nobody, and the arrival is one all the same.
  EXPECT_TRUE(arrival->from.empty());
  EXPECT_EQ(arrival->bytes->asText(), "from nowhere named");
}

TEST_F(IOFeed, ReceiveHandsOutEveryArrivalInOrderAndThenNothing) {
  const auto feed = std::make_shared<Feed>("udp://:27020");
  const Inlet inlet = sigil::io::testing::inletOf(feed);
  for (int number = 0; number != 3; ++number)
    inlet.deliver(message(std::to_string(number)));

  for (int number = 0; number != 3; ++number) {
    const std::optional<Arrival> arrival = feed->receive();
    ASSERT_TRUE(arrival.has_value());
    EXPECT_EQ(arrival->generation, (uint64_t)number + 1);
    EXPECT_EQ(arrival->bytes->asText(), std::to_string(number));
    EXPECT_GE(arrival->at, 0.0);
  }
  EXPECT_FALSE(feed->receive().has_value());
  // Draining is not forgetting: the newest is still the newest.
  EXPECT_EQ(feed->latest()->bytes->asText(), "2");
}

TEST_F(IOFeed, AFullFeedDropsTheOldestAndCountsIt) {
  const auto feed = std::make_shared<Feed>("udp://:27020", FeedPolicy{.capacity = 2});
  const Inlet inlet = sigil::io::testing::inletOf(feed);
  inlet.deliver(message("one"));
  inlet.deliver(message("two"));
  inlet.deliver(message("three"));

  EXPECT_EQ(feed->dropped(), 1u);
  std::optional<Arrival> arrival = feed->receive();
  ASSERT_TRUE(arrival.has_value());
  EXPECT_EQ(arrival->bytes->asText(), "two");
  arrival = feed->receive();
  ASSERT_TRUE(arrival.has_value());
  EXPECT_EQ(arrival->bytes->asText(), "three");
  EXPECT_FALSE(feed->receive().has_value());
  // What fell off the front is what a reader could not keep up with.
  // The newest message and the count of them are untouched.
  EXPECT_EQ(feed->latest()->bytes->asText(), "three");
  EXPECT_EQ(feed->generation(), 3u);
}

TEST_F(IOFeed, AClosedFeedKeepsWhatItHoldsAndTakesNothingNew) {
  const auto feed = std::make_shared<Feed>("udp://:27020");
  const Inlet inlet = sigil::io::testing::inletOf(feed);
  inlet.deliver(message("before"));
  feed->close();
  EXPECT_TRUE(feed->closed());

  inlet.deliver(message("after"));
  EXPECT_EQ(feed->generation(), 1u);
  EXPECT_EQ(feed->latest()->bytes->asText(), "before");
  const std::optional<Arrival> arrival = feed->receive();
  ASSERT_TRUE(arrival.has_value());
  EXPECT_EQ(arrival->bytes->asText(), "before");
  EXPECT_FALSE(feed->receive().has_value());
}

TEST_F(IOFeed, AHubHoldsOneFeedPerUriWhileSomebodyHoldsItAndOpensAgainAfter) {
  std::vector<std::string> opened;
  hub.setFeedTransport("udp",
                       [&opened](std::string_view uri, Inlet) {
                         opened.emplace_back(uri);
                         return OpenedFeed{};
                       });

  std::shared_ptr<Feed> scene = hub.feed("udp://:27020");
  std::shared_ptr<Feed> again = hub.feed("udp://:27020");
  ASSERT_NE(scene, nullptr);
  EXPECT_EQ(scene, again);  // one door, however many asks
  const std::shared_ptr<Feed> other = hub.feed("udp://:27021");
  EXPECT_NE(other, scene);
  const std::vector<std::string> both = {"udp://:27020", "udp://:27021"};
  EXPECT_EQ(opened, both);
  ASSERT_EQ(hub.feeds().size(), 2u);
  EXPECT_EQ(hub.feeds().front(), scene);  // opening order

  scene.reset();
  again.reset();
  EXPECT_EQ(hub.feeds().size(), 1u);
  const std::shared_ptr<Feed> reopened = hub.feed("udp://:27020");
  ASSERT_NE(reopened, nullptr);
  // Nobody was holding that URI any more, so it is a new door.
  ASSERT_EQ(opened.size(), 3u);
  EXPECT_EQ(opened.back(), "udp://:27020");
}

TEST_F(IOFeed, ADoorThatCouldNotBeOpenedIsOpenedAgainByTheNextAskForItsUri) {
  int opens = 0;
  hub.setFeedTransport("udp",
                       [&opens](std::string_view, Inlet into) {
                         OpenedFeed opened;
                         // The first ask finds the outside world in the way and
                         // says so with nothing to hand back; by the second it
                         // is clear.
                         if (++opens == 1) {
                           into.fail("the port is taken");
                           return opened;
                         }
                         opened.address = "udp://[::]:27020";
                         return opened;
                       });

  const std::shared_ptr<Feed> refused = hub.feed("udp://:27020");
  ASSERT_NE(refused, nullptr);
  EXPECT_EQ(refused->error(), "the port is taken");
  EXPECT_FALSE(refused->opened());
  EXPECT_TRUE(refused->address().empty());

  const std::shared_ptr<Feed> again = hub.feed("udp://:27020");
  // The same feed, opened this time: a reader that held it through the
  // failure is reading the door that opened, with the reason gone.
  EXPECT_EQ(again, refused);
  EXPECT_EQ(opens, 2);
  EXPECT_TRUE(again->opened());
  EXPECT_TRUE(again->error().empty()) << again->error();
  EXPECT_EQ(again->address(), "udp://[::]:27020");

  // A feed that has a door is handed back as it stands: asking twice
  // for a URI that opened is one socket and not two.
  EXPECT_EQ(hub.feed("udp://:27020"), refused);
  EXPECT_EQ(opens, 2);
}

TEST_F(IOFeed, AUriWithNoSchemeIsAFeedWhoseErrorSaysSo) {
  const std::shared_ptr<Feed> feed = hub.feed("no-door-here");
  ASSERT_NE(feed, nullptr);
  EXPECT_NE(feed->error().find("scheme"), std::string::npos);
  EXPECT_EQ(feed->generation(), 0u);
}

TEST_F(IOFeed, AUriWithNoTransportIsAFeedWhoseErrorSaysSo) {
  const std::shared_ptr<Feed> feed = hub.feed("udp://:27020");
  ASSERT_NE(feed, nullptr);
  EXPECT_NE(feed->error().find("udp"), std::string::npos);
  EXPECT_FALSE(feed->send(message("nowhere to go")));
  EXPECT_FALSE(feed->sendTo("udp://127.0.0.1:52341", message("nobody")));
  EXPECT_TRUE(feed->address().empty());
}

TEST_F(IOFeed, ATransportsOpenedEndIsClosedExactlyOnceWhenTheFeedGoes) {
  const auto closes = std::make_shared<int>(0);
  hub.setFeedTransport("udp", [closes](std::string_view, Inlet) {
    OpenedFeed opened;
    opened.close = [closes] { ++*closes; };
    opened.address = "udp://[::]:52341";
    return opened;
  });

  std::shared_ptr<Feed> feed = hub.feed("udp://:27020");
  EXPECT_EQ(feed->address(), "udp://[::]:52341");
  feed->close();
  EXPECT_EQ(*closes, 1);
  feed->close();
  EXPECT_EQ(*closes, 1);
  feed.reset();  // the destructor closes what is already closed
  EXPECT_EQ(*closes, 1);
}

TEST_F(IOFeed, SendGoesThroughTheOpenedEnd) {
  const auto sent = std::make_shared<std::string>();
  hub.setFeedTransport("udp", [sent](std::string_view, Inlet) {
    OpenedFeed opened;
    opened.send = [sent](const Bytes& bytes) {
      *sent = bytes.asText();
      return true;
    };
    return opened;
  });

  const std::shared_ptr<Feed> feed = hub.feed("udp://:27020");
  EXPECT_TRUE(feed->send(message("outward")));
  EXPECT_EQ(*sent, "outward");
  // There is no end to send through once it has been closed.
  feed->close();
  EXPECT_FALSE(feed->send(message("too late")));
}

TEST_F(IOFeed, AOneWayFeedAnswersFalseToSend) {
  hub.setFeedTransport("udp", [](std::string_view, Inlet) {
    OpenedFeed opened;  // listening only: no way back out
    opened.address = "udp://[::]:52341";
    return opened;
  });

  const std::shared_ptr<Feed> feed = hub.feed("udp://:27020");
  EXPECT_FALSE(feed->send(message("outward")));
  EXPECT_FALSE(feed->sendTo("udp://127.0.0.1:52341", message("outward")));
  EXPECT_FALSE(feed->closed());
}

TEST_F(IOFeed, SendToGoesThroughTheOpenedEndNamingTheSenderToAnswer) {
  const auto answered = std::make_shared<std::string>();
  hub.setFeedTransport(
      "udp", [answered](std::string_view, Inlet) {
        // A door that answers one sender and broadcasts to none, which
        // is what a listening socket is.
        OpenedFeed opened;
        opened.sendTo = [answered](std::string_view to, const Bytes& bytes) {
          *answered = std::string(to) + " " + std::string(bytes.asText());
          return true;
        };
        return opened;
      });

  const std::shared_ptr<Feed> feed = hub.feed("udp://:27020");
  EXPECT_FALSE(feed->send(message("to nobody in particular")));
  EXPECT_TRUE(feed->sendTo("udp://127.0.0.1:52341", message("answered")));
  EXPECT_EQ(*answered, "udp://127.0.0.1:52341 answered");
  // There is no end to answer through once it has been closed.
  feed->close();
  EXPECT_FALSE(feed->sendTo("udp://127.0.0.1:52341", message("too late")));
}

TEST_F(IOFeed, ARecordingReadsBackTheBytesAndTimesItWasWrittenWith) {
  const fs::path path = dir.path / "arrivals.feed";
  {
    RecordingWriter writer(path);
    ASSERT_TRUE(writer.good());
    EXPECT_TRUE(writer.append({7, 0.25, shared("first")}));
    EXPECT_TRUE(writer.append({8, 1.5, shared("second")}));
  }

  const std::optional<std::vector<Arrival>> read = readRecording(path);
  ASSERT_TRUE(read.has_value());
  ASSERT_EQ(read->size(), 2u);
  EXPECT_EQ((*read)[0].bytes->asText(), "first");
  EXPECT_EQ((*read)[1].bytes->asText(), "second");
  EXPECT_EQ((*read)[0].at, 0.25);
  EXPECT_EQ((*read)[1].at, 1.5);
  // The file carries no generations: what was written as 7 and 8 reads
  // back numbered from one, because the count belongs to a feed.
  EXPECT_EQ((*read)[0].generation, 1u);
  EXPECT_EQ((*read)[1].generation, 2u);
  EXPECT_FALSE(readRecording(dir.path / "never-written.feed").has_value());
}

TEST_F(IOFeed, ARecordingCutShortKeepsTheWholeFramesBeforeTheCut) {
  const fs::path path = dir.path / "killed.feed";
  {
    RecordingWriter writer(path);
    writer.append({1, 0.0, shared("whole")});
    writer.append({2, 1.0, shared("cut through the middle")});
  }
  fs::resize_file(path, fs::file_size(path) - 6);

  const std::optional<std::vector<Arrival>> read = readRecording(path);
  ASSERT_TRUE(read.has_value());
  ASSERT_EQ(read->size(), 1u);
  EXPECT_EQ((*read)[0].bytes->asText(), "whole");
}

TEST_F(IOFeed, AReplayedUriPlaysItsRecordingByTheTimeDispatched) {
  const fs::path path = dir.path / "scene.feed";
  {
    RecordingWriter writer(path);
    writer.append({1, 0.0, shared("at zero")});
    writer.append({2, 1.0, shared("at one")});
  }
  // Named through the mount table, as a sketch names its own files.
  const std::shared_ptr<Feed> replaying =
      hub.replay("udp://:27020", "res://scene.feed");
  const std::shared_ptr<Feed> feed = hub.feed("udp://:27020");
  // Every later ask for the URI is handed the replaying feed.
  EXPECT_EQ(feed, replaying);
  ASSERT_NE(feed, nullptr);
  EXPECT_TRUE(feed->error().empty());
  EXPECT_EQ(feed->generation(), 0u);  // nothing arrives until time moves

  hub.dispatch(0.0);
  EXPECT_EQ(feed->generation(), 1u);
  EXPECT_EQ(feed->latest()->bytes->asText(), "at zero");
  EXPECT_FALSE(feed->closed());

  hub.dispatch(1.0);
  EXPECT_EQ(feed->generation(), 2u);
  EXPECT_EQ(feed->latest()->bytes->asText(), "at one");
  EXPECT_TRUE(feed->closed());  // the recording ran out

  const std::optional<Arrival> first = feed->receive();
  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(first->at, 0.0);  // the recorded time, not a clock's
  // A recording is the messages and not who sent them.
  EXPECT_TRUE(first->from.empty());
  const std::optional<Arrival> second = feed->receive();
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(second->at, 1.0);
}

TEST_F(IOFeed, AFileThatIsNoRecordingIsAFeedWhoseErrorSaysSo) {
  dir.write("scene.bin", "these are not frames");
  const std::shared_ptr<Feed> feed =
      hub.replay("udp://:27020", (dir.path / "scene.bin").string());
  ASSERT_NE(feed, nullptr);
  EXPECT_FALSE(feed->error().empty());
  hub.dispatch(1.0);
  EXPECT_EQ(feed->generation(), 0u);
}

TEST_F(IOFeed, RecordingALiveFeedWritesWhatArrives) {
  const fs::path path = dir.path / "live.feed";
  const auto feed = std::make_shared<Feed>("udp://:27020");
  const Inlet inlet = sigil::io::testing::inletOf(feed);
  {
    const Recording recording = feed->record(path);
    EXPECT_FALSE(recording.stopped());
    inlet.deliver(message("one"));
    inlet.deliver(message("two"));
  }  // the handle is gone, so the recording is: the file stands as it is
  inlet.deliver(message("three"));

  const std::optional<std::vector<Arrival>> read = readRecording(path);
  ASSERT_TRUE(read.has_value());
  ASSERT_EQ(read->size(), 2u);
  EXPECT_EQ((*read)[0].bytes->asText(), "one");
  EXPECT_EQ((*read)[1].bytes->asText(), "two");
  // A live feed stamps the seconds since it was made, in the order it
  // took the messages.
  EXPECT_GE((*read)[0].at, 0.0);
  EXPECT_LE((*read)[0].at, (*read)[1].at);
}

TEST_F(IOFeed, ARecordingStopsWhenToldAndWhenAnotherTakesItsPlace) {
  const auto feed = std::make_shared<Feed>("udp://:27020");
  const Inlet inlet = sigil::io::testing::inletOf(feed);
  Recording first = feed->record(dir.path / "first.feed");
  inlet.deliver(message("one"));
  Recording second = feed->record(dir.path / "second.feed");
  // One recording at a time: the second ended the first.
  EXPECT_TRUE(first.stopped());
  inlet.deliver(message("two"));
  second.stop();
  EXPECT_TRUE(second.stopped());
  inlet.deliver(message("three"));

  const std::optional<std::vector<Arrival>> one =
      readRecording(dir.path / "first.feed");
  ASSERT_TRUE(one.has_value());
  ASSERT_EQ(one->size(), 1u);
  EXPECT_EQ((*one)[0].bytes->asText(), "one");
  const std::optional<std::vector<Arrival>> two =
      readRecording(dir.path / "second.feed");
  ASSERT_TRUE(two.has_value());
  ASSERT_EQ(two->size(), 1u);
  EXPECT_EQ((*two)[0].bytes->asText(), "two");

  // A file that cannot be opened is a handle that has already stopped.
  const Recording nowhere = feed->record(dir.path / "absent" / "x.feed");
  EXPECT_TRUE(nowhere.stopped());
  EXPECT_FALSE(feed->error().empty());
}

TEST_F(IOFeed, ReplayClosesTheLiveFeedStandingAtItsUri) {
  const fs::path path = dir.path / "taken.feed";
  {
    RecordingWriter writer(path);
    writer.append({1, 0.0, shared("recorded")});
  }
  hub.setFeedTransport("udp", [](std::string_view, Inlet) {
    return OpenedFeed{};
  });
  const std::shared_ptr<Feed> live = hub.feed("udp://:27020");
  const std::shared_ptr<Feed> replaying =
      hub.replay("udp://:27020", path.string());
  EXPECT_TRUE(live->closed());
  EXPECT_NE(live, replaying);
  hub.dispatch(0.0);
  ASSERT_TRUE(replaying->latest().has_value());
}

TEST_F(IOFeed, ArrivalsFromAnotherThreadAreAllReceivedInOrder) {
  const int count = 1000;
  const auto feed = std::make_shared<Feed>("udp://:27020", FeedPolicy{.capacity = 2048});
  const Inlet inlet = sigil::io::testing::inletOf(feed);
  std::thread sender([&inlet, count] {
    for (int number = 0; number != count; ++number)
      inlet.deliver(message(std::to_string(number)));
  });

  std::vector<std::string> received;
  while ((int)received.size() != count)
    if (const std::optional<Arrival> arrival = feed->receive())
      received.push_back(std::string(arrival->bytes->asText()));
  sender.join();

  ASSERT_EQ((int)received.size(), count);
  for (int number = 0; number != count; ++number)
    ASSERT_EQ(received[(size_t)number], std::to_string(number));
  EXPECT_EQ(feed->dropped(), 0u);
}

TEST_F(IOFeed, DispatchRunsEveryRegisteredCallbackInOrderUntilItsLeaseGoes) {
  std::vector<std::string> ran;
  std::vector<double> given;
  DispatchLease first = hub.onDispatch([&ran, &given](double seconds) {
    ran.emplace_back("first");
    given.push_back(seconds);
  });
  const DispatchLease second =
      hub.onDispatch([&ran](double) { ran.emplace_back("second"); });
  EXPECT_TRUE(first.registered());

  hub.dispatch(0.5);
  const std::vector<std::string> both = {"first", "second"};
  EXPECT_EQ(ran, both);
  const std::vector<double> once = {0.5};
  EXPECT_EQ(given, once);  // the seconds the dispatch was given

  first.release();
  EXPECT_FALSE(first.registered());
  {
    const DispatchLease third =
        hub.onDispatch([&ran](double) { ran.emplace_back("third"); });
  }

  ran.clear();
  hub.dispatch(1.5);
  // A released lease and a lease that is gone both leave nothing to
  // run; the one still held runs on.
  const std::vector<std::string> onlySecond = {"second"};
  EXPECT_EQ(ran, onlySecond);
  EXPECT_EQ(given, once);
}

TEST_F(IOFeed, ACallbackSeesWhatTheSameDispatchDelivered) {
  const fs::path path = dir.path / "seen.feed";
  {
    RecordingWriter writer(path);
    writer.append({1, 0.0, shared("at zero")});
  }
  const std::shared_ptr<Feed> feed =
      hub.replay("udp://:27020", path.string());

  std::vector<std::string> seen;
  const DispatchLease lease = hub.onDispatch([&seen, feed](double) {
    if (const std::optional<Arrival> arrival = feed->receive())
      seen.emplace_back(arrival->bytes->asText());
  });

  hub.dispatch(0.0);
  // The recordings move first and the callbacks run after them, so what
  // a reader is driven for is already there when it is driven.
  const std::vector<std::string> one = {"at zero"};
  EXPECT_EQ(seen, one);
}

TEST_F(IOFeed, LiveArrivalTimeIsIndependentOfQueueDrainTime) {
  using Clock = std::chrono::steady_clock;
  const auto feed = std::make_shared<Feed>("test://clock");
  const Inlet inlet = sigil::io::testing::inletOf(feed);
  const auto before = Clock::now();
  inlet.deliver(message("one"));
  const auto after = Clock::now();
  const auto arrival = feed->receive();
  ASSERT_TRUE(arrival);
  const auto received = feed->receivedAt(*arrival);
  EXPECT_GE(received, before);
  EXPECT_LE(received, after);
  EXPECT_EQ(received, feed->receivedAt(*arrival));
}

TEST_F(IOFeed, ReplayArrivalTimeUsesFirstDispatchAndRecordedSpacing) {
  using Clock = std::chrono::steady_clock;
  using namespace std::chrono_literals;
  const fs::path path = dir.path / "clock.feed";
  {
    RecordingWriter writer(path);
    writer.append({1, 0.0, shared("one")});
    writer.append({2, 0.25, shared("two")});
  }
  const std::shared_ptr<Feed> feed = hub.replay("test://clock", path.string());
  const auto before = Clock::now();
  hub.dispatch(900.0);
  const auto after = Clock::now();
  const auto first = feed->receive();
  ASSERT_TRUE(first);
  const auto origin = feed->receivedAt(*first);
  EXPECT_GE(origin, before);
  EXPECT_LE(origin, after);
  hub.dispatch(900.25);
  const auto second = feed->receive();
  ASSERT_TRUE(second);
  EXPECT_EQ(feed->receivedAt(*second) - origin, 250ms);
}

TEST_F(IOFeed, InvalidOrUnrepresentableArrivalTimesMapToTheClockOrigin) {
  using Clock = std::chrono::steady_clock;
  const auto feed = std::make_shared<Feed>("test://clock");
  Arrival arrival{};
  const auto origin = feed->receivedAt(arrival);
  for (const double invalid :
       {-1.0, -std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::max(),
        std::chrono::duration<double>(Clock::duration::max()).count()}) {
    arrival.at = invalid;
    EXPECT_EQ(feed->receivedAt(arrival), origin);
  }
}
