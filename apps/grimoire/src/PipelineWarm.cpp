/** @file
 * Standing a sketch's device programs up before its first frame: what
 * is declared so a program can be written down, what is replayed from
 * the last launch, what is built ahead of any draw when there is
 * nothing to replay, and the tally the run closes with.
 */

#include "PipelineWarm.h"

#include <gpu/graphite/Context.h>
#include <gpu/graphite/PrecompileContext.h>
#include <gpu/graphite/precompile/PaintOptions.h>
#include <gpu/graphite/precompile/Precompile.h>
#include <gpu/graphite/precompile/PrecompileBase.h>
#include <gpu/graphite/precompile/PrecompileColorFilter.h>
#include <gpu/graphite/precompile/PrecompileRuntimeEffect.h>
#include <gpu/graphite/precompile/PrecompileShader.h>
#include <include/core/SkBlendMode.h>
#include <include/core/SkColorType.h>
#include <include/core/SkData.h>
#include <include/core/SkSpan.h>
#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/advanced/Program.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Target.h>
#include <sigilmaterial/skia/Filter.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "PipelineStore.h"
#include "Startup.h"

namespace material = sigil::material;

namespace {

/** Whether each program is named on stderr as it is built or replayed.
 *  Off unless `--pipeline-names` asks, because it is one line per program
 *  and there are hundreds — and on, it is the only way to see WHICH
 *  program a first frame still had to build. Set before any context
 *  exists, read from the pool that builds the programs. */
std::atomic<bool> g_namesEveryPipeline{false};

bool namesEveryPipeline() {
  return g_namesEveryPipeline.load(std::memory_order_relaxed);
}

/** WHAT THIS RUN'S PROGRAMS WERE: one key per program built, which of
 *  them each sketch's draws asked for, and the counts the closing line
 *  reports.
 *
 *  Reached from the pool every context builds its programs on and from
 *  every thread that records a draw, at the same time, so everything
 *  here is under the one lock. Deduplicated by the hash the backend
 *  gives each program, because the same program is reported once per
 *  context that builds it and the file wants it once. */
class PipelineRecord final
    : public sigil::skia::GraphiteContext::PipelineReporter {
 public:
  void added(const std::string& label, std::uint32_t uniqueHash,
             bool fromPrecompile, sk_sp<SkData> key) override {
    const std::lock_guard<std::mutex> held(m_lock);
    if (fromPrecompile) {
      ++m_builtAhead;
    } else {
      ++m_builtForADraw;
      if (m_countingFirstFrame) ++m_builtInFirstFrame;
    }
    // A program built for a draw is one the sketch drawing wanted, there
    // and then. One built ahead is a guess until a draw finds it.
    if (!fromPrecompile) m_wantedBy[m_drawing].insert(uniqueHash);
    if (namesEveryPipeline())
      std::fprintf(stderr, "[grimoire] pipeline %s %08x%s: %s\n",
                   fromPrecompile ? "ahead" : "for a draw", uniqueHash,
                   key ? "" : " (no key)", label.c_str());
    if (!key || !m_seen.insert(uniqueHash).second) return;
    m_recorded.push_back({uniqueHash, {std::move(key), label}});
  }

  void found(const std::string&, std::uint32_t uniqueHash,
             bool fromPrecompile) override {
    const std::lock_guard<std::mutex> held(m_lock);
    ++m_found;
    if (fromPrecompile) ++m_foundStanding;
    // A draw asking for a program and getting one wanted it, however it
    // came to be standing — which is how a key stood up from the file
    // earns its place in the next one.
    m_wantedBy[m_drawing].insert(uniqueHash);
  }

  /** THE SKETCH WHOSE DRAWS COME NEXT, and the start of its first
   *  frame's count. */
  void drawing(std::string sketch) {
    const std::lock_guard<std::mutex> held(m_lock);
    m_drawing = std::move(sketch);
    m_countingFirstFrame = true;
    m_builtInFirstFrame = 0;
  }

  /** The end of the first frame's count: the sketch's name and how many
   *  programs its first frame's draws built, once; nothing after. */
  std::optional<std::pair<std::string, size_t>> firstFrameDrawn() {
    const std::lock_guard<std::mutex> held(m_lock);
    if (!m_countingFirstFrame) return std::nullopt;
    m_countingFirstFrame = false;
    return std::pair{m_drawing, m_builtInFirstFrame};
  }

  /** Every sketch some draw was made for, by the name it was drawn
   *  under. */
  std::vector<std::string> sketchesDrawn() const {
    const std::lock_guard<std::mutex> held(m_lock);
    std::vector<std::string> sketches;
    for (const auto& [sketch, wanted] : m_wantedBy)
      if (!sketch.empty() && !wanted.empty()) sketches.push_back(sketch);
    return sketches;
  }

  /** THE SET TO WRITE BACK FOR @p sketch: the programs its DRAWS
   *  wanted in this run, and only those.
   *
   *  A program stood up from the file is reported as built again, so a
   *  run that wrote back everything it recorded would write back its
   *  own replay: the set would then be every program every sketch ever
   *  opened on the machine needed. What one sketch's draws wanted is the
   *  scope that keeps the warm-up before it opens about the work in
   *  front of it, which is why no set is ever cut to a size. */
  std::vector<pipelines::RecordedPipeline> toWriteDown(
      const std::string& sketch) const {
    const std::lock_guard<std::mutex> held(m_lock);
    std::vector<pipelines::RecordedPipeline> writing;
    const auto wanted = m_wantedBy.find(sketch);
    if (wanted == m_wantedBy.end()) return writing;
    for (const Built& one : m_recorded)
      if (wanted->second.count(one.hash) != 0) writing.push_back(one.program);
    return writing;
  }

  void report(size_t replayed, size_t written) const {
    const std::lock_guard<std::mutex> held(m_lock);
    std::fprintf(stderr,
                 "[grimoire] pipelines: %zu built for a draw, %zu built "
                 "ahead of one, %zu found standing (%zu of them stood up "
                 "ahead), %zu replayed, %zu of %zu keys written down\n",
                 m_builtForADraw, m_builtAhead, m_found, m_foundStanding,
                 replayed, written, m_recorded.size());
  }

 private:
  /** One program, and the hash the wanted set is kept by — which is the
   *  backend's own, so the same program reported twice is one entry. */
  struct Built {
    std::uint32_t hash;
    pipelines::RecordedPipeline program;
  };

  mutable std::mutex m_lock;
  std::vector<Built> m_recorded;
  std::unordered_set<std::uint32_t> m_seen;
  /** What each sketch's draws wanted, by the name the host drew it
   *  under; the empty name holds what was drawn before any sketch. */
  std::map<std::string, std::unordered_set<std::uint32_t>> m_wantedBy;
  std::string m_drawing;
  bool m_countingFirstFrame = false;
  size_t m_builtInFirstFrame = 0;
  size_t m_builtForADraw = 0;
  size_t m_builtAhead = 0;
  size_t m_found = 0;
  size_t m_foundStanding = 0;
};

/** Leaked deliberately: every context this process builds is given a
 *  pointer to it and the last of them goes during static teardown, so a
 *  destructor here would be one a still-live context could reach. */
PipelineRecord* recorder() {
  static auto* one = new PipelineRecord;
  return one;
}

/** The effects declared, in the order they were declared — which is
 *  part of the name each one was given, so the same list names the file
 *  the keys are kept in. Written once before any context exists and
 *  only read afterwards. */
std::vector<sk_sp<SkRuntimeEffect>> g_declared;
/** How many of `g_declared` are the stock ones, ahead of the effects a
 *  past run wrote down. The sets are named by the stock ones alone: the
 *  recorded effects only ever grow at the end, so a key naming one keeps
 *  naming it as more are written down after it. */
size_t g_stockDeclared = 0;
/** The text of every effect offered for declaration, the ones past the
 *  backend's limit among them: what a run's own compiles are told apart
 *  from when they are written down. */
std::unordered_set<std::string> g_offered;
/** The ones a cold store stands up ahead of any draw, which is a much
 *  shorter list than the one above — see `buildStagesAhead`. */
std::vector<sk_sp<SkRuntimeEffect>> g_stages;
std::filesystem::path g_storeDirectory;

/** WHAT THE WARM-UPS SHARE, and the lock over it. A window stands more
 *  than one canvas up, each with a context and a warm-up of its own —
 *  the programs are a context's, so each has to build its own — and
 *  they run at the same time on their own workers. The files they read
 *  and write back are the store's, so only one of them is inside here at
 *  a time and the name the sets answer to is worked out once.
 *
 *  The flag is under the same lock as everything it gates, and for the
 *  same reason: it is set on the thread the process starts on and
 *  cleared there while a worker may still be inside, and a worker that
 *  reads it outside the lock could go on to record into a set already
 *  written. */
std::mutex g_warmupLock;
bool g_open = false;
/** The name every sketch's set answers to under this declaration and
 *  backend; empty until a context was warmed or recorded for. */
std::string g_setName;
size_t g_replayed = 0;
/** Whether this run may only FILL the store — see
 *  `recordPipelinesForAColdStore`. */
bool g_fillsAColdStoreOnly = false;

/** Every distinct compiled program under @p from, itself first and then
 *  the materials filling its slots, in recipe order. Its program is the
 *  one the warm-up already compiled and left in the cache — the same
 *  object the draw will use, which is the only kind a backend can be
 *  asked to name. */
void gatherPrograms(const material::Material& from,
                    std::vector<sk_sp<SkRuntimeEffect>>& into,
                    std::unordered_set<const SkRuntimeEffect*>& seen) {
  const std::shared_ptr<material::Program> compiled =
      material::program(from.recipePointer(), material::Target::SkSL);
  if (const auto* skia =
          compiled ? compiled->as<material::skia::SkiaProgram>() : nullptr) {
    const sk_sp<SkRuntimeEffect>& effect = skia->effect();
    if (effect && seen.insert(effect.get()).second) into.push_back(effect);
  }
  for (const auto& filled : from.slots())
    if (filled.second.material)
      gatherPrograms(*filled.second.material, into, seen);
}

/** Every program a key may name: the bodies the effects themselves are
 *  made of first, because a chain of those is what a first frame waits
 *  longest for, and then the stock recipes in catalogue order. The
 *  effects come first so that adding a recipe leaves their names where
 *  they were and moves only the recipes' — a smaller change to throw
 *  recorded keys away over. */
std::vector<sk_sp<SkRuntimeEffect>> everyDeclarableProgram() {
  std::vector<sk_sp<SkRuntimeEffect>> programs;
  std::unordered_set<const SkRuntimeEffect*> seen;
  for (const sk_sp<SkRuntimeEffect>& effect :
       material::skia::everyFilterProgram())
    if (effect && seen.insert(effect.get()).second) programs.push_back(effect);
  for (const material::Material& recipe : stockRecipes())
    gatherPrograms(recipe, programs, seen);
  return programs;
}

/** What @p effect is for, by what its text allows. */
pipelines::EffectKind kindOf(const SkRuntimeEffect& effect) {
  if (effect.allowShader()) return pipelines::EffectKind::Shader;
  if (effect.allowColorFilter()) return pipelines::EffectKind::ColorFilter;
  return pipelines::EffectKind::Blender;
}

/** @p recorded compiled again from its text, as the backend it was
 *  written down from compiled it; null where it no longer compiles. */
sk_sp<SkRuntimeEffect> compiled(const pipelines::RecordedEffect& recorded) {
  const SkString text(recorded.source.c_str());
  switch (recorded.kind) {
    case pipelines::EffectKind::Shader:
      return SkRuntimeEffect::MakeForShader(text).effect;
    case pipelines::EffectKind::ColorFilter:
      return SkRuntimeEffect::MakeForColorFilter(text).effect;
    case pipelines::EffectKind::Blender:
      return SkRuntimeEffect::MakeForBlender(text).effect;
  }
  return nullptr;
}

/** THE EFFECTS PAST RUNS WROTE DOWN, put after @p declarable in the order
 *  they were written, each one not already there. One that no longer
 *  compiles ends the list: every effect after it would be declared one
 *  place earlier than a recorded key names it. */
void appendRecordedEffects(std::vector<sk_sp<SkRuntimeEffect>>& declarable,
                           const std::filesystem::path& storeDirectory) {
  std::unordered_set<std::string> present;
  for (const sk_sp<SkRuntimeEffect>& effect : declarable)
    if (effect) present.insert(effect->source());
  for (const pipelines::RecordedEffect& recorded :
       pipelines::readEffects(pipelines::effectsFile(storeDirectory))) {
    if (!present.insert(recorded.source).second) continue;
    sk_sp<SkRuntimeEffect> effect = compiled(recorded);
    if (!effect) break;
    declarable.push_back(std::move(effect));
  }
}

/** WRITES DOWN THE RUNTIME EFFECTS THIS RUN COMPILED THAT NO DECLARATION
 *  NAMED — a body's own shaders, compiled as it set up — after the ones
 *  already written, so the next launch declares them before its contexts
 *  exist. Answers how many it added. */
size_t writeDownUndeclaredEffects(const std::filesystem::path& storeDirectory) {
  const std::filesystem::path file = pipelines::effectsFile(storeDirectory);
  std::vector<pipelines::RecordedEffect> effects = pipelines::readEffects(file);
  std::unordered_set<std::string> known = g_offered;
  for (const pipelines::RecordedEffect& recorded : effects)
    known.insert(recorded.source);
  size_t added = 0;
  for (const std::shared_ptr<material::Program>& program :
       material::ProgramCache::shared().programs(material::Target::SkSL)) {
    const auto* skia = program->as<material::skia::SkiaProgram>();
    const sk_sp<SkRuntimeEffect>& effect = skia ? skia->effect() : nullptr;
    if (!effect || !known.insert(effect->source()).second) continue;
    effects.push_back({kindOf(*effect), effect->source()});
    ++added;
  }
  if (added && !pipelines::writeEffects(file, effects)) {
    std::fprintf(stderr, "[grimoire] pipelines: could not write %s\n",
                 file.c_str());
    return 0;
  }
  return added;
}

/** The declared effects the sets are named by: the stock ones. */
std::span<const sk_sp<SkRuntimeEffect>> stockDeclared() {
  return std::span<const sk_sp<SkRuntimeEffect>>(g_declared)
      .first(g_stockDeclared);
}

/** WHAT THE PRESENTED CANVAS IS: the item's texture is eight bits a
 *  channel with no multisampling, and a draw into it is depth-ordered
 *  rather than stencilled. A program built for a pass other than the
 *  one it is drawn in is a program no draw finds. */
skgpu::graphite::RenderPassProperties presentedPass() {
  skgpu::graphite::RenderPassProperties pass;
  pass.fDSFlags = skgpu::graphite::DepthStencilFlags::kDepth;
  pass.fDstCT = kRGBA_8888_SkColorType;
  pass.fDstCS = nullptr;
  pass.fRequiresMSAA = false;
  return pass;
}

/** THE EFFECT STAGES, BUILT AHEAD OF ANY DRAW.
 *
 *  What a stage draws is the layer beneath it through one program: the
 *  layer arrives as an image and the effect maps it, so the paint is an
 *  image shader with the body as its colour filter.
 *
 *  ONLY A BODY THAT DECLARES NO CHILD, and this is a hard limit rather
 *  than a simplification. A described paint is expanded into EVERY
 *  combination it allows, and a child offered as an image is itself
 *  several of them, so a body with two children is hundreds of
 *  programs: the device is asked to hold hundreds it may never draw,
 *  the driver stops compiling once its compiled variants no longer fit,
 *  and the warm-up never returns — which is the stall it exists to
 *  remove, moved rather than removed. A childless body has exactly one
 *  combination per render step.
 *
 *  So this reaches a stage that maps the layer and nothing else, and it
 *  cannot reach a STACK: a backend inlines a whole chain into one
 *  program, and a described chain expands into every combination it
 *  allows. A stack's programs are the ones a sketch's own draws asked
 *  for, written down under the name the host knows the sketch by, and
 *  stood up from that set before the sketch next opens. */
size_t buildStagesAhead(skgpu::graphite::PrecompileContext& precompile,
                        std::span<const sk_sp<SkRuntimeEffect>> effects) {
  const skgpu::graphite::RenderPassProperties pass = presentedPass();
  const SkBlendMode blends[] = {SkBlendMode::kSrcOver};
  size_t stages = 0;
  for (const sk_sp<SkRuntimeEffect>& effect : effects) {
    if (!effect || !effect->children().empty()) continue;
    skgpu::graphite::PaintOptions stage;
    const sk_sp<skgpu::graphite::PrecompileShader> layer =
        skgpu::graphite::PrecompileShaders::Image(
            skgpu::graphite::PrecompileShaders::ImageShaderFlags::
                kExcludeCubic);
    if (effect->allowColorFilter()) {
      const sk_sp<skgpu::graphite::PrecompileColorFilter> map =
          skgpu::graphite::PrecompileRuntimeEffects::MakePrecompileColorFilter(
              effect);
      if (!map) continue;
      stage.setShaders(SkSpan(&layer, 1));
      stage.setColorFilters(SkSpan(&map, 1));
    } else if (effect->allowShader()) {
      const sk_sp<skgpu::graphite::PrecompileShader> shader =
          skgpu::graphite::PrecompileRuntimeEffects::MakePrecompileShader(
              effect);
      if (!shader) continue;
      stage.setShaders(SkSpan(&shader, 1));
    } else {
      continue;
    }
    stage.setBlendModes(SkSpan(blends, 1));
    skgpu::graphite::Precompile(&precompile, stage,
                                skgpu::graphite::DrawTypeFlags::kSimpleShape,
                                SkSpan(&pass, 1));
    ++stages;
  }
  return stages;
}

}  // namespace

