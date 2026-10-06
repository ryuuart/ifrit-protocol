/** @file
 * The Slang compile: one session per specialisation, one module per
 * program, the reflection that says where a uniform's bytes go, and the
 * write into those offsets.
 *
 * The vendor's own namespace is `slang` and so is this one, so every
 * name of theirs is spelled `::slang::` here.
 */

#include <sigilmaterial/advanced/Terms.h>
#include <sigilmaterial/slang/SlangCompiler.h>
#include <sigilshaders/MaterialSlang.h>
#include <slang-com-ptr.h>
#include <slang.h>

#include <algorithm>
#include <boost/unordered/unordered_flat_map.hpp>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <system_error>

namespace sigil::material::slang {

namespace {

/** The compiler's diagnostics, or an empty string when it produced
 *  none. */
std::string textOf(const Slang::ComPtr<::slang::IBlob>& blob) {
  if (!blob || blob->getBufferSize() == 0) return {};
  return std::string(static_cast<const char*>(blob->getBufferPointer()),
                     blob->getBufferSize());
}

/** ONE SESSION PER SPECIALISATION, holding the portable subset and the
 *  shading terms as modules every program imports. A session is the unit
 *  the preprocessor macros belong to, so the lit and unlit builds are two
 *  of them.
 *
 *  The global session loads the compiler's own standard library, which
 *  is the expensive part and is paid once for the process. */
class Compiler {
 public:
  static Compiler& shared() {
    static Compiler compiler;
    return compiler;
  }

  /** The session for @p lit, or null with the reason in @p error. */
  ::slang::ISession* session(bool lit, std::string* error) {
    Slang::ComPtr<::slang::ISession>& slot = lit ? m_lit : m_unlit;
    if (slot) return slot.get();
    if (!m_global) {
      if (error) *error = "the Slang compiler is unavailable";
      return nullptr;
    }
    ::slang::TargetDesc target{};
    target.format = SLANG_SPIRV;
    target.profile = m_global->findProfile("spirv_1_5");
    ::slang::PreprocessorMacroDesc lifted{"SIGIL_LIT", "1"};
    ::slang::SessionDesc desc{};
    desc.targets = &target;
    desc.targetCount = 1;
    if (lit) {
      desc.preprocessorMacros = &lifted;
      desc.preprocessorMacroCount = 1;
    }
    if (SLANG_FAILED(m_global->createSession(desc, slot.writeRef()))) {
      if (error) *error = "the Slang session could not be created";
      return nullptr;
    }
    // The portable subset and the shading terms are compiled into the
    // archives they belong to and are loaded into the session by name, so
    // every module's `import` resolves with nothing to look up. The terms
    // are the material kit's own text: a renderer's shading and every
    // material body compiled beside it call one definition rather than a
    // copy apiece.
    Slang::ComPtr<::slang::IBlob> diagnostics;
    const std::string portable(shaderSource("Portable.slang"));
    slot->loadModuleFromSourceString("Portable", "Portable.slang",
                                     portable.c_str(), diagnostics.writeRef());
    slot->loadModuleFromSourceString("Shading", "Shading.slang",
                                     termsSource(Target::Slang).c_str(),
                                     diagnostics.writeRef());
    return slot.get();
  }

 private:
  Compiler() { ::slang::createGlobalSession(m_global.writeRef()); }

