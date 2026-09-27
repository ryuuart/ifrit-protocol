/** @file
 * One graph cooking: the instance and the renderer that cooks it, inputs
 * written under one lock, cooks run apart from the caller with the newest
 * replacing a stale one, and each result turned into an image on the
 * engine's thread as it lands. A cooked output is a pixel source over the
 * same state, whose revision bumps when a cook lands and whose frame read
 * is where bound values are followed.
 */

#include <sigilmaterial/substance/advanced/Cook.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkImageInfo.h>

#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <utility>

#include "Internal.h"

namespace sigil::material::sbsar {

namespace air = SubstanceAir;

namespace {

/** The precision an output is cooked at, as the engine's format bits
 *  over the channels the output was authored with; zero to keep the
 *  author's format. */
unsigned formatFor(const air::OutputDesc& output, const std::string& usage,
                   Format format) {
  const unsigned authored = (unsigned)output.mFormat;
  const unsigned channels = authored & Substance_PF_MASK_RAWChannels;
  const unsigned precision = authored & Substance_PF_MASK_RAWPrecision;
  switch (format) {
    case Format::Automatic:
      // A normal or a height bands at 8 bits: its gradients are what a
      // surface is lit from.
      if ((usage == "normal" || usage == "height") &&
          precision == Substance_PF_8I)
        return channels | Substance_PF_16I;
      return 0;
    case Format::Unorm8:
      return channels | Substance_PF_8I;
    case Format::Unorm16:
      return channels | Substance_PF_16I;
    case Format::Float16:
      return channels | Substance_PF_16F;
    case Format::Float32:
      return channels | Substance_PF_32F;
  }
  return 0;
}

}  // namespace

struct CookScheduler::State {
  /** What the engine calls back with, on its own thread. */
  struct Callbacks final : air::RenderCallbacks {
    State* state = nullptr;
    void outputComputed(air::UInt, size_t, const air::GraphInstance*,
                        air::OutputInstance* output) override {
      state->land(*output);
    }
    void jobComputed(air::UInt, size_t) override {
      state->revision.fetch_add(1);
    }
  };

  /** One output of the graph as this cook sees it. */
  struct Cooked {
    std::string usage;
    std::string name;
    Encoding encoding = Encoding::Raw;
    bool enabled = false;
    sk_sp<SkImage> image;
    /** The revision the last frame read of this output handed out. */
    std::atomic<uint64_t> handedOut{0};
  };

  /** A value an input follows, and the value it was last written. */
  struct Binding {
    std::string identifier;
    motion::Animatable<float> value{0.0f};
    float written = std::numeric_limits<float>::quiet_NaN();
  };

  std::shared_ptr<const Archive> archive;
  size_t graph = 0;
  std::unique_ptr<air::GraphInstance> instance;
  Callbacks callbacks;
  std::unique_ptr<air::Renderer> renderer;
  /** Guards the instance, the renderer, the bindings and the presets. */
  mutable std::mutex engine;
  /** Guards the cooked images. */
  mutable std::mutex results;
  std::vector<std::unique_ptr<Cooked>> outputs;
  std::vector<Binding> bindings;
  std::map<std::string, air::InputImage::SPtr, std::less<>> heldImages;
  air::Presets addedPresets;
  std::atomic<uint64_t> revision{0};
  air::UInt lastRun = 0;

  ~State() {
    // Results the engine still holds must go before its renderer, and
    // the renderer before the instance it cooks.
    if (renderer) renderer->cancelAll();
    renderer.reset();
    instance.reset();
  }

  air::InputInstanceBase* input(std::string_view identifier) const {
    for (air::InputInstanceBase* candidate : instance->getInputs())
      if (std::string_view(candidate->mDesc.mIdentifier.data(),
                           candidate->mDesc.mIdentifier.size()) == identifier)
        return candidate;
    return nullptr;
  }

  bool write(std::string_view identifier, std::span<const float> values) {
    air::InputInstanceBase* found = input(identifier);
    if (!found) return false;
    return withNumeric(*found, [&](auto& numeric, int count) {
      if ((int)values.size() != count) return false;
      using Value = std::decay_t<decltype(numeric.getValue())>;
      numeric.setValue(fromFloats<Value>(values, count));
      return true;
    });
  }

