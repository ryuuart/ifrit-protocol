#import "SCKEngine.h"

#import <Metal/Metal.h>
#import <Syphon/SyphonMetalServer.h>

#include "SceneGeometry.h"
#include "SceneModel.h"
#include "SceneRenderer.h"
#include "SkiaGraphiteContext.h"
#include "SkiaOffscreenSurface.h"
#include "UdpReceiver.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkShader.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkGradient.h>
#include <include/effects/SkImageFilters.h>
#include <include/gpu/graphite/BackendTexture.h>
#include <include/gpu/graphite/Image.h>
#include <include/gpu/graphite/mtl/MtlGraphiteTypes_cpp.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#if !__has_feature(objc_arc)
#error "SCKEngine.mm must be compiled with ARC (-fobjc-arc)"
#endif

namespace {

constexpr int kDefaultPort = 27015; // matches the Qt app / python sender

constexpr double kDefaultTargetFps = 60.0;
constexpr double kMinTargetFps = 1.0;
constexpr double kMaxTargetFps = 240.0;

// The window-blit chrome (backdrop stage behind the canvas, the alpha
// checkerboard under the transparent scene texture, and the canvas border),
// per system appearance. The light palette mirrors the Qt viewer's themed
// viewport (Theme.viewportBackground + Checkerboard defaults). Never part
// of the offscreen/Syphon texture — drawn only in the window blit.
struct BlitPalette {
  SkColor backdrop;
  SkColor checkerBase; // the checkerboard's ground cell
  SkColor checkerAlt;  // the alternating cell
  SkColor canvasBorder;
};

constexpr BlitPalette kDarkPalette = {
    SkColorSetRGB(0x0d, 0x0d, 0x10),
    SkColorSetRGB(0x1a, 0x1a, 0x20),
    SkColorSetRGB(0x2e, 0x2e, 0x38),
    SkColorSetARGB(0x40, 0xff, 0xff, 0xff),
};

constexpr BlitPalette kLightPalette = {
    SkColorSetRGB(0xd6, 0xd6, 0xda),
    SkColorSetRGB(0xcc, 0xcc, 0xcc),
    SkColorSetRGB(0x99, 0x99, 0x99),
    SkColorSetARGB(0x40, 0x00, 0x00, 0x00),
};

// Checkerboard cell size in drawable pixels, screen-fixed so it doesn't
// scale with zoom.
constexpr int kCheckerCellPixels = 16;

SkColor toSkColor(NSColor *color) {
  NSColor *srgb = [color colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
  if (!srgb)
    return SK_ColorRED;
  return SkColorSetARGB(
      static_cast<U8CPU>(std::clamp(srgb.alphaComponent, 0.0, 1.0) * 255.0),
      static_cast<U8CPU>(std::clamp(srgb.redComponent, 0.0, 1.0) * 255.0),
      static_cast<U8CPU>(std::clamp(srgb.greenComponent, 0.0, 1.0) * 255.0),
      static_cast<U8CPU>(std::clamp(srgb.blueComponent, 0.0, 1.0) * 255.0));
}

} // namespace

@interface SCKFeedEntry ()
- (instancetype)initWithTimestamp:(NSString *)timestamp
                           source:(NSString *)source
                          message:(NSString *)message;
@end

@implementation SCKFeedEntry
- (instancetype)initWithTimestamp:(NSString *)timestamp
                           source:(NSString *)source
                          message:(NSString *)message {
  if ((self = [super init])) {
    _timestamp = [timestamp copy];
    _source = [source copy];
    _message = [message copy];
  }
  return self;
}
@end

@implementation SCKEngine {
  id<MTLDevice> _device;
  id<MTLCommandQueue> _queue;
  std::unique_ptr<SkiaGraphiteContext> _graphite;
  std::unique_ptr<spellcircle::SceneRenderer> _sceneRenderer;
  spellcircle::SceneDocument _document;
  spellcircle::ResolvedScene _resolved;
  BOOL _hasScene;

  // Offscreen scene canvas at the configured native size — the texture
  // published over Syphon and blitted into on-screen layers.
  id<MTLTexture> _sceneTexture;

  SyphonMetalServer *_syphon;

  // Shared transport (src/net): the same ASIO receiver the Qt app's
  // NetworkManager wraps. Datagrams arrive on its I/O thread and are
  // marshalled onto the main queue here.
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
}

- (instancetype)init {
  if (!(self = [super init]))
    return nil;

  _device = MTLCreateSystemDefaultDevice();
  _queue = [_device newCommandQueue];
  if (_device && _queue)
    _graphite = SkiaGraphiteContext::createMetal((__bridge void *)_device,
                                                 (__bridge void *)_queue);
  _sceneRenderer = std::make_unique<spellcircle::SceneRenderer>();

  // Same Syphon server name as the Qt app, so downstream clients
  // (TouchDesigner) don't care which receiver is running. Created up front:
  // SyphonMetalServer announces itself immediately and keeps the last
  // published frame for late-joining clients.
  if (_device)
    _syphon = [[SyphonMetalServer alloc] initWithName:@"SpellCircle"
                                               device:_device
                                              options:nil];

  _receiver = std::make_unique<spellcircle::UdpReceiver>();
  _port = kDefaultPort;
  _statusText = @"Stopped";
  _targetFramesPerSecond = kDefaultTargetFps;

  // Defaults mirror the Qt GraphicsConfig so both apps render a scene
  // identically out of the box.
  _canvasWidth = 4000;
  _canvasHeight = 4000;
  _scale = 1.0;
  _strokeWidth = 4.0;
  _labelOffset = 0.0;
  _pointDistance = 40.0;
  _boxWidth = 360.0;
  _boxHeight = 140.0;
  _boxPadding = 16.0;
  _boxDistance = 40.0;
  _fontSize = 36.0;
  _fontFamily = @"";
  _fontWeight = 700; // bold, matching the Qt app's default QFont
  _fontItalic = NO;
  _accentColor = [NSColor colorWithSRGBRed:1.0 green:0.0 blue:0.0 alpha:1.0];
  _darkAppearance = YES; // the canvas view syncs the real appearance

  _timestampFormatter = [[NSDateFormatter alloc] init];
  _timestampFormatter.dateFormat = @"yyyy-MM-dd'T'HH:mm:ss.SSS";

  // Give Syphon (and the first drawInLayer:) an initial empty frame.
  [self renderScene];
  return self;
}

- (void)dealloc {
  _receiver->stop(); // joins the I/O thread; no callbacks after this
  [_syphon stop];
}

// ── Network ────────────────────────────────────────────────────────────────

- (void)setPort:(int)port {
  const int boundedPort = std::clamp(port, 1, 65535);
  if (_port == boundedPort)
    return;
  _port = boundedPort;
  if (_listening)
    [self start];
}

- (BOOL)start {
  // The shared receiver rebinds in place (a port change while listening
  // tears the previous socket down first), matching NetworkManager.
  __weak SCKEngine *weakSelf = self;
  const std::string error = _receiver->start(
      static_cast<uint16_t>(_port),
      [weakSelf](std::vector<uint8_t> payload, std::string source) {
        // I/O thread → main queue; the engine is main-thread only.
        NSData *data = [NSData dataWithBytes:payload.data()
                                      length:payload.size()];
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
               statusText:[NSString
                              stringWithFormat:@"Listening on UDP :%d", _port]];
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
- (BOOL)ingestPayload:(const void *)payload
                 size:(size_t)size
               source:(NSString *)source {
  // Arrival rate over a one-second window — never per-packet intervals:
  // queued datagrams are drained back-to-back microseconds apart, so an
  // interval-based rate explodes into the thousands whenever the main
  // thread was briefly busy (e.g. during a drag).
  const CFTimeInterval now = CACurrentMediaTime();
  if (_lastPacketTime > 0 && now - _lastPacketTime > 2.0)
    _rateWindowStart = 0; // stream gap: restart the window
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
  if (size == _lastPayload.size() && size > 0 &&
      std::memcmp(bytes, _lastPayload.data(), size) == 0)
    return NO;
  _lastPayload.assign(bytes, bytes + size);

  const spellcircle::SceneStats stats = _document.decode(payload, size);
  _hasScene = stats.hasGeometry();
  // Rendering happens through the paced governor (see drainSocket).

  SCKFeedEntry *entry = [[SCKFeedEntry alloc]
      initWithTimestamp:[_timestampFormatter stringFromDate:[NSDate date]]
                 source:source
                message:[NSString stringWithFormat:
                                      @"SpellCircle received — %d circles, "
                                      @"%d edges, %d boxes",
                                      stats.circles, stats.edges, stats.boxes]];
  [self.delegate engine:self didAppendFeedEntry:entry];
  return YES;
}

// Reads as zero once the stream has been silent for a couple of seconds,
// so a stopped sender doesn't leave a stale rate on screen.
- (double)scenesPerSecond {
  if (_lastPacketTime <= 0 || CACurrentMediaTime() - _lastPacketTime > 2.0)
    return 0.0;
  return _scenesPerSecond;
}

- (void)clearScene {
  _document.clear();
  _resolved.clear();
  _hasScene = NO;
  // Re-sending the last scene after a clear must not be deduplicated.
  _lastPayload.clear();
  [self renderScene];
}

// ── Graphics configuration ─────────────────────────────────────────────────

// Every setter funnels through here: geometry resolution depends on the
// canvas size, style only on the render pass — re-running both keeps the
// setters trivially uniform, and a config change is user-interaction rate.
- (void)configDidChange {
  _sceneDirty = YES;
  [self.delegate engineDidRenderScene:self]; // wake the view's display link
  [self renderTickIfDue];
}

- (void)setTargetFramesPerSecond:(double)targetFramesPerSecond {
  const double bounded =
      std::clamp(targetFramesPerSecond, kMinTargetFps, kMaxTargetFps);
  if (_targetFramesPerSecond == bounded)
    return;
  _targetFramesPerSecond = bounded;
  [self renderTickIfDue];
}

- (CFTimeInterval)renderInterval {
  return 1.0 / _targetFramesPerSecond;
}

/** Display-link entry point: renders the pending scene unless the render
 *  clock already produced this frame (then there is nothing left to do). */
- (void)renderPendingScene {
  if (!_sceneDirty)
    return;
  const CFTimeInterval now = CACurrentMediaTime();
  if (now - _lastRenderTime < [self renderInterval])
    return;
  _sceneDirty = NO;
  _lastRenderTime = now;
  [self resolveAndRender];
}

/** The render clock: draws pending scenes at targetFramesPerSecond —
 *  display asleep or not, Syphon client or not — self-scheduling its
 *  trailing edge so the newest scene always lands. Costs nothing while no
 *  scenes arrive (the loop parks as soon as nothing is dirty). */
- (void)renderTickIfDue {
  if (!_sceneDirty || _renderTickScheduled)
    return;
  const CFTimeInterval now = CACurrentMediaTime();
  const CFTimeInterval due = _lastRenderTime + [self renderInterval];
  if (now >= due) {
    _sceneDirty = NO;
    _lastRenderTime = now;
    [self resolveAndRender];
    return;
  }
  _renderTickScheduled = YES;
  __weak SCKEngine *weakSelf = self;
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW,
                    static_cast<int64_t>((due - now) * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        SCKEngine *strongSelf = weakSelf;
        if (!strongSelf)
          return;
        strongSelf->_renderTickScheduled = NO;
        [strongSelf renderTickIfDue];
      });
}

- (const BlitPalette &)blitPalette {
  return _darkAppearance ? kDarkPalette : kLightPalette;
}

- (void)setDarkAppearance:(BOOL)darkAppearance {
  if (_darkAppearance == darkAppearance)
    return;
  _darkAppearance = darkAppearance;
  _checkerShader = nullptr; // rebuilt from the new palette on the next blit
}

- (sk_sp<SkShader>)checkerShader {
  if (!_checkerShader) {
    const BlitPalette &palette = [self blitPalette];
    SkBitmap bitmap;
    bitmap.allocN32Pixels(kCheckerCellPixels * 2, kCheckerCellPixels * 2,
                          /*isOpaque=*/true);
    SkCanvas cells(bitmap);
    cells.clear(palette.checkerBase);
    SkPaint light;
    light.setColor(palette.checkerAlt);
    cells.drawRect(SkRect::MakeXYWH(0, 0, kCheckerCellPixels,
                                    kCheckerCellPixels),
                   light);
    cells.drawRect(SkRect::MakeXYWH(kCheckerCellPixels, kCheckerCellPixels,
                                    kCheckerCellPixels, kCheckerCellPixels),
                   light);
    bitmap.setImmutable();
    // Graphite does not auto-upload raster-backed shader images the way
    // Ganesh did — a raster tile silently draws nothing. Upload explicitly.
    sk_sp<SkImage> tile = bitmap.asImage();
    if (sk_sp<SkImage> uploaded = SkImages::TextureFromImage(
            _graphite->recorder(), tile.get(), {}))
      tile = std::move(uploaded);
    _checkerShader =
        tile->makeShader(SkTileMode::kRepeat, SkTileMode::kRepeat,
                         SkSamplingOptions(SkFilterMode::kNearest));
  }
  return _checkerShader;
}

- (void)resolveAndRender {
  _resolved = spellcircle::resolveScene(_document,
                                        static_cast<float>(_canvasWidth),
                                        static_cast<float>(_canvasHeight));
  [self renderScene];
}

#define SCK_CONFIG_SETTER(Type, Name, Setter)                                  \
  -(void)Setter : (Type)value {                                                \
    if (_##Name == value)                                                      \
      return;                                                                  \
    _##Name = value;                                                           \
    [self configDidChange];                                                    \
  }

SCK_CONFIG_SETTER(double, scale, setScale)
SCK_CONFIG_SETTER(double, strokeWidth, setStrokeWidth)
SCK_CONFIG_SETTER(double, labelOffset, setLabelOffset)
SCK_CONFIG_SETTER(double, pointDistance, setPointDistance)
SCK_CONFIG_SETTER(double, boxWidth, setBoxWidth)
SCK_CONFIG_SETTER(double, boxHeight, setBoxHeight)
SCK_CONFIG_SETTER(double, boxPadding, setBoxPadding)
SCK_CONFIG_SETTER(double, boxDistance, setBoxDistance)
SCK_CONFIG_SETTER(double, fontSize, setFontSize)
SCK_CONFIG_SETTER(int, fontWeight, setFontWeight)
SCK_CONFIG_SETTER(BOOL, fontItalic, setFontItalic)

#undef SCK_CONFIG_SETTER

// Clamped separately from the macro setters: a 0-sized or Metal-exceeding
// texture allocation must never happen (it aborts under Metal validation),
// no matter what a settings field feeds in.
- (void)setCanvasWidth:(int)canvasWidth {
  const int bounded = std::clamp(canvasWidth, 16, 8192);
  if (_canvasWidth == bounded)
    return;
  _canvasWidth = bounded;
  [self configDidChange];
}

- (void)setCanvasHeight:(int)canvasHeight {
  const int bounded = std::clamp(canvasHeight, 16, 8192);
  if (_canvasHeight == bounded)
    return;
  _canvasHeight = bounded;
  [self configDidChange];
}

- (void)setFontFamily:(NSString *)fontFamily {
  if ([_fontFamily isEqualToString:fontFamily])
    return;
  _fontFamily = [fontFamily copy];
  [self configDidChange];
}

- (void)setAccentColor:(NSColor *)accentColor {
  if ([_accentColor isEqual:accentColor])
    return;
  _accentColor = [accentColor copy];
  [self configDidChange];
}

// ── Rendering ──────────────────────────────────────────────────────────────

- (spellcircle::SceneStyle)currentStyle {
  spellcircle::SceneStyle style;
  style.accentColor = toSkColor(_accentColor);
  style.strokeWidth = static_cast<float>(_strokeWidth * _scale);
  style.labelOffset = static_cast<float>(_labelOffset * _scale);
  style.pointDistance = static_cast<float>(_pointDistance * _scale);
  style.boxWidth = static_cast<float>(_boxWidth * _scale);
  style.boxHeight = static_cast<float>(_boxHeight * _scale);
  style.boxPadding = static_cast<float>(_boxPadding * _scale);
  style.boxDistance = static_cast<float>(_boxDistance * _scale);
  style.fontSize = static_cast<float>(_fontSize * _scale);

  const SkFontStyle fontStyle(std::clamp(_fontWeight, 100, 1000),
                              SkFontStyle::kNormal_Width,
                              _fontItalic ? SkFontStyle::kItalic_Slant
                                          : SkFontStyle::kUpright_Slant);
  SkFontMgr *fontManager = _sceneRenderer->fontContext().fontManager();
  if (fontManager)
    style.typeface = fontManager->matchFamilyStyle(
        _fontFamily.length > 0 ? _fontFamily.UTF8String : nullptr, fontStyle);
  return style;
}

- (void)ensureSceneTexture {
  if (_sceneTexture && static_cast<int>(_sceneTexture.width) == _canvasWidth &&
      static_cast<int>(_sceneTexture.height) == _canvasHeight)
    return;

  MTLTextureDescriptor *descriptor = [MTLTextureDescriptor
      texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                   width:static_cast<NSUInteger>(_canvasWidth)
                                  height:static_cast<NSUInteger>(_canvasHeight)
                               mipmapped:NO];
  descriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
  descriptor.storageMode = MTLStorageModePrivate;
  _sceneTexture = [_device newTextureWithDescriptor:descriptor];
}

- (void)renderScene {
  if (!_graphite)
    return;
  [self ensureSceneTexture];
  if (!_sceneTexture)
    return;

  SkiaOffscreenSurface surface(*_graphite, (__bridge void *)_sceneTexture,
                               _canvasWidth, _canvasHeight);
  SkCanvas *canvas = surface.canvas();
  if (!canvas)
    return;

  const CFTimeInterval renderStart = CACurrentMediaTime();

  // Transparent like the Qt offscreen canvas: Syphon clients composite the
  // scene over their own content.
  canvas->clear(SK_ColorTRANSPARENT);
  _sceneRenderer->draw(canvas, _resolved, [self currentStyle]);
  surface.submit();

  const double renderMillis = (CACurrentMediaTime() - renderStart) * 1000.0;
  _renderMillis = _renderMillis == 0 ? renderMillis
                                     : _renderMillis * 0.9 + renderMillis * 0.1;

  // Graphite's submission is already queued; a command buffer created on
  // the same queue afterwards is ordered behind it, so Syphon's blit sees
  // the finished frame.
  if (_syphon && _syphon.hasClients) {
    id<MTLCommandBuffer> commandBuffer = [_queue commandBuffer];
    [_syphon publishFrameTexture:_sceneTexture
                 onCommandBuffer:commandBuffer
                     imageRegion:NSMakeRect(0, 0, _canvasWidth, _canvasHeight)
                         flipped:YES];
    [commandBuffer commit];
  }

  [self.delegate engineDidRenderScene:self];
}

- (void)drawInLayer:(CAMetalLayer *)layer
              scale:(double)scale
            centerX:(double)centerX
            centerY:(double)centerY
           topInset:(double)topInsetPixels {
  if (!_graphite || !layer || scale <= 0)
    return;

  id<CAMetalDrawable> drawable = [layer nextDrawable];
  if (!drawable)
    return;

  id<MTLTexture> target = drawable.texture;
  const int width = static_cast<int>(target.width);
  const int height = static_cast<int>(target.height);

  SkiaOffscreenSurface surface(*_graphite, (__bridge void *)target, width,
                               height);
  SkCanvas *canvas = surface.canvas();
  if (!canvas)
    return;

  canvas->clear([self blitPalette].backdrop);

  // The view owns the whole transform (fit region, zoom, pan): this method
  // just places the canvas rectangle where it was told to.
  const double drawWidth = _canvasWidth * scale;
  const double drawHeight = _canvasHeight * scale;
  const SkRect destination = SkRect::MakeXYWH(
      static_cast<float>(centerX - drawWidth / 2.0),
      static_cast<float>(centerY - drawHeight / 2.0),
      static_cast<float>(drawWidth), static_cast<float>(drawHeight));

  // Transparency checkerboard as the canvas plate: shows the publishable
  // extent and reads transparent scene regions as alpha, like the Qt
  // viewport. Tiled in device space, so cells stay screen-sized while
  // zooming.
  SkPaint platePaint;
  platePaint.setShader([self checkerShader]);
  canvas->drawRect(destination, platePaint);

  if (_sceneTexture) {
    const skgpu::graphite::BackendTexture backendTexture =
        skgpu::graphite::BackendTextures::MakeMetal(
            SkISize::Make(static_cast<int>(_sceneTexture.width),
                          static_cast<int>(_sceneTexture.height)),
            (__bridge CFTypeRef)_sceneTexture);
    sk_sp<SkImage> sceneImage = SkImages::WrapTexture(
        _graphite->recorder(), backendTexture, kBGRA_8888_SkColorType,
        kPremul_SkAlphaType, /*colorSpace=*/nullptr);
    if (sceneImage) {
      SkPaint imagePaint;
      imagePaint.setAntiAlias(true);
      canvas->drawImageRect(
          sceneImage, destination,
          SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNone),
          &imagePaint);
    }
  }

  SkPaint borderPaint;
  borderPaint.setAntiAlias(true);
  borderPaint.setStyle(SkPaint::kStroke_Style);
  borderPaint.setStrokeWidth(1.0f);
  borderPaint.setColor([self blitPalette].canvasBorder);
  canvas->drawRect(destination, borderPaint);

  // Progressive top-edge blur emulating the system toolbar scroll-edge
  // effect: macOS 26 only composes that effect over chrome-managed
  // scrollable content, and this canvas is deliberately a window backdrop
  // outside the chrome — so the blit draws its own. Backdrop-blur bands of
  // decreasing sigma feathering out below the bar, plus a faint backdrop
  // tint for toolbar-item legibility.
  if (topInsetPixels > 1.0) {
    const float blurBottom = static_cast<float>(topInsetPixels) * 1.5f;
    constexpr int kBandCount = 5;
    constexpr float kMaxSigma = 13.0f;
    for (int band = 0; band < kBandCount; ++band) {
      const float y0 = blurBottom * band / kBandCount;
      const float y1 = blurBottom * (band + 1) / kBandCount;
      const float remaining = 1.0f - static_cast<float>(band) / kBandCount;
      const float sigma = kMaxSigma * remaining * remaining;
      if (sigma < 0.4f)
        continue;
      sk_sp<SkImageFilter> bandBlur = SkImageFilters::Blur(sigma, sigma, nullptr);
      canvas->save();
      canvas->clipRect(SkRect::MakeXYWH(0, y0, static_cast<float>(width),
                                        y1 - y0));
      SkCanvas::SaveLayerRec blurLayer(nullptr, nullptr, bandBlur.get(), 0);
      canvas->saveLayer(blurLayer);
      canvas->restore(); // composite the backdrop-blurred layer
      canvas->restore(); // drop the band clip
    }

    const SkColor backdropColor = [self blitPalette].backdrop;
    const SkPoint tintSpan[2] = {{0.0f, 0.0f}, {0.0f, blurBottom}};
    const SkColor4f tintColors[2] = {
        SkColor4f::FromColor(SkColorSetA(backdropColor, 0x55)),
        SkColor4f::FromColor(SkColorSetA(backdropColor, 0x00))};
    SkPaint tint;
    tint.setShader(SkShaders::LinearGradient(
        tintSpan,
        SkGradient(SkGradient::Colors(SkSpan(tintColors, 2),
                                      SkTileMode::kClamp),
                   SkGradient::Interpolation())));
    canvas->drawRect(
        SkRect::MakeXYWH(0, 0, static_cast<float>(width), blurBottom), tint);
  }

  surface.submit();

  // Present after Graphite's queued work: command buffers on one queue
  // execute in commit order. During transaction-bound presentation (the
  // view sets presentsWithTransaction for animated resizes) the present
  // must happen on this thread after the work is scheduled, so the
  // in-flight CoreAnimation commit picks the drawable up.
  id<MTLCommandBuffer> commandBuffer = [_queue commandBuffer];
  if (layer.presentsWithTransaction) {
    [commandBuffer commit];
    [commandBuffer waitUntilScheduled];
    [drawable present];
  } else {
    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
  }
}

@end
