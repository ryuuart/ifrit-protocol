#import "SCKCanvasView.h"

#import <Metal/Metal.h>
#import <QuartzCore/CADisplayLink.h>
#import <QuartzCore/CAMetalLayer.h>

#include <algorithm>
#include <cmath>

#if !__has_feature(objc_arc)
#error "SCKCanvasView.mm must be compiled with ARC (-fobjc-arc)"
#endif

namespace {
// Absolute bounds on drawable-pixels-per-canvas-pixel; generous on both
// ends since the fit scale of a large canvas in a small window is tiny.
constexpr double kMinScale = 0.002;
constexpr double kMaxScale = 64.0;
// Breathing room around the fitted canvas inside the uncovered region.
constexpr double kFitMarginPoints = 20.0;
} // namespace

@implementation SCKCanvasView {
  // The viewport transform is stored frame-independently — zoom in view
  // points per canvas px, canvas center in window coordinates — and only
  // converted to drawable pixels at render time. Anything stored in
  // drawable/view-local space shifts on screen whenever the frame, origin,
  // or backing scale changes underneath it: the pane reveal animations
  // slide this view's origin (canvas drifts, then snaps), and dragging
  // the window across displays flips the backing scale (canvas offsets).
  double _pointScale;    // view points per canvas px; <= 0 means "needs fit"
  NSPoint _anchorWindow; // canvas center, window coords (bottom-up points)
  BOOL _userAdjusted; // stop auto-refit once the user zoomed or panned
  BOOL _leftDragPans; // NO when the drag began in the titlebar/toolbar strip
  CADisplayLink *_displayLink;
  BOOL _redrawNeeded;
  NSSize _lastWindowSize; // distinguishes window resizes from column moves
}

- (instancetype)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    _pointScale = 0.0;
    _leftDragPans = YES; // a drag stream missing its mouseDown still pans
    self.wantsLayer = YES;
    // Redraws are explicit ([self redraw]) — never through display callbacks.
    self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawNever;
  }
  return self;
}

- (CALayer *)makeBackingLayer {
  CAMetalLayer *layer = [CAMetalLayer layer];
  layer.device = MTLCreateSystemDefaultDevice();
  layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
  // Skia wraps the drawable as a render target and draws quads into it;
  // framebufferOnly would restrict the texture to pure attachment usage.
  layer.framebufferOnly = NO;
  return layer;
}

