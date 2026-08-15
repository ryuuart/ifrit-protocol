#include "sigilsubstance/Substance.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkData.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPixmap.h>
#include <substance/framework/framework.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <mutex>

namespace sigil::substance {

namespace {

namespace air = SubstanceAir;

/** The framework's strings use their own allocator; copy out. */
std::string str(const air::string& s) { return std::string(s.c_str()); }

Parameter::Kind kindOf(SubstanceIOType type) {
  switch (type) {
    case Substance_IOType_Float:
      return Parameter::Kind::Float;
    case Substance_IOType_Float2:
      return Parameter::Kind::Float2;
    case Substance_IOType_Float3:
      return Parameter::Kind::Float3;
    case Substance_IOType_Float4:
      return Parameter::Kind::Float4;
    case Substance_IOType_Integer:
      return Parameter::Kind::Int;
    case Substance_IOType_Integer2:
      return Parameter::Kind::Int2;
    case Substance_IOType_Integer3:
      return Parameter::Kind::Int3;
    case Substance_IOType_Integer4:
      return Parameter::Kind::Int4;
    case Substance_IOType_Image:
      return Parameter::Kind::Image;
    case Substance_IOType_String:
      return Parameter::Kind::Text;
    default:
      return Parameter::Kind::Other;
  }
}

Parameter::Widget widgetOf(air::InputWidget widget) {
  switch (widget) {
    case air::Input_Slider:
      return Parameter::Widget::Slider;
    case air::Input_Angle:
      return Parameter::Widget::Angle;
    case air::Input_Color:
      return Parameter::Widget::Color;
    case air::Input_Togglebutton:
      return Parameter::Widget::Toggle;
    case air::Input_Enumbuttons:
      return Parameter::Widget::Buttons;
    case air::Input_Combobox:
      return Parameter::Widget::Combobox;
    case air::Input_Image:
      return Parameter::Widget::Image;
    case air::Input_Position:
      return Parameter::Widget::Position;
    default:
      return Parameter::Widget::None;
  }
}

/** Visit a numeric input instance with its concrete type. The callback
 *  receives (InputInstanceNumerical<T>&, componentCount) and the vector
 *  element type through the template. */
template <class F>
bool withNumeric(air::InputInstanceBase& input, F&& f) {
  switch (input.mDesc.mType) {
    case Substance_IOType_Float:
      return f(static_cast<air::InputInstanceFloat&>(input), 1);
    case Substance_IOType_Float2:
      return f(static_cast<air::InputInstanceFloat2&>(input), 2);
    case Substance_IOType_Float3:
      return f(static_cast<air::InputInstanceFloat3&>(input), 3);
    case Substance_IOType_Float4:
      return f(static_cast<air::InputInstanceFloat4&>(input), 4);
    case Substance_IOType_Integer:
      return f(static_cast<air::InputInstanceInt&>(input), 1);
    case Substance_IOType_Integer2:
      return f(static_cast<air::InputInstanceInt2&>(input), 2);
    case Substance_IOType_Integer3:
      return f(static_cast<air::InputInstanceInt3&>(input), 3);
    case Substance_IOType_Integer4:
      return f(static_cast<air::InputInstanceInt4&>(input), 4);
    default:
      return false;
  }
}

template <class T>
void toFloats(const T& v, int n, std::vector<float>& out) {
  out.resize((size_t)n);
  if constexpr (std::is_arithmetic_v<T>) {
    out[0] = (float)v;
  } else {
    for (int i = 0; i < n; ++i) out[(size_t)i] = (float)v[i];
  }
}

template <class T>
T fromFloats(const std::vector<float>& in, int n) {
  T v{};
  if constexpr (std::is_arithmetic_v<T>) {
    v = (T)in[0];
  } else {
    using E = std::decay_t<decltype(v[0])>;
    for (int i = 0; i < n; ++i) v[i] = (E)in[(size_t)i];
  }
  return v;
}

/** The engine's blend-platform result → an SkImage. RGBA8 becomes an
 *  N32 image, L8 a grey one; anything else (16-bit, float, compressed)
 *  is refused, which the package's output options prevent by only
 *  allowing those two. */
sk_sp<SkImage> imageFrom(const SubstanceTexture& tex) {
  const int w = tex.level0Width, h = tex.level0Height;
  if (!tex.buffer || w <= 0 || h <= 0) return nullptr;
  const unsigned fmt = tex.pixelFormat & Substance_PF_MASK;
  const unsigned channels = fmt & Substance_PF_MASK_RAWChannels;
  const unsigned precision = fmt & Substance_PF_MASK_RAWPrecision;
  if ((fmt & Substance_PF_MASK_RAWFormat) != Substance_PF_RAW ||
      precision != Substance_PF_8I)
    return nullptr;
  if (channels == Substance_PF_L) {
    SkBitmap bm;
    bm.allocPixels(
        SkImageInfo::Make(w, h, kGray_8_SkColorType, kOpaque_SkAlphaType));
    for (int y = 0; y < h; ++y)
      std::memcpy(bm.getAddr(0, y),
                  (const uint8_t*)tex.buffer + (size_t)y * (size_t)w,
                  (size_t)w);
    bm.setImmutable();
    return bm.asImage();
  }
  if (channels != Substance_PF_RGBA && channels != Substance_PF_RGBx)
    return nullptr;
  // Channel order: the engine may hand back RGBA or BGRA; Skia has a
  // colour type for each, so no swizzle pass is needed.
  const SkColorType ct = tex.channelsOrder == Substance_ChanOrder_BGRA
                             ? kBGRA_8888_SkColorType
                             : kRGBA_8888_SkColorType;
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::Make(w, h, ct, kUnpremul_SkAlphaType));
  std::memcpy(bm.getPixels(), tex.buffer, (size_t)w * (size_t)h * 4);
  bm.setImmutable();
  return bm.asImage();
}

}  // namespace