  Slang::ComPtr<::slang::IGlobalSession> m_global;
  Slang::ComPtr<::slang::ISession> m_lit;
  Slang::ComPtr<::slang::ISession> m_unlit;
};

/** The reflected layout of the program's global uniform buffer and its
 *  sampled slots. */
void reflect(::slang::ProgramLayout* layout, Compiled* out) {
  if (!layout) return;
  const unsigned count = layout->getParameterCount();
  for (unsigned i = 0; i < count; ++i) {
    ::slang::VariableLayoutReflection* param = layout->getParameterByIndex(i);
    const char* name = param->getName();
    if (!name) continue;
    ::slang::TypeLayoutReflection* type = param->getTypeLayout();
    const size_t bytes = type->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM);
    if (bytes == 0) {
      // No bytes in the uniform buffer means a resource: a sampled slot,
      // which a renderer binds by the name it was declared with.
      out->textures.emplace_back(name);
      continue;
    }
    const size_t offset = param->getOffset(SLANG_PARAMETER_CATEGORY_UNIFORM);
    UniformSlot slot{offset, bytes, 1, 0};
    switch (type->getKind()) {
      case ::slang::TypeReflection::Kind::Matrix:
        slot.count = type->getRowCount();
        slot.stride = slot.count ? bytes / slot.count : 0;
        break;
      case ::slang::TypeReflection::Kind::Array:
        slot.count = type->getElementCount();
        slot.stride = type->getElementStride(SLANG_PARAMETER_CATEGORY_UNIFORM);
        break;
      default:
        break;
    }
    out->uniforms.emplace(name, slot);
    out->uniformBytes = std::max(out->uniformBytes, offset + bytes);
  }
}

/** Where @p name sits among the linked program's entry points; -1 when
 *  it is not one of them. Asked by name rather than assumed from the
 *  order the components were handed over, because the layout is what
 *  says which index a stage's code is under. */
int entryIndexOf(::slang::ProgramLayout* layout, std::string_view name) {
  if (!layout) return -1;
  const unsigned count = layout->getEntryPointCount();
  for (unsigned i = 0; i < count; ++i) {
    const char* found = layout->getEntryPointByIndex(i)->getName();
    if (found && name == found) return (int)i;
  }
  return -1;
}

/** One stage's SPIR-V words. */
bool codeOf(::slang::IComponentType& linked, int entry,
            std::vector<uint32_t>* out, std::string* error) {
  Slang::ComPtr<::slang::IBlob> code;
  Slang::ComPtr<::slang::IBlob> diagnostics;
  if (SLANG_FAILED(linked.getEntryPointCode(entry, 0, code.writeRef(),
                                            diagnostics.writeRef())) ||
      !code) {
    if (error) *error = textOf(diagnostics);
    return false;
  }
  const auto* words = static_cast<const uint32_t*>(code->getBufferPointer());
  const size_t count = code->getBufferSize() / sizeof(uint32_t);
  out->assign(words, words + count);
  return true;
}

/** The stem every diagnostic names the compiled source by. */
constexpr std::string_view programStem = "SigilProgram";

/** @p text with @p name cut back to the stem.
 *
 *  A module is loaded under a name no other module shares, and the
 *  compiler quotes that name back in every diagnostic that carries a
 *  source location. A caller handed over one source and is being told
 *  about that one, so what distinguishes the name from every other
 *  compile's comes back out: the same mistake in the same body reads the
 *  same whatever else was compiled first. */
std::string withoutSerial(std::string text, std::string_view name) {
  for (size_t at = text.find(name); at != std::string::npos;
       at = text.find(name, at + programStem.size()))
    text.replace(at, name.size(), programStem);
  return text;
}

/** One compile, of a module loaded under @p name. */
bool compileNamed(const std::string& name, std::string_view source,
                  std::string_view vertexEntry, std::string_view fragmentEntry,
                  bool lit, Compiled* out, std::string* error) {
  ::slang::ISession* session = Compiler::shared().session(lit, error);
  if (!session) return false;

  Slang::ComPtr<::slang::IBlob> diagnostics;
  const std::string text(source);
  ::slang::IModule* module = session->loadModuleFromSourceString(
      name.c_str(), (name + ".slang").c_str(), text.c_str(),
      diagnostics.writeRef());
  if (!module) {
    if (error) *error = textOf(diagnostics);
    return false;
  }

  Slang::ComPtr<::slang::IEntryPoint> vertex;
  Slang::ComPtr<::slang::IEntryPoint> fragment;
  module->findEntryPointByName(std::string(vertexEntry).c_str(),
                               vertex.writeRef());
  module->findEntryPointByName(std::string(fragmentEntry).c_str(),
                               fragment.writeRef());
  if (!vertex || !fragment) {
    if (error)
      *error = "the module declares no entry point named " +
               std::string(vertex ? fragmentEntry : vertexEntry);
    return false;
  }

  ::slang::IComponentType* parts[] = {module, vertex.get(), fragment.get()};
  Slang::ComPtr<::slang::IComponentType> composite;
  if (SLANG_FAILED(session->createCompositeComponentType(
          parts, 3, composite.writeRef(), diagnostics.writeRef()))) {
    if (error) *error = textOf(diagnostics);
    return false;
  }
  Slang::ComPtr<::slang::IComponentType> linked;
  if (SLANG_FAILED(
          composite->link(linked.writeRef(), diagnostics.writeRef()))) {
    if (error) *error = textOf(diagnostics);
    return false;
  }

  // ONE LINK FOR BOTH STAGES, because the layout is a property of the
  // linked program: linking them apart would let an unused uniform be
  // dropped from one and not the other, and the two would then read one
  // buffer at two sets of offsets.
  ::slang::ProgramLayout* layout = linked->getLayout(0);
  const int vertexAt = entryIndexOf(layout, vertexEntry);
  const int fragmentAt = entryIndexOf(layout, fragmentEntry);
  if (vertexAt < 0 || fragmentAt < 0) {
    if (error) *error = "the linked program carries neither stage";
    return false;
  }
  Compiled built;
  if (!codeOf(*linked, vertexAt, &built.vertex, error)) return false;
  if (!codeOf(*linked, fragmentAt, &built.fragment, error)) return false;
  reflect(layout, &built);
  *out = std::move(built);
  return true;
}

/** EVERYTHING A COMPILE'S OUTPUT DEPENDS ON, as one text: the compiler's
 *  own build, the modules every session loads, the specialisation, the
 *  two entry points and the source. Two compiles with the same text
 *  produce the same words, so the text is what a kept program is found
 *  by — and compared against whole, never by a digest alone. */
std::string compileKey(std::string_view source, std::string_view vertexEntry,
                       std::string_view fragmentEntry, bool lit) {
  static const std::string fixed = [] {
    std::string text = "slang ";
    if (const char* tag = spGetBuildTagString()) text += tag;
    text += '\n';
    text += shaderSource("Portable.slang");
    text += '\n';
    text += termsSource(Target::Slang);
    text += '\n';
    return text;
  }();
  std::string key = fixed;
  key += lit ? "lit\n" : "unlit\n";
  key += vertexEntry;
  key += '\n';
  key += fragmentEntry;
  key += '\n';
  key += source;
  return key;
}

/** A stable 64-bit digest of @p text, which names a kept program's file
 *  and nothing more: the file carries the whole key and is read only
 *  when that matches. */
uint64_t digestOf(std::string_view text) {
  uint64_t hash = 0xcbf29ce484222325ull;
  for (const char c : text) {
    hash ^= (uint8_t)c;
    hash *= 0x100000001b3ull;
  }
  return hash;
}

/** The directory compiled programs are kept in between runs, empty for
 *  none. */
std::filesystem::path& storeDirectory() {
  static std::filesystem::path directory;
  return directory;
}

constexpr char kStoreMark[8] = {'S', 'I', 'G', 'P', 'R', 'G', '0', '1'};

/** A kept file longer than this is not one this code wrote. */
constexpr uint64_t kLongestStoredField = 64ull << 20;

void writeNumber(std::ofstream& out, uint64_t value) {
  out.write(reinterpret_cast<const char*>(&value), sizeof value);
}

void writeText(std::ofstream& out, std::string_view text) {
  writeNumber(out, text.size());
  out.write(text.data(), (std::streamsize)text.size());
}

void writeWords(std::ofstream& out, const std::vector<uint32_t>& words) {
  writeNumber(out, words.size());
  out.write(reinterpret_cast<const char*>(words.data()),
            (std::streamsize)(words.size() * sizeof(uint32_t)));
}

bool readNumber(std::ifstream& in, uint64_t* value) {
  in.read(reinterpret_cast<char*>(value), sizeof *value);
  return (bool)in;
}

bool readText(std::ifstream& in, std::string* text) {
  uint64_t size = 0;
  if (!readNumber(in, &size) || size > kLongestStoredField) return false;
  text->resize(size);
  in.read(text->data(), (std::streamsize)size);
  return (bool)in;
}

bool readWords(std::ifstream& in, std::vector<uint32_t>* words) {
  uint64_t count = 0;
  if (!readNumber(in, &count) || count * sizeof(uint32_t) > kLongestStoredField)
    return false;
  words->resize(count);
  in.read(reinterpret_cast<char*>(words->data()),
          (std::streamsize)(count * sizeof(uint32_t)));
  return (bool)in;
}

std::filesystem::path storedFile(const std::string& key) {
  char name[32];
  std::snprintf(name, sizeof name, "%016llx.program",
                (unsigned long long)digestOf(key));
  return storeDirectory() / name;
}

/** The program kept for @p key, when one is and its key is @p key. */
bool readStored(const std::string& key, Compiled* out) {
  if (storeDirectory().empty()) return false;
  std::ifstream in(storedFile(key), std::ios::binary);
  if (!in) return false;
  char mark[sizeof kStoreMark];
  in.read(mark, sizeof mark);
  if (!in || std::memcmp(mark, kStoreMark, sizeof mark) != 0) return false;
  std::string kept;
  if (!readText(in, &kept) || kept != key) return false;
  Compiled read;
  uint64_t count = 0;
  uint64_t bytes = 0;
  if (!readWords(in, &read.vertex) || !readWords(in, &read.fragment) ||
      !readNumber(in, &bytes) || !readNumber(in, &count) || count > 1u << 16)
    return false;
  read.uniformBytes = (size_t)bytes;
  for (uint64_t i = 0; i < count; ++i) {
    std::string name;
    uint64_t fields[4];
    if (!readText(in, &name)) return false;
    for (uint64_t& field : fields)
      if (!readNumber(in, &field)) return false;
    read.uniforms.emplace(std::move(name),
                          UniformSlot{(size_t)fields[0], (size_t)fields[1],
                                      (size_t)fields[2], (size_t)fields[3]});
  }
  if (!readNumber(in, &count) || count > 1u << 16) return false;
  for (uint64_t i = 0; i < count; ++i) {
    std::string name;
    if (!readText(in, &name)) return false;
    read.textures.push_back(std::move(name));
  }
  if (read.vertex.empty() || read.fragment.empty()) return false;
  *out = std::move(read);
  return true;
}

/** @p compiled kept for @p key, written beside its place and moved into
 *  it, so a reader never meets half a file. A store that cannot be
 *  written to keeps nothing and the compile stands. */
void writeStored(const std::string& key, const Compiled& compiled) {
  if (storeDirectory().empty()) return;
  std::error_code ignored;
  std::filesystem::create_directories(storeDirectory(), ignored);
  const std::filesystem::path into = storedFile(key);
  std::filesystem::path partial = into;
  partial += ".partial";
  {
    std::ofstream out(partial, std::ios::binary | std::ios::trunc);
    if (!out) return;
    out.write(kStoreMark, sizeof kStoreMark);
    writeText(out, key);
    writeWords(out, compiled.vertex);
    writeWords(out, compiled.fragment);
    writeNumber(out, compiled.uniformBytes);
    writeNumber(out, compiled.uniforms.size());
    for (const auto& [name, slot] : compiled.uniforms) {
      writeText(out, name);
      writeNumber(out, slot.offset);
      writeNumber(out, slot.bytes);
      writeNumber(out, slot.count);
      writeNumber(out, slot.stride);
    }
    writeNumber(out, compiled.textures.size());
    for (const std::string& name : compiled.textures) writeText(out, name);
    if (!out) {
      out.close();
      std::filesystem::remove(partial, ignored);
      return;
    }
  }
  std::filesystem::rename(partial, into, ignored);
  if (ignored) std::filesystem::remove(partial, ignored);
}

}  // namespace

