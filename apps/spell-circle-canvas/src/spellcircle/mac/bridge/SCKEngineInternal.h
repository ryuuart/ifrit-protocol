#pragma once

// The engine's own state — the Metal device and its Graphite context, the
// scene the receiver decoded, the offscreen texture and its Syphon server,
// the packet-rate and render-clock bookkeeping — and the few calls its
// parts make of each other. ObjC++ only, and never part of the Clang
// module the Swift app imports.

#import "SCKEngine.h"

#import <Metal/Metal.h>
#import <Syphon/SyphonMetalServer.h>

#include <sigilskia/graphite/GraphiteContext.h>
#include "SceneGeometry.h"
#include "SceneModel.h"
#include "SceneRenderer.h"
#include "UdpReceiver.h"

#include <include/core/SkColor.h>
#include <include/core/SkShader.h>

#include <cstdint>
#include <memory>
#include <vector>

#if !__has_feature(objc_arc)
#error "the SCKEngine sources must be compiled with ARC (-fobjc-arc)"
#endif

NS_ASSUME_NONNULL_BEGIN

/** The window-blit chrome (backdrop stage behind the canvas, the alpha
 *  checkerboard under the transparent scene texture, and the canvas border),
 *  per system appearance. The light palette mirrors the Qt viewer's themed
 *  viewport (Theme.viewportBackground + Checkerboard defaults). Never part
 *  of the offscreen/Syphon texture — drawn only in the window blit. */
struct BlitPalette {
  SkColor backdrop;
  SkColor checkerBase;  // the checkerboard's ground cell
  SkColor checkerAlt;   // the alternating cell
  SkColor canvasBorder;
};

@interface SCKFeedEntry ()
- (instancetype)initWithTimestamp:(NSString *)timestamp
                           source:(NSString *)source
                          message:(NSString *)message;
@end

@interface SCKEngine () {
  id<MTLDevice> _device;
  id<MTLCommandQueue> _queue;
  std::unique_ptr<sigil::skia::GraphiteContext> _graphite;
  std::unique_ptr<spellcircle::SceneRenderer> _sceneRenderer;
  spellcircle::SceneDocument _document;
  spellcircle::ResolvedScene _resolved;
  BOOL _hasScene;

  // Offscreen scene canvas at the configured native size — the texture
  // published over Syphon and blitted into on-screen layers.
  id<MTLTexture> _sceneTexture;

  SyphonMetalServer *_syphon;

  // The same ASIO receiver the Qt app's NetworkManager wraps, so both
  // frontends have one transport. Datagrams arrive on its I/O thread and
  // are marshalled onto the main queue here.
  std::unique_ptr<spellcircle::UdpReceiver> _receiver;
  NSDateFormatter *_timestampFormatter;
  CFTimeInterval _lastPacketTime;
  CFTimeInterval _rateWindowStart;
  int _rateWindowPackets;
  // Backing for the custom staleness-aware scenesPerSecond getter (a
  // readonly property with a hand-written getter isn't auto-synthesized).
  double _scenesPerSecond;

  // Paced rendering: packets only mark the scene dirty; the render clock
  // (renderTickIfDue, at targetFramesPerSecond) and the display link (via
  // renderPendingScene) actually render. Byte-identical payloads are
  // dropped before decode.
  std::vector<uint8_t> _lastPayload;
  BOOL _sceneDirty;
  BOOL _renderTickScheduled;
  CFTimeInterval _lastRenderTime;

  sk_sp<SkShader> _checkerShader;

  // The properties' backing, declared here because the parts that read
  // them are not all in the file that synthesizes them.
  int _port;
  BOOL _listening;
  NSString *_statusText;
  double _targetFramesPerSecond;
  int _canvasWidth;
  int _canvasHeight;
  double _scale;
  double _strokeWidth;
  double _labelOffset;
  double _pointDistance;
  double _boxWidth;
  double _boxHeight;
  double _boxPadding;
  double _boxDistance;
  double _fontSize;
  NSString *_fontFamily;
  int _fontWeight;
  BOOL _fontItalic;
  NSColor *_accentColor;
  BOOL _darkAppearance;
  double _renderMillis;
}

@end

/** The calls the engine's parts make of each other. */
@interface SCKEngine (Internal)
/** Draws the resolved scene into the offscreen texture and publishes it. */
- (void)renderScene;
/** The render clock's step: draws now when a frame is due, and otherwise
 *  schedules itself for the moment one becomes due. */
- (void)renderTickIfDue;
@end

NS_ASSUME_NONNULL_END
