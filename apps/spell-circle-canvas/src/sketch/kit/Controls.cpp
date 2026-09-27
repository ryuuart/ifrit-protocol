#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigildraw/Constants.h>
#include <sigildraw/Pen.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/substance/Substance.h>
#include <sigilsketch/kit/Controls.h>
#include <sigilsketch/kit/Theme.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace sigil::sketch::kit {

using compose::Element;

/** One row's parameter, its live value, and where a drag stands. */
struct Controls::Row {
  Control control;
  motion::Animatable<float> value;
  /** A press began on this row's rail and has not been let go. */
  bool dragging = false;
  /** Whether the button was down at the last paint, so a press is read
   *  once, on the frame it goes down. */
  bool wasPressed = false;
};

namespace {

/** @p value moved onto @p control's steps and into its range. */
float settled(const Control& control, float value) {
  const float low = std::min(control.minimum, control.maximum);
  const float high = std::max(control.minimum, control.maximum);
  if (control.step > 0)
    value = control.minimum +
            std::round((value - control.minimum) / control.step) * control.step;
  return std::clamp(value, low, high);
}

std::string_view bytesOf(const compose::Utf8& words) {
  return {reinterpret_cast<const char*>(words.bytes().data()),
          words.bytes().size()};
}

/** The words a figure shows for @p control at @p value. */
std::string figureOf(const Control& control, float value) {
  switch (control.widget) {
    case Widget::Toggle:
      return value > 0.5f ? "on" : "off";
    case Widget::Choice:
      for (const Option& option : control.options)
        if (option.value == value) return std::string(bytesOf(option.label));
      break;
    case Widget::Slider:
      break;
  }
  if (control.step >= 1 && std::floor(control.step) == control.step)
    return compose::kit::formatted("%.0f", value);
  return compose::kit::formatted("%.3g", value);
}

/** THE PROGRAM ONE ROW PAINTS WITH: the label, the control and the figure,
 *  and the pointer read against the rail. */
compose::PaintProgram rowProgram(std::shared_ptr<Controls::Row> row,
                                 const Theme& look, const ControlsView& how) {
  return [row, look, how](draw::Pen& pen, const compose::PaintContext& context) {
    Controls::Row& state = *row;
    const Control& control = state.control;
    const float height = context.size.y;
    const float middle = height * 0.5f;
    const float railLeft = how.labelWidth;
    const float railRight =
        std::max(railLeft + 1, context.size.x - how.figureWidth -
                                   look.spacing.labelGap);
    const float railWidth = railRight - railLeft;

    // The pointer, in this row's own box.
    const glm::vec2 at = context.pointer.at;
    const bool pressed = context.pointer.pressed;
    const bool onRail =
        at.x >= railLeft && at.x <= railRight && at.y >= 0 && at.y <= height;
    const bool pressedNow = pressed && !state.wasPressed;
    const float along = std::clamp((at.x - railLeft) / railWidth, 0.0f, 1.0f);
    float value = state.value.value();
    switch (control.widget) {
      case Widget::Slider:
        if (pressedNow && onRail) state.dragging = true;
        if (!pressed) state.dragging = false;
        if (state.dragging)
          value = settled(control, control.minimum +
                                       along * (control.maximum - control.minimum));
        break;
      case Widget::Toggle:
        if (pressedNow && onRail) value = value > 0.5f ? 0.0f : 1.0f;
        break;
      case Widget::Choice:
        if (pressedNow && onRail && !control.options.empty()) {
          const std::size_t count = control.options.size();
          const std::size_t index =
              std::min(count - 1, (std::size_t)(along * (float)count));
          value = control.options[index].value;
        }
        break;
    }
    state.wasPressed = pressed;
    if (value != state.value.value()) state.value = value;

    pen.noStroke();
    pen.fill(look.palette.ash);
    pen.textFont(look.font(look.type.captionNote));
    pen.textAlign(draw::LEFT, draw::CENTER);
    pen.text(control.label.empty() ? std::string_view(control.name)
                                   : bytesOf(control.label),
             0, middle);

    const float bar = look.spacing.barHeight;
    switch (control.widget) {
      case Widget::Slider: {
        const float span = control.maximum - control.minimum;
        const float fraction =
            span != 0 ? std::clamp((value - control.minimum) / span, 0.0f, 1.0f)
                      : 0.0f;
        pen.fill(look.palette.cellGround);
        pen.rect(railLeft, middle - bar * 0.5f, railWidth, bar);
        pen.fill(look.palette.figure);
        pen.rect(railLeft, middle - bar * 0.5f, railWidth * fraction, bar);
        pen.circle(railLeft + railWidth * fraction, middle, height * 0.6f);
        break;
      }
      case Widget::Toggle: {
        const float side = height * 0.7f;
        pen.fill(value > 0.5f ? look.palette.figure : look.palette.cellGround);
        pen.rect(railLeft, middle - side * 0.5f, side, side);
        break;
      }
      case Widget::Choice: {
        const std::size_t count = std::max<std::size_t>(1, control.options.size());
        const float gap = 2;
        const float segment =
            (railWidth - gap * (float)(count - 1)) / (float)count;
        for (std::size_t index = 0; index < control.options.size(); ++index) {
          pen.fill(control.options[index].value == value
                       ? look.palette.figure
                       : look.palette.cellGround);
          pen.rect(railLeft + (segment + gap) * (float)index,
                   middle - bar * 0.5f, segment, bar);
        }
        break;
      }
    }

    pen.fill(look.palette.figure);
    pen.textFont(look.font(look.type.captionLabel));
    pen.textAlign(draw::RIGHT, draw::CENTER);
    pen.text(figureOf(control, value), context.size.x, middle);
  };
}

}  // namespace

