/** @file What the native engine's door reports when it opens, what a
 *  drain hands the session, and what a closed door leaves behind. */

#import "SCKEngine.h"

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <functional>
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

using Clock = std::chrono::steady_clock;

/** One socket bound the way the UDP transport binds its own: a dual-stack
 *  IPv6 socket, so a port this holds is a port the engine cannot take.
 *  -1 when the bind failed. */
int bindDualStack(std::uint16_t port) {
  const int handle = ::socket(AF_INET6, SOCK_DGRAM, 0);
  if (handle < 0) return -1;
  int both = 0;
  ::setsockopt(handle, IPPROTO_IPV6, IPV6_V6ONLY, &both, sizeof(both));
  sockaddr_in6 address{};
  address.sin6_family = AF_INET6;
  address.sin6_addr = in6addr_any;
  address.sin6_port = htons(port);
  if (::bind(handle, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0) {
    ::close(handle);
    return -1;
  }
  return handle;
}

/** The port a socket was given. */
std::uint16_t portOf(int handle) {
  sockaddr_in6 address{};
  socklen_t size = sizeof(address);
  if (::getsockname(handle, reinterpret_cast<sockaddr *>(&address), &size) != 0) return 0;
  return ntohs(address.sin6_port);
}

/** A port nobody holds: one taken the transport's way and given back. */
std::uint16_t availablePort() {
  const int handle = bindDualStack(0);
  if (handle < 0) return 0;
  const std::uint16_t port = portOf(handle);
  ::close(handle);
  return port;
}

/** One datagram to a port on loopback, from a socket of its own; answers
 *  the port that socket was given, which is what the arrival should name
 *  it by. */
std::uint16_t sendTo(std::uint16_t port, const std::vector<std::uint8_t> &payload) {
  const int handle = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (handle < 0) return 0;
  sockaddr_in destination{};
  destination.sin_family = AF_INET;
  destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  destination.sin_port = htons(port);
  ::sendto(handle, payload.data(), payload.size(), 0,
           reinterpret_cast<const sockaddr *>(&destination), sizeof(destination));
  sockaddr_in bound{};
  socklen_t size = sizeof(bound);
  ::getsockname(handle, reinterpret_cast<sockaddr *>(&bound), &size);
  ::close(handle);
  return ntohs(bound.sin_port);
}

/** Reads the engine's door and runs the main queue until @p complete
 *  holds. The datagram lands on the transport's own thread, so what this
 *  waits for is the drain, which is the call under test. */
bool pumpUntil(SCKEngine *engine, const std::function<bool()> &complete) {
  const auto deadline = Clock::now() + std::chrono::seconds(2);
  do {
    [engine readArrivals];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.001, true);
    if (complete()) return true;
  } while (Clock::now() < deadline);
  return complete();
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

TEST(SpellCircleMacEngine, LoopbackSeparatesChangedDuplicateAndInvalidPackets) {
  ASSERT_TRUE(NSThread.isMainThread);
  @autoreleasepool {
    SCKEngine *engine = [[SCKEngine alloc] init];
    engine.canvasWidth = 128;
    engine.canvasHeight = 128;
    engine.port = availablePort();
    ASSERT_NE(engine.port, 0);
    EngineRecorder *recorder = [[EngineRecorder alloc] init];
    engine.delegate = recorder;
    [engine start];
    ASSERT_TRUE(engine.listening) << engine.statusText.UTF8String;
    const auto port = static_cast<std::uint16_t>(engine.port);

    const auto original = circleScene("first");
    const std::uint16_t sender = sendTo(port, original);
    ASSERT_NE(sender, 0);
    ASSERT_TRUE(pumpUntil(engine, [&] { return recorder.feeds == 1u; }));
    EXPECT_EQ(recorder.packets, 1u);
    EXPECT_EQ(recorder.scenes, 1u);
    NSString *senderText =
        [NSString stringWithFormat:@"127.0.0.1:%u", static_cast<unsigned>(sender)];
    EXPECT_TRUE([recorder.lastSource isEqualToString:senderText])
        << recorder.lastSource.UTF8String;
    EXPECT_TRUE([recorder.lastMessage containsString:@"1 circles"]);

    // The same bytes again count as an arrival and change nothing else.
    sendTo(port, original);
    ASSERT_TRUE(pumpUntil(engine, [&] { return recorder.packets == 2u; }));
    EXPECT_EQ(recorder.scenes, 1u);
    EXPECT_EQ(recorder.feeds, 1u);

    // A payload that does not verify leaves every count where it was.
    sendTo(port, std::vector<std::uint8_t>{1, 2, 3, 4});
    const auto settle = Clock::now() + std::chrono::milliseconds(200);
    while (Clock::now() < settle) {
      [engine readArrivals];
      CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.001, true);
    }
    EXPECT_EQ(recorder.packets, 2u);
    EXPECT_EQ(recorder.scenes, 1u);
    EXPECT_EQ(recorder.feeds, 1u);

    // Different bytes are a new scene, and the door kept delivering
    // across the one it could not read.
    sendTo(port, circleScene("replacement"));
    ASSERT_TRUE(pumpUntil(engine, [&] { return recorder.feeds == 2u; }));
    EXPECT_EQ(recorder.packets, 3u);
    EXPECT_EQ(recorder.scenes, 2u);
  }
}

