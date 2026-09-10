#import "SCKEngineInternal.h"

#include <cstring>
#include <string>
#include <vector>

// The socket and what arrives on it: binding and rebinding, the main-queue
// landing point for a datagram, and the feed entry and arrival rate one
// produces.
@implementation SCKEngine (Network)

- (BOOL)start {
  // The shared receiver rebinds in place (a port change while listening
  // tears the previous socket down first), matching NetworkManager.
  __weak SCKEngine *weakSelf = self;
  const std::string error =
      _receiver->start(static_cast<uint16_t>(_port),
                       [weakSelf](std::vector<uint8_t> payload, const std::string &source) {
                         // I/O thread → main queue; the engine is main-thread only.
                         NSData *data = [NSData dataWithBytes:payload.data() length:payload.size()];
                         NSString *sourceText = @(source.c_str());
                         dispatch_async(dispatch_get_main_queue(), ^{
                           [weakSelf receiveDatagram:data source:sourceText];
                         });
                       });

  if (!error.empty()) {
    [self setListeningState:NO statusText:@(error.c_str())];
    return NO;
  }

  [self setListeningState:YES
               statusText:[NSString stringWithFormat:@"Listening on UDP :%d", _port]];
  return YES;
}

- (void)stop {
  _receiver->stop();
  [self setListeningState:NO statusText:@"Stopped"];
}

- (void)setListeningState:(BOOL)listening statusText:(NSString *)statusText {
  _listening = listening;
  _statusText = [statusText copy];
  [self.delegate engineStatusDidChange:self];
}

/** Main-queue landing point for one datagram from the shared receiver. */
- (void)receiveDatagram:(NSData *)payload source:(NSString *)source {
  if (!spellcircle::verifyScenePayload(payload.bytes, payload.length)) {
    NSLog(@"Dropped invalid SpellCircle buffer from %@", source);
    return;
  }

  // Never render from the packet path: mark the scene pending and let the
  // render clock draw it at the configured rate (engineDidRenderScene also
  // wakes the view's display link for the on-screen blit).
  if ([self ingestPayload:payload.bytes size:payload.length source:source]) {
    _sceneDirty = YES;
    [self.delegate engineDidRenderScene:self];
    [self renderTickIfDue];
  }
}

/** Returns YES when the payload decoded into a changed scene. */
- (BOOL)ingestPayload:(const void *)payload size:(size_t)size source:(NSString *)source {
  // Arrival rate over a one-second window — never per-packet intervals:
  // queued datagrams are drained back-to-back microseconds apart, so an
  // interval-based rate explodes into the thousands whenever the main
  // thread was briefly busy (e.g. during a drag).
  const CFTimeInterval now = CACurrentMediaTime();
  if (_lastPacketTime > 0 && now - _lastPacketTime > 2.0)
    _rateWindowStart = 0;  // stream gap: restart the window
  if (_rateWindowStart <= 0) {
    _rateWindowStart = now;
    _rateWindowPackets = 0;
    _scenesPerSecond = 0;
  }
  ++_rateWindowPackets;
  const CFTimeInterval windowElapsed = now - _rateWindowStart;
  if (windowElapsed >= 1.0) {
    _scenesPerSecond = _rateWindowPackets / windowElapsed;
    _rateWindowStart = now;
    _rateWindowPackets = 0;
  }
  _lastPacketTime = now;

  // A payload byte-identical to the previous one decodes to the same
  // scene — a static sender pushing at a fixed rate costs nothing beyond
  // the receive itself.
  const auto *bytes = static_cast<const uint8_t *>(payload);
  if (size == _lastPayload.size() && size > 0 && std::memcmp(bytes, _lastPayload.data(), size) == 0)
    return NO;
  _lastPayload.assign(bytes, bytes + size);

  const spellcircle::SceneStats stats = _document.decode(payload, size);
  _hasScene = stats.hasGeometry();
  // Decoding never draws: a packet only marks the scene pending, and the
  // render clock draws it at the configured rate.

  SCKFeedEntry *entry = [[SCKFeedEntry alloc]
      initWithTimestamp:[_timestampFormatter stringFromDate:[NSDate date]]
                 source:source
                message:[NSString stringWithFormat:@"SpellCircle received — %d circles, "
                                                   @"%d edges, %d boxes",
                                                   stats.circles, stats.edges, stats.boxes]];
  [self.delegate engine:self didAppendFeedEntry:entry];
  return YES;
}

- (void)clearScene {
  _document.clear();
  _resolved.clear();
  _hasScene = NO;
  // Re-sending the last scene after a clear must not be deduplicated.
  _lastPayload.clear();
  [self renderScene];
}

@end