// ---------------------------------------------------------------------------

struct Graph::Impl {
  air::GraphInstance* instance = nullptr;  // owned by the package's list
  air::Renderer* renderer = nullptr;       // owned by the package
  std::string label;
  std::string url;
  std::map<std::string, sk_sp<SkImage>> byIdentifier;
  std::map<std::string, sk_sp<SkImage>> byUsage;
  std::vector<air::InputImage::SPtr> heldImages;  // keep inputs alive

  air::InputInstanceBase* input(std::string_view identifier) const {
    for (air::InputInstanceBase* in : instance->getInputs())
      if (std::string_view(in->mDesc.mIdentifier.c_str()) == identifier)
        return in;
    return nullptr;
  }
};

Graph::Graph() : m_impl(std::make_unique<Impl>()) {}
Graph::~Graph() = default;

const std::string& Graph::label() const { return m_impl->label; }
const std::string& Graph::url() const { return m_impl->url; }

std::vector<Parameter> Graph::parameters() const {
  std::vector<Parameter> out;
  for (air::InputInstanceBase* in : m_impl->instance->getInputs()) {
    Parameter p;
    p.identifier = str(in->mDesc.mIdentifier);
    p.label = str(in->mDesc.mLabel);
    p.group = str(in->mDesc.mGuiGroup);
    p.kind = kindOf(in->mDesc.mType);
    p.widget = widgetOf(in->mDesc.mGuiWidget);
    withNumeric(*in, [&](auto& numeric, int n) {
      using Inst = std::decay_t<decltype(numeric)>;
      const typename Inst::Desc& desc = numeric.getDesc();
      toFloats(numeric.getValue(), n, p.values);
      toFloats(desc.mDefaultValue, n, p.defaults);
      toFloats(desc.mMinValue, n, p.minimum);
      toFloats(desc.mMaxValue, n, p.maximum);
      for (const auto& [value, label] : desc.mEnumValues) {
        if constexpr (std::is_arithmetic_v<std::decay_t<decltype(value)>>)
          p.choices.emplace_back((int)value, str(label));
      }
      return true;
    });
    out.push_back(std::move(p));
  }
  return out;
}

