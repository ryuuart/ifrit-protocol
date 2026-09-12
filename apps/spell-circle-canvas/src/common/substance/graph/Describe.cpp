/** @file
 * A graph described: its inputs as Parameters (kind, widget, current
 * and authored values, combobox choices) and its outputs as Outputs
 * (the channel usage and colour space the graph declared).
 */

#include "GraphImpl.h"
#include "Numeric.h"

namespace sigil::substance {

namespace {

namespace air = SubstanceAir;

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

}  // namespace

std::vector<Parameter> Graph::parameters() const {
  std::vector<Parameter> out;
  for (air::InputInstanceBase* in : m_impl->instance->getInputs()) {
    Parameter p;
    p.identifier = toString(in->mDesc.mIdentifier);
    p.label = toString(in->mDesc.mLabel);
    p.group = toString(in->mDesc.mGuiGroup);
    p.kind = kindOf(in->mDesc.mType);
    p.widget = widgetOf(in->mDesc.mGuiWidget);
    withNumeric(*in, [&](auto& numeric, int n) {
      using Instance = std::decay_t<decltype(numeric)>;
      const typename Instance::Desc& description = numeric.getDesc();
      toFloats(numeric.getValue(), n, p.values);
      toFloats(description.mDefaultValue, n, p.defaults);
      toFloats(description.mMinValue, n, p.minimum);
      toFloats(description.mMaxValue, n, p.maximum);
      for (const auto& [value, label] : description.mEnumValues) {
        if constexpr (std::is_arithmetic_v<std::decay_t<decltype(value)>>)
          p.choices.emplace_back((int)value, toString(label));
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
    d.identifier = toString(o->mDesc.mIdentifier);
    d.label = toString(o->mDesc.mLabel);
    d.image = o->mDesc.isImage();
    if (!o->mDesc.mChannelsFull.empty()) {
      const air::ChannelFullDesc& ch = o->mDesc.mChannelsFull.front();
      d.usage = ch.mUsage == air::Channel_UNKNOWN
                    ? toString(ch.mUsageStr)
                    : std::string(air::getChannelNames()[ch.mUsage]);
      d.srgb = ch.mColorSpace == air::ColorSpace_sRGB;
    } else if (!o->mDesc.mChannelsStr.empty()) {
      d.usage = toString(o->mDesc.mChannelsStr.front());
    }
    out.push_back(std::move(d));
  }
  return out;
}

}  // namespace sigil::substance