std::string graphiteBackendName(const skgpu::graphite::Context& context) {
  switch (context.backend()) {
    case skgpu::BackendApi::kMetal:
      return "metal";
    case skgpu::BackendApi::kVulkan:
      return "vulkan";
    case skgpu::BackendApi::kDawn:
      return "dawn";
    default:
      return "other";
  }
}

void openPipelineWarmup(const std::filesystem::path& storeDirectory,
                        bool namesEveryProgram) {
  g_namesEveryPipeline.store(namesEveryProgram, std::memory_order_relaxed);
  const std::lock_guard<std::mutex> alone(g_warmupLock);
  g_storeDirectory = storeDirectory;
  std::vector<sk_sp<SkRuntimeEffect>> declarable = everyDeclarableProgram();
  const size_t stock = declarable.size();
  appendRecordedEffects(declarable, storeDirectory);
  g_offered.clear();
  for (const sk_sp<SkRuntimeEffect>& effect : declarable)
    if (effect) g_offered.insert(effect->source());
  g_stages.clear();
  for (const sk_sp<SkRuntimeEffect>& effect :
       material::skia::everyFilterProgram())
    if (effect) g_stages.push_back(effect);
  const size_t taken =
      sigil::skia::GraphiteContext::registerRuntimeEffects(declarable);
  // THE LIST THE KEYS ARE KEYED ON IS THE LIST THAT WAS DECLARED, not
  // the one that was offered: an effect past the backend's limit keeps
  // an unstable name, so no key can carry it and it says nothing about
  // which set a file holds.
  g_declared.assign(declarable.begin(), declarable.begin() + taken);
  g_stockDeclared = std::min(stock, taken);
  if (taken < declarable.size())
    std::fprintf(stderr,
                 "[grimoire] pipelines: %zu of %zu programs declared; the "
                 "rest keep names no recorded key can carry\n",
                 taken, declarable.size());
  sigil::skia::GraphiteContext::reportPipelinesTo(recorder());
  g_open = true;
}