  /** A result the engine finished, turned into an image and kept. */
  void land(air::OutputInstance& output) {
    air::OutputInstance::Result result(output.grabResult());
    if (!result || !result->isImage()) return;
    const size_t index = output.mUserData;
    if (index >= outputs.size()) return;
    auto* picture = static_cast<air::RenderResultImage*>(result.get());
    sk_sp<SkImage> image =
        imageOf(picture->getTexture(), outputs[index]->encoding);
    if (!image) return;
    std::lock_guard lock(results);
    outputs[index]->image = std::move(image);
  }

  /** Pushes the current values and runs; called with `engine` held. */
  void schedule() {
    renderer->push(*instance);
    lastRun = renderer->run(air::Renderer::Run_Asynchronous |
                            air::Renderer::Run_Replace);
  }

  bool pending() const {
    std::lock_guard lock(engine);
    return lastRun != 0 && renderer->isPending(lastRun);
  }

  void follow() {
    std::lock_guard lock(engine);
    bool moved = false;
    for (Binding& binding : bindings) {
      const float now = binding.value.value();
      if (now == binding.written) continue;
      const float values[] = {now};
      if (write(binding.identifier, values)) moved = true;
      binding.written = now;
    }
    if (moved) schedule();
  }

  bool running() const {
    {
      std::lock_guard lock(engine);
      for (const Binding& binding : bindings)
        if (binding.value.isRunning()) return true;
    }
    if (pending()) return true;
    const uint64_t landed = revision.load();
    std::lock_guard lock(results);
    for (const auto& output : outputs)
      if (output->enabled && output->image &&
          output->handedOut.load() < landed)
        return true;
    return false;
  }
};

namespace {

/** A cooked output as a pixel source: the newest picture it landed. */
struct CookedOutput {
  std::shared_ptr<CookScheduler::State> state;
  size_t index = 0;