std::vector<Output> Graph::outputs() const {
  std::vector<Output> out;
  for (air::OutputInstance* o : m_impl->instance->getOutputs()) {
    Output d;
    d.identifier = str(o->mDesc.mIdentifier);
    d.label = str(o->mDesc.mLabel);
    d.image = o->mDesc.isImage();
    if (!o->mDesc.mChannelsFull.empty()) {
      const air::ChannelFullDesc& ch = o->mDesc.mChannelsFull.front();
      d.usage = ch.mUsage == air::Channel_UNKNOWN
                    ? str(ch.mUsageStr)
                    : std::string(air::getChannelNames()[ch.mUsage]);
      d.srgb = ch.mColorSpace == air::ColorSpace_sRGB;
    } else if (!o->mDesc.mChannelsStr.empty()) {
      d.usage = str(o->mDesc.mChannelsStr.front());
    }
    out.push_back(std::move(d));
  }
  return out;
}

bool Graph::set(std::string_view identifier, std::initializer_list<float> v) {
  return set(identifier, std::vector<float>(v));
}

bool Graph::set(std::string_view identifier, const std::vector<float>& value) {
  air::InputInstanceBase* in = m_impl->input(identifier);
  if (!in) return false;
  return withNumeric(*in, [&](auto& numeric, int n) {
    if ((int)value.size() != n) return false;
    using Inst = std::decay_t<decltype(numeric)>;
    using T = std::decay_t<decltype(numeric.getValue())>;
    numeric.setValue(fromFloats<T>(value, n));
    static_cast<void>(sizeof(Inst));
    return true;
  });
}

bool Graph::setImage(std::string_view identifier, const sk_sp<SkImage>& image) {
  air::InputInstanceBase* in = m_impl->input(identifier);
  if (!in || !in->mDesc.isImage()) return false;
  auto* imageInput = static_cast<air::InputInstanceImage*>(in);
  if (!image) {
    imageInput->reset();
    return true;
  }
  const int w = image->width(), h = image->height();
  SkBitmap bm;
  bm.allocPixels(
      SkImageInfo::Make(w, h, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType));
  if (!image->readPixels(nullptr, bm.pixmap(), 0, 0)) return false;
  SubstanceTexture texture = {};
  texture.buffer = nullptr;  // the framework allocates; filled below
  texture.level0Width = (unsigned short)w;
  texture.level0Height = (unsigned short)h;
  texture.pixelFormat = Substance_PF_RGBA;
  texture.channelsOrder = Substance_ChanOrder_RGBA;
  texture.mipmapCount = 1;
  air::InputImage::SPtr held = air::InputImage::create(texture);
  if (!held) return false;
  {
    air::InputImage::ScopedAccess access(held);
    for (int y = 0; y < h; ++y)
      std::memcpy((uint8_t*)access->buffer + (size_t)y * (size_t)w * 4,
                  bm.getAddr(0, y), (size_t)w * 4);
  }
  imageInput->setImage(held);
  m_impl->heldImages.push_back(held);
  return true;
}

bool Graph::setText(std::string_view identifier, std::string_view text) {
  air::InputInstanceBase* in = m_impl->input(identifier);
  if (!in || !in->mDesc.isString()) return false;
  static_cast<air::InputInstanceString*>(in)->setString(
      air::string(text.begin(), text.end()));
  return true;
}

bool Graph::setResolution(int log2Width, int log2Height) {
  return set("$outputsize", {(float)log2Width, (float)log2Height});
}

void Graph::reset() {
  for (air::InputInstanceBase* in : m_impl->instance->getInputs()) in->reset();
}

bool Graph::render() {
  Impl& impl = *m_impl;
  impl.renderer->push(*impl.instance);
  impl.renderer->run();
  impl.byIdentifier.clear();
  impl.byUsage.clear();
  const std::vector<Output> described = outputs();
  size_t index = 0;
  for (air::OutputInstance* o : impl.instance->getOutputs()) {
    const Output& d = described[index++];
    air::OutputInstance::Result result(o->grabResult());
    if (!result || !result->isImage()) continue;
    auto* img = static_cast<air::RenderResultImage*>(result.get());
    sk_sp<SkImage> image = imageFrom(img->getTexture());
    if (!image) continue;
    impl.byIdentifier[d.identifier] = image;
    impl.byUsage[d.usage.empty() ? d.identifier : d.usage] = image;
  }
  return !impl.byIdentifier.empty();
}

