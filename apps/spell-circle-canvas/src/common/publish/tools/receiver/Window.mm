// A publication on screen: one window, one view, and the newest frame
// fitted into it every time the display asks for one.

#include "Window.h"

#import <AppKit/AppKit.h>
#import <MetalKit/MetalKit.h>
#import <simd/simd.h>

#include <sigilpublish/Subscription.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>

namespace receiver {

namespace {

/** THE ONE PROGRAM THE WINDOW NEEDS: the newest frame, fitted into the
 *  drawable it is presented in. The frame's rows are taken in the order
 *  the texture holds them — the first row at the top — and its colour is
 *  written through as it stands, premultiplied, so a frame with
 *  transparency in it reads the way it would over the black behind it.
 *
 *  Compiled from source at start-up because it is one quad: a shader
 *  toolchain in the build for this would be more machinery than the
 *  drawing it does. */
constexpr char kProgram[] = R"(
#include <metal_stdlib>
using namespace metal;

struct Fitted {
  float4 position [[position]];
  float2 coordinate;
};

vertex Fitted frameVertex(uint id [[vertex_id]],
                          constant float2 &fit [[buffer(0)]]) {
  const float2 corners[4] = {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
  const float2 corner = corners[id];
  Fitted out;
  out.position = float4(corner * fit, 0, 1);
  out.coordinate = float2((corner.x + 1) * 0.5, (1 - corner.y) * 0.5);
  return out;
}

fragment float4 frameFragment(Fitted in [[stage_in]],
                              texture2d<float> frame [[texture(0)]]) {
  constexpr sampler read(mag_filter::linear, min_filter::linear);
  return frame.sample(read, in.coordinate);
}
)";

/** How often the window looks at what it is subscribed to: whether the
 *  publication is still there, how fast its frames are arriving, and
 *  whether one it lost has come back. Often enough to feel immediate,
 *  seldom enough that a window waiting for a publisher that never comes
 *  is idle. */
constexpr double kFollowSeconds = 0.5;

/** How long the frame rate in the title is averaged over. */
constexpr double kRateSeconds = 1.0;

}  // namespace

}  // namespace receiver

/** The window's own object: the view's delegate, the application's
 *  delegate, and the owner of the subscription both of them read. */
@interface ReceiverWindow : NSObject <MTKViewDelegate, NSApplicationDelegate>
@end

@implementation ReceiverWindow {
  NSWindow *_window;
  MTKView *_view;
  NSTimer *_follower;
  id<MTLCommandQueue> _queue;
  id<MTLRenderPipelineState> _pipeline;
  std::unique_ptr<sigil::publish::Subscription> _subscription;
  std::string _name;
  /** The size the last frame arrived at, so the window is resized when
   *  the publication's own size changes and not on every frame. */
  NSUInteger _width;
  NSUInteger _height;
  NSTimeInterval _rateAt;
  uint64_t _rateFrames;
  double _rate;
  /** Whether a subscription stood the last time anything was said about
   *  it, so coming and going are each said once. */
  BOOL _standing;
}