  media::Frame frameAt(std::chrono::duration<double>) const {
    state->follow();
    media::Frame frame;
    const uint64_t landed = state->revision.load();
    std::lock_guard lock(state->results);
    frame.image = state->outputs[index]->image;
    state->outputs[index]->handedOut.store(landed);
    return frame;
  }
  bool isRunning() const { return state->running(); }
  uint64_t revision() const { return state->revision.load(); }
  SkISize size() const {
    std::lock_guard lock(state->results);
    const sk_sp<SkImage>& image = state->outputs[index]->image;
    return image ? image->dimensions() : SkISize::MakeEmpty();
  }
  bool operator==(const CookedOutput& other) const {
    return state == other.state && index == other.index;
  }
};

}  // namespace

CookScheduler::CookScheduler(std::shared_ptr<const Archive> archive,
                             size_t graph, const CookOptions& options) {
  if (!archive || graph >= archive->graphCount()) return;
  auto state = std::make_shared<State>();
  state->archive = std::move(archive);
  state->graph = graph;
  const air::GraphDesc& source =
      state->archive->decoded()->package->getGraphs()[graph];
  state->instance = std::make_unique<air::GraphInstance>(source);
  state->callbacks.state = state.get();
  state->renderer = std::make_unique<air::Renderer>();
  state->renderer->setRenderCallbacks(&state->callbacks);

  const Description& described = state->archive->graph(graph);
  const air::GraphInstance::Outputs& outputs = state->instance->getOutputs();
  for (size_t index = 0; index < outputs.size(); ++index) {
    air::OutputInstance& output = *outputs[index];
    auto cooked = std::make_unique<State::Cooked>();
    cooked->usage = described.outputs[index].usage;
    cooked->name = described.outputs[index].name;
    cooked->encoding = described.outputs[index].encoding;
    const OutputRequest* request = nullptr;
    for (const OutputRequest& asked : options.outputs)
      if (asked.usage == cooked->usage || asked.usage == cooked->name)
        request = &asked;
    cooked->enabled =
        described.outputs[index].image && (options.outputs.empty() || request);
    output.mEnabled = cooked->enabled;
    output.mUserData = index;
    if (cooked->enabled) {
      if (const unsigned format =
              formatFor(output.mDesc, cooked->usage,
                        request ? request->format : Format::Automatic)) {
        air::OutputFormat override;
        override.format = format;
        override.mipmapLevelsCount = air::OutputFormat::MipmapNone;
        output.overrideFormat(override);
      }
    }
    state->outputs.push_back(std::move(cooked));
  }
  if (options.resolution > 0) {
    const float side = std::ceil(std::log2((float)options.resolution));
    const float log2Size[] = {side, side};
    state->write("$outputsize", log2Size);
  }
  if (options.seed) {
    const float seed[] = {(float)*options.seed};
    state->write("$randomseed", seed);
  }
  m_state = std::move(state);
}

const Description& CookScheduler::description() const {
  static const Description none;
  return m_state ? m_state->archive->graph(m_state->graph) : none;
}

bool CookScheduler::set(std::string_view identifier,
                        std::span<const float> values) {
  if (!m_state) return false;
  std::lock_guard lock(m_state->engine);
  return m_state->write(identifier, values);
}

bool CookScheduler::setText(std::string_view identifier, std::string_view text) {
  if (!m_state) return false;
  std::lock_guard lock(m_state->engine);
  air::InputInstanceBase* found = m_state->input(identifier);
  if (!found || !found->mDesc.isString()) return false;
  static_cast<air::InputInstanceString*>(found)->setString(
      air::string(text.begin(), text.end()));
  return true;
}

bool CookScheduler::setImage(std::string_view identifier,
                             const media::PixelSource& pixels) {
  if (!m_state) return false;
  std::lock_guard lock(m_state->engine);
  air::InputInstanceBase* found = m_state->input(identifier);
  if (!found || !found->mDesc.isImage()) return false;
  auto* imageInput = static_cast<air::InputInstanceImage*>(found);
  const sk_sp<SkImage> image = pixels.frameAt({}).image;
  if (!image) {
    imageInput->reset();
    m_state->heldImages.erase(std::string(identifier));
    return true;
  }
  const int width = image->width(), height = image->height();
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::Make(width, height, kRGBA_8888_SkColorType,
                                       kUnpremul_SkAlphaType));
  if (!image->readPixels(nullptr, bitmap.pixmap(), 0, 0)) return false;
  SubstanceTexture texture = {};
  texture.level0Width = (unsigned short)width;
  texture.level0Height = (unsigned short)height;
  texture.pixelFormat = Substance_PF_RGBA;
  texture.channelsOrder = Substance_ChanOrder_RGBA;
  texture.mipmapCount = 1;
  air::InputImage::SPtr held = air::InputImage::create(texture);
  if (!held) return false;
  {
    air::InputImage::ScopedAccess access(held);
    for (int row = 0; row < height; ++row)
      std::memcpy(static_cast<uint8_t*>(access->buffer) +
                      (size_t)row * (size_t)width * 4,
                  bitmap.getAddr(0, row), (size_t)width * 4);
  }
  imageInput->setImage(held);
  // The engine reads the buffer rather than copying it, so the image is
  // held for as long as the input names it: one per input, replaced when
  // the input is fed again.
  m_state->heldImages[std::string(identifier)] = std::move(held);
  return true;
}

std::vector<float> CookScheduler::get(std::string_view identifier) const {
  std::vector<float> values;
  if (!m_state) return values;
  std::lock_guard lock(m_state->engine);
  if (air::InputInstanceBase* found = m_state->input(identifier))
    withNumeric(*found, [&](auto& numeric, int count) {
      toFloats(numeric.getValue(), count, values);
      return true;
    });
  return values;
}

bool CookScheduler::isHeavyDuty(std::string_view identifier) const {
  if (!m_state) return false;
  std::lock_guard lock(m_state->engine);
  const air::InputInstanceBase* found = m_state->input(identifier);
  return found && found->mIsHeavyDuty;
}

bool CookScheduler::setHeavyDuty(std::string_view identifier, bool heavyDuty) {
  if (!m_state) return false;
  std::lock_guard lock(m_state->engine);
  air::InputInstanceBase* found = m_state->input(identifier);
  if (!found) return false;
  found->mIsHeavyDuty = heavyDuty;
  return true;
}

void CookScheduler::reset() {
  if (!m_state) return;
  std::lock_guard lock(m_state->engine);
  for (air::InputInstanceBase* input : m_state->instance->getInputs())
    input->reset();
  m_state->heldImages.clear();
}

