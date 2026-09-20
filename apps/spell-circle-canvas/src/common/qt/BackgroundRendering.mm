#import <AppKit/AppKit.h>
#import <objc/message.h>
#import <objc/runtime.h>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QWindow>
#include <atomic>
#include "WindowChrome.h"

namespace {
// Retained for the lifetime of the process to keep the activity active.
id<NSObject> g_activityToken = nil;

/** WHICH WINDOW ASKED, held beside the window rather than in it.
 *
 *  An associated object lives in a side table keyed by the object and
 *  changes neither the window's Objective-C class nor its isa, so
 *  nothing here can collide with the class Cocoa's key-value observing
 *  installs on the same object — which the system's own window-sharing
 *  chrome does install, because it is hosted in a remote view that
 *  observes the window it sits in. */
const void *const kKeepRenderingKey = &kKeepRenderingKey;

bool keepsRendering(id window) {
  return objc_getAssociatedObject(window, kKeepRenderingKey) != nil;
}

/** ONE CLASS THE OVERRIDE WAS ADDED TO, and the class whose
 *  -occlusionState it must chain into.
 *
 *  The superclass is recorded rather than derived from the receiver:
 *  under key-value observing the receiver's isa is a generated
 *  notifying class whose superclass is the very class this override was
 *  added to, so walking up from the isa would call the override again. */
struct ClassOverride {
  std::atomic<Class> overridden{Nil};
  Class chainsInto = Nil;
};

// Qt names one native window class per window flavour, and there are two
// of them; the spare entries cost nothing and the table never grows at
// runtime beyond what the process actually shows.
constexpr int kMaxOverriddenClasses = 8;
ClassOverride g_overrides[kMaxOverriddenClasses];
std::atomic<int> g_overrideCount{0};

/** The class @p windowClass or its nearest overridden ancestor chains
 *  into, or Nil when neither was overridden.
 *
 *  THE OVERRIDDEN CLASSES MUST BE SIBLINGS, never one an ancestor of
 *  another. Were a class overridden and an ancestor of it overridden
 *  afterwards, this would answer a receiver of the first with the
 *  second — which now carries the override too, so the chained call
 *  would re-enter the implementation below and never stop. The two
 *  classes handed in are the native window classes Qt names for a
 *  window and for a panel, which descend from NSWindow and NSPanel
 *  separately, so neither is above the other. */
Class chainForClass(Class windowClass) {
  const int count = g_overrideCount.load(std::memory_order_acquire);
  for (Class candidate = windowClass; candidate;
       candidate = class_getSuperclass(candidate))
    for (int at = 0; at < count; ++at)
      if (g_overrides[at].overridden.load(std::memory_order_acquire) ==
          candidate)
        return g_overrides[at].chainsInto;
  return Nil;
}

// Qt's Cocoa platform plugin responds to
// NSWindowDidChangeOcclusionStateNotification by querying -occlusionState,
// and unexposes a window it reports as not visible, which stops that
// window's render loop. Returning NSWindowOcclusionStateVisible for a
// window that asked keeps its render thread alive without changing the
// window's ordering or level. Every other window of the same class is
// answered exactly as it was.
NSWindowOcclusionState continuousOcclusionState(id self, SEL selector) {
  // -class, not object_getClass: a key-value observing notifying class
  // answers the class it was generated from, which is the one this
  // override was added to.
  const Class chain = chainForClass([self class]);
  NSWindowOcclusionState reported = 0;
  if (chain) {
    struct objc_super target = {self, chain};
    using SuperOcclusionState = NSWindowOcclusionState (*)(struct objc_super *,
                                                           SEL);
    reported = reinterpret_cast<SuperOcclusionState>(objc_msgSendSuper)(
        &target, selector);
  } else {
    // Unreachable: the entry is published before the method is added, so
    // a receiver reaching this implementation has one. Reporting visible
    // is the answer that cannot stop a window from drawing.
    reported = NSWindowOcclusionStateVisible;
  }
  return keepsRendering(self) ? reported | NSWindowOcclusionStateVisible
                              : reported;
}

/** Adds the override to @p windowClass, once. True when the class now
 *  answers through it.
 *
 *  workaround: Qt's Cocoa plugin decides whether to keep rendering from
 *  the native window's -occlusionState and offers no way to say that a
 *  window covered by another must keep drawing. The override is on the
 *  CLASS: changing one window's Objective-C class instead would collide
 *  with the class key-value observing installs on the same object, and
 *  the system's own window-sharing chrome observes a window it is hosted
 *  in.
 */
bool overrideOcclusionState(Class windowClass) {
  if (!windowClass) return false;
  if (chainForClass(windowClass)) return true;  // already answers through it

  Class chain = class_getSuperclass(windowClass);
  if (!chain) return false;
  const Method inherited =
      class_getInstanceMethod(windowClass, @selector(occlusionState));
  if (!inherited) return false;

  const int at = g_overrideCount.load(std::memory_order_relaxed);
  if (at >= kMaxOverriddenClasses) return false;
  // PUBLISHED BEFORE THE METHOD IS ADDED, because the implementation
  // reads this table the moment it becomes reachable. The count cannot
  // then be wound back, so a class that fails below spends its entry
  // for the life of the process. It fails only for a class that
  // implements -occlusionState itself, which no NSWindow subclass in
  // reach does, and the spare entries absorb it either way.
  g_overrides[at].chainsInto = chain;
  g_overrides[at].overridden.store(windowClass, std::memory_order_release);
  g_overrideCount.store(at + 1, std::memory_order_release);

  if (class_addMethod(windowClass, @selector(occlusionState),
                      reinterpret_cast<IMP>(continuousOcclusionState),
                      method_getTypeEncoding(inherited)))
    return true;

  // The class implements -occlusionState itself, so there is nothing to
  // shadow and the entry above describes a chain that is not this one.
  g_overrides[at].overridden.store(Nil, std::memory_order_release);
  return false;
}

}  // namespace