void recordPipelinesForAColdStore(const skgpu::graphite::Context& context) {
  const std::string backend = graphiteBackendName(context);
  const std::lock_guard<std::mutex> alone(g_warmupLock);
  // A lane that already named the sets is a lane that replays from them,
  // and a run does one or the other.
  if (!g_open || !g_setName.empty()) return;
  g_setName = pipelines::keySetName(stockDeclared(), backend);
  g_fillsAColdStoreOnly = true;
}

void warmStockPipelines(
    std::unique_ptr<skgpu::graphite::PrecompileContext> precompile,
    std::string backend) {
  if (!precompile) return;
  const std::lock_guard<std::mutex> alone(g_warmupLock);
  if (!g_open) return;
  if (g_setName.empty())
    g_setName = pipelines::keySetName(stockDeclared(), backend);
  // A STORE THAT ANSWERS FOR THIS BUILD stands each sketch's programs up
  // before that sketch opens, and nothing here is a better guess.
  if (pipelines::holdsAnySketchSet(g_storeDirectory, g_setName)) return;
  // NOTHING WRITTEN DOWN: a fresh machine, an edited shader, a Skia that
  // no longer reads the last run's keys. The effect stages are what can
  // be known without having read a sketch.
  const size_t stages = buildStagesAhead(*precompile, g_stages);
  std::fprintf(stderr,
               "[grimoire] pipelines: no recorded set for this build, %zu "
               "effect stages stood up instead\n",
               stages);
}

