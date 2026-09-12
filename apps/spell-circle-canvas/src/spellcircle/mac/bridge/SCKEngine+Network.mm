#import "SCKEngineInternal.h"

#include <chrono>
#include <string>

@interface SCKEngine (NetworkCallbacks)
- (void)receiveDatagram:(NSData *)payload
                 source:(NSString *)source
             receivedAt:(std::chrono::steady_clock::time_point)receivedAt;
- (void)setListeningState:(BOOL)listening statusText:(NSString *)statusText;
@end

// The socket and what arrives on it: binding and rebinding, the main-queue
// landing point for a datagram, and the feed entry an accepted change makes.
@implementation SCKEngine (Network)

- (void)start {
  const uint64_t generation = ++_networkGeneration;
  _networkRequested = YES;
  __weak SCKEngine *weakSelf = self;
  _receiver->start(
      static_cast<uint16_t>(_port),
      [weakSelf, generation](spellcircle::Datagram datagram) {
        @autoreleasepool {
          NSData *data = [NSData dataWithBytes:datagram.payload.data()
                                        length:datagram.payload.size()];
          NSString *source = @(datagram.source.c_str());
          const auto receivedAt = datagram.receivedAt;
          dispatch_async(dispatch_get_main_queue(), ^{
            SCKEngine *engine = weakSelf;
            if (!engine || engine->_networkGeneration != generation) return;
            [engine receiveDatagram:data source:source receivedAt:receivedAt];
          });
        }
      },
      [weakSelf, generation](spellcircle::UdpReceiver::Status status) {
        @autoreleasepool {
          const BOOL failed = static_cast<bool>(status.error);
          const uint16_t port = status.port;
          NSString *message = failed ? @(status.error.message().c_str()) : @"";
          dispatch_async(dispatch_get_main_queue(), ^{
            SCKEngine *engine = weakSelf;
            if (!engine || engine->_networkGeneration != generation) return;
            if (failed) {
              engine->_networkRequested = NO;
              [engine setListeningState:NO
                             statusText:[NSString stringWithFormat:@"UDP :%u — %@",
                                                                   static_cast<unsigned>(port),
                                                                   message]];
            } else {
              [engine setListeningState:YES
                             statusText:[NSString stringWithFormat:@"Listening on UDP :%u",
                                                                   static_cast<unsigned>(port)]];
            }
          });
        }
      });
  [self setListeningState:NO statusText:[NSString stringWithFormat:@"Binding UDP :%d", _port]];
}

- (void)stop {
  ++_networkGeneration;
  _networkRequested = NO;
  _receiver->stop();
  [self setListeningState:NO statusText:@"Stopped"];
}

- (void)setListeningState:(BOOL)listening statusText:(NSString *)statusText {
  _listening = listening;
  _statusText = [statusText copy];
  [self.delegate engineStatusDidChange:self];
}

/** Main-queue landing point for one datagram from the current binding. */
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