- (CAMetalLayer *)metalLayer {
  return static_cast<CAMetalLayer *>(self.layer);
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (void)setEngine:(SCKEngine *)engine {
  _engine = engine;
  [self syncAppearance];
  [self resetView];
}

// ── Appearance ────────────────────────────────────────────────────────────

/** Mirrors the view's effective light/dark appearance into the engine's
 *  blit palette (backdrop, checkerboard, border). */
- (void)syncAppearance {
  if (!_engine)
    return;
  NSAppearanceName match = [self.effectiveAppearance
      bestMatchFromAppearancesWithNames:@[
        NSAppearanceNameAqua, NSAppearanceNameDarkAqua
      ]];
  const BOOL dark = [match isEqualToString:NSAppearanceNameDarkAqua];
  if (_engine.darkAppearance != dark) {
    _engine.darkAppearance = dark;
    [self redraw];
  }
}

- (void)viewDidChangeEffectiveAppearance {
  [super viewDidChangeEffectiveAppearance];
  [self syncAppearance];
}

// ── Sizing ────────────────────────────────────────────────────────────────

- (double)backingScale {
  return self.window ? self.window.backingScaleFactor : 2.0;
}

// ── Transform storage: window points <-> drawable pixels ─────────────────

/** This view's top-left corner in window coordinates. */
- (NSPoint)viewTopLeftInWindow {
  return [self convertPoint:NSMakePoint(0, self.bounds.size.height)
                     toView:nil];
}

/** Current zoom as drawable pixels per canvas pixel. */
- (double)drawableScale {
  return _pointScale * [self backingScale];
}

/** The stored window-space anchor as a drawable-pixel center (top-left
 *  origin), for the current frame, origin, and backing scale. */
- (CGPoint)drawableCenter {
  const NSPoint topLeft = [self viewTopLeftInWindow];
  const double backing = [self backingScale];
  return CGPointMake((_anchorWindow.x - topLeft.x) * backing,
                     (topLeft.y - _anchorWindow.y) * backing);
}

/** Stores a drawable-pixel center as the window-space anchor. */
- (void)setDrawableCenter:(CGPoint)center {
  const NSPoint topLeft = [self viewTopLeftInWindow];
  const double backing = [self backingScale];
  _anchorWindow = NSMakePoint(topLeft.x + center.x / backing,
                              topLeft.y - center.y / backing);
}

- (void)updateDrawableSize {
  const CGFloat scale = [self backingScale];
  CAMetalLayer *layer = self.metalLayer;
  layer.contentsScale = scale;
  const CGSize pixelSize =
      CGSizeMake(self.bounds.size.width * scale, self.bounds.size.height * scale);
  if (pixelSize.width < 1 || pixelSize.height < 1)
    return;
  const NSSize windowSize = self.window ? self.window.frame.size : NSZeroSize;
  const BOOL windowResized = !NSEqualSizes(windowSize, _lastWindowSize);
  _lastWindowSize = windowSize;

  if (CGSizeEqualToSize(layer.drawableSize, pixelSize))
    return;

  // The resized drawable must be presented in the same CoreAnimation
  // transaction as the layout change, or the layer stretches the previous
  // drawable for a frame or two — the visible glitch whenever the
  // inspector/sidebar panes animate or the window live-resizes. Same
  // pattern MTKView uses during live resize: render synchronously with
  // presentsWithTransaction so CoreAnimation picks the frame up in the
  // current commit.
  layer.drawableSize = pixelSize;
  // Refit only on first layout and real window resizes. A column change
  // (the inspector or sidebar revealing) resizes this view while the
  // window stays put — keeping the transform means the canvas holds its
  // on-screen position and the pane simply covers/uncovers it, instead of
  // the fit shoving the canvas sideways on every toggle.
  if (_pointScale <= 0 || (!_userAdjusted && windowResized))
    [self fitTransform];
  layer.presentsWithTransaction = YES;
  [_engine renderPendingScene];
  _redrawNeeded = NO; // this render supersedes any queued display-link tick
  [self renderNow];
  layer.presentsWithTransaction = NO;
}

- (void)setFrameSize:(NSSize)newSize {
  [super setFrameSize:newSize];
  [self updateDrawableSize];
}

- (void)setFrameOrigin:(NSPoint)newOrigin {
  [super setFrameOrigin:newOrigin];
  // Pane reveal animations slide this view's origin, carrying the layer's
  // presented contents with it. Re-render so the window-anchored transform
  // puts the canvas back in place for the moved frame.
  [self redraw];
}

- (void)viewDidChangeBackingProperties {
  [super viewDidChangeBackingProperties];
  [self updateDrawableSize];
}

- (void)viewDidMoveToWindow {
  [super viewDidMoveToWindow];
  if (self.window) {
    if (!_displayLink) {
      // Frame pacing: redraw requests only set a dirty flag; the display
      // link renders at most once per refresh and pauses itself when idle.
      // Common modes so it keeps firing during drag/scroll event tracking.
      _displayLink = [self displayLinkWithTarget:self
                                        selector:@selector(displayLinkFired:)];
      [_displayLink addToRunLoop:NSRunLoop.mainRunLoop
                         forMode:NSRunLoopCommonModes];
      _displayLink.paused = YES;
    }
    [self syncAppearance]; // the window's appearance is authoritative
    [self updateDrawableSize];
  } else {
    [_displayLink invalidate];
    _displayLink = nil;
  }
}

- (void)displayLinkFired:(CADisplayLink *)link {
  if (!_redrawNeeded) {
    link.paused = YES;
    return;
  }
  // Consumer-driven rendering: the display tick pulls the pending scene
  // into the offscreen texture first, then blits it. The engine's
  // frame-ready callback inside renderPendingScene re-marks this view
  // dirty, so clear the flag afterwards to keep one tick = one frame.
  [_engine renderPendingScene];
  _redrawNeeded = NO;
  [self renderNow];
}

// The inset setters are auto-synthesized and deliberately passive: insets
// participate in the next fit (see the header) but never trigger one — a
// pane reveal updating them must not move the canvas.

// ── Transform ─────────────────────────────────────────────────────────────

/** Center of the region not covered by floating chrome, in drawable px. */
- (CGPoint)uncoveredRegionCenter {
  CAMetalLayer *layer = self.metalLayer;
  const double backing = [self backingScale];
  const double leftPx = _leftContentInset * backing;
  const double rightPx = _rightContentInset * backing;
  return CGPointMake(
      leftPx + (layer.drawableSize.width - leftPx - rightPx) / 2.0,
      layer.drawableSize.height / 2.0);
}

/** Fits the whole canvas into the uncovered region (does not redraw). */
- (void)fitTransform {
  CAMetalLayer *layer = self.metalLayer;
  if (!_engine || _engine.canvasWidth < 1 || _engine.canvasHeight < 1 ||
      layer.drawableSize.width < 1)
    return;

  const double backing = [self backingScale];
  const double margin = kFitMarginPoints * backing;
  const double insetPx =
      (_leftContentInset + _rightContentInset) * backing;
  const double availableWidth =
      std::max(layer.drawableSize.width - insetPx - 2.0 * margin, 32.0);
  const double availableHeight =
      std::max(layer.drawableSize.height - 2.0 * margin, 32.0);

  const double drawableScale =
      std::min(availableWidth / _engine.canvasWidth,
               availableHeight / _engine.canvasHeight);
  _pointScale = drawableScale / backing;
  [self setDrawableCenter:[self uncoveredRegionCenter]];
  _userAdjusted = NO;
}

- (void)redraw {
  _redrawNeeded = YES;
  if (_displayLink) {
    // Called per received scene (60+ Hz): only touch the link when it is
    // actually paused — repeated pause toggling round-trips to the server.
    if (_displayLink.paused)
      _displayLink.paused = NO;
  } else {
    // No window yet — render directly so early state still lands.
    _redrawNeeded = NO;
    [self renderNow];
  }
}

- (void)renderNow {
  CAMetalLayer *layer = self.metalLayer;
  if (!_engine || !layer || layer.drawableSize.width < 1)
    return;
  if (_pointScale <= 0)
    [self fitTransform];
  if (_pointScale <= 0)
    return;
  // Derived per render: the same window-space anchor keeps the canvas
  // stationary on screen no matter how the view's frame, origin, or
  // backing scale changed since the last frame.
  const double drawableScale = [self drawableScale];
  const CGPoint center = [self drawableCenter];
  [_engine drawInLayer:layer
                 scale:drawableScale
               centerX:center.x
               centerY:center.y
              topInset:self.drawsTopEdgeBlur ? [self topChromeInsetPixels]
                                             : 0.0];
  if (self.onViewChanged)
    self.onViewChanged(drawableScale * 100.0);
}

/** Titlebar + toolbar height over this view in drawable pixels, from the
 *  window's content layout rect — sizes the blit's top-edge blur. */
- (double)topChromeInsetPixels {
  NSWindow *window = self.window;
  NSView *contentView = window.contentView;
  if (!window || !contentView)
    return 0.0;
  const double insetPoints =
      NSMaxY(contentView.frame) - NSMaxY(window.contentLayoutRect);
  return std::max(0.0, insetPoints) * [self backingScale];
}

- (void)resetView {
  [self fitTransform];
  [self redraw];
}

- (void)zoomToActualSize {
  // One canvas pixel per drawable pixel on the current display.
  _pointScale = 1.0 / [self backingScale];
  [self setDrawableCenter:[self uncoveredRegionCenter]];
  _userAdjusted = YES;
  [self redraw];
}

// ── Gestures: wheel zooms at the cursor, middle/left drag pans ────────────

/** Cursor position in drawable pixels (top-left origin). */
- (CGPoint)drawablePointForEvent:(NSEvent *)event {
  const NSPoint viewPoint = [self convertPoint:event.locationInWindow
                                      fromView:nil];
  const double backing = [self backingScale];
  return CGPointMake(viewPoint.x * backing,
                     (self.bounds.size.height - viewPoint.y) * backing);
}

/** Zooms by `factor` keeping the content under `cursor` stationary. */
- (void)zoomBy:(double)factor atDrawablePoint:(CGPoint)cursor {
  const double currentScale = [self drawableScale];
  const double newScale =
      std::clamp(currentScale * factor, kMinScale, kMaxScale);
  const double appliedFactor = currentScale > 0 ? newScale / currentScale : 1.0;
  if (appliedFactor == 1.0)
    return;

  // The canvas point under the cursor keeps its screen position: the
  // cursor-to-center distance scales by the zoom factor.
  CGPoint center = [self drawableCenter];
  center.x = cursor.x - (cursor.x - center.x) * appliedFactor;
  center.y = cursor.y - (cursor.y - center.y) * appliedFactor;
  _pointScale = newScale / [self backingScale];
  [self setDrawableCenter:center];
  _userAdjusted = YES;
  [self redraw];
}

- (void)scrollWheel:(NSEvent *)event {
  // Trackpads deliver fine-grained pixel deltas; wheel mice deliver coarse
  // line deltas that need a larger step per notch.
  const double delta = event.scrollingDeltaY *
                       (event.hasPreciseScrollingDeltas ? 1.0 : 12.0);
  const double factor = std::exp(delta * 0.0015);
  [self zoomBy:factor atDrawablePoint:[self drawablePointForEvent:event]];
}

- (void)magnifyWithEvent:(NSEvent *)event {
  [self zoomBy:(1.0 + event.magnification)
      atDrawablePoint:[self drawablePointForEvent:event]];
}

- (void)smartMagnifyWithEvent:(NSEvent *)event {
  [self resetView];
}

- (void)panByEventDelta:(NSEvent *)event {
  const double backing = [self backingScale];
  CGPoint center = [self drawableCenter];
  center.x += event.deltaX * backing;
  center.y += event.deltaY * backing;
  [self setDrawableCenter:center];
  _userAdjusted = YES;
  [self redraw];
}

// A left drag that begins in the titlebar/toolbar strip is a window move —
// AppKit drags the window AND still delivers the mouseDragged stream here,
// so panning too would double-handle it (the canvas swims while the window
// moves). Record the down location's region and let those drags through.
- (void)mouseDown:(NSEvent *)event {
  NSWindow *window = self.window;
  _leftDragPans =
      !window || event.locationInWindow.y <= NSMaxY(window.contentLayoutRect);
}

- (void)mouseDragged:(NSEvent *)event {
  if (!_leftDragPans)
    return;
  [self panByEventDelta:event];
}

/** Middle-button drag pans, matching common viewport tools. */
- (void)otherMouseDragged:(NSEvent *)event {
  [self panByEventDelta:event];
}

@end

// ── SCKCanvasDetailProxyView ──────────────────────────────────────────────

@implementation SCKCanvasDetailProxyView

- (SCKCanvasView *)canvas {
  return self.canvasProvider ? self.canvasProvider() : nil;
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

// Gesture forwarding: coordinates travel inside the NSEvent in window
// space, and the canvas converts them against its own (full-window) frame,
// so events received here resolve to the same on-screen points there.

// Claim the button-down events: AppKit routes the subsequent mouseDragged
// stream to the view that took mouseDown, and NSView's default
// implementation would hand it up into SwiftUI's hosting machinery — which
// is exactly how drag-panning goes dead. The left-button down also travels
// to the canvas so it can record whether the drag began below the chrome
// (titlebar drags move the window; the canvas must not pan along).
- (void)mouseDown:(NSEvent *)event {
  [[self canvas] mouseDown:event];
}

- (void)otherMouseDown:(NSEvent *)event {
}

- (void)scrollWheel:(NSEvent *)event {
  [[self canvas] scrollWheel:event];
}

- (void)magnifyWithEvent:(NSEvent *)event {
  [[self canvas] magnifyWithEvent:event];
}

- (void)smartMagnifyWithEvent:(NSEvent *)event {
  [[self canvas] smartMagnifyWithEvent:event];
}

- (void)mouseDragged:(NSEvent *)event {
  [[self canvas] mouseDragged:event];
}

- (void)otherMouseDragged:(NSEvent *)event {
  [[self canvas] otherMouseDragged:event];
}

// ── Content insets: how much window the chrome covers ────────────────────

- (void)layout {
  [super layout];
  [self pushContentInsets];
}

- (void)setFrameSize:(NSSize)newSize {
  [super setFrameSize:newSize];
  [self pushContentInsets];
}

- (void)viewDidMoveToWindow {
  [super viewDidMoveToWindow];
  [self pushContentInsets];
}

/** Reports this view's window-space bounds to the canvas as fit-centering
 *  insets. Passive on the canvas side — nothing moves until the next fit. */
- (void)pushContentInsets {
  SCKCanvasView *canvas = [self canvas];
  if (!canvas || !self.window || !canvas.window)
    return;
  const NSRect proxyInWindow = [self convertRect:self.bounds toView:nil];
  const NSRect canvasInWindow = [canvas convertRect:canvas.bounds toView:nil];
  canvas.leftContentInset =
      std::max(0.0, NSMinX(proxyInWindow) - NSMinX(canvasInWindow));
  canvas.rightContentInset =
      std::max(0.0, NSMaxX(canvasInWindow) - NSMaxX(proxyInWindow));
}

@end
