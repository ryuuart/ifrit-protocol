#import "SCKEngine.h"
#import "SCKNetworkRuntimeInternal.h"

#include <gtest/gtest.h>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/post.hpp>

#include <chrono>
#include <cstdint>
#include <vector>

#include "SpellCircle_generated.h"

@interface EngineRecorder : NSObject <SCKEngineDelegate>
@property(nonatomic, readonly, strong) NSMutableArray<NSString *> *changes;
@property(nonatomic) NSUInteger packets;
@property(nonatomic) NSUInteger scenes;
@property(nonatomic) NSUInteger feeds;
@property(nonatomic, copy) NSString *lastSource;
@property(nonatomic, copy) NSString *lastMessage;
@end

@implementation EngineRecorder
- (instancetype)init {
  self = [super init];
  if (self) _changes = [NSMutableArray array];
  return self;
}
- (void)engineStatusDidChange:(SCKEngine *)engine {
  [_changes addObject:engine.statusText];
}
- (void)engineDidRenderScene:(SCKEngine *)engine {
}
- (void)engineSceneDidChange:(SCKEngine *)engine {
  ++_scenes;
}
- (void)enginePacketRateDidChange:(SCKEngine *)engine {
  ++_packets;
}
- (void)engine:(SCKEngine *)engine didAppendFeedEntry:(SCKFeedEntry *)entry {
  ++_feeds;
  _lastSource = entry.source;
  _lastMessage = entry.message;
}
@end

namespace {

/** A queue marker observes every callback enqueued before this call. The
 *  deadline turns a missing main-queue integration into a test failure. */
bool drainMainQueue() {
  __block bool reached = false;
  dispatch_async(dispatch_get_main_queue(), ^{
    reached = true;
  });
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
  while (!reached && std::chrono::steady_clock::now() < deadline)
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, true);
  return reached;
}

std::vector<std::uint8_t> circleScene(const char *name) {
  flatbuffers::FlatBufferBuilder builder;
  const SpellCircle::Vec2 center(64, 64);
  const auto circle = SpellCircle::CreateCircleDirect(builder, &center, name, 20);
  const std::vector<flatbuffers::Offset<SpellCircle::Circle>> circles{circle};
  SpellCircle::FinishSceneBuffer(
      builder, SpellCircle::CreateSceneDirect(builder, &circles, nullptr, nullptr, 128, 128));
  return {builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize()};
}

TEST(SpellCircleMacEngine, LoopbackSeparatesChangedDuplicateAndRetiredPackets) {
  ASSERT_TRUE(NSThread.isMainThread);
  boost::asio::io_context context;
  auto work = boost::asio::make_work_guard(context);
  boost::asio::ip::udp::socket reservation(context, {boost::asio::ip::udp::v6(), 0});
  const std::uint16_t port = reservation.local_endpoint().port();
  reservation.close();
  boost::asio::ip::udp::socket sender(context, boost::asio::ip::udp::v4());
  const boost::asio::ip::udp::endpoint destination(boost::asio::ip::address_v4::loopback(), port);

  @autoreleasepool {
    SCKNetworkRuntime *runtime =
        [[SCKNetworkRuntime alloc] initWithExecutor:context.get_executor()];
    SCKEngine *engine = [[SCKEngine alloc] initWithRuntime:runtime];
    engine.canvasWidth = 128;
    engine.canvasHeight = 128;
    engine.port = port;
    EngineRecorder *recorder = [[EngineRecorder alloc] init];
    engine.delegate = recorder;
    [engine start];
    context.poll();
    ASSERT_TRUE(drainMainQueue());
    ASSERT_TRUE(engine.listening) << engine.statusText.UTF8String;

    const auto original = circleScene("first");
    sender.send_to(boost::asio::buffer(original), destination);
    // The receive is the only outstanding I/O completion. Running it queues
    // a main-thread delivery, which is observed separately below.
    ASSERT_EQ(context.run_one_for(std::chrono::seconds(1)), 1u);
    ASSERT_TRUE(drainMainQueue());
    EXPECT_EQ(recorder.packets, 1u);
    EXPECT_EQ(recorder.scenes, 1u);
    EXPECT_EQ(recorder.feeds, 1u);
    EXPECT_TRUE([recorder.lastSource hasPrefix:@"127.0.0.1:"]);
    EXPECT_TRUE([recorder.lastMessage containsString:@"1 circles"]);

    sender.send_to(boost::asio::buffer(original), destination);
    ASSERT_EQ(context.run_one_for(std::chrono::seconds(1)), 1u);
    ASSERT_TRUE(drainMainQueue());
    EXPECT_EQ(recorder.packets, 2u);
    EXPECT_EQ(recorder.scenes, 1u);
    EXPECT_EQ(recorder.feeds, 1u);

    const auto changed = circleScene("replacement");
    sender.send_to(boost::asio::buffer(changed), destination);
    ASSERT_EQ(context.run_one_for(std::chrono::seconds(1)), 1u);
    [engine stop];
    ASSERT_TRUE(drainMainQueue());
    EXPECT_EQ(recorder.packets, 2u);
    EXPECT_EQ(recorder.scenes, 1u);
    EXPECT_EQ(recorder.feeds, 1u);
    EXPECT_FALSE(engine.listening);
  }
}

