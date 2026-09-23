#pragma once

/** @file
 * The QML-embedded sketch surface: the registry on one side, one running
 * sketch on the other, and the live host between them.
 */

#include <sigilsketch/core/Session.h>
#include <sigilsketch/live/Residency.h>

#include <QtCore/QMutex>
#include <QtCore/QPointF>
#include <QtCore/QTimer>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <QtQuick/QQuickRhiItem>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "CanvasView.h"

namespace sigil::sketch {
class Host;
}

namespace sigil::weave {
class FontContext;
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
  /** WHETHER THIS CANVAS IS OFFERING ITS FRAMES to other applications.
   *  A run that asked for it on the command line comes up with it on;
   *  the window turns it on and off from here. */
  Q_PROPERTY(bool publishing READ publishing WRITE setPublishing NOTIFY
                 publishingChanged)
  Q_PROPERTY(
      QString publicationError READ publicationError NOTIFY publishingChanged)
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
  /** HOW FAR THE CANVAS IS MAGNIFIED on this item: item units per canvas
   *  unit. Zero, the default, fits the whole canvas into the item. A
   *  pan-zoom host sets it rather than growing the item, so the texture
   *  a frame is drawn into stays the size of what is on screen however
   *  far the reader zooms in, and the canvas off the item is clipped
   *  away before it is drawn. */
  Q_PROPERTY(qreal canvasScale READ canvasScale WRITE setCanvasScale NOTIFY
                 canvasViewChanged)
  /** Where the canvas's centre stands from this item's centre, in item
   *  units: the pan a host has moved it by. */
  Q_PROPERTY(QPointF canvasOffset READ canvasOffset WRITE setCanvasOffset
                 NOTIFY canvasViewChanged)

 public:
  explicit SketchbookView(QQuickItem* parent = nullptr);
  ~SketchbookView() override;

  QQuickRhiItemRenderer* createRenderer() override;

  /** Requests a capture of the current frame; render-thread work, so the
   *  saved path (or an empty string on failure) arrives via
   *  captureReady(). Writes beside the sketch, under captures/. */
  Q_INVOKABLE void capture();
  /** Starts the presented sketch's session from zero on the render thread,
   *  preserving its resident host, source watcher and build state. */
  Q_INVOKABLE void replay();
  /** Offers every frame drawn from here on to other applications, or
   *  stops offering them. Render-thread work, so it travels the way a
   *  capture request does: the renderer reads it on the next
   *  synchronize. A window that cannot publish what it draws turns this
   *  back off and says why, rather than publishing something else. */
  Q_INVOKABLE void setPublishing(bool publishing);
  /** Moves the viewpoint of a sketch that has one. */
  Q_INVOKABLE void orbit(float yawDeg, float pitchDeg, float distance);
  /** WHERE THE POINTER STANDS over this item, in its own coordinates,
   *  and whether its button is down. The item puts the point into the
   *  sketch's canvas units through the same view the frame is drawn
   *  through, so a sketch reads the pointer on the canvas it declared
   *  however far the reader has zoomed or panned it. A pointer off the
   *  canvas reaches the sketch only while a press begun on it is
   *  held. */
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
  [[nodiscard]] bool publishing() const { return m_publishing; }
  [[nodiscard]] QString publicationError() const { return m_publicationError; }
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
  [[nodiscard]] qreal canvasScale() const { return m_canvasScale; }
  void setCanvasScale(qreal scale);
  [[nodiscard]] QPointF canvasOffset() const { return m_canvasOffset; }
  void setCanvasOffset(const QPointF& offset);

