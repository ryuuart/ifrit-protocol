#include <gtest/gtest.h>

#include <atomic>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/post.hpp>
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include "UdpReceiver.h"

namespace {

using boost::asio::ip::udp;
using spellcircle::UdpReceiver;
using namespace std::chrono_literals;

bool pumpUntil(boost::asio::io_context& context, const auto& ready) {
  const auto deadline = std::chrono::steady_clock::now() + 2s;
  while (!ready() && std::chrono::steady_clock::now() < deadline) {
    if (context.stopped()) context.restart();
    context.run_one_for(10ms);
  }
  return ready();
}

std::uint16_t bindReceiver(boost::asio::io_context& context,
                           UdpReceiver& receiver,
                           UdpReceiver::DatagramHandler handler) {
  struct Binding {
    std::uint16_t port = 0;
    bool completed = false;
  };
  const auto binding = std::make_shared<Binding>();
  receiver.start(0, std::move(handler), [binding](UdpReceiver::Status status) {
    EXPECT_FALSE(status.error) << status.error.message();
    binding->port = status.port;
    binding->completed = true;
  });
  EXPECT_TRUE(pumpUntil(context, [&] { return binding->completed; }));
  return binding->port;
}

void send(udp::socket& socket, std::uint16_t port,
          std::string_view bytes = "scene") {
  socket.send_to(boost::asio::buffer(bytes.data(), bytes.size()),
                 udp::endpoint(boost::asio::ip::address_v4::loopback(), port));
}

struct RunningContext {
  boost::asio::io_context context;
  boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work{
      context.get_executor()};
  std::vector<std::jthread> workers;

  explicit RunningContext(int count = 1) {
    for (int i = 0; i < count; ++i)
      workers.emplace_back([this] { context.run(); });
  }
  ~RunningContext() {
    context.stop();
    for (auto& worker : workers) worker.join();
  }
};

}  // namespace

TEST(UdpReceiver, RunsOnTheProvidedContextAndTimestampsArrivals) {
  boost::asio::io_context context;
  UdpReceiver receiver(context.get_executor());
  udp::socket sender(context, udp::endpoint(udp::v4(), 0));
  const auto thread = std::this_thread::get_id();
  bool bound = false;
  bool received = false;
  const auto before = std::chrono::steady_clock::now();
  receiver.start(
      0,
      [&](spellcircle::Datagram packet) {
        EXPECT_EQ(std::this_thread::get_id(), thread);
        EXPECT_EQ(std::string(packet.payload.begin(), packet.payload.end()),
                  "scene");
        EXPECT_EQ(
            packet.source,
            "127.0.0.1:" + std::to_string(sender.local_endpoint().port()));
        EXPECT_GE(packet.receivedAt, before);
        EXPECT_LE(packet.receivedAt, std::chrono::steady_clock::now());
        received = true;
        receiver.stop();
      },
      [&](UdpReceiver::Status status) {
        EXPECT_EQ(std::this_thread::get_id(), thread);
        ASSERT_FALSE(status.error) << status.error.message();
        ASSERT_NE(status.port, 0);
        bound = true;
        send(sender, status.port);
      });
  EXPECT_FALSE(bound);
  EXPECT_FALSE(received);
  ASSERT_TRUE(pumpUntil(context, [&] { return received; }));
  context.run();
}

TEST(UdpReceiver, ReceiversShareOneContextAndStopLeavesOtherWorkRunning) {
  boost::asio::io_context context;
  UdpReceiver first(context.get_executor());
  UdpReceiver second(context.get_executor());
  udp::socket sender(context, udp::endpoint(udp::v4(), 0));
  int firstPackets = 0;
  int secondPackets = 0;
  const auto firstPort =
      bindReceiver(context, first, [&](auto) { ++firstPackets; });
  const auto secondPort =
      bindReceiver(context, second, [&](auto) { ++secondPackets; });
  ASSERT_NE(firstPort, 0);
  ASSERT_NE(secondPort, 0);
  send(sender, firstPort);
  ASSERT_TRUE(pumpUntil(context, [&] { return firstPackets == 1; }));
  first.stop();
  bool otherWork = false;
  boost::asio::post(context, [&] { otherWork = true; });
  send(sender, secondPort);
  ASSERT_TRUE(
      pumpUntil(context, [&] { return secondPackets == 1 && otherWork; }));
  EXPECT_EQ(firstPackets, 1);
  second.stop();
  context.run();
}

