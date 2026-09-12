#import "SCKEngineInternal.h"
#import "SCKNetworkRuntimeInternal.h"

#include <algorithm>

namespace {

constexpr int kDefaultPort = 27015;  // matches the Qt app / python sender

constexpr double kDefaultTargetFps = 60.0;
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

- (instancetype)initWithRuntime:(SCKNetworkRuntime *)runtime {
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
  // SyphonMetalServer announces itself immediately and keeps the last
  // published frame for late-joining clients.
  if (_device)
    _syphon = [[SyphonMetalServer alloc] initWithName:@"SpellCircle" device:_device options:nil];

  _networkRuntime = runtime;
  _receiver = std::make_unique<spellcircle::UdpReceiver>([runtime executor]);
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
  _fontWeight = 700;  // bold, matching the Qt app's default QFont
  _fontItalic = NO;
  _accentColor = [NSColor colorWithSRGBRed:1.0 green:0.0 blue:0.0 alpha:1.0];
  _darkAppearance = YES;  // the canvas view syncs the real appearance

  _timestampFormatter = [[NSDateFormatter alloc] init];
  _timestampFormatter.dateFormat = @"yyyy-MM-dd'T'HH:mm:ss.SSS";

  // Give Syphon (and the first drawInLayer:) an initial empty frame.
  [self renderScene];
  return self;
}

- (void)dealloc {
  ++_networkGeneration;
  _receiver->stop();
  _receiver.reset();
  [_syphon stop];
}

- (void)setPort:(int)port {
  const int boundedPort = std::clamp(port, 1, 65535);
  if (_port == boundedPort) return;
  _port = boundedPort;
  if (_networkRequested) [self start];
}

- (BOOL)starting {
  return _networkRequested && !_listening;
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
  const int bounded = std::clamp(canvasWidth, 16, 8192);
  if (_canvasWidth == bounded) return;
  _canvasWidth = bounded;
  [self configDidChange];
}

- (void)setCanvasHeight:(int)canvasHeight {
  const int bounded = std::clamp(canvasHeight, 16, 8192);
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
