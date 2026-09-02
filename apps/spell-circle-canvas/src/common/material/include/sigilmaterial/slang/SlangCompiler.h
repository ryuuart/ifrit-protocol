#pragma once

/** @file
 * The Slang backend: Slang source compiled to SPIR-V, the reflected
 * layout that says where each uniform's bytes go, and the buffer one
 * draw's uniforms are written into at those offsets.
 *
 * A material's body exists only as a value in memory, so its program is
 * assembled and compiled when the library RUNS: a renderer's scaffold
 * text, the recipe's generated declarations, the recipe's body, and one
 * entry point that calls it. What the build compiles on its own is the
 * scaffold, which is what makes a mistake in it a build failure.
 *
 * NOTHING HERE GUESSES A LAYOUT. Every uniform's offset is the one the
 * compiler reported for the program it just built, so a body that
 * declares one more parameter moves nothing a renderer has to be told
 * about.
 *
 * Every session this opens already carries two modules by name, so a
 * shader may `import` them and nothing is looked for on disk: `Portable`,
 * the subset whose transcendentals a host and a device answer alike, and
 * `Shading`, the material kit's own terms — so a renderer's shading and
 * every material body compiled beside it call one definition of a term
 * rather than a copy apiece.
 */

#include <sigilmaterial/core/Program.h>
#include <sigilmaterial/core/Recipe.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <glm/mat4x4.hpp>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::material::slang {

/** ONE UNIFORM, as the compiler laid it out in the program's single
 *  uniform buffer.
 *
 *  A matrix's rows and an array's elements are laid out at a STRIDE
 *  wider than the values in them, so writing such a member is one copy
 *  per row or element rather than one copy of the whole thing. A scalar
 *  or a vector has `count` 1 and `stride` 0, which is the contiguous
 *  case. */
struct UniformSlot {
  size_t offset = 0;
  /** The whole member's size in bytes, padding included. */
  size_t bytes = 0;
  /** Rows, or elements; 1 for a scalar or a vector. */
  size_t count = 1;
  /** Bytes from one row or element to the next; 0 when there is one. */
  size_t stride = 0;
};

/** TWO STAGES AND THEIR LAYOUT.
 *
 *  The entry point of each stage is named `main` whatever the Slang
 *  function was called, which is what the SPIR-V emitter does and not a
 *  choice made here. */
struct Compiled {
  std::vector<uint32_t> vertex;
  std::vector<uint32_t> fragment;
  std::map<std::string, UniformSlot, std::less<>> uniforms;
  /** The sampled slots, in the order they were declared. */
  std::vector<std::string> textures;
  /** How large the program's one uniform buffer is. */
  size_t uniformBytes = 0;

  /** Where @p name's bytes go, or null when the program has no such
   *  uniform — one the optimiser dropped as unused, or a scaffold
   *  uniform this build does not carry. */
  [[nodiscard]] const UniformSlot* uniform(std::string_view name) const;
  [[nodiscard]] bool empty() const { return vertex.empty(); }
};

/** Compiles @p source — a whole module, imports and entry points and all
 *  — taking @p vertexEntry and @p fragmentEntry from it. @p lit defines
 *  `SIGIL_LIT`, which is the one axis a renderer's scaffold specialises
 *  on. False with the compiler's diagnostics in @p error. */
bool compileModule(std::string_view source, std::string_view vertexEntry,
                   std::string_view fragmentEntry, bool lit, Compiled* out,
                   std::string* error);

/** A recipe compiled for this target. A renderer reaches it off a
 *  `Program` with `as<SlangProgram>()`. */
class SlangProgram : public Program {
 public:
  SlangProgram(std::shared_ptr<const Recipe> recipe, Variant variant,
               Compiled compiled)
      : Program(std::move(recipe), Target::Slang, variant),
        m_compiled(std::move(compiled)) {}

  const Compiled& compiled() const { return m_compiled; }

 private:
  Compiled m_compiled;
};

/** ONE DRAW'S UNIFORMS, written at the offsets the program reported. */
class Uniforms {
 public:
  explicit Uniforms(const Compiled& program)
      : m_program(&program), m_bytes(program.uniformBytes, std::byte{0}) {}

  /** @p count floats into @p name, spread over the member's rows or
   *  elements where the layout put them apart. A name the program does
   *  not carry is skipped: an optimiser that dropped an unused uniform
   *  is not a mistake to report. */
  void set(std::string_view name, const float* values, size_t count);
  void set(std::string_view name, const glm::mat4& m);
  void set(std::string_view name, float x, float y, float z, float w);
  /** Element @p index of an array member. */
  void setElement(std::string_view name, size_t index, const float* values,
                  size_t count);

  [[nodiscard]] const std::vector<std::byte>& bytes() const { return m_bytes; }

 private:
  const Compiled* m_program;
  std::vector<std::byte> m_bytes;
};

}  // namespace sigil::material::slang
