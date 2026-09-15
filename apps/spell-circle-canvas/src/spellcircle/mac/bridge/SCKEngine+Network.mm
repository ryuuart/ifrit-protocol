#import "SCKEngineInternal.h"

#include <sigilio/transport/Transport.h>

#include <chrono>
#include <optional>
#include <string>
#include <utility>

namespace {

/** The drain runs at the render rate, so a scene waits at most one frame
 *  in the door before it is decoded. */
constexpr uint64_t kDrainIntervalNanoseconds = 16 * NSEC_PER_MSEC;

/** How many arrivals the door holds for the drain. A sender at animation
 *  frame rates leaves about one per drain, so this is a second of
 *  backlog: a frame the application spent elsewhere costs nothing, and a
 *  reader that falls further behind than that wants the newest scenes
 *  rather than the whole queue. */
constexpr size_t kArrivalCapacity = 64;

/** The address an arrival names, with the scheme taken off: a feed entry
 *  reads "127.0.0.1:52341" and the door it came through is the engine's
 *  own concern. */
NSString *addressOf(const std::string &from) {
  const size_t scheme = from.find("://");
  if (scheme == std::string::npos) return @(from.c_str());
  return @(from.substr(scheme + 3).c_str());
}

}  // namespace

@interface SCKEngine (NetworkCallbacks)
- (void)receiveDatagram:(NSData *)payload
                 source:(NSString *)source
             receivedAt:(std::chrono::steady_clock::time_point)receivedAt;
- (void)setListeningState:(BOOL)listening statusText:(NSString *)statusText;
@end

// The port and what arrives on it: the door the hub opens, the timer that
// drains it onto the main queue, and the feed entry an accepted change
// makes.
@implementation SCKEngine (Network)

- (void)start {
  const std::string uri = "udp://:" + std::to_string(_port);
  // The hub answers one feed per URI for as long as anyone holds it, so
  // asking for the port already open keeps the door standing — and with
  // it the origin every arrival's time is counted from — rather than
  // closing a socket in order to bind the same one again.
  const auto openedAt = std::chrono::steady_clock::now();
  std::shared_ptr<sigil::io::Feed> opened = _hub.feed(uri, {.capacity = kArrivalCapacity});
  if (opened != _door) {
    _door = std::move(opened);
    _doorOpenedAt = openedAt;
  }

  const std::string error = _door->error();
  if (!error.empty()) {
    [self closeDoor];
    [self setListeningState:NO
                 statusText:[NSString
                                stringWithFormat:@"UDP failed on :%d — %s", _port, error.c_str()]];
    return;
  }

  if (!_drain) {
    _drain = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    dispatch_source_set_timer(_drain, DISPATCH_TIME_NOW, kDrainIntervalNanoseconds,
                              kDrainIntervalNanoseconds / 4);
    // Weak, so the timer the engine owns is no reason for the engine to
    // stay alive.
    __weak SCKEngine *weakSelf = self;
    dispatch_source_set_event_handler(_drain, ^{
      [weakSelf readArrivals];
    });
    dispatch_resume(_drain);
  }

  [self setListeningState:YES
               statusText:[NSString stringWithFormat:@"Listening on UDP :%d", _port]];
}

- (void)stop {
  [self closeDoor];
  [self setListeningState:NO statusText:@"Stopped"];
}

- (void)closeDoor {
  if (_drain) {
    dispatch_source_cancel(_drain);
    _drain = nil;
  }
  _door.reset();
}

- (void)readArrivals {
  // The door is re-read each turn: a scene reaching the session may take
  // the receiver down, and what a closed door still holds is nobody's to
  // ingest.
  while (const std::shared_ptr<sigil::io::Feed> door = _door) {
    const std::optional<sigil::io::Arrival> arrival = door->receive();
    if (!arrival) return;
    @autoreleasepool {
      NSData *payload = [NSData dataWithBytes:arrival->bytes->bytes.data()
                                       length:arrival->bytes->bytes.size()];
      const auto receivedAt =
          _doorOpenedAt + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                              std::chrono::duration<double>(arrival->at));
      [self receiveDatagram:payload source:addressOf(arrival->from) receivedAt:receivedAt];
    }
  }
}

- (void)setListeningState:(BOOL)listening statusText:(NSString *)statusText {
  _listening = listening;
  _statusText = [statusText copy];
  [self.delegate engineStatusDidChange:self];
}

/** Main-queue landing point for one datagram off the door. */
- (void)receiveDatagram:(NSData *)payload
                 source:(NSString *)source
             receivedAt:(std::chrono::steady_clock::time_point)receivedAt {
  const spellcircle::SceneUpdate update =
      _session.ingest(payload.bytes, payload.length, receivedAt);
  if (update == spellcircle::SceneUpdate::Invalid) {
    NSLog(@"Dropped invalid SpellCircle buffer from %@", source);
    return;
  }

  [self.delegate enginePacketRateDidChange:self];
  if (update == spellcircle::SceneUpdate::Unchanged) return;

  const spellcircle::SceneStats stats = _session.stats();
  SCKFeedEntry *entry = [[SCKFeedEntry alloc]
      initWithTimestamp:[_timestampFormatter stringFromDate:[NSDate date]]
                 source:source
                message:[NSString stringWithFormat:@"SpellCircle received — %d circles, "
                                                   @"%d edges, %d boxes",
                                                   stats.circles, stats.edges, stats.boxes]];
  [self.delegate engine:self didAppendFeedEntry:entry];

  // Accepted changes mark the scene pending. The render clock and display
  // link share the frame deadline, so only a due frame is drawn.
  _sceneDirty = YES;
  [self.delegate engineSceneDidChange:self];
  [self renderTickIfDue];
}

- (void)clearScene {
  _session.clear();
  _resolved.clear();
  _sceneDirty = NO;
  [self.delegate enginePacketRateDidChange:self];
  [self renderScene];
}

@end
