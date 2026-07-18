#pragma once
#include <QObject>
#include <QQuickWindow>
#include <QtQml/qqmlregistration.h>

/**
 * QML singleton for native window dressing. On macOS applyVibrancy() slips
 * an NSVisualEffectView behind the window's Qt content so a transparent
 * window picks up the system "liquid glass" material and adapts to light
 * and dark mode automatically. On other platforms it is a no-op returning
 * false, letting callers keep an opaque palette-driven background.
 */
class WindowChrome : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

public:
  explicit WindowChrome(QObject *parent = nullptr) : QObject(parent) {}

  /** Installs the native glass background behind @p window. Returns true
   *  when the window is now vibrant and should be made transparent. Safe to
   *  call more than once per window. */
  Q_INVOKABLE bool applyVibrancy(QQuickWindow *window);

  /** Sets the native title-bar subtitle (macOS 11+). Returns false where
   *  subtitles are unavailable so callers can fall back to a composite
   *  window title. */
  Q_INVOKABLE bool setSubtitle(QQuickWindow *window, const QString &subtitle);
};
