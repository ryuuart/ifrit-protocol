/** @file
 * The window door: a Qt Quick window holding one pane, or no window at
 * all, and the host it shows.
 */

#include "sigilsketch/window/Window.h"

#include <sigilcore/hardware/GpuDevice.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilsketch/core/Device.h>
#include <sigilsketch/core/Painting.h>
#include <sigilsketch/live/Host.h>

#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QEventLoop>
#include <QtCore/QMutexLocker>
#include <QtCore/QPointer>
#include <QtGui/QColor>
#include <QtGui/QGuiApplication>
#include <QtQuick/QQuickGraphicsDevice>
#include <QtQuick/QQuickWindow>
#include <exception>
#include <utility>

#include "WindowCanvas.h"

#ifdef __APPLE__
#include <pthread.h>
#endif

namespace sigil::sketch {

namespace {

/** Writes @p reason to @p why when the caller asked for one, and answers
 *  null. */
std::unique_ptr<Window> refused(std::string* why, std::string reason) {
  if (why) *why = std::move(reason);
  return nullptr;
}

bool onMainThread() {
#ifdef __APPLE__
  return pthread_main_np() != 0;
#else
  return !QCoreApplication::instance() ||
         QCoreApplication::instance()->thread() == QThread::currentThread();
#endif
}

/** THE PROCESS'S ONE GUI APPLICATION, made on first ask when the process
 *  has none and kept for the rest of its life: Qt allows one per process,
 *  and a second window joins it. Frames are drawn on the thread that
 *  pumps it — Qt Quick's basic render loop — unless the process chose a
 *  loop itself before the application was made. Null, with the reason,
 *  where the process already holds an application that draws no
 *  windows. */
QGuiApplication* application(std::string* why) {
  if (QCoreApplication* existing = QCoreApplication::instance()) {
    auto* gui = qobject_cast<QGuiApplication*>(existing);
    if (!gui && why)
      *why = "this process runs a Qt application that draws no windows";
    return gui;
  }
  if (qEnvironmentVariableIsEmpty("QSG_RENDER_LOOP"))
    qputenv("QSG_RENDER_LOOP", QByteArray("basic"));
  static int argumentCount = 1;
  static char name[] = "sigil";
  static char* arguments[] = {name, nullptr};
  return new QGuiApplication(argumentCount, arguments);
}

/** HANDS @p window THE PROCESS'S OWN PAINTER'S DEVICE AND QUEUE to render
 *  on, where no device is installed and the window's frames are drawn on
 *  the thread that opened it, which is the thread the painter's recorder
 *  belongs to: the window's frames are then drawn through the painter's
 *  context, where a picture drawn with no frame open already stands. */
void renderOnOwnPainter(QQuickWindow& window) {
#ifdef __APPLE__
  if (device() || qgetenv("QSG_RENDER_LOOP") != QByteArray("basic")) return;
  const Painter own = ownPainter();
  if (!own.graphite || hostlessGraphite() != own.graphite) return;
  const core::hardware::NativeDevice& native = own.device->native();
  if (native.backend != core::hardware::Backend::Metal || !native.mtlDevice ||
      !native.mtlCommandQueue)
    return;
  window.setGraphicsDevice(QQuickGraphicsDevice::fromDeviceAndCommandQueue(
      static_cast<MTLDevice*>(native.mtlDevice),
      static_cast<MTLCommandQueue*>(native.mtlCommandQueue)));
#else
  (void)window;
#endif
}

/** A WINDOW ON SCREEN, or, headless, the host alone stepped on the wall's
 *  clock. */
class SketchWindow final : public Window {
 public:
  explicit SketchWindow(const WindowOptions& options)
      : m_headless(options.headless),
        m_presented(std::make_shared<PresentedHost>()) {
    if (m_headless) return;
    m_window = new QQuickWindow();
    m_window->setTitle(QString::fromStdString(options.title));
    m_window->resize(options.width, options.height);
    m_window->setColor(QColor::fromRgb(kPaneMatte));
    renderOnOwnPainter(*m_window);
    m_canvas = new WindowCanvas(m_presented, m_window->contentItem());
    m_canvas->setSize(QSizeF(options.width, options.height));
    QQuickItem* content = m_window->contentItem();
    QObject::connect(
        content, &QQuickItem::widthChanged, m_canvas,
        [canvas = m_canvas, content] { canvas->setWidth(content->width()); });
    QObject::connect(
        content, &QQuickItem::heightChanged, m_canvas,
        [canvas = m_canvas, content] { canvas->setHeight(content->height()); });
    // The reader closing the window is the end of it: pumping answers
    // false from then on.
    QObject::connect(m_window, &QWindow::visibleChanged, m_window,
                     [this](bool visible) {
                       if (!visible) m_open = false;
                     });
    m_window->show();
    m_window->raise();
    m_window->requestActivate();
  }

