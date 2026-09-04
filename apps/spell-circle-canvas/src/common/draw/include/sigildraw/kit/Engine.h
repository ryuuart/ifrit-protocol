#pragma once

/** @file
 * Instance-owned brush state over the stateless sampler and dab executor.
 */

#include <include/core/SkMatrix.h>
#include <sigildraw/kit/Box.h>
#include <sigildraw/kit/Geometry.h>

#include <functional>
#include <optional>
#include <unordered_map>

namespace sigil::draw::brush {

enum class StrokeKind { Curve, Segments };
enum class RectMode { Corner, Center, Corners };

/** A brush engine owns its catalogue, selection and in-progress stroke. Two
 * engines may draw through one pen without sharing any mutable state. */
class Engine {
 public:
  using Field = std::function<float(SkPoint, float)>;

  Engine();
  explicit Engine(Box box);

  /** Selects the units accepted by public brush operations that take an
   * angle. Fields and stored Brush, Hatch, Wash and Plot values remain
   * normalized to radians. */
  bool angleMode(Constant mode);
  [[nodiscard]] Constant angleMode() const { return m_angleMode; }

  bool add(std::string name, Brush brush);
  [[nodiscard]] std::vector<std::string> box() const;
  void scaleBrushes(float factor);

  bool pick(std::string_view name);
  bool set(std::string_view name, SkColor4f color, float weight = 1.0f);
  void stroke(SkColor4f color);
  void noStroke();
  void strokeWeight(float weight);
  [[nodiscard]] bool hasStroke() const;
  [[nodiscard]] const Brush* definition() const;
  [[nodiscard]] Brush brush() const;

  void fill(SkColor4f color, float opacity = 150.0f / 255.0f);
  void noFill();
  void wash(SkColor4f color, float opacity = 150.0f / 255.0f);
  void noWash();
  void fillBleed(float bleed, BleedDirection direction = BleedDirection::Out,
                 std::optional<float> angle = std::nullopt);
  void fillTexture(float texture = 0.4f, float border = 0.4f,
                   bool scatter = true);
  bool hatch(Hatch style = {});
  void noHatch();
  bool hatchStyle(std::string_view name, SkColor4f color = SkColors::kBlack,
                  float weight = 1.0f);
  bool mass(std::string_view name, SkColor4f color, const Mass& style = {});
  void noMass();

  bool addField(std::string name, Field field, Constant units = RADIANS);
  [[nodiscard]] std::vector<std::string> listFields() const;
  bool field(std::string_view name);
  void noField();
  bool refreshField(float seconds = 0.0f);
  void wiggle(float amount = 1.0f);

  void clip(SkRect region);
  void clip(const Pen& pen, SkRect region);
  void noClip();
  void push();
  bool pop();

  /** Paints geometry with the current definition, colour and weight. */
  bool paint(Pen& pen, std::span<const Sample> path) const;
  bool line(Pen& pen, SkPoint from, SkPoint to, float startPressure = 1.0f,
            float endPressure = 1.0f) const;
  bool flowLine(Pen& pen, SkPoint start, float length, float direction) const;
  Plot spline(Pen& pen, std::span<const Sample> controls,
              float curvature = 0.5f) const;
  Polygon polygon(Pen& pen, std::span<const SkPoint> points) const;
  Polygon polygon(Pen& pen, const Polygon& polygon) const;
  bool hatchArray(Pen& pen, std::span<const Polygon> polygons) const;
  bool hatchArray(Pen& pen, const Polygon& polygon) const;
  bool massArray(Pen& pen, std::span<const Polygon> polygons) const;
  bool massArray(Pen& pen, const Polygon& polygon) const;
  bool rect(Pen& pen, float x, float y, float width, float height) const;
  bool rect(Pen& pen, float x, float y, float width, float height,
            RectMode mode) const;
  bool rect(Pen& pen, float x, float y, float width, float height,
            float radius) const;
  PlacedPlot circle(Pen& pen, float x, float y, float radius) const;
  PlacedPlot circle(Pen& pen, float x, float y, float radius,
                    float irregularity) const;
  std::optional<Plot> arc(Pen& pen, float x, float y, float radius, float start,
                          float stop) const;
  [[nodiscard]] Plot plot(PlotType type = PlotType::Curve) const {
    return Plot(type, m_angleMode);
  }
  void beginShape(float curvature = 0.0f);
  void vertex(float x, float y, float pressure = 1.0f);
  std::optional<Plot> endShape(Pen& pen, bool close = false);

