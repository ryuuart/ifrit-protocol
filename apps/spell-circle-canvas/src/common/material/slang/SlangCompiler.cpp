/** @file
 * The Slang compile: one session per specialisation, one module per
 * program, the reflection that says where a uniform's bytes go, and the
 * write into those offsets.
 *
 * The vendor's own namespace is `slang` and so is this one, so every
 * name of theirs is spelled `::slang::` here.
 */

#include <sigilio/hub/TextCatalog.h>
#include <sigilmaterial/kit/Terms.h>
#include <sigilmaterial/slang/SlangCompiler.h>
#include <slang-com-ptr.h>
#include <slang.h>

#include <algorithm>
#include <cstring>
#include <mutex>

namespace sigil::material::slang {

namespace {

constexpr char kShaderPrefix[] = "shader://material/slang/";

io::TextCatalog& shaders() {
  static io::TextCatalog catalog(kShaderPrefix, SIGIL_MATERIAL_SLANG_SHADER_DIR);
  return catalog;
}

std::string portableSource() {
  return shaders().text("Portable.slang").value_or("");
}

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
    // The portable subset and shading terms have been fetched through the
    // source libraries and are loaded into the session by name, so every
    // module's `import` resolves without another resource lookup. The terms
    // are the material kit's own text: a renderer's shading and every material
    // body compiled beside it call one definition rather than a copy apiece.
    Slang::ComPtr<::slang::IBlob> diagnostics;
    const std::string portable = portableSource();
    slot->loadModuleFromSourceString("Portable", "Portable.slang",
                                     portable.c_str(), diagnostics.writeRef());
    slot->loadModuleFromSourceString("Shading", "Shading.slang",
                                     kit::termsSource(Target::Slang).c_str(),
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

  ::slang::ISession* session = Compiler::shared().session(lit, error);
  if (!session) return false;

  Slang::ComPtr<::slang::IBlob> diagnostics;
  const std::string text(source);
  // A NAME NO OTHER MODULE HAS. A session remembers a module by its
  // name, so two recipes loaded under one name would be one module and
  // every material after the first would be drawn with the first one's
  // program.
  static uint64_t serial = 0;
  const std::string name = "SigilProgram" + std::to_string(++serial);
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

void Uniforms::set(std::string_view name, const glm::mat4& m) {
  // The shader reads a matrix row by row, and glm holds it column by
  // column, so what is written is the transpose.
  float rows[16];
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c) rows[r * 4 + c] = m[c][r];
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
