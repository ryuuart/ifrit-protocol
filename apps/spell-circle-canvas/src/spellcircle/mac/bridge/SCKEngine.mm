#import "SCKEngineInternal.h"

#include <sigilio/transport/Transport.h>

#include <include/core/SkColor.h>
#include <algorithm>

#include "ReceiverDefaults.h"

namespace {

using Defaults = spellcircle::ReceiverDefaults;

constexpr double kMinTargetFps = 1.0;
constexpr double kMaxTargetFps = 240.0;

}  // namespace

@implementation SCKFeedEntry
- (instancetype)initWithTimestamp:(NSString *)timestamp
                           source:(NSString *)source
                          message:(NSString *)message {
  self = [super init];
  if (self) {
    _timestamp = [timestamp copy];
    _source = [source copy];
    _message = [message copy];
  }
  return self;
}
@end

@implementation SCKEngine

+ (int)minimumCanvasSize {
  return Defaults::minimumCanvasSize;
}
+ (int)maximumCanvasSize {
  return Defaults::maximumCanvasSize;
}

- (instancetype)init {
  self = [super init];
  if (!self) return nil;

  _device = MTLCreateSystemDefaultDevice();
  _queue = [_device newCommandQueue];
  if (_device && _queue)
    _graphite = sigil::skia::GraphiteContext::createMetal((__bridge void *)_device,
                                                          (__bridge void *)_queue);
  _sceneRenderer = std::make_unique<spellcircle::SceneRenderer>();

  // Same Syphon server name as the Qt app, so downstream clients
  // (TouchDesigner) don't care which receiver is running. Created up front:
  // The publisher announces itself immediately and keeps the last
  // published frame for late-joining clients.
  if (_device)
    _publisher = sigil::io::publish::createPublisher(
        "SpellCircle", sigil::io::publish::Backend::Metal, (__bridge void *)_device);

  // Only UDP: this product speaks nothing else, so the hub is taught the
  // one scheme its port is opened on.
  sigil::io::registerUdp(_hub);
  _port = Defaults::port;
  _statusText = @"Stopped";
  _targetFramesPerSecond = Defaults::targetFramesPerSecond;

  _canvasWidth = Defaults::canvasWidth;
  _canvasHeight = Defaults::canvasHeight;
  _scale = Defaults::scale;
  _strokeWidth = Defaults::strokeWidth;
  _labelOffset = Defaults::labelOffset;
  _pointDistance = Defaults::pointDistance;
  _boxWidth = Defaults::boxWidth;
  _boxHeight = Defaults::boxHeight;
  _boxPadding = Defaults::boxPadding;
  _boxDistance = Defaults::boxDistance;
  _fontSize = Defaults::fontSize;
  _fontFamily = [NSString stringWithUTF8String:Defaults::fontFamily];
  _fontWeight = Defaults::fontWeight;
  _fontItalic = Defaults::fontItalic;
  _accentColor = [NSColor colorWithSRGBRed:SkColorGetR(Defaults::color) / 255.0
                                     green:SkColorGetG(Defaults::color) / 255.0
                                      blue:SkColorGetB(Defaults::color) / 255.0
                                     alpha:SkColorGetA(Defaults::color) / 255.0];
  _darkAppearance = YES;  // the canvas view syncs the real appearance

  _timestampFormatter = [[NSDateFormatter alloc] init];
  _timestampFormatter.dateFormat = @"yyyy-MM-dd'T'HH:mm:ss.SSS";

  // Give Syphon (and the first drawInLayer:) an initial empty frame.
  [self renderScene];
  return self;
}

- (void)dealloc {
  [self closeDoor];
  _publisher.reset();
}

- (void)setPort:(int)port {
  const int boundedPort = std::clamp(port, 1, 65535);
  if (_port == boundedPort) return;
  _port = boundedPort;
  if (_listening) [self start];
}

// Reads as zero once the stream has been silent for a couple of seconds,
// so a stopped sender doesn't leave a stale rate on screen.
- (double)scenesPerSecond {
  return _session.packetRate();
}

// Every setter funnels through here: geometry resolution depends on the
// canvas size, style only on the render pass — re-running both keeps the
// setters trivially uniform, and a config change is user-interaction rate.
- (void)configDidChange {
  _sceneDirty = YES;
  [self.delegate engineSceneDidChange:self];
  [self renderTickIfDue];
}

- (void)setTargetFramesPerSecond:(double)targetFramesPerSecond {
  const double bounded = std::clamp(targetFramesPerSecond, kMinTargetFps, kMaxTargetFps);
  if (_targetFramesPerSecond == bounded) return;
  _targetFramesPerSecond = bounded;
  [self renderTickIfDue];
}

- (void)setDarkAppearance:(BOOL)darkAppearance {
  if (_darkAppearance == darkAppearance) return;
  _darkAppearance = darkAppearance;
  _checkerShader = nullptr;  // rebuilt from the new palette on the next blit
}

// NOLINTBEGIN(bugprone-macro-parentheses): a selector cannot be parenthesised
#define SCK_CONFIG_SETTER(Type, Name, Setter) \
  -(void)Setter : (Type)value {               \
    if (_##Name == value) return;             \
    _##Name = value;                          \
    [self configDidChange];                   \
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
// NOLINTEND(bugprone-macro-parentheses)

// Clamped separately from the macro setters: a 0-sized or Metal-exceeding
// texture allocation must never happen (it aborts under Metal validation),
// no matter what a settings field feeds in.
- (void)setCanvasWidth:(int)canvasWidth {
  const int bounded =
      std::clamp(canvasWidth, Defaults::minimumCanvasSize, Defaults::maximumCanvasSize);
  if (_canvasWidth == bounded) return;
  _canvasWidth = bounded;
  [self configDidChange];
}

- (void)setCanvasHeight:(int)canvasHeight {
  const int bounded =
      std::clamp(canvasHeight, Defaults::minimumCanvasSize, Defaults::maximumCanvasSize);
  if (_canvasHeight == bounded) return;
  _canvasHeight = bounded;
  [self configDidChange];
}

- (void)setFontFamily:(NSString *)fontFamily {
  if ([_fontFamily isEqualToString:fontFamily]) return;
  _fontFamily = [fontFamily copy];
  [self configDidChange];
}

- (void)setAccentColor:(NSColor *)accentColor {
  if ([_accentColor isEqual:accentColor]) return;
  _accentColor = [accentColor copy];
  [self configDidChange];
}

@end
