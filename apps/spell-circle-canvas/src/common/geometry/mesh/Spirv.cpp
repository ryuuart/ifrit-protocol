/** @file
 * The decoration that forbids fusing, added to a module the emitter left
 * without one.
 */

#include "sigilgeometry/mesh/Spirv.h"

namespace sigil::geometry::mesh {

namespace {

/** The words a SPIR-V module opens with before its first instruction:
 *  the magic, the version, the generator, the id bound and the schema. */
constexpr size_t kHeaderWords = 5;

/** The magic number a module opens with. */
constexpr uint32_t kMagic = 0x07230203u;

/** OpDecorate, and the decoration that forbids fusing an arithmetic
 *  result into the operation after it. */
constexpr uint32_t kOpDecorate = 71;
constexpr uint32_t kNoContraction = 42;

/** The annotation instructions, which are the ones a decoration must be
 *  added among: a module states every decoration in one block, and one
 *  placed outside it is not a valid module. */
bool annotation(uint32_t opcode) {
  return opcode == kOpDecorate ||  // OpDecorate
         opcode == 72 ||           // OpMemberDecorate
         opcode == 73 ||           // OpDecorationGroup
         opcode == 74 ||           // OpGroupDecorate
         opcode == 75;             // OpGroupMemberDecorate
}

/** The FLOATING-POINT ARITHMETIC whose result a driver may fuse. Every
 *  one of them carries its result type in the first word after the
 *  opcode and its result id in the second, which is what lets the id be
 *  read without knowing anything else about the instruction. */
bool arithmetic(uint32_t opcode) {
  switch (opcode) {
    case 127:  // OpFNegate
    case 129:  // OpFAdd
    case 131:  // OpFSub
    case 133:  // OpFMul
    case 136:  // OpFDiv
    case 140:  // OpFRem
    case 141:  // OpFMod
    case 142:  // OpVectorTimesScalar
    case 143:  // OpMatrixTimesScalar
    case 144:  // OpVectorTimesMatrix
    case 145:  // OpMatrixTimesVector
    case 146:  // OpMatrixTimesMatrix
    case 147:  // OpOuterProduct
    case 148:  // OpDot
      return true;
    default:
      return false;
  }
}

}  // namespace

std::vector<uint32_t> noContraction(std::span<const uint32_t> words) {
  const size_t count = words.size();
  std::vector<uint32_t> out(words.begin(), words.end());
  if (count < kHeaderWords || out[0] != kMagic) return out;

  std::vector<uint32_t> results;
  size_t insertAt = 0;
  for (size_t at = kHeaderWords; at < count;) {
    const uint32_t length = out[at] >> 16u;
    const uint32_t opcode = out[at] & 0xFFFFu;
    if (length == 0 || at + length > count) return {words.begin(), words.end()};
    if (annotation(opcode)) insertAt = at + length;
    if (arithmetic(opcode) && length >= 3) results.push_back(out[at + 2]);
    at += length;
  }
  if (results.empty() || insertAt == 0) return out;

  std::vector<uint32_t> added;
  added.reserve(results.size() * 3);
  for (uint32_t id : results) {
    added.push_back((3u << 16u) | kOpDecorate);
    added.push_back(id);
    added.push_back(kNoContraction);
  }
  out.insert(out.begin() + (long)insertAt, added.begin(), added.end());
  return out;
}

}  // namespace sigil::geometry::mesh