const UniformSlot* Compiled::uniform(std::string_view name) const {
  const auto it = uniforms.find(name);
  return it == uniforms.end() ? nullptr : &it->second;
}

bool compileModule(std::string_view source, std::string_view vertexEntry,
                   std::string_view fragmentEntry, bool lit, Compiled* out,
                   std::string* error) {
  // The compiler is not re-entrant across sessions it shares a global
  // session with, and a program cache is asked from whichever thread
  // resolves a material first.
  static std::mutex mutex;
  const std::lock_guard<std::mutex> lock(mutex);

  // ONE COMPILE PER TEXT FOR THE PROCESS, AND ONE PER TEXT FOR THE STORE.
  // A sketch reloaded, a recipe made again with the same body and two
  // recipes whose bodies agree all hand over the same text, and the words
  // it compiles to are the same words.
  static boost::unordered_flat_map<std::string, Compiled> compiled;
  std::string key = compileKey(source, vertexEntry, fragmentEntry, lit);
  if (const auto found = compiled.find(key); found != compiled.end()) {
    *out = found->second;
    return true;
  }
  if (Compiled kept; readStored(key, &kept)) {
    *out = kept;
    compiled.emplace(std::move(key), std::move(kept));
    return true;
  }

  // A NAME NO OTHER MODULE HAS. A session remembers a module by its
  // name, so two recipes loaded under one name would be one module and
  // every material after the first would be drawn with the first one's
  // program. The serial that keeps them apart counts what the process
  // compiled before, which is nothing the caller asked about, so it is
  // taken back out of anything the caller reads.
  static uint64_t serial = 0;
  const std::string name = std::string(programStem) + std::to_string(++serial);
  std::string diagnostic;
  if (compileNamed(name, source, vertexEntry, fragmentEntry, lit, out,
                   &diagnostic)) {
    writeStored(key, *out);
    compiled.emplace(std::move(key), *out);
    return true;
  }
  if (error) *error = withoutSerial(std::move(diagnostic), name);
  return false;
}

