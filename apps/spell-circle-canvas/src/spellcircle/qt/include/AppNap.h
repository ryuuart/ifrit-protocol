#pragma once

class QWindow;

namespace AppNap {

// Disables App Nap for the lifetime of the process. Must be called before
// the Qt event loop starts, since Syphon publishing and UDP packet handling
// both need to keep running at full rate even when the window is occluded
// or the app is backgrounded.
void disable();

// Keeps an on-screen Qt window exposed to Qt's render loop when another
// window fully covers it. macOS normally reports a fully occluded NSWindow as
// non-renderable; Qt responds by removing that QQuickWindow from its render
// thread, which also stops offscreen Syphon publishing tied to that thread.
// Returns false if the native Cocoa window could not be configured.
bool keepRenderingWhileOccluded(QWindow *window);

} // namespace AppNap