  ~SketchWindow() override { close(); }

  void show(std::unique_ptr<Host> host) override {
    std::unique_ptr<Host> leaving;
    {
      QMutexLocker lock(&m_presented->mutex);
      leaving = std::exchange(m_host, std::move(host));
      m_presented->host = m_host.get();
    }
    // Outside the lock: a host lets go of its session and waits on any
    // build it is in the middle of.
    leaving.reset();
    m_clock.restart();
    if (m_canvas) m_canvas->update();
  }

  Host* host() override { return m_host.get(); }

  bool pump(std::chrono::milliseconds patience) override {
    if (!m_open) return false;
    if (m_headless) {
      QMutexLocker lock(&m_presented->mutex);
      if (m_host) {
        m_host->poll();
        if (m_host->live()) (void)m_host->frame(m_clock.advance().count());
      }
      return true;
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents,
                                    (int)patience.count());
    return m_open;
  }

  bool still(const std::filesystem::path& path, float density) override {
    QMutexLocker lock(&m_presented->mutex);
    if (!m_host || !m_host->live()) return false;
    return m_host->capture(path, density);
  }

  void close() override {
    m_open = false;
    // The host goes first: its captures may be routed through the
    // renderer's context, which goes with the window.
    show(nullptr);
    if (m_window) {
      QPointer<QQuickWindow> window = m_window;
      m_window = nullptr;
      m_canvas = nullptr;
      if (window) {
        window->hide();
        delete window.data();
      }
    }
  }

  bool isOpen() const override { return m_open; }

 private:
  bool m_headless = false;
  bool m_open = true;
  std::shared_ptr<PresentedHost> m_presented;
  std::unique_ptr<Host> m_host;
  /** The headless window's clock; a window on screen steps its host on
   *  its renderer's. */
  motion::Engine m_clock;
  QQuickWindow* m_window = nullptr;
  WindowCanvas* m_canvas = nullptr;
};

}  // namespace

std::unique_ptr<Window> Window::open(const WindowOptions& options,
                                     std::string* why) {
  if (options.width < 1 || options.height < 1)
    return refused(why, "a window is at least one point wide and tall");
  if (options.headless) return std::make_unique<SketchWindow>(options);
  if (!onMainThread())
    return refused(why,
                   "a window is opened on the process's main thread, the "
                   "one that pumps it");
  try {
    std::string reason;
    if (!application(&reason))
      return refused(
          why, reason.empty() ? "the window system could not start" : reason);
    return std::make_unique<SketchWindow>(options);
  } catch (const std::exception& error) {
    return refused(why, error.what());
  }
}

}  // namespace sigil::sketch

extern "C" sigil::sketch::Window* sigil_sketch_window_open(
    const sigil::sketch::WindowOptions* options, std::string* why) {
  return sigil::sketch::Window::open(
             options ? *options : sigil::sketch::WindowOptions{}, why)
      .release();
}
