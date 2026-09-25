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
using seconds = std::chrono::duration<double>;

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
  EXPECT_EQ(feed->state().revision, 0u);

  inlet.deliver(message("first"));
  EXPECT_EQ(feed->state().revision, 1u);
  inlet.deliver(message("second"));
  ASSERT_TRUE(feed->latest().has_value());
  EXPECT_EQ(feed->latest()->payload->asText(), "second");
  EXPECT_EQ(feed->state().revision, 2u);
  EXPECT_EQ(feed->uri(), "udp://:27020");
  EXPECT_TRUE(feed->state().error.empty());
}

TEST_F(IOFeed, TheLatestIsTheWholeArrivalAndOutlastsDraining) {
  const auto feed = std::make_shared<Feed>("udp://:27020");
  const Inlet inlet = sigil::io::testing::inletOf(feed);
  EXPECT_FALSE(feed->latest().has_value());

  inlet.deliver(message("first"), "udp://127.0.0.1:52341");
  inlet.deliver(message("second"), "udp://127.0.0.1:52342");

  std::optional<Message> newest = feed->latest();
  ASSERT_TRUE(newest.has_value());
  EXPECT_EQ(newest->payload->asText(), "second");
  EXPECT_EQ(newest->sender(), "udp://127.0.0.1:52342");
  EXPECT_EQ(newest->revision(), 2u);
  EXPECT_GE(newest->arrivedAt().count(), 0.0);

  while (feed->receive().has_value()) {
  }
  // Draining is not taking it: the newest message stands whole after
  // the queue it was also put on is empty.
  newest = feed->latest();
  ASSERT_TRUE(newest.has_value());
  EXPECT_EQ(newest->revision(), 2u);
  EXPECT_EQ(newest->sender(), "udp://127.0.0.1:52342");
}

TEST_F(IOFeed, AnArrivalCarriesTheSenderItWasDeliveredWithAndNoOther) {
  const auto feed = std::make_shared<Feed>("udp://:27020");
  const Inlet inlet = sigil::io::testing::inletOf(feed);
  inlet.deliver(message("from a peer"), "udp://127.0.0.1:52341");
  inlet.deliver(message("from nowhere named"));

  std::optional<Message> arrival = feed->receive();
  ASSERT_TRUE(arrival.has_value());
  EXPECT_EQ(arrival->sender(), "udp://127.0.0.1:52341");
  arrival = feed->receive();
  ASSERT_TRUE(arrival.has_value());
  // A transport with no way of knowing who sent a message names
  // nobody, and the arrival is one all the same.
  EXPECT_TRUE(arrival->sender().empty());
  EXPECT_EQ(arrival->payload->asText(), "from nowhere named");
}

TEST_F(IOFeed, ReceiveHandsOutEveryArrivalInOrderAndThenNothing) {
  const auto feed = std::make_shared<Feed>("udp://:27020");
  const Inlet inlet = sigil::io::testing::inletOf(feed);
  for (int number = 0; number != 3; ++number)
    inlet.deliver(message(std::to_string(number)));

  for (int number = 0; number != 3; ++number) {
    const std::optional<Message> arrival = feed->receive();
    ASSERT_TRUE(arrival.has_value());
    EXPECT_EQ(arrival->revision(), (uint64_t)number + 1);
    EXPECT_EQ(arrival->payload->asText(), std::to_string(number));
    EXPECT_GE(arrival->arrivedAt().count(), 0.0);
  }
  EXPECT_FALSE(feed->receive().has_value());
  // Draining is not forgetting: the newest is still the newest.
  EXPECT_EQ(feed->latest()->payload->asText(), "2");
}

TEST_F(IOFeed, AFullFeedDropsTheOldestAndCountsIt) {
  const auto feed = std::make_shared<Feed>("udp://:27020", FeedPolicy{.capacity = 2});
  const Inlet inlet = sigil::io::testing::inletOf(feed);
  inlet.deliver(message("one"));
  inlet.deliver(message("two"));
  inlet.deliver(message("three"));

  EXPECT_EQ(feed->state().dropped, 1u);
  std::optional<Message> arrival = feed->receive();
  ASSERT_TRUE(arrival.has_value());
  EXPECT_EQ(arrival->payload->asText(), "two");
  arrival = feed->receive();
  ASSERT_TRUE(arrival.has_value());
  EXPECT_EQ(arrival->payload->asText(), "three");
  EXPECT_FALSE(feed->receive().has_value());
  // What fell off the front is what a reader could not keep up with.
  // The newest message and the count of them are untouched.
  EXPECT_EQ(feed->latest()->payload->asText(), "three");
  EXPECT_EQ(feed->state().revision, 3u);
}

