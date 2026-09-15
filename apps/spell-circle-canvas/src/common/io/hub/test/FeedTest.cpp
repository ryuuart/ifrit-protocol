/** @file
 * Feeds: what a reader sees of what a transport delivers — the newest
 * message as bytes and whole, the ones it has not drained, the sender
 * each one names, and the ones a feed too full to hold them dropped —
 * the one door a hub opens per URI and closes when nobody holds it any
 * more, and the recording a feed writes as it runs and plays back
 * afterwards.
 */

#include <gtest/gtest.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/hub/Recording.h>

#include <cstdint>
#include <filesystem>
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
  Bytes bytes;
  bytes.bytes.assign(first, first + text.size());
  return bytes;
}

std::shared_ptr<const Bytes> shared(std::string_view text) {
  return std::make_shared<const Bytes>(message(text));
}

}  // namespace

/** A hub with one scratch directory mounted at res://, which is where
 *  the cases that record write their files. */
class IOFeed : public MountedHub {};

TEST_F(IOFeed, TheLatestIsTheNewestArrivalAndGenerationsCountFromOne) {
  Feed feed("udp://:27020");
  EXPECT_EQ(feed.latest(), nullptr);
  EXPECT_EQ(feed.generation(), 0u);

  feed.deliver(message("first"));
  EXPECT_EQ(feed.generation(), 1u);
  feed.deliver(message("second"));
  ASSERT_NE(feed.latest(), nullptr);
  EXPECT_EQ(feed.latest()->asText(), "second");
  EXPECT_EQ(feed.generation(), 2u);
  EXPECT_EQ(feed.uri(), "udp://:27020");
  EXPECT_TRUE(feed.error().empty());
}

TEST_F(IOFeed, TheNewestIsTheWholeArrivalAndOutlastsDraining) {
  Feed feed("udp://:27020");
  EXPECT_FALSE(feed.newest().has_value());

  feed.deliver(message("first"), "udp://127.0.0.1:52341");
  feed.deliver(message("second"), "udp://127.0.0.1:52342");

  std::optional<Arrival> newest = feed.newest();
  ASSERT_TRUE(newest.has_value());
  EXPECT_EQ(newest->bytes->asText(), "second");
  EXPECT_EQ(newest->from, "udp://127.0.0.1:52342");
  EXPECT_EQ(newest->generation, 2u);
  EXPECT_GE(newest->at, 0.0);

  while (feed.receive().has_value()) {
  }
  // Draining is not taking it: the newest message stands whole after
  // the queue it was also put on is empty.
  newest = feed.newest();
  ASSERT_TRUE(newest.has_value());
  EXPECT_EQ(newest->generation, 2u);
  EXPECT_EQ(newest->from, "udp://127.0.0.1:52342");
}

TEST_F(IOFeed, AnArrivalCarriesTheSenderItWasDeliveredWithAndNoOther) {
  Feed feed("udp://:27020");
  feed.deliver(message("from a peer"), "udp://127.0.0.1:52341");
  feed.deliver(message("from nowhere named"));

  std::optional<Arrival> arrival = feed.receive();
  ASSERT_TRUE(arrival.has_value());
  EXPECT_EQ(arrival->from, "udp://127.0.0.1:52341");
  arrival = feed.receive();
  ASSERT_TRUE(arrival.has_value());
  // A transport with no way of knowing who sent a message names
  // nobody, and the arrival is one all the same.
  EXPECT_TRUE(arrival->from.empty());
  EXPECT_EQ(arrival->bytes->asText(), "from nowhere named");
}

TEST_F(IOFeed, ReceiveHandsOutEveryArrivalInOrderAndThenNothing) {
  Feed feed("udp://:27020");
  for (int number = 0; number != 3; ++number)
    feed.deliver(message(std::to_string(number)));

  for (int number = 0; number != 3; ++number) {
    const std::optional<Arrival> arrival = feed.receive();
    ASSERT_TRUE(arrival.has_value());
    EXPECT_EQ(arrival->generation, (uint64_t)number + 1);
    EXPECT_EQ(arrival->bytes->asText(), std::to_string(number));
    EXPECT_GE(arrival->at, 0.0);
  }
  EXPECT_FALSE(feed.receive().has_value());
  // Draining is not forgetting: the newest is still the newest.
  EXPECT_EQ(feed.latest()->asText(), "2");
}

TEST_F(IOFeed, AFullFeedDropsTheOldestAndCountsIt) {
  Feed feed("udp://:27020", {.capacity = 2});
  feed.deliver(message("one"));
  feed.deliver(message("two"));
  feed.deliver(message("three"));

  EXPECT_EQ(feed.dropped(), 1u);
  std::optional<Arrival> arrival = feed.receive();
  ASSERT_TRUE(arrival.has_value());
  EXPECT_EQ(arrival->bytes->asText(), "two");
  arrival = feed.receive();
  ASSERT_TRUE(arrival.has_value());
  EXPECT_EQ(arrival->bytes->asText(), "three");
  EXPECT_FALSE(feed.receive().has_value());
  // What fell off the front is what a reader could not keep up with.
  // The newest message and the count of them are untouched.
  EXPECT_EQ(feed.latest()->asText(), "three");
  EXPECT_EQ(feed.generation(), 3u);
}

