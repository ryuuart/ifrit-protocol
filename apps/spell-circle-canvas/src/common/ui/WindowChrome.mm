#include "WindowChrome.h"
#import <AppKit/AppKit.h>
#include <QQuickWindow>

/** Subclass exists only so repeat applyVibrancy() calls can recognize the
 *  effect view we inserted and stay idempotent. */
@interface IfritVibrancyView : NSVisualEffectView
@end

@implementation IfritVibrancyView
@end

bool WindowChrome::setSubtitle(QQuickWindow *window, const QString &subtitle) {
  if (!window)
    return false;
  auto *contentView = reinterpret_cast<NSView *>(window->winId());
  if (!contentView || !contentView.window)
    return false;
  if (@available(macOS 11.0, *)) {
    contentView.window.subtitle = subtitle.toNSString();
    return true;
  }
  return false;
}

bool WindowChrome::applyVibrancy(QQuickWindow *window) {
  if (!window)
    return false;

  // winId() forces platform-window creation for windows that are not yet
  // visible (the settings window starts hidden), so the NSWindow exists.
  auto *contentView = reinterpret_cast<NSView *>(window->winId());
  if (!contentView)
    return false;
  NSWindow *nativeWindow = contentView.window;
  if (!nativeWindow)
    return false;

  // Escape hatch for debugging compositing issues: opaque palette-driven
  // windows instead of the glass background.
  if (qEnvironmentVariableIsSet("IFRIT_NO_VIBRANCY"))
    return false;

  nativeWindow.opaque = NO;
  nativeWindow.backgroundColor = NSColor.clearColor;
  nativeWindow.titlebarAppearsTransparent = YES;

  // Qt's view is the NSWindow content view, and subviews always draw above
  // their superview, so the effect view goes into the frame view *behind*
  // the content view instead.
  NSView *frameView = nativeWindow.contentView.superview;
  if (!frameView)
    return false;
  for (NSView *subview in frameView.subviews) {
    if ([subview isKindOfClass:IfritVibrancyView.class])
      return true;
  }

  IfritVibrancyView *effectView =
      [[IfritVibrancyView alloc] initWithFrame:frameView.bounds];
  effectView.material = NSVisualEffectMaterialUnderWindowBackground;
  effectView.blendingMode = NSVisualEffectBlendingModeBehindWindow;
  effectView.state = NSVisualEffectStateFollowsWindowActiveState;
  effectView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

  [frameView addSubview:effectView
             positioned:NSWindowBelow
             relativeTo:nativeWindow.contentView];

  return true;
}