  /** WHAT MOUNTS AT res:// FOR THE SESSIONS THIS WINDOW OPENS: the
   *  command line's `--assets`, else the demo root for a launch on the
   *  registry, else empty for a launch on a file — whose own sessions
   *  default to the `assets/` beside it, which is what makes a directory
   *  of sketches outside this repository a place to work, and whose
   *  registry sketches take the demo root. */
  static std::filesystem::path assetsDirectory;
  /** The directory the compiled-in sketches stand in, mounted at
   *  `sketch://` so a sketch this binary carries reaches its own files. */
  static std::filesystem::path sketchesDirectory;
  static std::filesystem::path flagsFile;
  /** WHAT THIS WINDOW'S FRAMES ARE OFFERED UNDER, and whether they are
   *  offered from the moment it opens — the command line's answer,
   *  written before anything is created. A subscriber binds to the
   *  name, so it is the run's and does not follow the sketch on screen.
   *  Defaults to Sketchbook when no name was supplied. */
  static std::string publishName;
  static bool publishAtStart;
  /** WHAT EVERY SESSION THIS WINDOW OPENS SHAPES TEXT WITH — the
   *  process's one font context, handed over by main() before QML loads.
   *  One owner: a context of this window's own would pay for the shaping
   *  and glyph caches a second time, beside the one the stills and the
   *  headless lanes already fill. Nothing opens until it is here. */
  static sigil::weave::FontContext* fonts;
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
  /** ONE SESSION AT A TIME. The session on screen is let go before the
   *  next one opens, rather than kept warm behind it. What a frame-rate
   *  sweep is asked for is one sketch's own rate, and a set of sessions
   *  standing behind it — holding their scenes, their images and their
   *  pipelines, and let go inside a later sketch's frames — is a cost
   *  that belongs to the window and not to the sketch being read. */
  static bool oneSessionAtATime;
  static QMutex hostMutex;

 signals:
  void sketchIndexChanged();
  void pausedChanged();
  void publishingChanged();
  void timeScaleChanged();
  void metricsChanged();
  void orbitChanged();
  void canvasViewChanged();
  void stateChanged();
  void captureReady(const QString& path);
  /** THE SKETCH ON SCREEN HAS BEEN PHOTOGRAPHED for the thumbnail store,
   *  at @p index, under the key its source stands at now. Emitted once
   *  per sketch opened, as the presented session reaches the moment it
   *  declared, so the browser's stills refresh as sketches are looked at
   *  and nothing renders in the background to keep them current. */
  void thumbnailCaptured(int index);

 protected:
  /** A window that is publishing keeps rendering whatever covers it, so
   *  the item asks that of each window it is placed in. */
  void itemChange(ItemChange change, const ItemChangeData& data) override;

 private:
  friend class SketchbookRenderer;

  QTimer m_timer;
  /** NOTHING IS PRESENTED UNTIL SOMETHING IS OPENED. The window comes up
   *  on the browser, and the canvas stays dark until a sketch is chosen —
   *  which is what leaves the machine to the thumbnail fill while the
   *  reader is still reading rows. */
  int m_sketchIndex = -1;
  bool m_paused = false;
  /** Written here, read by the renderer on the next synchronize. */
  bool m_publishing = false;
  QString m_publicationError;
  /** Refusals from an earlier render-thread request cannot cancel a retry. */
  std::uint64_t m_publicationRequest = 0;
  bool m_orbitable = false;
  double m_timeScale = 1.0;
  float m_yawDeg = 0.0f;
  float m_pitchDeg = 0.0f;
  float m_distance = 0.0f;
  bool m_orbitDirty = false;
  /** Published by the renderer from the running session: where the
   *  sketch is seen from, whether or not a pointer has moved it. */
  sigil::geometry::mesh::camera::Orbit m_orbit;
  /** The reader's view of the canvas, in item units; read by the
   *  renderer on every synchronize, and by the pointer as it arrives. */
  qreal m_canvasScale = 0.0;
  QPointF m_canvasOffset;
  /** Which pointer events reach the sketch: a press begun off the canvas
   *  is not the sketch's, and nor is a pointer hovering there, but a
   *  press begun on it is followed wherever it is dragged until it lets
   *  go. */
  PointerGate m_pointerGate;
  QVariantMap m_metrics = {
      {QStringLiteral("backend"), QStringLiteral("Waiting for a sketch")}};
  QString m_status;
  QString m_errorLog;
  QString m_state = QStringLiteral("waiting");
  int m_captureRequests = 0;  // consumed by the renderer in synchronize()
  int m_replayIndex = -1;     // consumed by the renderer in synchronize()
};