bool hasRecordedPipelines(std::string_view sketch, const std::string& backend) {
  const std::lock_guard<std::mutex> alone(g_warmupLock);
  if (!g_open || sketch.empty()) return false;
  const std::string set = g_setName.empty()
                              ? pipelines::keySetName(stockDeclared(), backend)
                              : g_setName;
  std::error_code error;
  return std::filesystem::exists(
      pipelines::keySetFile(g_storeDirectory,
                            pipelines::sketchKeySetName(set, sketch)),
      error);
}

void warmSketchPipelines(
    std::unique_ptr<skgpu::graphite::PrecompileContext> precompile,
    std::string backend, std::string sketch) {
  if (!precompile) return;
  const std::lock_guard<std::mutex> alone(g_warmupLock);
  if (!g_open) return;
  if (g_setName.empty())
    g_setName = pipelines::keySetName(stockDeclared(), backend);
  const std::vector<pipelines::RecordedPipeline> recorded =
      pipelines::readKeySet(pipelines::keySetFile(
          g_storeDirectory, pipelines::sketchKeySetName(g_setName, sketch)));
  size_t replayed = 0;
  size_t stale = 0;
  for (const pipelines::RecordedPipeline& one : recorded) {
    if (!one.key) continue;
    // A KEY NAMES ITS PIECES BY NUMBER, and the numbers are the
    // backend's: a backend that numbers its pieces differently reads the
    // same key as another program. The description is read back first and
    // a key that no longer describes what it described is dropped rather
    // than built as something else.
    const std::string description = precompile->getPipelineLabel(one.key);
    if (namesEveryPipeline())
      std::fprintf(stderr, "[grimoire] pipeline replaying: %s\n",
                   description.c_str());
    if (description != one.description) {
      ++stale;
      continue;
    }
    if (precompile->precompile(one.key)) ++replayed;
  }
  g_replayed += replayed;
  // WHAT THE SET WAS WORTH, said whenever there was one. A set whose
  // every key has gone stale is the one state an operator has to be able
  // to see: a set silently dropped whole reads as a warm-up that helps
  // and costs nothing while being neither.
  if (!recorded.empty())
    std::fprintf(stderr,
                 "[grimoire] pipelines: %zu of %zu recorded programs stood "
                 "up before %s opened, %zu no longer described what they "
                 "described\n",
                 replayed, recorded.size(), sketch.c_str(), stale);
}