TEST_F(IOFeed, AClosedFeedKeepsWhatItHoldsAndTakesNothingNew) {
  Feed feed("udp://:27020");
  feed.deliver(message("before"));
  feed.close();
  EXPECT_TRUE(feed.closed());

  feed.deliver(message("after"));
  EXPECT_EQ(feed.generation(), 1u);
  EXPECT_EQ(feed.latest()->asText(), "before");
  const std::optional<Arrival> arrival = feed.receive();
  ASSERT_TRUE(arrival.has_value());
  EXPECT_EQ(arrival->bytes->asText(), "before");
  EXPECT_FALSE(feed.receive().has_value());
}

TEST_F(IOFeed, AHubHoldsOneFeedPerUriWhileSomebodyHoldsItAndOpensAgainAfter) {
  std::vector<std::string> opened;
  hub.setFeedTransport("udp",
                       [&opened](std::string_view uri, std::weak_ptr<Feed>) {
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
  EXPECT_TRUE(feed->address().empty());
}

TEST_F(IOFeed, ATransportsOpenedEndIsClosedExactlyOnceWhenTheFeedGoes) {
  const auto closes = std::make_shared<int>(0);
  hub.setFeedTransport("udp", [closes](std::string_view, std::weak_ptr<Feed>) {
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
  hub.setFeedTransport("udp", [sent](std::string_view, std::weak_ptr<Feed>) {
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
  hub.setFeedTransport("udp", [](std::string_view, std::weak_ptr<Feed>) {
    OpenedFeed opened;  // listening only: no way back out
    opened.address = "udp://[::]:52341";
    return opened;
  });

  const std::shared_ptr<Feed> feed = hub.feed("udp://:27020");
  EXPECT_FALSE(feed->send(message("outward")));
  EXPECT_FALSE(feed->closed());
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

TEST_F(IOFeed, ARecordingMountedOnAUriReplaysByTheTimeDispatched) {
  const fs::path path = dir.path / "scene.feed";
  {
    RecordingWriter writer(path);
    writer.append({1, 0.0, shared("at zero")});
    writer.append({2, 1.0, shared("at one")});
  }
  // The URI is mounted straight onto the file: nothing is beneath it.
  hub.mount("udp://:27020", path);

  const std::shared_ptr<Feed> feed = hub.feed("udp://:27020");
  ASSERT_NE(feed, nullptr);
  EXPECT_TRUE(feed->error().empty());
  EXPECT_EQ(feed->generation(), 0u);  // nothing arrives until time moves

  hub.dispatch(0.0);
  EXPECT_EQ(feed->generation(), 1u);
  EXPECT_EQ(feed->latest()->asText(), "at zero");
  EXPECT_FALSE(feed->closed());

  hub.dispatch(1.0);
  EXPECT_EQ(feed->generation(), 2u);
  EXPECT_EQ(feed->latest()->asText(), "at one");
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
  hub.mount("udp://:27020", dir.path / "scene.bin");

  const std::shared_ptr<Feed> feed = hub.feed("udp://:27020");
  ASSERT_NE(feed, nullptr);
  EXPECT_FALSE(feed->error().empty());
  hub.dispatch(1.0);
  EXPECT_EQ(feed->generation(), 0u);
}

TEST_F(IOFeed, RecordingALiveFeedWritesWhatArrives) {
  const fs::path path = dir.path / "live.feed";
  Feed feed("udp://:27020");
  feed.record(path);
  feed.deliver(message("one"));
  feed.deliver(message("two"));
  feed.record({});  // stopped: the file stands as it is
  feed.deliver(message("three"));

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

TEST_F(IOFeed, ArrivalsFromAnotherThreadAreAllReceivedInOrder) {
  const int count = 1000;
  Feed feed("udp://:27020", {.capacity = 2048});
  std::thread sender([&feed, count] {
    for (int number = 0; number != count; ++number)
      feed.deliver(message(std::to_string(number)));
  });

  std::vector<std::string> received;
  while ((int)received.size() != count)
    if (const std::optional<Arrival> arrival = feed.receive())
      received.push_back(std::string(arrival->bytes->asText()));
  sender.join();

  ASSERT_EQ((int)received.size(), count);
  for (int number = 0; number != count; ++number)
    ASSERT_EQ(received[(size_t)number], std::to_string(number));
  EXPECT_EQ(feed.dropped(), 0u);
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
  hub.mount("udp://:27020", path);
  const std::shared_ptr<Feed> feed = hub.feed("udp://:27020");

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