TEST(UdpReceiver, StopBeforeRunSuppressesTheQueuedBinding) {
  boost::asio::io_context context;
  UdpReceiver receiver(context.get_executor());
  receiver.start(
      0, [](auto) { ADD_FAILURE() << "retired datagram"; },
      [](auto) { ADD_FAILURE() << "retired bind"; });
  receiver.stop();
  bool otherWork = false;
  boost::asio::post(context, [&] { otherWork = true; });
  context.run();
  EXPECT_TRUE(otherWork);
}

TEST(UdpReceiver, RebindingRetiresQueuedPacketsAndReusesTheSamePort) {
  boost::asio::io_context context;
  UdpReceiver receiver(context.get_executor());
  udp::socket sender(context, udp::endpoint(udp::v4(), 0));
  int oldPackets = 0;
  const auto port =
      bindReceiver(context, receiver, [&](auto) { ++oldPackets; });
  ASSERT_NE(port, 0);
  send(sender, port, "old");
  bool rebound = false;
  bool received = false;
  receiver.start(
      port,
      [&](spellcircle::Datagram packet) {
        EXPECT_EQ(std::string(packet.payload.begin(), packet.payload.end()),
                  "new");
        received = true;
        receiver.stop();
      },
      [&](UdpReceiver::Status status) {
        ASSERT_FALSE(status.error) << status.error.message();
        EXPECT_EQ(status.port, port);
        rebound = true;
        send(sender, port, "new");
      });
  ASSERT_TRUE(pumpUntil(context, [&] { return received; }));
  EXPECT_TRUE(rebound);
  EXPECT_EQ(oldPackets, 0);
  context.run();
}

TEST(UdpReceiver, OnlyTheLatestQueuedStartBinds) {
  boost::asio::io_context context;
  UdpReceiver receiver(context.get_executor());
  receiver.start(0, {}, [](auto) { ADD_FAILURE() << "obsolete bind"; });
  receiver.stop();
  receiver.start(0, {}, [](auto) { ADD_FAILURE() << "obsolete bind"; });
  bool bound = false;
  receiver.start(0, {}, [&](UdpReceiver::Status status) {
    EXPECT_FALSE(status.error);
    bound = true;
    receiver.stop();
  });
  context.run();
  EXPECT_TRUE(bound);
}

TEST(UdpReceiver, BindFailureIsReportedAndDoesNotPoisonTheContext) {
  boost::asio::io_context context;
  udp::socket occupied(context, udp::endpoint(udp::v6(), 0));
  UdpReceiver receiver(context.get_executor());
  bool failed = false;
  bool recovered = false;
  receiver.start(occupied.local_endpoint().port(), {},
                 [&](UdpReceiver::Status status) {
                   EXPECT_EQ(status.error, boost::asio::error::address_in_use);
                   failed = true;
                   receiver.start(0, {}, [&](UdpReceiver::Status retry) {
                     EXPECT_FALSE(retry.error);
                     recovered = true;
                     receiver.stop();
                   });
                 });
  context.run();
  EXPECT_TRUE(failed);
  EXPECT_TRUE(recovered);
}

TEST(UdpReceiver, CanDestroyTheReceiverInsideItsOwnCallback) {
  boost::asio::io_context context;
  auto receiver = std::make_unique<UdpReceiver>(context.get_executor());
  udp::socket sender(context, udp::endpoint(udp::v4(), 0));
  int packets = 0;
  const auto port = bindReceiver(context, *receiver, [&](auto) {
    ++packets;
    receiver.reset();
  });
  ASSERT_NE(port, 0);
  send(sender, port);
  send(sender, port);
  ASSERT_TRUE(pumpUntil(context, [&] { return !receiver; }));
  context.run();
  EXPECT_EQ(packets, 1);
}

TEST(UdpReceiver, DestructionDoesNotRunOrRestartAStoppedContext) {
  boost::asio::io_context context;
  auto receiver = std::make_unique<UdpReceiver>(context.get_executor());
  ASSERT_NE(bindReceiver(context, *receiver, [](auto) {}), 0);
  context.stop();
  receiver.reset();
  EXPECT_TRUE(context.stopped());
  bool otherWork = false;
  boost::asio::post(context, [&] { otherWork = true; });
  EXPECT_EQ(context.poll(), 0);
  EXPECT_FALSE(otherWork);
  context.restart();
  context.run();
  EXPECT_TRUE(otherWork);
}

TEST(UdpReceiver, ContextCanDestroyUnrunAndCanceledOperations) {
  boost::asio::io_context context;
  {
    UdpReceiver receiver(context.get_executor());
    receiver.start(0, [](auto) { ADD_FAILURE(); }, [](auto) { ADD_FAILURE(); });
  }
  // Destruction of the context discards queued socket work and its captures.
}

