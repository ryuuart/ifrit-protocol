#pragma once

// The engine's own state — the Metal device and its Graphite context, the
// scene the receiver decoded, the offscreen texture and its Syphon server,
// the shared scene session and render-clock bookkeeping — and the calls its
// parts make of each other. ObjC++ only, and never part of the Clang
// module the Swift app imports.

#import "SCKEngine.h"

#import <Metal/Metal.h>
#import <Syphon/SyphonMetalServer.h>

#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include "SceneGeometry.h"
#include "SceneModel.h"
#include "SceneRenderer.h"
#include "SceneSession.h"

#include <include/core/SkColor.h>
#include <include/core/SkShader.h>

#include <chrono>
#include <memory>

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
  spellcircle::SceneSession _session;
  spellcircle::ResolvedScene _resolved;

  // Offscreen scene canvas at the configured native size — the texture
  // published over Syphon and blitted into on-screen layers.
  id<MTLTexture> _sceneTexture;

  SyphonMetalServer *_syphon;

  // The port, as a door on a resource hub: the transport takes datagrams
  // on a thread of its own and the door holds them until the drain timer
  // reads them here, on the main queue. An arrival carries the seconds
  // since its door was made, which _doorOpenedAt turns back into a clock
  // reading.
  sigil::io::Hub _hub;
  std::shared_ptr<sigil::io::Feed> _door;
  std::chrono::steady_clock::time_point _doorOpenedAt;
  dispatch_source_t _drain;
  NSDateFormatter *_timestampFormatter;

  // Paced rendering: packets only mark the scene dirty; the render clock
  // (renderTickIfDue, at targetFramesPerSecond) and the display link (via
  // renderPendingScene) actually render.
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
/** Lets the door go and stops the drain that was reading it, so no socket
 *  stands open and nothing is left to read. */
- (void)closeDoor;
/** Draws the resolved scene into the offscreen texture and publishes it. */
- (void)renderScene;
/** The render clock's step: draws now when a frame is due, and otherwise
 *  schedules itself for the moment one becomes due. */
- (void)renderTickIfDue;
@end

NS_ASSUME_NONNULL_END
