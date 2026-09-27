#pragma once

/** @file
 * @ingroup material-substance
 *
 * THE COOK BEHIND A SUBSTANCE MATERIAL: one graph instance and the engine
 * renderer that cooks it, its inputs written by raw identifier
 * (`$outputsize` in log2, `$randomseed`, `$normalformat`), presets
 * applied, and cooks scheduled apart from the caller with the newest
 * request replacing a stale one. Each cooked output is a
 * `media::PixelSource` whose `revision()` bumps when a cook lands: on
 * the CPU engine a frame holding an image in host memory, on the GPU
 * engine a frame standing on the device as a texture, which a leaf, a
 * pen or a material texture binds for the recorder drawing it with no
 * copy back. `material::substance()` builds its material over one of
 * these; reach for it directly for control the material entrance does
 * not give.
 */

#include <sigilmaterial/substance/Substance.h>
#include <sigilmaterial/substance/advanced/Archive.h>
#include <sigilmedia/core/PixelSource.h>
#include <sigilmotion/values/Animatable.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::material::sbsar {

/** How a preset meets the values already written. */
enum class PresetMode : uint8_t {
  Reset,  ///< every input the preset does not list goes back to its default
  Merge,  ///< inputs the preset does not list keep their values
};

/** How a cook is set up before its first run. */
struct CookOptions {
  /** Pixels per side, rounded up to a power of two; zero keeps the
   *  author's. */
  int resolution = 0;
  std::optional<int> seed;
  /** The outputs to cook; empty cooks every image output. */
  std::vector<OutputRequest> outputs;
  /** The engine; unset takes `engine()`. One that does not start here
   *  falls back to the CPU, said once. */
  std::optional<Engine> engine;
};

/** How many times a cooked output standing on the device has been read
 *  back into host memory in this process — which only a caller drawing
 *  with no recorder, or feeding one cook's output to another's image
 *  input, causes. */
uint64_t deviceReadbacks();

/** ONE GRAPH, COOKING. A handle: copies share the instance, the renderer
 *  and the cooked pictures. Every member is safe to call from any thread;
 *  the engine calls back on its own.
 *  @trap Writing an input does not cook. `cook()` schedules one;
 *  `cookNow()` runs one on the caller's thread. */
class CookScheduler {
 public:
  /** No graph: every write fails and every output is empty. */
  CookScheduler() = default;
  /** The graph at @p graph of @p archive, set up by @p options, not yet
   *  cooked. */
  CookScheduler(std::shared_ptr<const Archive> archive, size_t graph,
                const CookOptions& options = {});

  explicit operator bool() const { return m_state != nullptr; }

  /** The graph described — the archive's own description, built when it
   *  was decoded and never again. */
  const Description& description() const;
  /** The engine this graph cooks on; `None` for no graph. */
  Engine engine() const;

  /** @name Inputs by raw identifier
   *  @{ */
  /** Writes @p values, one per component, to the numeric input
   *  @p identifier; integers are truncated. False for an input the graph
   *  does not have or a count that is not its component count. */
  bool set(std::string_view identifier, std::span<const float> values);
  /** Writes a text input. */
  bool setText(std::string_view identifier, std::string_view text);
  /** Feeds an image input with the frame @p pixels stands at time zero,
   *  flattened to 8-bit RGBA; an empty source resets the input. */
  bool setImage(std::string_view identifier, const media::PixelSource& pixels);
  /** The input's value as floats; empty for an input the graph lacks. */
  std::vector<float> get(std::string_view identifier) const;
  /** Whether the input invalidates every cached intermediate when it
   *  moves (a seed does), as the author marked it or as last set. */
  bool isHeavyDuty(std::string_view identifier) const;
  bool setHeavyDuty(std::string_view identifier, bool heavyDuty);
  /** Every input back to its authored default. */
  void reset();
  /** Which way the normal output points its green channel: true for
   *  DirectX (green down, the engine's default), false for OpenGL. */
  bool normalsAreDirectX() const;
  /** @} */

  /** @name Following a value
   *  @{ */
  /** Makes @p identifier follow @p value: each `follow()` reads it and
   *  schedules a cook when it moved. */
  void bind(std::string_view identifier, motion::Animatable<float> value);
  void unbind(std::string_view identifier);
  /** Reads every bound value and schedules a cook when one moved. Every
   *  cooked output's `frameAt()` calls it. */
  void follow() const;
  /** @} */

  /** @name Presets
   *  @{ */
  /** Adds the presets a `.sbsprs` file's text declares; answers how many
   *  it held. */
  size_t addPresets(std::string_view text);
  /** The labels of every preset this graph can be set to: embedded ones
   *  first, then added ones. */
  std::vector<std::string> presets() const;
  /** Applies the preset labelled @p label; false when there is none. */
  bool applyPreset(std::string_view label, PresetMode mode = PresetMode::Merge);
  /** @} */

  /** @name Cooking
   *  @{ */
  /** Schedules a cook of the current values apart from the caller; one
   *  still waiting is replaced. */
  void cook() const;
  /** Cooks the current values on the caller's thread. False when the
   *  engine reports a failure. */
  bool cookNow() const;
  /** Holds until every scheduled cook has landed. */
  void wait() const;
  /** Whether a scheduled cook has not landed. */
  bool isPending() const;
  /** Whether a picture can change without another write: a bound value
   *  moves, a cook is pending, or a cook landed that no output has handed
   *  out yet. */
  bool isRunning() const;
  /** How many cooks have landed. */
  uint64_t revision() const;
  /** @} */

  /** The output @p usage (a channel or an identifier) as a pixel source;
   *  empty when the graph has no such image output or it is not cooked. */
  media::PixelSource output(std::string_view usage) const;
  /** The usages of the outputs this cook produces, in the author's order. */
  std::vector<std::string> cooked() const;

  bool operator==(const CookScheduler& other) const {
    return m_state == other.m_state;
  }

  struct State;

 private:
  std::shared_ptr<State> m_state;
};

}  // namespace sigil::material::sbsar
