/** @file
 * Background rendering on macOS: what asking for it does to the native
 * window, and what it must NOT do to it.
 *
 * The mechanism is one override on the window CLASS plus a mark on the
 * window itself. The thing it must never be is a change to the window's
 * own Objective-C class: key-value observing changes that too, the
 * system's window-sharing chrome observes a window it is hosted in, and
 * two parties swapping one object's isa leave Foundation's record of
 * which class it installed no longer describing the object.
 */

#import <AppKit/AppKit.h>
#import <objc/runtime.h>

#include <gtest/gtest.h>

#include <QtGui/QGuiApplication>
#include <QtQuick/QQuickWindow>
#include <memory>

#include "WindowChrome.h"

/** An observer that does nothing but exist, which is all key-value
 *  observing needs to reclassify the object it is registered on. */
@interface SilentObserver : NSObject
@end

@implementation SilentObserver
- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context {
}
@end

namespace {

/** A window server, or nothing this file asks can be answered. The
 *  cases carry the `cocoa` label for the same reason. */
bool hasWindowServer() {
  CFDictionaryRef session = CGSessionCopyCurrentDictionary();
  if (!session) return false;
  CFRelease(session);
  return true;
}

/** ONE QGuiApplication FOR THE PROCESS, stood up on first ask rather
 *  than in main(): listing the cases must not need a window server. */
bool applicationIsUp() {
  static const bool up = [] {
    if (!hasWindowServer()) return false;
    static int count = 1;
    static char name[] = "qt_test";
    static char *arguments[] = {name, nullptr};
    static auto *application = new QGuiApplication(count, arguments);
    (void)application;
    return QGuiApplication::platformName() == QStringLiteral("cocoa");
  }();
  return up;
}

#define SKIP_WITHOUT_A_WINDOW_SERVER()                                \
  do {                                                                \
    if (!applicationIsUp()) GTEST_SKIP() << "no Cocoa window server"; \
  } while (0)

/** A window with a native handle behind it, which is what
 *  `keepRendering` reaches for. Never ordered in: what is asserted is
 *  what the override answers, not what the compositor decides. */
std::unique_ptr<QQuickWindow> nativeWindow() {
  auto window = std::make_unique<QQuickWindow>();
  window->resize(160, 120);
  window->create();
  return window;
}

NSWindow *cocoaWindowOf(QQuickWindow &window) {
  // NOLINTNEXTLINE(performance-no-int-to-ptr,bugprone-casting-through-void)
  NSView *view = (__bridge NSView *)reinterpret_cast<void *>(window.winId());
  return view.window;
}

TEST(BackgroundRendering, LeavesTheWindowsClassAlone) {
  SKIP_WITHOUT_A_WINDOW_SERVER();
  std::unique_ptr<QQuickWindow> window = nativeWindow();
  NSWindow *native = cocoaWindowOf(*window);
  ASSERT_NE(native, nil);
  const Class before = object_getClass(native);
  const Class declaredBefore = [native class];

  EXPECT_TRUE(WindowChrome::keepRendering(window.get()));

  // THE WHOLE POINT. A window whose isa this changed would be a window
  // whose isa key-value observing can no longer account for.
  EXPECT_EQ(object_getClass(native), before);
  EXPECT_EQ([native class], declaredBefore);
}

TEST(BackgroundRendering, ReportsVisibleForAWindowThatAskedAndNoOther) {
  SKIP_WITHOUT_A_WINDOW_SERVER();
  std::unique_ptr<QQuickWindow> asked = nativeWindow();
  std::unique_ptr<QQuickWindow> quiet = nativeWindow();
  NSWindow *nativeAsked = cocoaWindowOf(*asked);
  NSWindow *nativeQuiet = cocoaWindowOf(*quiet);
  ASSERT_NE(nativeAsked, nil);
  ASSERT_NE(nativeQuiet, nil);

  ASSERT_TRUE(WindowChrome::keepRendering(asked.get()));
  EXPECT_TRUE(nativeAsked.occlusionState & NSWindowOcclusionStateVisible);

  // The override is on the CLASS, so it answers for every window of
  // that class — and a window that never asked has to come back exactly
  // as it would have. Neither window was ordered in, so neither is
  // visible unless the mark says so.
  EXPECT_FALSE(nativeQuiet.occlusionState & NSWindowOcclusionStateVisible);
}

TEST(BackgroundRendering, AsksForEveryWindowRatherThanTheFirst) {
  SKIP_WITHOUT_A_WINDOW_SERVER();
  std::unique_ptr<QQuickWindow> first = nativeWindow();
  std::unique_ptr<QQuickWindow> second = nativeWindow();
  EXPECT_TRUE(WindowChrome::keepRendering(first.get()));
  // A mechanism keyed on the first window's class alone answers a later
  // window with silence and says nothing about having done so.
  EXPECT_TRUE(WindowChrome::keepRendering(second.get()));
  NSWindow *nativeSecond = cocoaWindowOf(*second);
  ASSERT_NE(nativeSecond, nil);
  EXPECT_TRUE(nativeSecond.occlusionState & NSWindowOcclusionStateVisible);
}

TEST(BackgroundRendering, SurvivesAnObserverRegisteredBeforeAndAfter) {
  SKIP_WITHOUT_A_WINDOW_SERVER();
  std::unique_ptr<QQuickWindow> window = nativeWindow();
  NSWindow *native = cocoaWindowOf(*window);
  ASSERT_NE(native, nil);
  const Class declared = [native class];

  // AN OBSERVER BEFORE. This is what the system's window-sharing chrome
  // has already done by the time a publication is started: the window's
  // isa is a generated notifying class, and anything installed over
  // THAT class is installed under a class Foundation owns and disposes.
  SilentObserver *before = [[SilentObserver alloc] init];
  [native addObserver:before
           forKeyPath:@"frame"
              options:NSKeyValueObservingOptionNew
              context:nullptr];

  EXPECT_TRUE(WindowChrome::keepRendering(window.get()));
  EXPECT_EQ([native class], declared);
  EXPECT_TRUE(native.occlusionState & NSWindowOcclusionStateVisible);

  // AN OBSERVER AFTER, and then both withdrawn — the teardown that
  // faulted while one window's class was this process's to swap.
  SilentObserver *after = [[SilentObserver alloc] init];
  [native addObserver:after
           forKeyPath:@"title"
              options:NSKeyValueObservingOptionNew
              context:nullptr];
  [native removeObserver:after forKeyPath:@"title"];
  [native removeObserver:before forKeyPath:@"frame"];
  [after release];
  [before release];

  EXPECT_EQ([native class], declared);
  EXPECT_TRUE(native.occlusionState & NSWindowOcclusionStateVisible);
}

}  // namespace