  bool draw(Pen& pen, const Polygon& polygon) const;
  bool fill(Pen& pen, const Polygon& polygon) const;
  bool wash(Pen& pen, const Polygon& polygon) const;
  bool hatch(Pen& pen, const Polygon& polygon) const;
  bool mass(Pen& pen, const Polygon& polygon) const;
  bool draw(Pen& pen, const Plot& plot, float x = 0.0f, float y = 0.0f,
            float scale = 1.0f) const;
  bool fill(Pen& pen, const Plot& plot, float x = 0.0f, float y = 0.0f,
            float scale = 1.0f) const;
  bool wash(Pen& pen, const Plot& plot, float x = 0.0f, float y = 0.0f,
            float scale = 1.0f) const;
  bool hatch(Pen& pen, const Plot& plot, float x = 0.0f, float y = 0.0f,
             float scale = 1.0f) const;
  bool mass(Pen& pen, const Plot& plot, float x = 0.0f, float y = 0.0f,
            float scale = 1.0f) const;
  [[nodiscard]] Position position(float x = 0.0f, float y = 0.0f) const;
  [[nodiscard]] Position position(const Pen& pen, float x = 0.0f,
                                  float y = 0.0f) const;

  /** Streams absolute stylus or pointer observations through the sampler and
   * deposits them immediately. Device pressure is used directly. */
  bool beginInput(Pen& pen, Input input);
  bool moveInput(Pen& pen, Input input);
  bool endInput(Pen& pen, Input input);
  void cancelInput();

  /** Builds p5.brush's relative stroke form. Angles are radians and each move
   * appends a point at the given angle and length. endStroke paints and returns
   * the reusable centreline. */
  void beginStroke(StrokeKind kind, SkPoint position);
  bool move(float angle, float length, float pressure = 1.0f);
  [[nodiscard]] Stroke endStroke(Pen& pen, float angle, float pressure = 1.0f);
  void cancelStroke();

 private:
  struct Clip {
    SkRect region;
    std::optional<SkMatrix> transform;
  };

  struct State {
    std::string selected;
    SkColor4f color;
    float weight;
    bool strokeActive;
    Wash fill;
    bool fillActive;
    SkColor4f washColor;
    float washOpacity;
    bool washActive;
    std::optional<Hatch> hatch;
    std::optional<Brush> hatchBrush;
    std::optional<Mass> mass;
    Brush massBrush;
    std::string selectedField;
    float fieldSeconds;
    float fieldAmount;
    std::optional<Clip> clip;
    Constant angleMode;
  };

  [[nodiscard]] Brush current(bool devicePressure = false) const;
  [[nodiscard]] Stroke shaped(std::span<const Sample> path,
                              bool applyField) const;
  [[nodiscard]] std::vector<SkPoint> shapedPolygon(
      std::span<const SkPoint> points, bool applyField) const;
  bool paintStroke(Pen& pen, std::span<const Sample> path,
                   bool applyField) const;
  void depositInput(Pen& pen, std::span<const Dab> sampled,
                    DepositOptions options);
  void applyClip(Pen& pen) const;
  bool paintPolygon(Pen& pen, std::span<const SkPoint> points, bool applyField,
                    bool paintOutline = true) const;

  Box m_box;
  std::string m_selected = "HB";
  SkColor4f m_color{0, 0, 0, 1};
  float m_weight = 1.0f;
  bool m_strokeActive = false;
  Sampler m_sampler;
  std::optional<Brush> m_liveBrush;
  std::optional<Dab> m_liveLastDab;
  std::optional<Dab> m_liveLastSourceDab;
  std::optional<StrokeKind> m_strokeKind;
  Stroke m_stroke;
  Stroke m_shape;
  float m_shapeCurvature = 0.0f;
  Wash m_fill;
  bool m_fillActive = false;
  SkColor4f m_washColor{0, 0, 0, 1};
  float m_washOpacity = 150.0f / 255.0f;
  bool m_washActive = false;
  std::optional<Hatch> m_hatch;
  std::optional<Brush> m_hatchBrush;
  std::optional<Mass> m_mass;
  Brush m_massBrush;
  std::unordered_map<std::string, Field> m_fields;
  std::string m_selectedField;
  float m_fieldSeconds = 0.0f;
  float m_fieldAmount = 1.0f;
  std::optional<Clip> m_clip;
  Constant m_angleMode = RADIANS;
  std::vector<State> m_states;
};

}  // namespace sigil::draw::brush