std::vector<Control> controlsOf(const material::sbsar::Description& description) {
  using material::sbsar::InputType;
  using material::sbsar::Widget;
  std::vector<Control> rows;
  for (const material::sbsar::Input& input : description.inputs) {
    const bool integer = input.type == InputType::Integer;
    if (input.type != InputType::Float && !integer) continue;
    if (input.components() != 1) continue;
    Control row{.name = input.name,
                .label = compose::Utf8(input.label),
                .group = input.group,
                .value = input.defaultValue.empty() ? 0 : input.defaultValue[0]};
    if (!input.minimum.empty() && !input.maximum.empty() &&
        input.minimum[0] != input.maximum[0]) {
      row.minimum = input.minimum[0];
      row.maximum = input.maximum[0];
    }
    row.step = integer ? std::max(1.0f, input.step) : input.step;
    if (input.widget == Widget::Toggle) {
      row.widget = kit::Widget::Toggle;
      row.minimum = 0;
      row.maximum = 1;
      row.step = 1;
    } else if (input.widget == Widget::Buttons ||
               input.widget == Widget::Combobox) {
      row.widget = kit::Widget::Choice;
      for (const material::sbsar::Choice& choice : input.choices)
        row.options.push_back(
            {.value = (float)choice.value, .label = compose::Utf8(choice.label)});
    }
    rows.push_back(std::move(row));
  }
  return rows;
}

std::vector<Control> controlsOf(const material::Material& material) {
  std::vector<Control> rows;
  if (!material.hasProgram()) return rows;
  for (const material::Field& field : material.recipe().parameters().fields) {
    if (field.kind != material::ParameterType::Float) continue;
    rows.push_back(
        {.name = field.name, .value = material.get<float>(field.name)});
  }
  return rows;
}

Controls::Controls(std::vector<Control> parameters) {
  m_rows.reserve(parameters.size());
  for (Control& parameter : parameters) {
    auto row = std::make_shared<Row>();
    const float start = parameter.value;
    row->control = std::move(parameter);
    row->value = motion::animatable(settled(row->control, start));
    m_rows.push_back(std::move(row));
  }
}

Controls::Controls(const material::sbsar::Description& description)
    : Controls(controlsOf(description)) {}

Controls::Controls(const material::Material& material)
    : Controls(controlsOf(material)) {}

Controls& Controls::range(std::string_view name, float minimum, float maximum,
                          float step) {
  for (const std::shared_ptr<Row>& row : m_rows) {
    if (row->control.name != name) continue;
    row->control.minimum = minimum;
    row->control.maximum = maximum;
    row->control.step = step;
    row->value = settled(row->control, row->value.value());
  }
  return *this;
}

motion::Animatable<float> Controls::value(std::string_view name) const {
  for (const std::shared_ptr<Row>& row : m_rows)
    if (row->control.name == name) return row->value;
  return 0.0f;
}

void Controls::set(std::string_view name, float value) {
  for (const std::shared_ptr<Row>& row : m_rows)
    if (row->control.name == name) row->value = settled(row->control, value);
}

void Controls::bind(material::Material& material) const {
  for (const std::shared_ptr<Row>& row : m_rows)
    material.bind(row->control.name, row->value);
}

std::vector<Control> Controls::parameters() const {
  std::vector<Control> rows;
  rows.reserve(m_rows.size());
  for (const std::shared_ptr<Row>& row : m_rows) {
    rows.push_back(row->control);
    rows.back().value = row->value.value();
  }
  return rows;
}

Element controls(const Controls& surface, const ControlsView& how) {
  const Theme& look = theme();
  // The groups in the order the description first names them.
  std::vector<std::string> order;
  for (const std::shared_ptr<Controls::Row>& row : surface.m_rows)
    if (std::find(order.begin(), order.end(), row->control.group) == order.end())
      order.push_back(row->control.group);
  std::vector<Element> children;
  for (const std::string& group : order) {
    if (!how.groups.empty() &&
        std::find(how.groups.begin(), how.groups.end(), group) ==
            how.groups.end())
      continue;
    if (!group.empty())
      children.push_back(compose::document::h2(compose::Utf8(group))
                             .role("h2", look.font(look.type.section,
                                                   look.palette.ink)));
    for (const std::shared_ptr<Controls::Row>& row : surface.m_rows) {
      if (row->control.group != group) continue;
      children.push_back(compose::custom(rowProgram(row, look, how))
                             .width(how.width)
                             .height(how.rowHeight)
                             .cache(compose::Cache::None));
    }
  }
  return compose::box().column().gap(look.spacing.rowGap).children(
      std::move(children));
}

}  // namespace sigil::sketch::kit