TEST_F(IOFeed, AClosedFeedKeepsWhatItHoldsAndTakesNothingNew) {
  const auto feed = std::make_shared<Feed>("udp://:27020");
  const Inlet inlet = sigil::io::testing::inletOf(feed);
  inlet.deliver(message("before"));
  feed->close();
  EXPECT_EQ(feed->state().readiness, sigil::io::ReadyState::Closed);

  inlet.deliver(message("after"));
  EXPECT_EQ(feed->state().revision, 1u);
  EXPECT_EQ(feed->latest()->payload->asText(), "before");
  const std::optional<Message> arrival = feed->receive();
  ASSERT_TRUE(arrival.has_value());
  EXPECT_EQ(arrival->payload->asText(), "before");
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
                         opened.localAddress = "udp://[::]:27020";
                         return opened;
                       });

  const std::shared_ptr<Feed> refused = hub.feed("udp://:27020");
  ASSERT_NE(refused, nullptr);
  EXPECT_EQ(refused->state().error, "the port is taken");
  EXPECT_FALSE(refused->state().isOpen());
  EXPECT_TRUE(refused->state().localAddress.empty());

  const std::shared_ptr<Feed> again = hub.feed("udp://:27020");
  // The same feed, opened this time: a reader that held it through the
  // failure is reading the door that opened, with the reason gone.
  EXPECT_EQ(again, refused);
  EXPECT_EQ(opens, 2);
  EXPECT_TRUE(again->state().isOpen());
  EXPECT_TRUE(again->state().error.empty()) << again->state().error;
  EXPECT_EQ(again->state().localAddress, "udp://[::]:27020");

  // A feed that has a door is handed back as it stands: asking twice
  // for a URI that opened is one socket and not two.
  EXPECT_EQ(hub.feed("udp://:27020"), refused);
  EXPECT_EQ(opens, 2);
}

TEST_F(IOFeed, AUriWithNoSchemeIsAFeedWhoseErrorSaysSo) {
  const std::shared_ptr<Feed> feed = hub.feed("no-door-here");
  ASSERT_NE(feed, nullptr);
  EXPECT_NE(feed->state().error.find("scheme"), std::string::npos);
  EXPECT_EQ(feed->state().revision, 0u);
}

TEST_F(IOFeed, AUriWithNoTransportIsAFeedWhoseErrorSaysSo) {
  const std::shared_ptr<Feed> feed = hub.feed("udp://:27020");
  ASSERT_NE(feed, nullptr);
  EXPECT_NE(feed->state().error.find("udp"), std::string::npos);
  EXPECT_FALSE(feed->send(message("nowhere to go")));
  EXPECT_FALSE(feed->send(message("nobody"), {.to = "udp://127.0.0.1:52341"}));
  EXPECT_TRUE(feed->state().localAddress.empty());
}

TEST_F(IOFeed, ATransportsOpenedEndIsClosedExactlyOnceWhenTheFeedGoes) {
  const auto closes = std::make_shared<int>(0);
  hub.setFeedTransport("udp", [closes](std::string_view, Inlet) {
    OpenedFeed opened;
    opened.close = [closes] { ++*closes; };
    opened.localAddress = "udp://[::]:52341";
    return opened;
  });

  std::shared_ptr<Feed> feed = hub.feed("udp://:27020");
  EXPECT_EQ(feed->state().localAddress, "udp://[::]:52341");
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
    opened.localAddress = "udp://[::]:52341";
    return opened;
  });

  const std::shared_ptr<Feed> feed = hub.feed("udp://:27020");
  EXPECT_FALSE(feed->send(message("outward")));
  EXPECT_FALSE(feed->send(message("outward"), {.to = "udp://127.0.0.1:52341"}));
  EXPECT_NE(feed->state().readiness, sigil::io::ReadyState::Closed);
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
  EXPECT_TRUE(feed->send(message("answered"), {.to = "udp://127.0.0.1:52341"}));
  EXPECT_EQ(*answered, "udp://127.0.0.1:52341 answered");
  // There is no end to answer through once it has been closed.
  feed->close();
  EXPECT_FALSE(feed->send(message("too late"), {.to = "udp://127.0.0.1:52341"}));
}

