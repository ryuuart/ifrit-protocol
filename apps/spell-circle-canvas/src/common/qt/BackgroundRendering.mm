#import <AppKit/AppKit.h>
#import <objc/runtime.h>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QWindow>
#include "WindowChrome.h"

namespace {
// Retained for the lifetime of the process to keep the activity active.
id<NSObject> g_activityToken = nil;
using OcclusionStateImplementation = NSWindowOcclusionState (*)(id, SEL);
OcclusionStateImplementation g_originalOcclusionState = nullptr;

// Qt's Cocoa platform plugin responds to
// NSWindowDidChangeOcclusionStateNotification by querying -occlusionState.
// Returning NSWindowOcclusionStateVisible for the window keeps
// its render thread alive without changing the window's ordering or level.
// The dynamically-created class inherits from Qt's private QNSWindow class so
// all of Qt's native window behavior is retained.
NSWindowOcclusionState continuousOcclusionState(id self, SEL selector) {
  return g_originalOcclusionState(self, selector) | NSWindowOcclusionStateVisible;
}

Class continuousWindowClass(Class nativeWindowClass) {
  static Class subclass = Nil;
  if (subclass)
    return nativeWindowClass == subclass || nativeWindowClass == class_getSuperclass(subclass)
               ? subclass
               : Nil;

  constexpr const char *className = "dev_ifrit_Qt_ContinuousRenderingNativeWindow";
  subclass = objc_lookUpClass(className);
  if (subclass) {
    if (class_getSuperclass(subclass) != nativeWindowClass) {
      subclass = Nil;
      return Nil;
    }
    Method originalMethod =
        class_getInstanceMethod(class_getSuperclass(subclass), @selector(occlusionState));
    if (!originalMethod) {
      subclass = Nil;
      return Nil;
    }
    g_originalOcclusionState =
        reinterpret_cast<OcclusionStateImplementation>(method_getImplementation(originalMethod));
    return subclass;
  }

  subclass = objc_allocateClassPair(nativeWindowClass, className, 0);
  if (!subclass) return Nil;

  Method method = class_getInstanceMethod(nativeWindowClass, @selector(occlusionState));
  if (!method) {
    objc_disposeClassPair(subclass);
    subclass = Nil;
    return Nil;
  }
  g_originalOcclusionState =
      reinterpret_cast<OcclusionStateImplementation>(method_getImplementation(method));
  if (!g_originalOcclusionState || !class_addMethod(subclass, @selector(occlusionState),
                                                    reinterpret_cast<IMP>(continuousOcclusionState),
                                                    method_getTypeEncoding(method))) {
    objc_disposeClassPair(subclass);
    subclass = Nil;
    return Nil;
  }

  objc_registerClassPair(subclass);
  return subclass;
}
}  // namespace

namespace {

void disable() {
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

  Class currentClass = object_getClass(nativeWindow);
  Class subclass = continuousWindowClass(currentClass);
  if (!subclass) return false;
  if (currentClass != subclass) object_setClass(nativeWindow, subclass);

  return true;
}

}  // namespace

bool WindowChrome::keepRendering(QQuickWindow *window) {
  if (!window) return false;
  window->setPersistentGraphics(true);
  window->setPersistentSceneGraph(true);
  if (QGuiApplication::platformName() != "cocoa") return false;
  disable();
  return keepRenderingWhileOccluded(window);
}
