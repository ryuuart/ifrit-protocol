#pragma once

/** @file
 * The frame graph: the order a frame's passes run in, derived from what
 * they declared they read and write; the surfaces two resources share
 * when their live ranges do not overlap; the barriers a backend needs
 * between them; and how each pass's selection is realised.
 *
 * Nothing here draws, and nothing here names a backend. A Plan is a
 * reading of the declarations, and the same reading whichever executor
 * performs it — which is what makes an ordering bug a test failure
 * rather than a picture.
 */

#include <sigilworld/frame/Frame.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::world::graph {

/** Which way a step touches a resource. */
enum class Access : uint8_t { Read, Write };

/** What a resource holds, taken from the stage that writes it: a
 *  compute pass writes points, and everything else writes pixels. */
enum class Kind : uint8_t { Image, Points };

/** ONE HAZARD between two steps over one resource, stated without
 *  naming a backend: what must finish, what may then begin, and which
 *  way each of them touched it. On the CPU executor the list is a
 *  no-op; a device turns each entry into the transition it needs. */
struct Barrier {
  std::string resource;
  size_t after = 0;   ///< the step that must finish
  size_t before = 0;  ///< the step that may then begin
  Access from = Access::Write;
  Access to = Access::Read;
  bool operator==(const Barrier&) const = default;
};

/** ONE RESOURCE the frame's passes named, and its life. */
struct Resource {
  std::string name;
  Kind kind = Kind::Image;
  /** The step that first writes it, and the last that touches it, in
   *  execution order. */
  size_t first = 0;
  size_t last = 0;
  /** The surface it was given; -1 for a resource that holds no surface
   *  or holds one of its own. */
  int slot = -1;
  /** It outlives the frame — read back, read as a previous, or the one
   *  the picture is presented from — so it is never aliased. */
  bool persistent = false;
  /** It was given a surface another resource also uses. */
  bool aliased = false;
};

/** THE ORDERING, READ OFF THE DECLARATIONS.
 *
 *  A plan points at the passes it was built from, so it stands only as
 *  long as they do; build it again when they change.
 *
 *  The rules, in full:
 *
 *  - a step runs after every step that WRITES what it reads; after
 *    every step that READS what it writes; and after every earlier
 *    declared step that writes what it writes;
 *  - `previous()` orders nothing, because it names last frame's copy —
 *    which is how a feedback loop is declared without a cycle;
 *  - among steps whose dependencies are all met, the one declared
 *    first runs first, so an order is a function of the declarations
 *    and never of the machine;
 *  - a cycle is an error naming the passes on it, and no plan is
 *    produced.
 */
class Plan {
 public:
  /** The passes in execution order, each with what the ordering decided
   *  about it. Empty when the plan failed. */
  [[nodiscard]] std::span<const PassWork> steps() const { return m_steps; }
  [[nodiscard]] std::span<const Barrier> barriers() const { return m_barriers; }
  [[nodiscard]] std::span<const Resource> resources() const {
    return m_resources;
  }
  /** How many surfaces the image resources need once the transients
   *  whose live ranges never overlap have been given the same one. */
  [[nodiscard]] int surfaces() const { return m_surfaces; }
  /** How many resources were given a surface another one also uses. */
  [[nodiscard]] int aliased() const;
  /** The resource @p name, or null when no pass named it. */
  [[nodiscard]] const Resource* resource(std::string_view name) const;
  /** The resource the finished picture is in; empty when no pass wrote
   *  an image. */
  [[nodiscard]] const std::string& present() const { return m_present; }
  /** The resources whose images must be kept for the frame after. */
  [[nodiscard]] std::span<const std::string> kept() const { return m_kept; }

  /** What stopped the plan being built, naming the passes. Empty when
   *  it was built. */
  [[nodiscard]] const std::string& error() const { return m_error; }
  explicit operator bool() const { return m_error.empty(); }

 private:
  friend Plan build(const Frame& frame);

  std::vector<PassWork> m_steps;
  std::vector<Barrier> m_barriers;
  std::vector<Resource> m_resources;
  std::vector<std::string> m_kept;
  std::string m_present;
  std::string m_error;
  int m_surfaces = 0;
};

/** Read @p frame's declarations into a plan. */
Plan build(const Frame& frame);

}  // namespace sigil::world::graph
