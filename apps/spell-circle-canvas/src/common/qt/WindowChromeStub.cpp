#include "WindowChrome.h"

bool WindowChrome::applyVibrancy(QQuickWindow*) { return false; }

bool WindowChrome::setSubtitle(QQuickWindow*, const QString&) { return false; }

bool WindowChrome::keepRendering(QQuickWindow* window) {
  if (!window) return false;
  window->setPersistentGraphics(true);
  window->setPersistentSceneGraph(true);
  return true;
}
