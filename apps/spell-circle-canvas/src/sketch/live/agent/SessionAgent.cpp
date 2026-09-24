/** @file
 * The one running sketch a protocol host holds: opened for its clock,
 * stepped, photographed as a plate is, and read back.
 */

#include "sigilsketch/live/agent/SessionAgent.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkData.h>
#include <include/core/SkSurface.h>
#include <sigilimage/encode/Encode.h>
#include <sigilio/source/Sink.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilsketch/core/Crash.h>
#include <sigilsketch/core/Session.h>
#include <sigilsketch/core/Sources.h>
#include <sigilsketch/live/agent/ClockAgent.h>
#include <sigilsketch/plate/Sweep.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <system_error>
#include <utility>

namespace sigil::sketch {

namespace {

namespace values = protocol::session::values;
using protocol::Answer;
using protocol::ErrorCode_failed;
using protocol::refusal;

/** The widest still a raster surface is asked for, in either direction. */
constexpr int kLargestStill = 16384;

Session::Promotion promotionOf(protocol::session::Promotion promotion) {
  switch (promotion) {
    case protocol::session::Promotion_Off:
      return Session::Promotion::Off;
    case protocol::session::Promotion_Eager:
      return Session::Promotion::Eager;
    default:
      return Session::Promotion::ByCost;
  }
}

/** The file a path the client named opens by: the file itself, or the
 *  entry of a sketch that is a directory. Empty where it names neither. */
std::filesystem::path entryAt(const std::filesystem::path& asked) {
  std::error_code error;
  if (std::filesystem::is_regular_file(asked, error))
    return std::filesystem::absolute(asked).lexically_normal();
  if (!std::filesystem::is_directory(asked, error)) return {};
  const std::filesystem::path directory =
      std::filesystem::absolute(asked).lexically_normal();
  const std::filesystem::path trimmed =
      directory.filename().empty() ? directory.parent_path() : directory;
  const std::filesystem::path entry =
      sourceOf(trimmed.parent_path(), trimmed.filename().string());
  return std::filesystem::is_regular_file(entry, error)
             ? entry
             : std::filesystem::path{};
}

/** Whether @p path stands inside @p root, once both are made whole. */
bool within(const std::filesystem::path& path,
            const std::filesystem::path& root) {
  const std::filesystem::path relative =
      std::filesystem::weakly_canonical(path).lexically_relative(
          std::filesystem::weakly_canonical(root));
  return !relative.empty() && *relative.begin() != "..";
}

/** @p pixels as a PNG at @p path, the directories above it made. */
bool writePng(const SkPixmap& pixels, const std::filesystem::path& path) {
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  const sk_sp<SkData> png = image::encodeImage(pixels, image::Format::Png);
  return png && io::writeBytes(path, png->data(), png->size());
}

/** `<stem>-0007<extension>`: the name of the seventh of its kind. */
std::string numbered(uint64_t number, const char* stem, const char* extension) {
  char name[96];
  std::snprintf(name, sizeof name, "%s-%04llu%s", stem,
                (unsigned long long)number, extension);
  return name;
}

}  // namespace

SessionAgent::SessionAgent(protocol::Dispatcher& dispatcher,
                           SessionAgentOptions options)
    : m_dispatcher(dispatcher), m_options(std::move(options)) {
  m_dispatcher.program().clockPolicy = [this] {
    return policyOf(m_clock.policy());
  };
  m_dispatcher.program().sessions = [this] { return sessions(); };
  // A client's overrides go with it. The listener stands as long as the
  // dispatcher does, so it reaches this agent only while the agent does.
  m_dispatcher.onDetach([alive = std::weak_ptr<SessionAgent*>(m_alive)](
                            const std::string& session) {
    if (const std::shared_ptr<SessionAgent*> agent = alive.lock())
      (*agent)->release(session);
  });
}

SessionAgent::~SessionAgent() {
  m_dispatcher.program().clockPolicy = nullptr;
  m_dispatcher.program().sessions = nullptr;
  m_alive.reset();
}

// --- opening ---------------------------------------------------------------

std::unique_ptr<Host> SessionAgent::build(const std::string& sketch,
                                          std::string* why) {
  if (!m_options.fonts) {
    *why = "this host lends no fonts, so no session can open";
    return nullptr;
  }
  Host::Options options;
  options.pythonLoader = m_options.pythonLoader;
  options.sketchesDirectory = m_options.sketchesDirectory;
  options.flagsFile = m_options.flagsFile;
  options.clock = m_clock.policy();
  if (const std::filesystem::path file = entryAt(sketch); !file.empty()) {
    options.sketchPath = file;
  } else {
    const int index = find(sketch);
    if (index < 0) {
      *why = "no registry entry or file answers to \"" + sketch + "\"";
      return nullptr;
    }
    const Entry& entry = registry()[(size_t)index];
    std::string unavailable;
    if (!entry.available(&unavailable)) {
      *why = std::string(entry.name) + " cannot be drawn here: " + unavailable;
      return nullptr;
    }
    options.compiledIn = &entry;
    options.sketchPath = sourceOf(m_options.sketchesDirectory, entry.key);
    options.assetsDirectory = m_options.assetsDirectory;
  }
  return std::make_unique<Host>(std::move(options), *m_options.fonts);
}

void SessionAgent::open(const values::OpenParameters& parameters,
                        protocol::Reply<values::Summary> reply) {
  if (m_pendingOpen && m_pendingOpen->reply)
    m_pendingOpen->reply(
        refusal(ErrorCode_failed, "session.open: " + m_pendingOpen->sketch +
                                      " was replaced by another open"));
  m_pendingOpen.reset();
  if (m_host) closed();
  m_host.reset();
  m_sketch.clear();
  std::string why;
  std::unique_ptr<Host> host = build(parameters.sketch, &why);
  if (!host) {
    failed(parameters.sketch, why);
    reply(refusal(ErrorCode_failed, "session.open: " + why));
    return;
  }
  m_host = std::move(host);
  m_sketch = parameters.sketch;
  m_bakeDensity = 0.0f;
  m_pendingOpen =
      PendingOpen{parameters.sketch, parameters.kind, std::move(reply)};
  settleOpen();
}

void SessionAgent::reopen() {
  if (m_sketch.empty()) return;
  const std::string sketch = m_sketch;
  std::optional<PendingOpen> waiting = std::move(m_pendingOpen);
  m_pendingOpen.reset();
  closed();
  m_host.reset();
  std::string why;
  m_host = build(sketch, &why);
  m_bakeDensity = 0.0f;
  if (!m_host) {
    m_sketch.clear();
    failed(sketch, why);
    if (waiting && waiting->reply)
      waiting->reply(refusal(ErrorCode_failed, "session.open: " + why));
    return;
  }
  m_pendingOpen =
      waiting ? std::move(*waiting) : PendingOpen{sketch, {}, nullptr};
  settleOpen();
}

void SessionAgent::settleOpen() {
  if (!m_pendingOpen || !m_host) return;
  if (!m_host->live()) {
    if (m_host->compiling() || m_host->errorLog().empty()) return;
    const std::string message = m_host->errorLog();
    PendingOpen pending = std::move(*m_pendingOpen);
    m_pendingOpen.reset();
    m_host.reset();
    m_sketch.clear();
    failed(pending.sketch, message);
    if (pending.reply)
      pending.reply(refusal(ErrorCode_failed, "session.open: " + message));
    return;
  }
  PendingOpen pending = std::move(*m_pendingOpen);
  m_pendingOpen.reset();
  if (!pending.kind.empty() && m_host->kind() != pending.kind) {
    const std::string why = pending.sketch + " draws through " +
                            std::string(m_host->kind()) + ", not " +
                            pending.kind;
    m_host.reset();
    m_sketch.clear();
    failed(pending.sketch, why);
    if (pending.reply)
      pending.reply(refusal(ErrorCode_failed, "session.open: " + why));
    return;
  }
  m_clock.restart();
  // Before the first frame: a bake formed at another density is formed
  // again only when its node describes again, so a density declared
  // later would leave the frames already drawn on another grid.
  applyPins();
  // Under the wall's clock a session is seen from its first frame; under
  // any other its first frame is the client's first step.
  if (m_clock.wall()) {
    std::string why;
    if (!drawFrame(m_clock.frame(), &why)) {
      m_host.reset();
      m_sketch.clear();
      failed(pending.sketch, why);
      if (pending.reply)
        pending.reply(refusal(ErrorCode_failed, "session.open: " + why));
      return;
    }
  }
  opened();
  if (pending.reply) pending.reply(summary());
}

void SessionAgent::applyPins() {
  Session* session = m_host ? m_host->session() : nullptr;
  if (!session) return;
  if (m_promotion) session->setAutoPromotion(promotionOf(*m_promotion));
  if (m_density) {
    m_bakeDensity =
        *m_density > 0 ? (float)*m_density : plateDensity(*session);
    session->setBakeDensity(m_bakeDensity);
  }
}

// --- pins ------------------------------------------------------------------

Answer<protocol::values::Empty> SessionAgent::pinDevice(
    const values::DeviceParameters& parameters) {
  if (parameters.device != protocol::session::Device_Cpu)
    return refusal(ErrorCode_failed,
                   "session.pinDevice: this host draws a session and its "
                   "stills on the CPU alone");
  return protocol::values::Empty{};
}

Answer<protocol::values::Empty> SessionAgent::pinPromotion(
    const values::PromotionParameters& parameters) {
  m_promotion = parameters.promotion;
  own("promotion");
  applyPins();
  return protocol::values::Empty{};
}

Answer<protocol::values::Empty> SessionAgent::pinDensity(
    const values::DensityParameters& parameters) {
  if (!std::isfinite(parameters.density) || parameters.density < 0)
    return refusal(ErrorCode_failed,
                   "session.pinDensity: a density is a finite number of "
                   "pixels per canvas unit, zero for a plate's");
  m_density = parameters.density;
  own("density");
  applyPins();
  return protocol::values::Empty{};
}

// --- frames ----------------------------------------------------------------

void SessionAgent::armFrame() {
  if (m_pendingProfile)
    if (Session* session = m_host->session()) session->setProfiling(true);
}

void SessionAgent::answerFrame() {
  if (!m_pendingProfile) return;
  Session* session = m_host ? m_host->session() : nullptr;
  PendingProfile pending = std::move(*m_pendingProfile);
  m_pendingProfile.reset();
  if (!session) {
    pending.reply(refusal(ErrorCode_failed,
                          "session.profile: the session closed before its "
                          "next frame"));
    return;
  }
  values::ProfileResult result;
  result.rows = session->costs(pending.limit);
  session->setProfiling(false);
  pending.reply(std::move(result));
}

bool SessionAgent::drawFrame(double delta, std::string* why) {
  armFrame();
  const bool drawn = m_host->frame(delta);
  answerFrame();
  if (!drawn) {
    *why = m_host->errorLog().empty() ? "the frame could not be drawn"
                                      : m_host->errorLog();
    return false;
  }
  if (m_clock.budgetExpired())
    for (const auto& listener : m_budgetListeners) listener(m_clock.elapsed());
  return true;
}

bool SessionAgent::step(uint64_t frames, double seconds, std::string* why) {
  if (!m_host || m_pendingOpen || !m_host->live()) {
    *why =
        m_pendingOpen ? "the session is still opening" : "no session is open";
    return false;
  }
  for (uint64_t frame = 0; frame < frames; ++frame)
    if (!drawFrame(m_clock.step(seconds), why)) return false;
  return true;
}

void SessionAgent::frame() {
  if (m_host) m_host->poll();
  if (m_pendingOpen) {
    settleOpen();
    return;
  }
  if (!m_host || !m_host->live()) return;
  const motion::ClockPolicy policy = m_clock.policy();
  if (policy != motion::ClockPolicy::Wall &&
      policy != motion::ClockPolicy::PauseWhileLoading)
    return;
  // Nothing is still arriving by now: the open drove every load its setup
  // asked for through before it was answered.
  std::string why;
  if (!drawFrame(m_clock.frame(), &why)) failed(m_sketch, why);
}

// --- stills ----------------------------------------------------------------

std::filesystem::path SessionAgent::underStateRoot(const std::string& asked,
                                                   const std::string& picked,
                                                   std::string* why) const {
  const std::filesystem::path& root = m_dispatcher.program().stateRoot;
  if (root.empty()) {
    *why = "this host names no state root to write under";
    return {};
  }
  const std::filesystem::path path =
      asked.empty() ? root / picked
                    : (std::filesystem::path(asked).is_absolute()
                           ? std::filesystem::path(asked)
                           : root / asked);
  if (!within(path, root)) {
    *why = path.string() + " stands outside the state root " + root.string();
    return {};
  }
  return path;
}

void SessionAgent::answerCounts(Session& session) {
  if (m_pendingCounts.empty()) return;
  const Session::CompositeCounts plane = session.compositeCounts();
  session.setCompositeCounting(false);
  std::vector<protocol::Reply<values::CompositeCountsResult>> owed =
      std::move(m_pendingCounts);
  m_pendingCounts.clear();
  std::string why;
  const std::filesystem::path file =
      underStateRoot({}, numbered(++m_planes, "counts/counts", ".png"), &why);
  SkBitmap grey;
  bool written = false;
  if (!file.empty() && plane.width > 0 && plane.height > 0 &&
      grey.tryAllocPixels(SkImageInfo::Make(plane.width, plane.height,
                                            kGray_8_SkColorType,
                                            kOpaque_SkAlphaType))) {
    for (int y = 0; y < plane.height; ++y)
      std::memcpy(grey.getAddr8(0, y),
                  plane.counts.data() + (size_t)y * (size_t)plane.width,
                  (size_t)plane.width);
    written = writePng(grey.pixmap(), file);
  }
  values::CompositeCountsResult counts;
  if (written) {
    counts.path = file.string();
    counts.width = (uint32_t)plane.width;
    counts.height = (uint32_t)plane.height;
    counts.maximum =
        *std::max_element(plane.counts.begin(), plane.counts.end());
  }
  for (auto& reply : owed) {
    if (written) {
      reply(counts);
      continue;
    }
    reply(refusal(
        ErrorCode_failed,
        "session.compositeCounts: " +
            (why.empty() ? std::string("the runtime counted no plane") : why)));
  }
}

std::optional<values::StillResult> SessionAgent::takeStill(
    double density, const std::string& path, std::string* why) {
  Session* session = m_host && !m_pendingOpen ? m_host->session() : nullptr;
  if (!session) {
    *why =
        m_pendingOpen ? "the session is still opening" : "no session is open";
    return std::nullopt;
  }
  if (!std::isfinite(density) || density <= 0) {
    *why = "a density is a finite number of pixels per canvas unit above zero";
    return std::nullopt;
  }
  const std::string name = numbered(++m_stills, "stills/still", ".png");
  const std::filesystem::path file = underStateRoot(path, name, why);
  if (file.empty()) return std::nullopt;

  const bool counting = !m_pendingCounts.empty();
  if (counting) session->setCompositeCounting(true);
  SkBitmap bitmap;
  if (m_clock.still()) {
    // A HELD CLOCK photographs the frame it holds as it was last drawn,
    // drawing nothing new — the host's own still, which is also what a
    // written capture has always been.
    bitmap = m_host->still((float)density);
    if (bitmap.isNull()) {
      *why = m_host->errorLog().empty() ? "the still could not be drawn"
                                        : m_host->errorLog();
      return std::nullopt;
    }
  } else {
    // A MOVING CLOCK photographs as a plate is taken: the runtime's own
    // still on a raster surface of the canvas times the density, cleared
    // to the declared ground, with every raster the session bakes taken
    // at that density — one frame more where the runtime re-renders its
    // still at this size, which the clock counts.
    const CanvasSpecification& specification = session->canvas();
    const double width = std::ceil(specification.size.width() * density);
    const double height = std::ceil(specification.size.height() * density);
    if (!(width >= 1) || !(height >= 1) || width > kLargestStill ||
        height > kLargestStill) {
      *why = "a still of this canvas at that density has no pixels to hold";
      return std::nullopt;
    }
    const SkImageInfo info =
        SkImageInfo::MakeN32Premul((int)width, (int)height);
    sk_sp<SkSurface> surface = SkSurfaces::Raster(info);
    if (!surface) {
      *why = "no raster surface could be made for the still";
      return std::nullopt;
    }
    SkCanvas& canvas = *surface->getCanvas();
    canvas.clear(material::skia::toSkColor(specification.background));
    canvas.scale((float)density, (float)density);
    if (m_bakeDensity != (float)density) {
      session->setBakeDensity((float)density);
      m_bakeDensity = (float)density;
    }
    armFrame();
    try {
      PhaseMark mark(Phase::Capture);
      session->still(canvas);
    } catch (const std::exception& error) {
      answerFrame();
      *why = error.what();
      return std::nullopt;
    }
    answerFrame();
    (void)m_clock.step(session->stillStep());
    bitmap.allocPixels(info);
    if (!surface->readPixels(bitmap.pixmap(), 0, 0)) {
      *why = "the still could not be read back";
      return std::nullopt;
    }
  }
  if (counting) answerCounts(*session);
  if (!writePng(bitmap.pixmap(), file)) {
    *why = "the still could not be written to " + file.string();
    return std::nullopt;
  }
  m_lastStill = file;
  values::StillResult result;
  result.path = file.string();
  result.width = (uint32_t)bitmap.width();
  result.height = (uint32_t)bitmap.height();
  result.seconds = m_clock.elapsed();
  return result;
}

void SessionAgent::still(const values::StillParameters& parameters,
                         protocol::Reply<values::StillResult> reply) {
  std::string why;
  std::optional<values::StillResult> result =
      takeStill(parameters.density, parameters.path, &why);
  if (!result) {
    reply(refusal(ErrorCode_failed, "session.still: " + why));
    return;
  }
  reply(std::move(*result));
}

void SessionAgent::sequence(const values::SequenceParameters& parameters,
                            protocol::Reply<values::SequenceResult> reply) {
  if (m_clock.policy() != motion::ClockPolicy::Advance) {
    reply(refusal(ErrorCode_failed,
                  "session.sequence: frames are stepped between stills only "
                  "under the Advance policy"));
    return;
  }
  if (!std::isfinite(parameters.rate) || parameters.rate < 4.0) {
    reply(refusal(ErrorCode_failed,
                  "session.sequence: a rate under four frames a second steps "
                  "longer than one frame of a clock moves"));
    return;
  }
  std::string why;
  const std::string directory =
      parameters.directory.empty()
          ? numbered(++m_sequences, "sequences/sequence", "")
          : parameters.directory;
  values::SequenceResult result;
  for (uint32_t frame = 0; frame < parameters.frames; ++frame) {
    const std::string name = numbered(frame + 1, "frame", ".png");
    const std::string path = (std::filesystem::path(directory) / name).string();
    std::optional<values::StillResult> still =
        takeStill(parameters.density, path, &why);
    if (!still) {
      reply(refusal(ErrorCode_failed, "session.sequence: " + why));
      return;
    }
    result.paths.push_back(still->path);
    result.width = still->width;
    result.height = still->height;
    if (frame + 1 < parameters.frames &&
        !step(1, 1.0 / parameters.rate, &why)) {
      reply(refusal(ErrorCode_failed, "session.sequence: " + why));
      return;
    }
  }
  reply(std::move(result));
}

// --- readings --------------------------------------------------------------

Answer<values::TimingResult> SessionAgent::timing() {
  Session* session = m_host ? m_host->session() : nullptr;
  if (!session)
    return refusal(ErrorCode_failed, "session.timing: no session is open");
  const Timing timing = session->timing();
  values::TimingResult result;
  result.total_milliseconds = timing.totalMs;
  result.update_milliseconds = timing.updateMs;
  result.draw_milliseconds = timing.drawMs;
  for (const LaneCost& lane : session->lanes()) {
    values::LaneCost cost;
    cost.name = lane.name;
    cost.milliseconds = lane.ms;
    result.lanes.push_back(std::move(cost));
  }
  return result;
}

Answer<values::MeasuredResult> SessionAgent::measured() {
  return refusal(ErrorCode_failed,
                 "session.measured: no runtime records what a sketch "
                 "measured about itself, since a measured value carries no "
                 "name to answer it by");
}

void SessionAgent::profile(const values::ProfileParameters& parameters,
                           protocol::Reply<values::ProfileResult> reply) {
  if (!m_host || !m_host->session()) {
    reply(refusal(ErrorCode_failed, "session.profile: no session is open"));
    return;
  }
  if (m_pendingProfile)
    m_pendingProfile->reply(refusal(
        ErrorCode_failed, "session.profile: replaced by another profile"));
  m_pendingProfile = PendingProfile{parameters.limit, std::move(reply)};
}

void SessionAgent::compositeCounts(
    protocol::Reply<values::CompositeCountsResult> reply) {
  if (!m_host || !m_host->session()) {
    reply(refusal(ErrorCode_failed,
                  "session.compositeCounts: no session is open"));
    return;
  }
  m_pendingCounts.push_back(std::move(reply));
}

// --- the clock -------------------------------------------------------------

void SessionAgent::setPolicy(motion::ClockPolicy policy,
                             std::optional<double> budgetSeconds) {
  const bool wasWall = m_clock.wall();
  m_clock.setPolicy(policy, budgetSeconds);
  own("policy");
  if (wasWall != m_clock.wall()) reopen();
}

void SessionAgent::setHeld(bool held) {
  m_clock.setHeld(held);
  own("held");
}

void SessionAgent::setTimeScale(double scale) {
  m_clock.setTimeScale(scale);
  own("scale");
}

void SessionAgent::onBudgetExpired(std::function<void(double)> listener) {
  m_budgetListeners.push_back(std::move(listener));
}

// --- clients ---------------------------------------------------------------

void SessionAgent::own(const std::string& what) {
  m_owners[what] = m_dispatcher.asking();
}

void SessionAgent::release(const std::string& session) {
  for (auto owner = m_owners.begin(); owner != m_owners.end();) {
    if (owner->second != session) {
      ++owner;
      continue;
    }
    const std::string what = owner->first;
    owner = m_owners.erase(owner);
    if (what == "held") {
      m_clock.setHeld(false);
    } else if (what == "scale") {
      m_clock.setTimeScale(1.0);
    } else if (what == "density") {
      m_density.reset();
      // No pin is the runtime's own density again, which only a session
      // opened anew holds from its first frame.
      if (m_host && m_host->session()) reopen();
    } else if (what == "promotion") {
      m_promotion.reset();
      // No pin is the session's own default again, which a session opened
      // anew holds from its first frame.
      if (m_host && m_host->session()) reopen();
    } else if (what == "policy") {
      const bool wasWall = m_clock.wall();
      m_clock.setPolicy(motion::ClockPolicy::Wall);
      if (!wasWall) reopen();
    }
  }
}

std::vector<values::Summary> SessionAgent::sessions() const {
  if (!m_host || !m_host->live() || m_pendingOpen) return {};
  return {summary()};
}

values::Summary SessionAgent::summary() const {
  values::Summary result;
  result.sketch = m_sketch;
  if (!m_host) return result;
  result.kind = std::string(m_host->kind());
  const SkSize size = m_host->canvasSize();
  result.width = size.width();
  result.height = size.height();
  result.moment = m_host->captureSeconds();
  return result;
}

void SessionAgent::opened() {
  values::OpenedEvent event;
  event.summary = summary();
  if (!protocol::session::SessionEvents(m_dispatcher.events()).opened(event))
    std::fprintf(stderr, "session.opened could not be sent for %s\n",
                 m_sketch.c_str());
}

void SessionAgent::closed() {
  values::ClosedEvent event;
  event.sketch = m_sketch;
  if (!protocol::session::SessionEvents(m_dispatcher.events()).closed(event))
    std::fprintf(stderr, "session.closed could not be sent for %s\n",
                 m_sketch.c_str());
}

void SessionAgent::failed(const std::string& sketch,
                          const std::string& message) {
  values::FailedEvent event;
  event.sketch = sketch;
  event.message = message;
  if (!protocol::session::SessionEvents(m_dispatcher.events()).failed(event))
    std::fprintf(stderr, "session.failed could not be sent for %s\n",
                 sketch.c_str());
}

}  // namespace sigil::sketch