TEST(SpellCircleMacEngine, StopDiscardsAStatusAlreadyQueuedToTheMainThread) {
  ASSERT_TRUE(NSThread.isMainThread);
  boost::asio::io_context context;
  auto work = boost::asio::make_work_guard(context);
  const boost::asio::ip::udp::socket occupied(context, {boost::asio::ip::udp::v6(), 0});
  @autoreleasepool {
    SCKNetworkRuntime *runtime =
        [[SCKNetworkRuntime alloc] initWithExecutor:context.get_executor()];
    SCKEngine *engine = [[SCKEngine alloc] initWithRuntime:runtime];
    engine.port = occupied.local_endpoint().port();
    [engine start];
    ASSERT_TRUE(engine.starting);
    context.poll();

    [engine stop];
    ASSERT_TRUE(drainMainQueue());
    EXPECT_FALSE(engine.starting);
    EXPECT_FALSE(engine.listening);
    EXPECT_TRUE([engine.statusText isEqualToString:@"Stopped"]);
  }
}

TEST(SpellCircleMacEngine, RebindingWhileStartingReportsOnlyTheNewestPort) {
  ASSERT_TRUE(NSThread.isMainThread);
  boost::asio::io_context context;
  auto work = boost::asio::make_work_guard(context);
  const boost::asio::ip::udp::socket first(context, {boost::asio::ip::udp::v6(), 0});
  const boost::asio::ip::udp::socket second(context, {boost::asio::ip::udp::v6(), 0});
  @autoreleasepool {
    SCKNetworkRuntime *runtime =
        [[SCKNetworkRuntime alloc] initWithExecutor:context.get_executor()];
    SCKEngine *engine = [[SCKEngine alloc] initWithRuntime:runtime];
    EngineRecorder *recorder = [[EngineRecorder alloc] init];
    engine.delegate = recorder;
    engine.port = first.local_endpoint().port();
    [engine start];
    context.poll();
    [recorder.changes removeAllObjects];

    engine.port = second.local_endpoint().port();
    ASSERT_TRUE(engine.starting);
    context.poll();
    ASSERT_TRUE(drainMainQueue());

    EXPECT_FALSE(engine.starting);
    EXPECT_FALSE(engine.listening);
    ASSERT_EQ(recorder.changes.count, 2u);
    NSString *portText =
        [NSString stringWithFormat:@":%u", static_cast<unsigned>(second.local_endpoint().port())];
    for (NSString *status in recorder.changes) EXPECT_TRUE([status containsString:portText]);
  }
}

TEST(SpellCircleMacEngine, DestructionLeavesTheSuppliedContextRunning) {
  ASSERT_TRUE(NSThread.isMainThread);
  boost::asio::io_context context;
  auto work = boost::asio::make_work_guard(context);
  @autoreleasepool {
    SCKNetworkRuntime *runtime =
        [[SCKNetworkRuntime alloc] initWithExecutor:context.get_executor()];
    SCKEngine *engine = [[SCKEngine alloc] initWithRuntime:runtime];
    [engine start];
    engine = nil;
    runtime = nil;
  }
  ASSERT_FALSE(context.stopped());
  bool ran = false;
  boost::asio::post(context, [&] { ran = true; });
  context.poll();
  EXPECT_TRUE(ran);
  EXPECT_FALSE(context.stopped());
  ASSERT_TRUE(drainMainQueue());
}

}  // namespace
