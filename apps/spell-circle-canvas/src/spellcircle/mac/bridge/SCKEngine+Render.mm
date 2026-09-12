#import "SCKEngineInternal.h"

#include <sigilskia/graphite/OffscreenSurface.h>
#include "SceneGeometry.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>

#include <algorithm>

namespace {

SkColor toSkColor(NSColor *color) {
  NSColor *srgb = [color colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
  if (!srgb) return SK_ColorRED;
  return SkColorSetARGB(static_cast<U8CPU>(std::clamp(srgb.alphaComponent, 0.0, 1.0) * 255.0),
                        static_cast<U8CPU>(std::clamp(srgb.redComponent, 0.0, 1.0) * 255.0),
                        static_cast<U8CPU>(std::clamp(srgb.greenComponent, 0.0, 1.0) * 255.0),
                        static_cast<U8CPU>(std::clamp(srgb.blueComponent, 0.0, 1.0) * 255.0));
}

}  // namespace

// The offscreen canvas and the clock that paces it: the configuration read
// as a draw style, the Metal texture the scene is drawn into at its native
// size, the Syphon publication that follows the draw, and the pacing that
// decides when a pending scene becomes a drawn one.
@implementation SCKEngine (Render)

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

  const SkFontStyle fontStyle(
      std::clamp(_fontWeight, 100, 1000), SkFontStyle::kNormal_Width,
      _fontItalic ? SkFontStyle::kItalic_Slant : SkFontStyle::kUpright_Slant);
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
  if (!_graphite) return;
  [self ensureSceneTexture];
  if (!_sceneTexture) return;

  sigil::skia::OffscreenSurface surface(*_graphite, (__bridge void *)_sceneTexture, _canvasWidth,
                                        _canvasHeight);
  SkCanvas *canvas = surface.canvas();
  if (!canvas) return;

  const CFTimeInterval renderStart = CACurrentMediaTime();

  // Transparent like the Qt offscreen canvas: Syphon clients composite the
  // scene over their own content.
  canvas->clear(SK_ColorTRANSPARENT);
  _sceneRenderer->draw(canvas, _resolved, [self currentStyle]);
  surface.submit();

  const double renderMillis = (CACurrentMediaTime() - renderStart) * 1000.0;
  _renderMillis = _renderMillis == 0 ? renderMillis : _renderMillis * 0.9 + renderMillis * 0.1;

  // Graphite's submission is already queued; a command buffer created on
  // the same queue afterwards is ordered behind it, so Syphon's blit sees
  // the finished frame.
  id<MTLTexture> sceneTexture = _sceneTexture;
  id<MTLCommandBuffer> commandBuffer =
      sceneTexture && _syphon && _syphon.hasClients ? [_queue commandBuffer] : nil;
  if (commandBuffer) {
    [_syphon publishFrameTexture:sceneTexture
                 onCommandBuffer:commandBuffer
                     imageRegion:NSMakeRect(0, 0, _canvasWidth, _canvasHeight)
                         flipped:YES];
    [commandBuffer commit];
  }

  [self.delegate engineDidRenderScene:self];
}

- (void)resolveAndRender {
  _resolved = spellcircle::resolveScene(_session.document(), static_cast<float>(_canvasWidth),
                                        static_cast<float>(_canvasHeight));
  [self renderScene];
}

- (CFTimeInterval)renderInterval {
  return 1.0 / _targetFramesPerSecond;
}

/** Display-link entry point: renders the pending scene unless the render
 *  clock already produced this frame (then there is nothing left to do). */
- (void)renderPendingScene {
  if (!_sceneDirty) return;
  const CFTimeInterval now = CACurrentMediaTime();
  if (now - _lastRenderTime < [self renderInterval]) return;
  _sceneDirty = NO;
  _lastRenderTime = now;
  [self resolveAndRender];
}

/** The render clock: draws pending scenes at targetFramesPerSecond —
 *  display asleep or not, Syphon client or not — self-scheduling its
 *  trailing edge so the newest scene always lands. Costs nothing while no
 *  scenes arrive (the loop parks as soon as nothing is dirty). */
- (void)renderTickIfDue {
  if (!_sceneDirty || _renderTickScheduled) return;
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
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>((due - now) * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   SCKEngine *strongSelf = weakSelf;
                   if (!strongSelf) return;
                   strongSelf->_renderTickScheduled = NO;
                   [strongSelf renderTickIfDue];
                 });
}

@end