TEST(SpellCircleMacEngine, APortSomebodyElseHoldsIsReportedWhenTheDoorIsOpened) {
  ASSERT_TRUE(NSThread.isMainThread);
  const int holder = bindDualStack(0);
  ASSERT_GE(holder, 0);
  const std::uint16_t port = portOf(holder);
  ASSERT_NE(port, 0);
  @autoreleasepool {
    SCKEngine *engine = [[SCKEngine alloc] init];
    engine.port = port;
    [engine start];
    // The bind happens inside start(), so its outcome is readable the
    // moment it returns and nothing has to be pumped for.
    EXPECT_FALSE(engine.listening);
    NSString *portText = [NSString stringWithFormat:@":%u", static_cast<unsigned>(port)];
    EXPECT_TRUE([engine.statusText containsString:portText]) << engine.statusText.UTF8String;
    EXPECT_TRUE([engine.statusText containsString:@"failed"]) << engine.statusText.UTF8String;
  }
  ::close(holder);
}

TEST(SpellCircleMacEngine, APortChangeReopensTheDoorAndTheStatusNamesTheNewPort) {
  ASSERT_TRUE(NSThread.isMainThread);
  @autoreleasepool {
    SCKEngine *engine = [[SCKEngine alloc] init];
    EngineRecorder *recorder = [[EngineRecorder alloc] init];
    engine.delegate = recorder;
    engine.port = availablePort();
    ASSERT_NE(engine.port, 0);
    [engine start];
    ASSERT_TRUE(engine.listening) << engine.statusText.UTF8String;

    const std::uint16_t replacement = availablePort();
    ASSERT_NE(replacement, 0);
    ASSERT_NE(static_cast<std::uint16_t>(engine.port), replacement);
    engine.port = replacement;
    EXPECT_TRUE(engine.listening);
    NSString *expected =
        [NSString stringWithFormat:@"Listening on UDP :%u", static_cast<unsigned>(replacement)];
    EXPECT_TRUE([engine.statusText isEqualToString:expected]) << engine.statusText.UTF8String;

    // The new port is the one that receives.
    ASSERT_NE(sendTo(replacement, circleScene("moved")), 0);
    ASSERT_TRUE(pumpUntil(engine, [&] { return recorder.feeds == 1u; }));
  }
}

TEST(SpellCircleMacEngine, AClosedDoorLeavesNothingToRead) {
  ASSERT_TRUE(NSThread.isMainThread);
  @autoreleasepool {
    SCKEngine *engine = [[SCKEngine alloc] init];
    EngineRecorder *recorder = [[EngineRecorder alloc] init];
    engine.delegate = recorder;
    engine.port = availablePort();
    ASSERT_NE(engine.port, 0);
    [engine start];
    ASSERT_TRUE(engine.listening) << engine.statusText.UTF8String;
    const auto port = static_cast<std::uint16_t>(engine.port);

    [engine stop];
    EXPECT_FALSE(engine.listening);
    EXPECT_TRUE([engine.statusText isEqualToString:@"Stopped"]);

    // Nothing is listening for it any more, and nothing the closed door
    // may still hold is read: the drain answers an engine with no door.
    sendTo(port, circleScene("after"));
    const auto deadline = Clock::now() + std::chrono::milliseconds(200);
    while (Clock::now() < deadline) {
      [engine readArrivals];
      CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.001, true);
    }
    EXPECT_EQ(recorder.packets, 0u);
    EXPECT_EQ(recorder.feeds, 0u);
  }
}

}  // namespace
