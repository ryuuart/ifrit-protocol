#pragma once

/** @file
 * @ingroup sketch-live
 *
 * The session domain's agent: the one running sketch a protocol host
 * holds — opened by registry name or path, pinned, photographed as a
 * plate is and read back — and the policy clock its frames are drawn at.
 */

#include <include/core/SkRefCnt.h>
#include <sigilmotion/clock/ClockPolicy.h>
#include <sigilprotocol/dispatch/Dispatcher.h>
#include <sigilprotocol/session/SessionAgent.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/live/Host.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace sigil::weave {
class FontContext;
}

namespace sigil::sketch {

/** WHERE THE SESSION AGENT FINDS WHAT IT OPENS: what every host it
 *  builds is told beside the file. Only `fonts` has no default. */
struct SessionAgentOptions {
  /** The font context every session shapes its text in; it outlives the
   *  agent. */
  weave::FontContext* fonts = nullptr;
  /** Where a registry entry's file stands, by its key. */
  std::filesystem::path sketchesDirectory;
  /** What mounts at `res://` for a registry entry; a file opened by path
   *  reads the assets beside itself. */
  std::filesystem::path assetsDirectory;
  /** The compiler line the build captured, for a file that must be
   *  compiled to open. */
  std::filesystem::path flagsFile;
  /** The importer for a Python file; null leaves Python files
   *  unavailable. */
  Kind (*pythonLoader)(const std::filesystem::path&) = nullptr;
};

/** THE ONE RUNNING SKETCH A PROTOCOL HOST HOLDS, answered over the
 *  session domain, and the policy clock its frames are drawn at, which
 *  the clock domain's agent drives.
 *
 *  A session is opened FOR ITS CLOCK: under any policy but the wall's it
 *  is opened as a repeatable run — what the sketch measured about itself
 *  pinned, the runtime's own re-baking held off — and draws no frame
 *  until a client steps, so its first frame is the client's first step,
 *  as a plate's is. A change between the wall's clock and any other
 *  opens it again at its own zero.
 *
 *  `PauseWhileLoading` HOLDS THE CLOCK THROUGH THE OPEN. A repeatable run
 *  has everything its setup asked for before the open is answered: a
 *  page's settle is driven through on the thread that opens it, and a
 *  file, a face or a fetched resource is read whole where it is asked
 *  for. So no frame the host draws afterwards finds anything still on
 *  its way, and those frames move by the wall.
 *
 *  A still under a moving clock is taken as a plate is: on a raster
 *  surface of the canvas times the density, cleared to the declared
 *  ground, through the runtime's own still — which draws one frame more
 *  where the runtime re-renders at the still's size, and the clock counts
 *  it. Every raster the session bakes is baked from its first frame at
 *  the density a plate of it is photographed at, `plateDensity()`, so a
 *  still at that density is the sweep's plate of the same moment; a still
 *  at another density bakes at it from then on, and a bake formed
 *  earlier is formed again only when its node describes again. Under a held clock it is the host's own still, the frame the clock
 *  holds as it was last drawn, so two stills under it are one picture.
 *  Either is written as a PNG under the state root.
 *
 *  What a client sets — the clock's policy, its hold and speed, the
 *  promotion pin — is keyed by the client, and goes when that client
 *  detaches: the next frame is the wall's again.
 *
 *  Everything runs on the thread the dispatcher is driven from; `frame()`
 *  is the host's loop, once a turn. */
class SessionAgent final : public protocol::session::SessionAgent {
 public:
  /** An agent answering on @p dispatcher, which outlives it, and
   *  building its hosts as @p options say. It fills the sessions open and
   *  the clock's policy into the dispatcher's program, and takes them
   *  back as it goes. */
  SessionAgent(protocol::Dispatcher& dispatcher, SessionAgentOptions options);
  ~SessionAgent() override;

  SessionAgent(const SessionAgent&) = delete;
  SessionAgent& operator=(const SessionAgent&) = delete;

  // --- the session domain -------------------------------------------------

  void open(const protocol::session::values::OpenParameters& parameters,
            protocol::Reply<protocol::session::values::Summary> reply) override;
  protocol::Answer<protocol::values::Empty> pinDevice(
      const protocol::session::values::DeviceParameters& parameters) override;
  protocol::Answer<protocol::values::Empty> pinPromotion(
      const protocol::session::values::PromotionParameters& parameters)
      override;
  void still(
      const protocol::session::values::StillParameters& parameters,
      protocol::Reply<protocol::session::values::StillResult> reply) override;
  void sequence(const protocol::session::values::SequenceParameters& parameters,
                protocol::Reply<protocol::session::values::SequenceResult>
                    reply) override;
  protocol::Answer<protocol::session::values::TimingResult> timing() override;
  protocol::Answer<protocol::session::values::MeasuredResult> measured()
      override;
  void profile(
      const protocol::session::values::ProfileParameters& parameters,
      protocol::Reply<protocol::session::values::ProfileResult> reply) override;
  void compositeCounts(
      protocol::Reply<protocol::session::values::CompositeCountsResult> reply)
      override;

  // --- the clock, as the clock domain's agent drives it -------------------