void pipelinesDrawnFor(std::string sketch) {
  recorder()->drawing(std::move(sketch));
}

void pipelinesFirstFrameDrawn() {
  const std::optional<std::pair<std::string, size_t>> counted =
      recorder()->firstFrameDrawn();
  if (!counted) return;
  std::fprintf(stderr,
               "[grimoire] pipelines: the first frame of %s built %zu "
               "programs for its draws\n",
               counted->first.c_str(), counted->second);
}

void finishPipelineWarmup() {
  const std::lock_guard<std::mutex> alone(g_warmupLock);
  if (!g_open) return;
  g_open = false;
  // No name means no context was ever warmed: this run drew through no
  // Graphite, nothing was recorded or replayed, and a tally of zeroes on
  // a lane the warm-up cannot serve says only that.
  if (g_setName.empty()) return;
  const size_t effects = writeDownUndeclaredEffects(g_storeDirectory);
  if (effects)
    std::fprintf(stderr,
                 "[grimoire] pipelines: %zu runtime effects no declaration "
                 "named written down for the next launch to declare\n",
                 effects);
  size_t written = 0;
  for (const std::string& sketch : recorder()->sketchesDrawn()) {
    const std::filesystem::path file = pipelines::keySetFile(
        g_storeDirectory, pipelines::sketchKeySetName(g_setName, sketch));
    // THE SWEEP FILLS A COLD STORE AND NEVER REPLACES A WARM ONE. What
    // it drew was a capture of the sketch rather than the sketch someone
    // is looking at, so its set is the better answer only where there is
    // no answer at all.
    if (g_fillsAColdStoreOnly && std::filesystem::exists(file)) continue;
    const std::vector<pipelines::RecordedPipeline> keys =
        recorder()->toWriteDown(sketch);
    if (keys.empty()) continue;
    if (pipelines::writeKeySet(file, keys))
      written += keys.size();
    else
      std::fprintf(stderr, "[grimoire] pipelines: could not write %s\n",
                   file.c_str());
  }
  recorder()->report(g_replayed, written);
}