namespace {

void beginContinuousActivity() {
  if (g_activityToken) return;

  g_activityToken = [[[NSProcessInfo processInfo]
      beginActivityWithOptions:NSActivityUserInitiated | NSActivityLatencyCritical
                        reason:@"Continuous rendering and background frame publication"] retain];
  QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [] {
    [[NSProcessInfo processInfo] endActivity:g_activityToken];
    [g_activityToken release];
    g_activityToken = nil;
  });
}

bool keepRenderingWhileOccluded(QWindow *window) {
  if (!window) return false;

  // On macOS QWindow::winId() is the native NSView. Calling it also ensures
  // the platform window has been created before we install the override.
  // Qt hands the view over as an integer, and ARC bridges only from void*.
  // NOLINTNEXTLINE(performance-no-int-to-ptr,bugprone-casting-through-void)
  NSView *nativeView = (__bridge NSView *)reinterpret_cast<void *>(window->winId());
  NSWindow *nativeWindow = nativeView.window;
  if (!nativeWindow) return false;

  // -class, not object_getClass: an observer already registered on this
  // window has replaced its isa with a generated notifying class, and
  // overriding a method on THAT class would put this process's override
  // under a class Foundation owns and disposes.
  if (!overrideOcclusionState([nativeWindow class])) return false;
  objc_setAssociatedObject(nativeWindow, kKeepRenderingKey, @YES,
                           OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  return true;
}

}  // namespace

bool WindowChrome::keepRendering(QQuickWindow *window) {
  if (!window) return false;
  window->setPersistentGraphics(true);
  window->setPersistentSceneGraph(true);
  if (QGuiApplication::platformName() != "cocoa") return false;
  beginContinuousActivity();
  return keepRenderingWhileOccluded(window);
}
