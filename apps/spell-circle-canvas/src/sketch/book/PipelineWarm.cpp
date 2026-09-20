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
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Program.h>
#include <sigilmaterial/core/Target.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
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
 *  Off unless the environment asks, because it is one line per program
 *  and there are hundreds — and on, it is the only way to see WHICH
 *  program a first frame still had to build. */
bool namesEveryPipeline() {
  static const bool asked = [] {
    const char* set = std::getenv("SIGIL_SKETCHBOOK_PIPELINE_NAMES");
    return set && *set;
  }();
  return asked;
}

/** WHAT THIS RUN'S PROGRAMS WERE: one key per program built, and the
 *  counts the closing line reports.
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
    if (fromPrecompile)
      ++m_builtAhead;
    else
      ++m_builtForADraw;
    if (namesEveryPipeline())
      std::fprintf(stderr, "[sketchbook] pipeline %s %08x%s: %s\n",
                   fromPrecompile ? "ahead" : "for a draw", uniqueHash,
                   key ? "" : " (no key)", label.c_str());
    if (!key || !m_seen.insert(uniqueHash).second) return;
    m_recorded.push_back({std::move(key), label});
  }

  void found(const std::string&, std::uint32_t, bool fromPrecompile) override {
    const std::lock_guard<std::mutex> held(m_lock);
    ++m_found;
    if (fromPrecompile) ++m_foundStanding;
  }

  std::vector<pipelines::RecordedPipeline> recorded() const {
    const std::lock_guard<std::mutex> held(m_lock);
    return m_recorded;
  }

  void report(size_t replayed, size_t recorded) const {
    const std::lock_guard<std::mutex> held(m_lock);
    std::fprintf(stderr,
                 "[sketchbook] pipelines: %zu built for a draw, %zu built "
                 "ahead of one, %zu found standing (%zu of them stood up "
                 "ahead), %zu replayed, %zu keys recorded\n",
                 m_builtForADraw, m_builtAhead, m_found, m_foundStanding,
                 replayed, recorded);
  }

 private:
  mutable std::mutex m_lock;
  std::vector<pipelines::RecordedPipeline> m_recorded;
  std::unordered_set<std::uint32_t> m_seen;
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
/** The ones a cold store stands up ahead of any draw, which is a much
 *  shorter list than the one above — see `buildStagesAhead`. */
std::vector<sk_sp<SkRuntimeEffect>> g_stages;
std::filesystem::path g_storeDirectory;
bool g_open = false;

/** WHAT THE WARM-UPS SHARE, and the lock over it. A window stands more
 *  than one canvas up, each with a context and a warm-up of its own —
 *  the programs are a context's, so each has to build its own — and
 *  they run at the same time on their own workers. The file they read
 *  and write back is one file, so only one of them is inside here at a
 *  time and the name is worked out once. */
std::mutex g_warmupLock;
std::filesystem::path g_keySetFile;
size_t g_replayed = 0;

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
       material::skia::everyEffectProgram())
    if (effect && seen.insert(effect.get()).second) programs.push_back(effect);
  for (const material::Material& recipe : stockRecipes())
    gatherPrograms(recipe, programs, seen);
  return programs;
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
 *  program, and which chains a sketch will wear is not knowable before
 *  the sketch has been read. Those are built when the draw that needs
 *  them is first recorded, and recorded, so the launch after this one
 *  replays them exactly. */
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

void openPipelineWarmup(const std::filesystem::path& storeDirectory) {
  g_storeDirectory = storeDirectory;
  const std::vector<sk_sp<SkRuntimeEffect>> declarable =
      everyDeclarableProgram();
  g_stages.clear();
  for (const sk_sp<SkRuntimeEffect>& effect :
       material::skia::everyEffectProgram())
    if (effect) g_stages.push_back(effect);
  const size_t taken =
      sigil::skia::GraphiteContext::registerRuntimeEffects(declarable);
  // THE LIST THE KEYS ARE KEYED ON IS THE LIST THAT WAS DECLARED, not
  // the one that was offered: an effect past the backend's limit keeps
  // an unstable name, so no key can carry it and it says nothing about
  // which set a file holds.
  g_declared.assign(declarable.begin(), declarable.begin() + taken);
  if (taken < declarable.size())
    std::fprintf(stderr,
                 "[sketchbook] pipelines: %zu of %zu programs declared; the "
                 "rest keep names no recorded key can carry\n",
                 taken, declarable.size());
  sigil::skia::GraphiteContext::reportPipelinesTo(recorder());
  g_open = true;
}

void warmStockPipelines(
    std::unique_ptr<skgpu::graphite::PrecompileContext> precompile,
    std::string backend) {
  if (!g_open || !precompile) return;
  const std::lock_guard<std::mutex> alone(g_warmupLock);
  if (g_keySetFile.empty())
    g_keySetFile = pipelines::keySetFile(
        g_storeDirectory, pipelines::keySetName(g_declared, backend));
  const std::vector<pipelines::RecordedPipeline> recorded =
      pipelines::readKeySet(g_keySetFile);
  size_t replayed = 0;
  size_t stale = 0;
  for (const pipelines::RecordedPipeline& one : recorded) {
    if (!one.key) continue;
    // workaround: a key naming a piece this run cannot put a name to
    // walks a null string inside the backend's shader generator rather
    // than being refused, so the description is read back first and a
    // key that no longer describes what it described is dropped. The
    // backend's own pieces are made on first use, so a key recorded
    // after a draw that made one can be read by a run where nothing has
    // yet — which reads back with the name missing.
    const std::string description = precompile->getPipelineLabel(one.key);
    if (namesEveryPipeline())
      std::fprintf(stderr, "[sketchbook] pipeline replaying: %s\n",
                   description.c_str());
    if (description != one.description) {
      ++stale;
      continue;
    }
    if (precompile->precompile(one.key)) ++replayed;
  }
  g_replayed += replayed;
  if (replayed > 0) {
    std::fprintf(stderr,
                 "[sketchbook] pipelines: %zu of %zu recorded programs stood "
                 "up before the first frame, %zu no longer described what "
                 "they described\n",
                 replayed, recorded.size(), stale);
    return;
  }
  // NOTHING TO REPLAY: a fresh machine, an edited shader, a Skia that
  // no longer reads the last run's keys. The effect stages are what can
  // be known without having read a sketch.
  const size_t stages = buildStagesAhead(*precompile, g_stages);
  std::fprintf(stderr,
               "[sketchbook] pipelines: no recorded set for this build, %zu "
               "effect stages stood up instead\n",
               stages);
}

void finishPipelineWarmup() {
  if (!g_open) return;
  g_open = false;
  const std::lock_guard<std::mutex> alone(g_warmupLock);
  const std::vector<pipelines::RecordedPipeline> keys = recorder()->recorded();
  recorder()->report(g_replayed, keys.size());
  // No file name means no context was ever warmed, so nothing in this
  // run knows which backend the keys would be for.
  if (g_keySetFile.empty() || keys.empty()) return;
  if (!pipelines::writeKeySet(g_keySetFile, keys))
    std::fprintf(stderr, "[sketchbook] pipelines: could not write %s\n",
                 g_keySetFile.c_str());
}