TEST_F(IOFeed, ARecordingReadsBackTheBytesAndTimesItWasWrittenWith) {
  const fs::path path = dir.path / "arrivals.feed";
  {
    RecordingWriter writer(path);
    ASSERT_TRUE(writer.good());
    EXPECT_TRUE(writer.append(Message(shared("first"), {}, seconds(0.25), 7)));
    EXPECT_TRUE(writer.append(Message(shared("second"), {}, seconds(1.5), 8)));
  }

  const std::optional<std::vector<Message>> read = readRecording(path);
  ASSERT_TRUE(read.has_value());
  ASSERT_EQ(read->size(), 2u);
  EXPECT_EQ((*read)[0].payload->asText(), "first");
  EXPECT_EQ((*read)[1].payload->asText(), "second");
  EXPECT_EQ((*read)[0].arrivedAt(), seconds(0.25));
  EXPECT_EQ((*read)[1].arrivedAt(), seconds(1.5));
  // The file carries no revisions: what was written as 7 and 8 reads
  // back numbered from one, because the count belongs to a feed.
  EXPECT_EQ((*read)[0].revision(), 1u);
  EXPECT_EQ((*read)[1].revision(), 2u);
  EXPECT_FALSE(readRecording(dir.path / "never-written.feed").has_value());
}

TEST_F(IOFeed, ARecordingCutShortKeepsTheWholeFramesBeforeTheCut) {
  const fs::path path = dir.path / "killed.feed";
  {
    RecordingWriter writer(path);
    writer.append(Message(shared("whole"), {}, seconds(0.0), 1));
    writer.append(Message(shared("cut through the middle"), {}, seconds(1.0), 2));
  }
  fs::resize_file(path, fs::file_size(path) - 6);

  const std::optional<std::vector<Message>> read = readRecording(path);
  ASSERT_TRUE(read.has_value());
  ASSERT_EQ(read->size(), 1u);
  EXPECT_EQ((*read)[0].payload->asText(), "whole");
}

TEST_F(IOFeed, AReplayedUriPlaysItsRecordingByTheTimeDispatched) {
  const fs::path path = dir.path / "scene.feed";
  {
    RecordingWriter writer(path);
    writer.append(Message(shared("at zero"), {}, seconds(0.0), 1));
    writer.append(Message(shared("at one"), {}, seconds(1.0), 2));
  }
  // Named through the mount table, as a sketch names its own files.
  const std::shared_ptr<Feed> replaying =
      hub.replay("udp://:27020", "res://scene.feed");
  const std::shared_ptr<Feed> feed = hub.feed("udp://:27020");
  // Every later ask for the URI is handed the replaying feed.
  EXPECT_EQ(feed, replaying);
  ASSERT_NE(feed, nullptr);
  EXPECT_TRUE(feed->state().error.empty());
  EXPECT_EQ(feed->state().revision, 0u);  // nothing arrives until time moves

  hub.dispatch(0.0);
  EXPECT_EQ(feed->state().revision, 1u);
  EXPECT_EQ(feed->latest()->payload->asText(), "at zero");
  EXPECT_NE(feed->state().readiness, sigil::io::ReadyState::Closed);

  hub.dispatch(1.0);
  EXPECT_EQ(feed->state().revision, 2u);
  EXPECT_EQ(feed->latest()->payload->asText(), "at one");
  EXPECT_EQ(feed->state().readiness, sigil::io::ReadyState::Closed);  // the recording ran out

  const std::optional<Message> first = feed->receive();
  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(first->arrivedAt().count(), 0.0);  // the recorded time, not a clock's
  // A recording is the messages and not who sent them.
  EXPECT_TRUE(first->sender().empty());
  const std::optional<Message> second = feed->receive();
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(second->arrivedAt().count(), 1.0);
}