- (instancetype)initWithDevice:(id<MTLDevice>)device
                  subscription:(std::unique_ptr<sigil::publish::Subscription>)subscription
                          name:(std::string)name {
  self = [super init];
  if (!self) return nil;
  _subscription = std::move(subscription);
  _name = std::move(name);
  _queue = [device newCommandQueue];

  NSError *failure = nil;
  id<MTLLibrary> library = [device newLibraryWithSource:@(receiver::kProgram)
                                                options:nil
                                                  error:&failure];
  if (!library) {
    std::fprintf(stderr, "the frame program would not compile: %s\n",
                 failure.localizedDescription.UTF8String);
    return nil;
  }
  MTLRenderPipelineDescriptor *description = [[MTLRenderPipelineDescriptor alloc] init];
  description.label = @"receiver frame";
  description.vertexFunction = [library newFunctionWithName:@"frameVertex"];
  description.fragmentFunction = [library newFunctionWithName:@"frameFragment"];
  description.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
  _pipeline = [device newRenderPipelineStateWithDescriptor:description error:&failure];
  if (!_pipeline) {
    std::fprintf(stderr, "the frame program would not link: %s\n",
                 failure.localizedDescription.UTF8String);
    return nil;
  }

  // A SIZE TO START AT, not the publication's: that one is known only
  // once a frame has arrived, and the window is resized to it then.
  _window = [[NSWindow alloc]
      initWithContentRect:NSMakeRect(0, 0, 960, 600)
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                          NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                  backing:NSBackingStoreBuffered
                    defer:NO];
  _view = [[MTKView alloc] initWithFrame:_window.contentLayoutRect device:device];
  _view.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
  _view.clearColor = MTLClearColorMake(0, 0, 0, 1);
  _view.delegate = self;
  _window.contentView = _view;
  [_window center];
  _rateAt = [NSDate timeIntervalSinceReferenceDate];
  [self retitle];

  // FOLLOWING THE PUBLICATION IS NOT DRAWING IT. A window nobody can see
  // is not drawn — the display stops asking a covered window for frames —
  // and a subscription that only kept up while it was being drawn would
  // be a window that reconnects when it is uncovered rather than when its
  // publisher came back.
  _follower = [NSTimer scheduledTimerWithTimeInterval:receiver::kFollowSeconds
                                               target:self
                                             selector:@selector(follow)
                                             userInfo:nil
                                              repeats:YES];
  [self follow];
  return self;
}

/** The title says what is being received and how fast it is arriving —
 *  the publication's rate, not this window's, because what a reader is
 *  judging is the frames another application is producing. */
- (void)retitle {
  NSString *title = nil;
  if (_subscription->standing()) {
    const std::string drawnIn(_subscription->publishingApplication());
    NSString *app = drawnIn.empty() ? @"" : @(drawnIn.c_str());
    title =
        [NSString stringWithFormat:@"%s%@%@", _name.c_str(), app.length > 0 ? @" · " : @"", app];
    if (_rateFrames > 0) title = [title stringByAppendingFormat:@" — %.1f FPS", _rate];
  } else {
    title = [NSString stringWithFormat:@"%s — waiting", _name.c_str()];
  }
  _window.title = title;
}

- (void)show {
  [_window makeKeyAndOrderFront:nil];
}

- (void)fitTo:(id<MTLTexture>)frame {
  if (frame.width == _width && frame.height == _height) return;
  // Only the first frame's size decides where the window stands: after
  // that the reader has put it where they want it, and a publication that
  // changes size is not a reason to move it back.
  const BOOL first = _width == 0 && _height == 0;
  _width = frame.width;
  _height = frame.height;
  // THE PUBLICATION'S SIZE IN PIXELS, AS POINTS ON THIS SCREEN, so a
  // frame is shown at one pixel each where the screen can — and shrunk,
  // keeping its shape, where the screen is smaller than the frame.
  const CGFloat density = _window.screen ? _window.screen.backingScaleFactor : 2.0;
  CGFloat width = (CGFloat)_width / density;
  CGFloat height = (CGFloat)_height / density;
  const NSRect visible =
      _window.screen ? _window.screen.visibleFrame : NSMakeRect(0, 0, width, height);
  const CGFloat fit = std::min({1.0, visible.size.width / width, visible.size.height / height});
  [_window setContentSize:NSMakeSize(std::round(width * fit), std::round(height * fit))];
  if (first) [_window center];
}

/** A PUBLISHER THAT WENT AWAY IS WAITED FOR, not reported and given up
 *  on: the name is what this window holds, so whatever stands up under it
 *  next is what it shows. Said out loud as well as in the title, because
 *  a window opened from a terminal is watched from there too. */
- (void)follow {
  // ASKING FOR A FRAME IS WHAT OPENS, so a window that is not being
  // drawn asks for one anyway and throws it away: what it is following
  // is the name, and whatever stands up under it next.
  if (!_subscription->standing()) _subscription->newestFrame();
  if (_subscription->standing() != _standing) {
    _standing = !_standing;
    if (_standing)
      std::fprintf(stderr, "subscribed to \"%s\" (%s)\n", _name.c_str(),
                   std::string(_subscription->publishingApplication()).c_str());
    else
      std::fprintf(stderr, "\"%s\" stopped publishing; waiting for it\n", _name.c_str());
  }
  const NSTimeInterval now = [NSDate timeIntervalSinceReferenceDate];
  if (now - _rateAt >= receiver::kRateSeconds) {
    const uint64_t frames = _subscription->generation();
    _rate = (double)(frames - _rateFrames) / (now - _rateAt);
    _rateFrames = frames;
    _rateAt = now;
  }
  [self retitle];
}

