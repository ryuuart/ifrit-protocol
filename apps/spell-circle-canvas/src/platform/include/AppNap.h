#pragma once

namespace AppNap {

// Disables App Nap for the lifetime of the process. Must be called before
// the Qt event loop starts, since Syphon publishing and UDP packet handling
// both need to keep running at full rate even when the window is occluded
// or the app is backgrounded.
void disable();

} // namespace AppNap