TEST_F(IOFeed, AFileThatIsNoRecordingIsAFeedWhoseErrorSaysSo) {
  dir.write("scene.bin", "these are not frames");
  const std::shared_ptr<Feed> feed =
      hub.replay("udp://:27020", (dir.path / "scene.bin").string());
  ASSERT_NE(feed, nullptr);
  EXPECT_FALSE(feed->state().error.empty());
  hub.dispatch(1.0);
  EXPECT_EQ(feed->state().revision, 0u);
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

  const std::optional<std::vector<Message>> read = readRecording(path);
  ASSERT_TRUE(read.has_value());
  ASSERT_EQ(read->size(), 2u);
  EXPECT_EQ((*read)[0].payload->asText(), "one");
  EXPECT_EQ((*read)[1].payload->asText(), "two");
  // A live feed stamps the seconds since it was made, in the order it
  // took the messages.
  EXPECT_GE((*read)[0].arrivedAt().count(), 0.0);
  EXPECT_LE((*read)[0].arrivedAt().count(), (*read)[1].arrivedAt().count());
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

  const std::optional<std::vector<Message>> one =
      readRecording(dir.path / "first.feed");
  ASSERT_TRUE(one.has_value());
  ASSERT_EQ(one->size(), 1u);
  EXPECT_EQ((*one)[0].payload->asText(), "one");
  const std::optional<std::vector<Message>> two =
      readRecording(dir.path / "second.feed");
  ASSERT_TRUE(two.has_value());
  ASSERT_EQ(two->size(), 1u);
  EXPECT_EQ((*two)[0].payload->asText(), "two");

  // A file that cannot be opened is a handle that has already stopped.
  const Recording nowhere = feed->record(dir.path / "absent" / "x.feed");
  EXPECT_TRUE(nowhere.stopped());
  EXPECT_FALSE(feed->state().error.empty());
}

TEST_F(IOFeed, ReplayClosesTheLiveFeedStandingAtItsUri) {
  const fs::path path = dir.path / "taken.feed";
  {
    RecordingWriter writer(path);
    writer.append(Message(shared("recorded"), {}, seconds(0.0), 1));
  }
  hub.setFeedTransport("udp", [](std::string_view, Inlet) {
    return OpenedFeed{};
  });
  const std::shared_ptr<Feed> live = hub.feed("udp://:27020");
  const std::shared_ptr<Feed> replaying =
      hub.replay("udp://:27020", path.string());
  EXPECT_EQ(live->state().readiness, sigil::io::ReadyState::Closed);
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
    if (const std::optional<Message> arrival = feed->receive())
      received.push_back(std::string(arrival->payload->asText()));
  sender.join();

  ASSERT_EQ((int)received.size(), count);
  for (int number = 0; number != count; ++number)
    ASSERT_EQ(received[(size_t)number], std::to_string(number));
  EXPECT_EQ(feed->state().dropped, 0u);
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
    writer.append(Message(shared("at zero"), {}, seconds(0.0), 1));
  }
  const std::shared_ptr<Feed> feed =
      hub.replay("udp://:27020", path.string());

  std::vector<std::string> seen;
  const DispatchLease lease = hub.onDispatch([&seen, feed](double) {
    if (const std::optional<Message> arrival = feed->receive())
      seen.emplace_back(arrival->payload->asText());
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
  const auto received = arrival->receivedAt();
  EXPECT_GE(received, before);
  EXPECT_LE(received, after);
  EXPECT_EQ(received, arrival->receivedAt());
}

TEST_F(IOFeed, ReplayArrivalTimeUsesFirstDispatchAndRecordedSpacing) {
  using Clock = std::chrono::steady_clock;
  using namespace std::chrono_literals;
  const fs::path path = dir.path / "clock.feed";
  {
    RecordingWriter writer(path);
    writer.append(Message(shared("one"), {}, seconds(0.0), 1));
    writer.append(Message(shared("two"), {}, seconds(0.25), 2));
  }
  const std::shared_ptr<Feed> feed = hub.replay("test://clock", path.string());
  const auto before = Clock::now();
  hub.dispatch(900.0);
  const auto after = Clock::now();
  const auto first = feed->receive();
  ASSERT_TRUE(first);
  const auto origin = first->receivedAt();
  EXPECT_GE(origin, before);
  EXPECT_LE(origin, after);
  hub.dispatch(900.25);
  const auto second = feed->receive();
  ASSERT_TRUE(second);
  EXPECT_EQ(second->receivedAt() - origin, 250ms);
}

TEST_F(IOFeed, InvalidOrUnrepresentableArrivalTimesMapToTheClockOrigin) {
  using Clock = std::chrono::steady_clock;
  const auto feed = std::make_shared<Feed>("test://clock");
  const Inlet inlet = sigil::io::testing::inletOf(feed);
  inlet.deliver(message("at the origin"), 0.0);
  const auto origin = feed->receive()->receivedAt();
  for (const double invalid :
       {-1.0, -std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::max(),
        std::chrono::duration<double>(Clock::duration::max()).count()}) {
    inlet.deliver(message("out of range"), invalid);
    EXPECT_EQ(feed->receive()->receivedAt(), origin);
  }
}