- (void)drawInMTKView:(MTKView *)view {
  id<MTLTexture> frame = (__bridge id<MTLTexture>)_subscription->newestFrame();
  if (frame) [self fitTo:frame];
  MTLRenderPassDescriptor *pass = view.currentRenderPassDescriptor;
  if (!pass) return;
  id<MTLCommandBuffer> commands = [_queue commandBuffer];
  id<MTLRenderCommandEncoder> encoder = [commands renderCommandEncoderWithDescriptor:pass];
  if (frame) {
    const CGSize drawable = view.drawableSize;
    const double drawableShape = drawable.height > 0 ? drawable.width / drawable.height : 1.0;
    const double frameShape = frame.height > 0 ? (double)frame.width / (double)frame.height : 1.0;
    simd_float2 fit = {1, 1};
    if (frameShape > drawableShape)
      fit.y = (float)(drawableShape / frameShape);
    else
      fit.x = (float)(frameShape / drawableShape);
    [encoder setRenderPipelineState:_pipeline];
    [encoder setVertexBytes:&fit length:sizeof(fit) atIndex:0];
    [encoder setFragmentTexture:frame atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
  }
  [encoder endEncoding];
  if (id<CAMetalDrawable> drawable = view.currentDrawable) [commands presentDrawable:drawable];
  [commands commit];
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
  // The frame is fitted into whatever the drawable is every time it is
  // drawn, so a resize needs nothing prepared for it.
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app {
  return YES;
}

- (void)applicationWillTerminate:(NSNotification *)notification {
  // The subscription is let go here rather than left to the process's
  // end, so the publisher sees its last client leave and stops copying.
  _subscription.reset();
}

@end

namespace receiver {

namespace {

/** The menu a window needs to be closed and quit from the keyboard. */
void installMenu() {
  NSMenu *bar = [[NSMenu alloc] init];
  NSMenuItem *application = [[NSMenuItem alloc] init];
  NSMenu *menu = [[NSMenu alloc] init];
  [menu addItemWithTitle:@"Hide Receiver" action:@selector(hide:) keyEquivalent:@"h"];
  [menu addItemWithTitle:@"Close Window" action:@selector(performClose:) keyEquivalent:@"w"];
  [menu addItemWithTitle:@"Quit Receiver" action:@selector(terminate:) keyEquivalent:@"q"];
  application.submenu = menu;
  [bar addItem:application];
  NSApp.mainMenu = bar;
}

}  // namespace

int runWindow(const Arguments &arguments) {
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  if (!device) {
    std::fprintf(stderr, "this machine has no Metal device to receive on\n");
    return 4;
  }
  // The publication may not be there yet; the window says so and keeps
  // looking, which is the same answer it gives when one goes away.
  std::unique_ptr<sigil::publish::Subscription> subscription =
      sigil::publish::subscribe(arguments.server, arguments.app, (__bridge void *)device);
  if (!subscription) {
    std::fprintf(stderr, "this build subscribes to nothing\n");
    return 4;
  }

  [NSApplication sharedApplication];
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
  installMenu();
  // THE ONE STRONG REFERENCE. An application's delegate and a view's
  // delegate are both weak, so nothing else here holds this object.
  static ReceiverWindow *window = nil;
  window = [[ReceiverWindow alloc] initWithDevice:device
                                     subscription:std::move(subscription)
                                             name:arguments.server];
  if (!window) return 4;
  NSApp.delegate = window;
  [window show];
  if (@available(macOS 14.0, *))
    [NSApp activate];
  else
    [NSApp activateIgnoringOtherApps:YES];
  [NSApp run];
  return 0;
}

}  // namespace receiver
