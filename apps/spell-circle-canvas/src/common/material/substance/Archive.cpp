/** @file
 * An archive decoded: the package parsed with every raw output format
 * allowed and no mip pyramid, one instance per graph made once to read
 * its inputs, and each graph's inputs, outputs and embedded presets
 * described into plain values that every cook of it then shares.
 */

#include "Internal.h"

#include <sigilmaterial/substance/advanced/Cook.h>

#include <sigilio/advanced/Decoding.h>
#include <sigilio/hub/Hub.h>

namespace sigil::material::sbsar {

namespace {

namespace air = SubstanceAir;

InputType typeOf(SubstanceIOType type) {
  switch (type) {
    case Substance_IOType_Float:
      return InputType::Float;
    case Substance_IOType_Float2:
      return InputType::Float2;
    case Substance_IOType_Float3:
      return InputType::Float3;
    case Substance_IOType_Float4:
      return InputType::Float4;
    case Substance_IOType_Integer:
      return InputType::Integer;
    case Substance_IOType_Integer2:
      return InputType::Integer2;
    case Substance_IOType_Integer3:
      return InputType::Integer3;
    case Substance_IOType_Integer4:
      return InputType::Integer4;
    case Substance_IOType_Image:
      return InputType::Image;
    case Substance_IOType_String:
      return InputType::Text;
    default:
      return InputType::Other;
  }
}

Widget widgetOf(air::InputWidget widget) {
  switch (widget) {
    case air::Input_Slider:
      return Widget::Slider;
    case air::Input_Angle:
      return Widget::Angle;
    case air::Input_Color:
      return Widget::Color;
    case air::Input_Togglebutton:
      return Widget::Toggle;
    case air::Input_Enumbuttons:
      return Widget::Buttons;
    case air::Input_Combobox:
      return Widget::Combobox;
    case air::Input_Image:
      return Widget::Image;
    case air::Input_Position:
      return Widget::Position;
    default:
      return Widget::Unspecified;
  }
}

Input describeInput(air::InputInstanceBase& instance) {
  const air::InputDescBase& source = instance.mDesc;
  Input input;
  input.name = toString(source.mIdentifier);
  input.label = toString(source.mLabel);
  input.group = toString(source.mGuiGroup);
  input.description = toString(source.mGuiDescription);
  input.type = typeOf(source.mType);
  input.widget = widgetOf(source.mGuiWidget);
  input.visibleIf = toString(source.mGuiVisibleIf);
  withNumeric(instance, [&](auto& numeric, int count) {
    using Instance = std::decay_t<decltype(numeric)>;
    const typename Instance::Desc& authored = numeric.getDesc();
    toFloats(authored.mDefaultValue, count, input.defaultValue);
    toFloats(authored.mMinValue, count, input.minimum);
    toFloats(authored.mMaxValue, count, input.maximum);
    input.step = authored.mSliderStep;
    input.clamp = authored.mSliderClamp;
    for (const auto& [value, label] : authored.mEnumValues) {
      if constexpr (std::is_arithmetic_v<std::decay_t<decltype(value)>>)
        input.choices.push_back({(int)value, toString(label)});
    }
    return true;
  });
  return input;
}

Output describeOutput(const air::OutputDesc& source) {
  Output output;
  output.name = toString(source.mIdentifier);
  output.label = toString(source.mLabel);
  output.usage = usageOf(source);
  output.encoding = encodingOf(source);
  output.image = source.isImage();
  return output;
}

}  // namespace

std::string usageOf(const air::OutputDesc& output) {
  if (!output.mChannelsFull.empty()) {
    const air::ChannelFullDesc& channel = output.mChannelsFull.front();
    if (channel.mUsage != air::Channel_UNKNOWN)
      return air::getChannelNames()[channel.mUsage];
    if (!channel.mUsageStr.empty()) return toString(channel.mUsageStr);
  }
  if (!output.mChannels.empty() && output.mChannels.front() != air::Channel_UNKNOWN)
    return air::getChannelNames()[output.mChannels.front()];
  if (!output.mChannelsStr.empty()) return toString(output.mChannelsStr.front());
  return toString(output.mIdentifier);
}

Encoding encodingOf(const air::OutputDesc& output) {
  const bool floating = (output.mFormat & Substance_PF_FP) != 0;
  air::ColorSpace space = air::ColorSpace_UNKNOWN;
  if (!output.mChannelsFull.empty())
    space = output.mChannelsFull.front().mColorSpace;
  if (space == air::ColorSpace_UNKNOWN) {
    air::ChannelUse use = air::Channel_UNKNOWN;
    if (!output.mChannelsFull.empty())
      use = output.mChannelsFull.front().mUsage;
    else if (!output.mChannels.empty())
      use = output.mChannels.front();
    if (use != air::Channel_UNKNOWN)
      space = air::getDefaultColorSpace(use, floating);
  }
  switch (space) {
    case air::ColorSpace_sRGB:
      return Encoding::Srgb;
    case air::ColorSpace_Linear:
      return Encoding::Linear;
    default:
      return Encoding::Raw;
  }
}

std::optional<Archive> Archive::decode(std::span<const std::byte> bytes) {
  if (bytes.empty()) return std::nullopt;
  auto decoded = std::make_shared<Decoded>();
  // Every uncompressed format, so a cook can ask for 16 bits or floats
  // per output; no pyramids, because a texture samples its own.
  air::OutputOptions options;
  options.mAllowedFormats = air::Format_Mask_Raw;
  options.mMipmap = air::Mipmap_ForceNone;
  decoded->package = std::make_unique<air::PackageDesc>(
      bytes.data(), bytes.size(), options);
  if (!decoded->package->isValid()) return std::nullopt;
  for (const air::GraphDesc& graph : decoded->package->getGraphs()) {
    // AN INSTANCE IS WHERE THE TYPED INPUT DESCRIPTIONS ARE REACHED, so
    // one is made here, read, and let go: the description is built once
    // for every cook of this archive rather than once per cook.
    air::GraphInstance instance(graph);
    Description description;
    description.graph = toString(graph.mLabel);
    for (air::InputInstanceBase* input : instance.getInputs())
      description.inputs.push_back(describeInput(*input));
    for (const air::OutputDesc& output : graph.mOutputs)
      description.outputs.push_back(describeOutput(output));
    for (const air::Preset& preset : graph.mPresets)
      description.presets.push_back(toString(preset.mLabel));
    decoded->graphs.push_back(std::move(description));
    decoded->urls.push_back(toString(graph.mPackageUrl));
  }
  Archive archive;
  archive.m_decoded = std::move(decoded);
  return archive;
}

size_t Archive::graphCount() const {
  return m_decoded ? m_decoded->graphs.size() : 0;
}

const Description& Archive::graph(size_t index) const {
  return m_decoded->graphs.at(index);
}

const std::string& Archive::url(size_t index) const {
  return m_decoded->urls.at(index);
}

std::optional<size_t> Archive::find(std::string_view labelOrUrl) const {
  if (!m_decoded || m_decoded->graphs.empty()) return std::nullopt;
  if (labelOrUrl.empty()) return 0;
  for (size_t index = 0; index < m_decoded->graphs.size(); ++index)
    if (m_decoded->graphs[index].graph == labelOrUrl ||
        m_decoded->urls[index] == labelOrUrl)
      return index;
  return std::nullopt;
}

void registerDecoder(io::Hub& hub) {
  io::registerDecoder<Archive>(hub, [](const io::Bytes& bytes) {
    return Archive::decode(bytes.span());
  });
}

std::shared_ptr<const Archive> load(io::Hub& hub, std::string_view uri) {
  if (std::shared_ptr<const Archive> archive = hub.load<Archive>(uri))
    return archive;
  // A hub answers null both when it has no decoder for the type and when
  // the decode failed; registering is harmless in the second case, and
  // the retry reads the cached bytes rather than fetching them again.
  registerDecoder(hub);
  return hub.load<Archive>(uri);
}

std::string engineVersion() {
  air::Renderer probe;
  const SubstanceVersion version = probe.getCurrentVersion();
  return std::string(version.platformImplName ? version.platformImplName
                                               : "") +
         " " + std::to_string(version.versionMajor) + "." +
         std::to_string(version.versionMinor) + "." +
         std::to_string(version.versionPatch);
}

bool available() { return true; }

Engine engine() { return Engine::Cpu; }

}  // namespace sigil::material::sbsar