TEST(UdpReceiver, MutableCallbackStatePersistsAcrossDatagrams) {
  boost::asio::io_context context;
  UdpReceiver receiver(context.get_executor());
  udp::socket sender(context, udp::endpoint(udp::v4(), 0));
  std::vector<int> counts;
  const auto port =
      bindReceiver(context, receiver,
                   [&, count = 0](auto) mutable { counts.push_back(++count); });
  send(sender, port);
  ASSERT_TRUE(pumpUntil(context, [&] { return counts.size() == 1; }));
  send(sender, port);
  ASSERT_TRUE(pumpUntil(context, [&] { return counts.size() == 2; }));
  EXPECT_EQ(counts, (std::vector<int>{1, 2}));
  receiver.stop();
  context.run();
}

TEST(UdpReceiver, IPv6SourcesAreUnambiguous) {
  boost::asio::io_context context;
  UdpReceiver receiver(context.get_executor());
  udp::socket sender(context, udp::endpoint(udp::v6(), 0));
  bool received = false;
  const auto port =
      bindReceiver(context, receiver, [&](spellcircle::Datagram packet) {
        EXPECT_EQ(packet.source,
                  "[::1]:" + std::to_string(sender.local_endpoint().port()));
        EXPECT_TRUE(packet.payload.empty());
        received = true;
        receiver.stop();
      });
  sender.send_to(boost::asio::buffer("", 0),
                 udp::endpoint(boost::asio::ip::address_v6::loopback(), port));
  ASSERT_TRUE(pumpUntil(context, [&] { return received; }));
  context.run();
}

TEST(UdpReceiver, CallbackExceptionCanBeCaughtByTheHostAndReceivingResumed) {
  boost::asio::io_context context;
  UdpReceiver receiver(context.get_executor());
  udp::socket sender(context, udp::endpoint(udp::v4(), 0));
  int packets = 0;
  const auto port = bindReceiver(context, receiver, [&](auto) {
    if (++packets == 1) throw std::runtime_error("consumer failure");
    receiver.stop();
  });
  send(sender, port);
  EXPECT_THROW(context.run_for(2s), std::runtime_error);
  send(sender, port);
  ASSERT_TRUE(pumpUntil(context, [&] { return packets == 2; }));
  context.run();
}

TEST(UdpReceiver, StopWaitsForAnExecutingCallbackWithoutJoiningTheContext) {
  RunningContext runtime(2);
  udp::socket sender(runtime.context, udp::endpoint(udp::v4(), 0));
  std::promise<void> entered;
  auto enteredFuture = entered.get_future();
  std::promise<void> release;
  auto released = release.get_future().share();
  UdpReceiver receiver(runtime.context.get_executor());
  receiver.start(
      0,
      [&](auto) {
        entered.set_value();
        released.wait();
      },
      [&](UdpReceiver::Status status) {
        EXPECT_FALSE(status.error);
        send(sender, status.port);
      });
  const auto started = enteredFuture.wait_for(2s);
  if (started != std::future_status::ready) {
    release.set_value();
    receiver.stop();
    FAIL() << "no datagram callback";
  }
  std::promise<void> stopping;
  auto stoppingFuture = stopping.get_future();
  auto stopped = std::async(std::launch::async, [&] {
    stopping.set_value();
    receiver.stop();
  });
  stoppingFuture.wait();
  EXPECT_EQ(stopped.wait_for(20ms), std::future_status::timeout);
  release.set_value();
  ASSERT_EQ(stopped.wait_for(2s), std::future_status::ready);
  stopped.get();
  std::promise<void> otherWork;
  auto otherFuture = otherWork.get_future();
  boost::asio::post(runtime.context, [&] { otherWork.set_value(); });
  EXPECT_EQ(otherFuture.wait_for(2s), std::future_status::ready);
}

TEST(UdpReceiver, ConcurrentControlsAndWorkersRetireEveryObsoleteBinding) {
  RunningContext runtime(2);
  std::atomic<int> callbacks = 0;
  std::promise<void> completed;
  auto completion = completed.get_future();
  UdpReceiver receiver(runtime.context.get_executor());
  const auto control = [&] {
    for (int i = 0; i < 100; ++i) {
      receiver.start(0, {}, [&](auto) { ++callbacks; });
      receiver.stop();
    }
  };
  std::jthread first(control);
  std::jthread second(control);
  first.join();
  second.join();
  receiver.stop();
  const int retired = callbacks.load();
  receiver.start(0, {}, [&](UdpReceiver::Status status) {
    EXPECT_FALSE(status.error);
    receiver.stop();
    completed.set_value();
  });
  EXPECT_EQ(completion.wait_for(2s), std::future_status::ready);
  EXPECT_EQ(callbacks.load(), retired);
}
