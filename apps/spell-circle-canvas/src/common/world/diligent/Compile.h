#pragma once

/** @file
 * Slang compiled to SPIR-V: the two stages a pipeline is built from and
 * the reflected layout that says where each uniform's bytes go.
 *
 * A material's body exists only as a value in memory, so its program is
 * assembled and compiled when the library RUNS: the scaffold's text, the
 * recipe's generated declarations, the recipe's body, and one fragment
 * entry point that calls it. The build compiles the scaffold on its own,
 * which is what makes a mistake in it a build failure.
 *
 * NOTHING HERE GUESSES A LAYOUT. Every uniform's offset is the one the
 * compiler reported for the program it just built, so a body that
 * declares one more parameter moves nothing a renderer has to be told
 * about.
 */

#include <sigilmaterial/core/Program.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::world::diligent {

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
 *  `SIGIL_LIT`, which is the one axis the scaffold specialises on.
 *  False with the compiler's diagnostics in @p error. */
bool compileModule(std::string_view source, std::string_view vertexEntry,
                   std::string_view fragmentEntry, bool lit, Compiled* out,
                   std::string* error);

/** A recipe compiled for this backend. */
class SlangProgram : public material::Program {
 public:
  SlangProgram(std::shared_ptr<const material::Recipe> recipe,
               material::Variant variant, Compiled compiled)
      : Program(std::move(recipe), material::Target::Slang, variant),
        m_compiled(std::move(compiled)) {}

  const Compiled& compiled() const { return m_compiled; }

 private:
  Compiled m_compiled;
};

/** THE SCAFFOLD ON ITS OWN, in each of its two builds. It is what a body
 *  whose material has no Slang body is drawn with — the colour the frame
 *  extracted, lit or not as the pass wants — and its unlit build is what
 *  a coverage or a variant re-draw uses. Empty when the scaffold failed
 *  to compile, which is reported once. */
const Compiled& scaffold(bool lit);

/** WHAT A POST PASS DOES, one program each. Each is a triangle covering
 *  the target and one fragment stage; none of them depends on a
 *  material, so all four are the same for every frame. */
struct PostPrograms {
  Compiled copy;
  Compiled blur;
  Compiled levels;
  /** The second draw of a masked pass: the graded picture, reaching the
   *  one beneath it only where the coverage stands. */
  Compiled masked;
};
const PostPrograms& postPrograms();

}  // namespace sigil::world::diligent