  /** The policy clock frames are drawn at. */
  [[nodiscard]] const motion::PolicyClock& clock() const { return m_clock; }

  /** Replaces the clock's policy and budget for the client asking, and
   *  opens the session again where the wall's clock gave way to another
   *  or took over from one. */
  void setPolicy(motion::ClockPolicy policy,
                 std::optional<double> budgetSeconds);

  /** Holds the clock or lets it go, for the client asking. */
  void setHeld(bool held);

  /** Sets the wall's clock seconds per wall second, for the client
   *  asking. */
  void setTimeScale(double scale);

  /** STEPS @p frames FRAMES of @p seconds each, drawing every one, and
   *  answers false with the reason in @p why where a frame could not be
   *  drawn or no session is open. */
  bool step(uint64_t frames, double seconds, std::string* why);

  /** Runs @p listener once for every budget that runs out, with the clock
   *  seconds it ran out at. */
  void onBudgetExpired(std::function<void(double seconds)> listener);

  // --- the host's loop ----------------------------------------------------

  /** ONE TURN OF THE HOST'S LOOP: the running host polled for an edit,
   *  an open that was waiting on a build answered, and — under the wall's
   *  clock or `PauseWhileLoading` — one frame drawn at the wall's
   *  delta. */
  void frame();

  /** Whether an open is still waiting on a build. */
  [[nodiscard]] bool opening() const { return m_pendingOpen.has_value(); }

  /** The sessions open, as `host.describe` lists them. */
  [[nodiscard]] std::vector<protocol::session::values::Summary> sessions()
      const;

  /** The path of the last still written; empty before the first. */
  [[nodiscard]] const std::filesystem::path& lastStill() const {
    return m_lastStill;
  }

 private:
  /** One open waiting on a build, and who asked for it. */
  struct PendingOpen {
    std::string sketch;
    std::string kind;  // the runtime asked for; empty for whichever
    protocol::Reply<protocol::session::values::Summary> reply;
  };
  /** A reply owed by the next frame drawn. */
  struct PendingProfile {
    uint32_t limit = 0;
    protocol::Reply<protocol::session::values::ProfileResult> reply;
  };

  /** The host for @p sketch, built for the clock as it stands; null with
   *  the reason in @p why where the name matches nothing. */
  std::unique_ptr<Host> build(const std::string& sketch, std::string* why);
  /** Where @p sketch is opened by is the one being opened again: the
   *  session goes and a new one opens at its own zero. */
  void reopen();
  /** Answers the waiting open where its host has built or failed. */
  void settleOpen();
  /** The pins a client set, put on the session now standing. */
  void applyPins();
  /** ONE FRAME of @p delta seconds drawn with the replies it owes: the
   *  next frame's profile and composite counts, and the budget. */
  bool drawFrame(double delta, std::string* why);
  /** Takes the counting and profiling a pending reply asked of the frame
   *  about to be drawn. */
  void armFrame();
  /** …and answers them from the frame just drawn. */
  void answerFrame();
  /** Answers every composite-count plane owed, off @p session's last
   *  counted frame, and stops the counting. */
  void answerCounts(Session& session);
  /** The still, written; nothing with the reason in @p why. */
  std::optional<protocol::session::values::StillResult> takeStill(
      double density, const std::string& path, std::string* why);
  /** Where a file a client named, or one the agent names, stands under
   *  the state root; empty with the reason where it would stand
   *  outside. */
  std::filesystem::path underStateRoot(const std::string& asked,
                                       const std::string& picked,
                                       std::string* why) const;
  /** Records @p owner as the client that set @p what, so its detaching
   *  takes it back. */
  void own(const std::string& what);
  /** Takes back what @p session alone set. */
  void release(const std::string& session);
  /** The summary of the session standing. */
  [[nodiscard]] protocol::session::values::Summary summary() const;
  /** Sends a session event, saying on the standard error where it could
   *  not be. */
  void opened();
  void closed();
  void failed(const std::string& sketch, const std::string& message);

  protocol::Dispatcher& m_dispatcher;
  SessionAgentOptions m_options;
  motion::PolicyClock m_clock;
  std::unique_ptr<Host> m_host;
  std::string m_sketch;  // what the standing session was opened by
  std::optional<PendingOpen> m_pendingOpen;
  std::optional<PendingProfile> m_pendingProfile;
  std::vector<protocol::Reply<protocol::session::values::CompositeCountsResult>>
      m_pendingCounts;
  std::optional<protocol::session::Promotion> m_promotion;
  /** Which client set each override: `policy`, `held`, `scale`,
   *  `promotion`. */
  std::map<std::string, std::string> m_owners;
  float m_bakeDensity = 0.0f;
  uint64_t m_stills = 0;
  uint64_t m_planes = 0;
  uint64_t m_sequences = 0;
  std::filesystem::path m_lastStill;
  std::vector<std::function<void(double)>> m_budgetListeners;
  /** What the dispatcher's detach listener reaches this agent through,
   *  gone with the agent. */
  std::shared_ptr<SessionAgent*> m_alive =
      std::make_shared<SessionAgent*>(this);
};

}  // namespace sigil::sketch
