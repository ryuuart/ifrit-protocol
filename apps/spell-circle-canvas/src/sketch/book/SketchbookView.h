#pragma once

/** @file
 * The QML-embedded sketch surface: the registry on one side, one running
 * sketch on the other, and the live host between them.
 */

#include <sigilsketch/core/Session.h>
#include <sigilsketch/live/Residency.h>

#include <QtCore/QMutex>
#include <QtCore/QTimer>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <QtQuick/QQuickRhiItem>
#include <filesystem>
#include <vector>

namespace sigil::sketch {
class Host;
}

/** THE LIVE CANVAS. Frames render on the render thread, through the
 *  shared Skia Graphite context straight into the item's texture when the
 *  QRhi backend supports it, with an explicit raster-and-upload fallback
 *  elsewhere.
 *
 *  Selecting a sketch replaces the running host, and every sketch this
 *  binary carries is already compiled in — so selection is instant and a
 *  rebuild happens only when the file on disk changes. */
class SketchbookView : public QQuickRhiItem {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(int sketchIndex READ sketchIndex WRITE setSketchIndex NOTIFY
                 sketchIndexChanged)
  Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
  Q_PROPERTY(double timeScale READ timeScale WRITE setTimeScale NOTIFY
                 timeScaleChanged)
  // Structured, not preformatted: the panel is narrow and its width is
  // the reader's to drag, so QML owns the elision and the formatting.
  Q_PROPERTY(QVariantMap metrics READ metrics NOTIFY metricsChanged)
  Q_PROPERTY(QString status READ status NOTIFY stateChanged)
  Q_PROPERTY(QString errorLog READ errorLog NOTIFY stateChanged)
  Q_PROPERTY(QString state READ state NOTIFY stateChanged)
  /** Whether the running sketch has a viewpoint a pointer can move. */
  Q_PROPERTY(bool orbitable READ orbitable NOTIFY sketchIndexChanged)
  /** WHERE THE RUNNING SKETCH STANDS: the yaw, pitch and distance of the
   *  viewpoint it is being seen from right now. A drag reads these at
   *  the moment it starts, so the first one continues the sketch's own
   *  framing instead of jumping to a viewpoint this host invented. */
  Q_PROPERTY(qreal orbitYaw READ orbitYaw NOTIFY orbitChanged)
  Q_PROPERTY(qreal orbitPitch READ orbitPitch NOTIFY orbitChanged)
  Q_PROPERTY(qreal orbitDistance READ orbitDistance NOTIFY orbitChanged)

 public:
  explicit SketchbookView(QQuickItem* parent = nullptr);
  ~SketchbookView() override;

  QQuickRhiItemRenderer* createRenderer() override;

  /** Requests a capture of the current frame; render-thread work, so the
   *  saved path (or an empty string on failure) arrives via
   *  captureReady(). Writes beside the sketch, under captures/. */
  Q_INVOKABLE void capture();
  /** Moves the viewpoint of a sketch that has one. */
  Q_INVOKABLE void orbit(float yawDeg, float pitchDeg, float distance);
  /** WHERE THE POINTER STANDS over this item, in its own coordinates,
   *  and whether its button is down. The item puts the point into the
   *  sketch's canvas units through the same fit the frame is drawn
   *  with, so a sketch reads the pointer on the canvas it declared
   *  whatever the window did to that canvas. */
  Q_INVOKABLE void pointer(qreal x, qreal y, bool pressed);
  /** A KEY GOING DOWN OR UP, as Qt reports it: its key and the text it
   *  types. The sketch is handed the name a keyboard spells it by —
   *  "a", "ArrowLeft", "Enter" — and the code p5 gives it, so a sketch
   *  pasted from p5 compares against the numbers it already knows. A
   *  key held down repeats, which the runtimes coalesce: a key already
   *  down stays down and the press is one event a frame. */
  Q_INVOKABLE void key(int qtKey, const QString& text, bool pressed);

  [[nodiscard]] int sketchIndex() const { return m_sketchIndex; }
  void setSketchIndex(int index);
  [[nodiscard]] bool paused() const { return m_paused; }
  void setPaused(bool paused);
  [[nodiscard]] double timeScale() const { return m_timeScale; }
  void setTimeScale(double scale);
  [[nodiscard]] QVariantMap metrics() const { return m_metrics; }
  [[nodiscard]] QString status() const { return m_status; }
  [[nodiscard]] QString errorLog() const { return m_errorLog; }
  // the QML-facing property is named for the sketch's state, not the
  // item's
  // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
  [[nodiscard]] QString state() const { return m_state; }
  [[nodiscard]] bool orbitable() const { return m_orbitable; }
  [[nodiscard]] qreal orbitYaw() const { return m_orbit.yawDeg; }
  [[nodiscard]] qreal orbitPitch() const { return m_orbit.pitchDeg; }
  [[nodiscard]] qreal orbitDistance() const { return m_orbit.distance; }

  /** Where the live host finds the file behind a registry entry, and the
   *  compiler line the build captured. Set by main() before QML loads. */
  static std::filesystem::path sketchDir;
  /** Where a sketch looks for what it did not generate. Empty means
   *  `assets/` beside whichever file is open, which is what makes a
   *  directory of sketches outside this repository a place to work. */
  static std::filesystem::path assetsDir;
  static std::filesystem::path flagsFile;
  /** The shared layer: the directory whose sources are units of every
   *  sketch and whose headers a sketch spells as `<shared/Name.h>`. */
  static std::filesystem::path sharedDir;
  /** SKETCHES THIS BINARY DOES NOT CARRY, opened from a path.
   *
   *  The registry is the compiled-in table and settles the first time it
   *  is read, so a file opened by path cannot join it. It joins this
   *  list instead, which the listing reads after the registry — the two
   *  cannot disagree, because an entry here is a file this binary was
   *  never built with. Its name is the file's stem: the dylib a
   *  hot-loaded sketch exports carries neither key nor name. */
  static std::vector<std::filesystem::path> externals;
  /** The host the render thread draws and the GUI thread polls — every
   *  access on either side takes the mutex beside it. It is the resident
   *  set's presented session, held as a pointer because that is what
   *  every frame, poll and capture already reaches for. */
  static sigil::sketch::Host* host;
  /** THE HOSTS THIS WINDOW HAS OPENED. Selecting a sketch swaps which host
   *  is presented rather than compiling it again. A returning host opens a
   *  fresh runtime session so setup and entrance animations replay, while its
   *  compiler, watched source and loaded libraries stay warm. Under the same
   *  mutex as `host`. */
  static sigil::sketch::Residency sessions;
  static QMutex hostMutex;

 signals:
  void sketchIndexChanged();
  void pausedChanged();
  void timeScaleChanged();
  void metricsChanged();
  void orbitChanged();
  void stateChanged();
  void captureReady(const QString& path);

 private:
  friend class SketchbookRenderer;

  QTimer m_timer;
  int m_sketchIndex = 0;
  bool m_paused = false;
  bool m_orbitable = false;
  double m_timeScale = 1.0;
  float m_yawDeg = 0.0f;
  float m_pitchDeg = 0.0f;
  float m_distance = 0.0f;
  bool m_orbitDirty = false;
  /** Published by the renderer from the running session: where the
   *  sketch is seen from, whether or not a pointer has moved it. */
  sigil::sketch::Orbit m_orbit;
  QVariantMap m_metrics = {{QStringLiteral("backend"),
                            QStringLiteral("hardware QRhi renderer required")}};
  QString m_status;
  QString m_errorLog;
  QString m_state = QStringLiteral("waiting");
  int m_captureRequests = 0;  // consumed by the renderer in synchronize()
};
