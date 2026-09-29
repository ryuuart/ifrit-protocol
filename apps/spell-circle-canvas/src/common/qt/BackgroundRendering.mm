#import <AppKit/AppKit.h>
#import <objc/message.h>
#import <objc/runtime.h>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QHash>
#include <QWindow>
#include <atomic>

#include "ActivityHold.h"
#include "WindowChrome.h"

namespace {

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

/** The system's record of the activity, present exactly while the hold
 *  below is taken. */
id<NSObject> g_activityToken = nil;

void beginActivity() {
  if (g_activityToken) return;
  g_activityToken = [[[NSProcessInfo processInfo]
      beginActivityWithOptions:NSActivityUserInitiated | NSActivityLatencyCritical
                        reason:@"A window on screen, or frames published to other applications"]
      retain];
}

void endActivity() {
  if (!g_activityToken) return;
  [[NSProcessInfo processInfo] endActivity:g_activityToken];
  [g_activityToken release];
  g_activityToken = nil;
}

/** THE ONE OWNER OF THE ACTIVITY TOKEN. Publishing and a window's
 *  visibility each record a reason here and never begin or end the
 *  activity themselves, so the token is taken while either stands and
 *  released only when neither does. Without it the system demotes every
 *  thread of an application that is not frontmost to the background
 *  tier however much the application draws, and a window behind another
 *  application stops presenting at the display's rate. GUI thread
 *  only. */
ifrit::qt::ActivityHold &activityHold() {
  static ifrit::qt::ActivityHold *hold = [] {
    auto *made = new ifrit::qt::ActivityHold([](bool held) {
      if (held)
        beginActivity();
      else
        endActivity();
    });
    QObject::connect(QCoreApplication::instance(),
                     &QCoreApplication::aboutToQuit, [] { endActivity(); });
    return made;
  }();
  return *hold;
}

NSWindow *nativeWindowOf(QWindow *window) {
  // On macOS QWindow::winId() is the native NSView, and calling it
  // creates the platform window when there is none yet. Qt hands the
  // view over as an integer, and ARC bridges only from void*.
  // NOLINTNEXTLINE(performance-no-int-to-ptr,bugprone-casting-through-void)
  NSView *nativeView = (__bridge NSView *)reinterpret_cast<void *>(window->winId());
  return nativeView.window;
}

/** What watches one window's visibility on the system's behalf: the
 *  notification observers registered for its native window. */
struct VisibilityWatch {
  NSWindow *nativeWindow = nil;
  id occlusionObserver = nil;
  id closeObserver = nil;
};

QHash<QWindow *, VisibilityWatch> &visibilityWatches() {
  static QHash<QWindow *, VisibilityWatch> watches;
  return watches;
}

/** Removes the observers on @p window's native window, which leaves
 *  the window watched for its next showing but no longer visible. */
void forgetNativeWindow(QWindow *window) {
  auto found = visibilityWatches().find(window);
  if (found == visibilityWatches().end()) return;
  NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
  if (found->occlusionObserver) [center removeObserver:found->occlusionObserver];
  if (found->closeObserver) [center removeObserver:found->closeObserver];
  *found = VisibilityWatch{};
  activityHold().set(window, ifrit::qt::ActivityReason::Visible, false);
}

void stopWatching(QWindow *window) {
  forgetNativeWindow(window);
  visibilityWatches().remove(window);
}

/** Reads whether any part of @p window is on screen. The native
 *  occlusion state is the system's own answer and covers every way a
 *  window leaves the screen — hidden, minimised, on another space,
 *  wholly covered, or the display asleep — where Qt's visibility says
 *  only whether the window was shown. */
void readVisibility(QWindow *window) {
  const VisibilityWatch &watch = visibilityWatches().value(window);
  const bool visible =
      watch.nativeWindow &&
      (watch.nativeWindow.occlusionState & NSWindowOcclusionStateVisible);
  activityHold().set(window, ifrit::qt::ActivityReason::Visible, visible);
}

/** Starts watching @p window's native window, which must exist. */
void watchNativeWindow(QWindow *window) {
  if (visibilityWatches().value(window).nativeWindow) return;
  NSWindow *nativeWindow = nativeWindowOf(window);
  if (!nativeWindow) return;
  NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
  VisibilityWatch watch;
  watch.nativeWindow = nativeWindow;
  watch.occlusionObserver = [center
      addObserverForName:NSWindowDidChangeOcclusionStateNotification
                  object:nativeWindow
                   queue:nil
              usingBlock:^(NSNotification *) {
                readVisibility(window);
              }];
  // Delivered synchronously: AppKit posts both on the main thread, which
  // is the thread the hold lives on.
  // A closed native window posts no further occlusion changes, and Qt
  // may make another one for the same QWindow when it is shown again.
  watch.closeObserver = [center addObserverForName:NSWindowWillCloseNotification
                                            object:nativeWindow
                                             queue:nil
                                        usingBlock:^(NSNotification *) {
                                          forgetNativeWindow(window);
                                        }];
  visibilityWatches()[window] = watch;
  readVisibility(window);
}

bool keepRenderingWhileOccluded(QWindow *window) {
  if (!window) return false;

  // Creates the platform window when there is none, so the override is
  // installed before the window is first shown.
  NSWindow *nativeWindow = nativeWindowOf(window);
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
  // A publisher's subscribers watch its frames whether or not the window
  // is on screen, so this reason is never withdrawn while the process
  // runs, not even when publishing stops.
  activityHold().set(window, ifrit::qt::ActivityReason::Publishing, true);
  return keepRenderingWhileOccluded(window);
}

bool WindowChrome::keepActiveWhileVisible(QQuickWindow *window) {
  if (!window) return false;
  if (QGuiApplication::platformName() != "cocoa") return false;
  if (visibilityWatches().contains(window)) return true;
  visibilityWatches().insert(window, VisibilityWatch{});
  QObject::connect(window, &QObject::destroyed, [window] { stopWatching(window); });
  // Watched from the first time the window is shown rather than now:
  // asking for the native window before then would create it before the
  // window's own properties are settled.
  QObject::connect(window, &QWindow::visibleChanged, [window](bool visible) {
    if (visible)
      watchNativeWindow(window);
    else
      readVisibility(window);
  });
  if (window->handle() && window->isVisible()) watchNativeWindow(window);
  return true;
}