bool CookScheduler::normalsAreDirectX() const {
  if (!m_state) return true;
  std::lock_guard lock(m_state->engine);
  air::InputInstanceBase* found = m_state->input("$normalformat");
  if (!found) return true;
  bool directX = true;
  withNumeric(*found, [&](auto& numeric, int count) {
    if (count != 1) return false;
    if constexpr (std::is_arithmetic_v<
                      std::decay_t<decltype(numeric.getValue())>>)
      directX = (int)numeric.getValue() == 0;
    return true;
  });
  return directX;
}

void CookScheduler::bind(std::string_view identifier,
                         motion::Animatable<float> value) {
  if (!m_state) return;
  std::lock_guard lock(m_state->engine);
  for (State::Binding& binding : m_state->bindings)
    if (binding.identifier == identifier) {
      binding.value = std::move(value);
      binding.written = std::numeric_limits<float>::quiet_NaN();
      return;
    }
  m_state->bindings.push_back({std::string(identifier), std::move(value)});
}

void CookScheduler::unbind(std::string_view identifier) {
  if (!m_state) return;
  std::lock_guard lock(m_state->engine);
  std::erase_if(m_state->bindings, [&](const State::Binding& binding) {
    return binding.identifier == identifier;
  });
}

void CookScheduler::follow() const {
  if (m_state) m_state->follow();
}

size_t CookScheduler::addPresets(std::string_view text) {
  if (!m_state) return 0;
  air::Presets parsed;
  const std::string terminated(text);
  if (!air::parsePresets(parsed, terminated.c_str())) return 0;
  std::lock_guard lock(m_state->engine);
  for (const air::Preset& preset : parsed)
    m_state->addedPresets.push_back(preset);
  return parsed.size();
}

std::vector<std::string> CookScheduler::presets() const {
  std::vector<std::string> labels;
  if (!m_state) return labels;
  std::lock_guard lock(m_state->engine);
  for (const air::Preset& preset : m_state->instance->mDesc.mPresets)
    labels.push_back(toString(preset.mLabel));
  for (const air::Preset& preset : m_state->addedPresets)
    labels.push_back(toString(preset.mLabel));
  return labels;
}

bool CookScheduler::applyPreset(std::string_view label, PresetMode mode) {
  if (!m_state) return false;
  std::lock_guard lock(m_state->engine);
  const auto apply = [&](const air::Presets& presets) {
    for (const air::Preset& preset : presets)
      if (toString(preset.mLabel) == label)
        return preset.apply(*m_state->instance,
                            mode == PresetMode::Reset
                                ? air::Preset::Apply_Reset
                                : air::Preset::Apply_Merge);
    return false;
  };
  return apply(m_state->instance->mDesc.mPresets) ||
         apply(m_state->addedPresets);
}

void CookScheduler::cook() const {
  if (!m_state) return;
  std::lock_guard lock(m_state->engine);
  m_state->schedule();
}

bool CookScheduler::cookNow() const {
  if (!m_state) return false;
  const uint64_t before = m_state->revision.load();
  {
    std::lock_guard lock(m_state->engine);
    m_state->renderer->push(*m_state->instance);
    m_state->renderer->run(air::Renderer::Run_Default);
    // A result the callback did not take is taken here.
    for (air::OutputInstance* output : m_state->instance->getOutputs())
      m_state->land(*output);
  }
  if (m_state->revision.load() == before) m_state->revision.fetch_add(1);
  return true;
}

void CookScheduler::wait() const {
  if (!m_state) return;
  std::lock_guard lock(m_state->engine);
  m_state->renderer->flush();
}

bool CookScheduler::isPending() const {
  return m_state && m_state->pending();
}

bool CookScheduler::isRunning() const {
  return m_state && m_state->running();
}

uint64_t CookScheduler::revision() const {
  return m_state ? m_state->revision.load() : 0;
}

media::PixelSource CookScheduler::output(std::string_view usage) const {
  if (!m_state) return {};
  for (size_t index = 0; index < m_state->outputs.size(); ++index) {
    const State::Cooked& cooked = *m_state->outputs[index];
    if (cooked.enabled && (cooked.usage == usage || cooked.name == usage))
      return media::PixelSource(CookedOutput{m_state, index});
  }
  return {};
}

std::vector<std::string> CookScheduler::cooked() const {
  std::vector<std::string> usages;
  if (!m_state) return usages;
  for (const auto& cooked : m_state->outputs)
    if (cooked->enabled) usages.push_back(cooked->usage);
  return usages;
}

}  // namespace sigil::material::sbsar