sk_sp<SkImage> Graph::output(std::string_view name) const {
  const std::string key(name);
  if (auto it = m_impl->byIdentifier.find(key);
      it != m_impl->byIdentifier.end())
    return it->second;
  if (auto it = m_impl->byUsage.find(key); it != m_impl->byUsage.end())
    return it->second;
  return nullptr;
}

std::map<std::string, sk_sp<SkImage>> Graph::outputsByUsage() const {
  return m_impl->byUsage;
}

// ---------------------------------------------------------------------------

struct Package::Impl {
  std::unique_ptr<air::PackageDesc> desc;
  air::GraphInstances instances;
  std::unique_ptr<air::Renderer> renderer;
  std::vector<std::unique_ptr<Graph>> graphs;
};

Package::Package() : m_impl(std::make_unique<Impl>()) {}
Package::~Package() {
  // Graphs hold raw pointers into instances and the renderer: drop them
  // first, then the renderer, then the instances, then the description.
  m_impl->graphs.clear();
  m_impl->renderer.reset();
  m_impl->instances.clear();
  m_impl->desc.reset();
}

std::unique_ptr<Package> Package::load(const void* bytes, size_t size,
                                       std::string* error) {
  if (!bytes || size == 0) {
    if (error) *error = "empty archive";
    return nullptr;
  }
  std::unique_ptr<Package> package(new Package());
  Impl& impl = *package->m_impl;
  // Only 8-bit raw outputs, no mip pyramids: the two forms imageFrom
  // turns into SkImages. The engine substitutes these for anything the
  // graph authored otherwise.
  air::OutputOptions options;
  options.mAllowedFormats = air::Format_RGBA8 | air::Format_L8;
  options.mMipmap = air::Mipmap_ForceNone;
  impl.desc = std::make_unique<air::PackageDesc>(bytes, size, options);
  if (!impl.desc->isValid()) {
    if (error) *error = "not a valid Substance archive";
    return nullptr;
  }
  air::instantiate(impl.instances, *impl.desc);
  impl.renderer = std::make_unique<air::Renderer>();
  for (const auto& instance : impl.instances) {
    std::unique_ptr<Graph> graph(new Graph());
    graph->m_impl->instance = instance.get();
    graph->m_impl->renderer = impl.renderer.get();
    graph->m_impl->label = str(instance->mDesc.mLabel);
    graph->m_impl->url = str(instance->mDesc.mPackageUrl);
    impl.graphs.push_back(std::move(graph));
  }
  return package;
}

std::unique_ptr<Package> Package::load(const std::filesystem::path& file,
                                       std::string* error) {
  std::ifstream in(file, std::ios::binary);
  if (!in) {
    if (error) *error = "cannot read " + file.string();
    return nullptr;
  }
  std::vector<char> bytes((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());
  return load(bytes.data(), bytes.size(), error);
}

size_t Package::graphCount() const { return m_impl->graphs.size(); }
Graph& Package::graph(size_t index) { return *m_impl->graphs.at(index); }
const Graph& Package::graph(size_t index) const {
  return *m_impl->graphs.at(index);
}
Graph* Package::find(std::string_view labelOrUrl) {
  for (auto& g : m_impl->graphs)
    if (g->label() == labelOrUrl || g->url() == labelOrUrl) return g.get();
  return nullptr;
}

std::string Package::engineVersion() {
  air::Renderer probe;
  const SubstanceVersion v = probe.getCurrentVersion();
  return std::string(v.platformImplName ? v.platformImplName : "") + " " +
         std::to_string(v.versionMajor) + "." + std::to_string(v.versionMinor) +
         "." + std::to_string(v.versionPatch);
}

}  // namespace sigil::substance