void useProgramStore(std::filesystem::path directory) {
  storeDirectory() = std::move(directory);
}

void Uniforms::set(std::string_view name, const float* values, size_t count) {
  const UniformSlot* slot = m_program->uniform(name);
  if (!slot) return;
  const size_t perRow = slot->count ? count / slot->count : count;
  if (slot->stride == 0 || slot->count <= 1) {
    const size_t bytes = std::min(count * sizeof(float), slot->bytes);
    std::memcpy(m_bytes.data() + slot->offset, values, bytes);
    return;
  }
  for (size_t row = 0; row < slot->count; ++row) {
    const size_t at = slot->offset + row * slot->stride;
    if (at + perRow * sizeof(float) > m_bytes.size()) break;
    std::memcpy(m_bytes.data() + at, values + row * perRow,
                perRow * sizeof(float));
  }
}

void Uniforms::set(std::string_view name, const glm::mat4& matrix) {
  // The shader reads a matrix row by row, and glm holds it column by
  // column, so what is written is the transpose.
  float rows[16];
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c) rows[r * 4 + c] = matrix[c][r];
  set(name, rows, 16);
}

void Uniforms::set(std::string_view name, float x, float y, float z, float w) {
  const float values[4] = {x, y, z, w};
  set(name, values, 4);
}

void Uniforms::setElement(std::string_view name, size_t index,
                          const float* values, size_t count) {
  const UniformSlot* slot = m_program->uniform(name);
  if (!slot || index >= slot->count) return;
  const size_t at = slot->offset + index * slot->stride;
  if (at + count * sizeof(float) > m_bytes.size()) return;
  std::memcpy(m_bytes.data() + at, values, count * sizeof(float));
}

}  // namespace sigil::material::slang
