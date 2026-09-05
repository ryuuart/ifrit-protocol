#pragma once

/** @file
 * Instance-owned brush state over the stateless sampler and executor.
 */

#include <include/core/SkColor.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <sigildraw/Constants.h>
#include <sigildraw/brush/Catalogue.h>
#include <sigildraw/brush/Dab.h>
#include <sigildraw/brush/Deposit.h>
#include <sigildraw/brush/Field.h>
#include <sigildraw/brush/Hatch.h>
#include <sigildraw/brush/Mass.h>
#include <sigildraw/brush/Plot.h>
#include <sigildraw/brush/Polygon.h>
#include <sigildraw/brush/Position.h>
#include <sigildraw/brush/Sampler.h>
#include <sigildraw/brush/Stroke.h>
#include <sigildraw/brush/Tool.h>
#include <sigildraw/brush/Wash.h>

#include <boost/unordered/unordered_flat_map.hpp>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::draw {
class Pen;
}

namespace sigil::draw::brush {

/** A brush engine owns its catalogue, its selection, its interiors, its
 *  field, its clip and its in-progress stroke. Two engines draw through
 *  one pen without sharing any mutable state.
 *
 *  Angles: a scalar angle passed beside a pen — `flowLine`, `arc`,
 *  `move`, `endStroke`, the scalar `hatch` — is in the pen's angle mode.
 *  An angle inside a value (`Hatch`, `Wash`, `Plot`, a field) is radians.
 *  All of them are clockwise-positive on the y-down canvas, as the pen's
 *  `rotate` is. Time: a field is read at the pen's clock, `millis()`, on
 *  every verb that takes a pen. */
class Engine {
 public:
  Engine();
  explicit Engine(Catalogue catalogue);

  // ---- the catalogue and the selection --------------------------------------
  /** Adds a tool and answers it; an empty name answers null. */
  const Tool* add(std::string name, Tool tool);
  [[nodiscard]] std::vector<std::string> names() const;
  void scaleBrushes(float factor);
  /** Selects a tool by name and answers it; an unknown name answers null
   *  and changes nothing. */
  const Tool* pick(std::string_view name);
  /** Selects a tool, its colour and its weight in one word. */
  const Tool* set(std::string_view name, SkColor4f color, float weight = 1.0f);
  void stroke(SkColor4f color);
  void noStroke();
  void strokeWeight(float weight);
  [[nodiscard]] bool hasStroke() const;
  /** The tool the next stroke deposits with: the selected definition
   *  carrying the colour, with width and scatter scaled by the weight. */
  [[nodiscard]] Tool tool() const;

  // ---- the interiors --------------------------------------------------------
  /** The pigment wash and the flat wash are independent and can be
   *  active together. */
  void fill(SkColor4f color, float opacity = 150.0f / 255.0f);
  void noFill();
  void wash(SkColor4f color, float opacity = 150.0f / 255.0f);
  void noWash();
  /** The bleed angle is radians. */
  void fillBleed(float bleed, BleedDirection direction = BleedDirection::Out,
                 std::optional<float> angle = std::nullopt);
  void fillTexture(float texture = 0.4f, float border = 0.4f,
                   bool scatter = true);
  /** The hatch value as it stands: its angle is radians. */
  void hatch(const Hatch& style = {});
  /** The same from scalars, the angle in the pen's angle mode. */
  void hatch(const Pen& pen, float spacing, float angle, float jitter = 0.0f,
             float gradient = 0.0f, bool continuous = false);
  void noHatch();
  /** A dedicated hatch tool; until one is set the hatch uses the
   *  selected tool. */
  const Tool* hatchStyle(std::string_view name,
                         SkColor4f color = SkColors::kBlack,
                         float weight = 1.0f);
  const Tool* mass(std::string_view name, SkColor4f color,
                   const Mass& style = {});
  void noMass();

  // ---- the field ------------------------------------------------------------
  /** Adds a field under a name, answering whether the name and the units
   *  were taken; a field declared in DEGREES is wrapped to answer
   *  radians. */
  bool addField(std::string name, Direction field, Constant units = RADIANS);
  [[nodiscard]] std::vector<std::string> listFields() const;
  /** Selects a field, answering whether the name is known. */
  bool field(std::string_view name);
  void noField();
  /** Selects the `hand` field and scales its influence by @p amount. */
  void wiggle(float amount = 1.0f);

  // ---- the clip and the state ----------------------------------------------
  /** A rectangle every mark, interior and outline is confined to, in the
   *  pen's space at the time of the mark. */
  void clip(SkRect region);
  /** The same rectangle captured in the pen's space at this call, and
   *  applied in that space whatever the transform is later. A clip
   *  captured on one canvas is for that canvas. */
  void clip(const Pen& pen, SkRect region);
  void noClip();
  void push();
  void pop();

  // ---- strokes --------------------------------------------------------------
  /** Paints geometry with the current tool through the current field. */
  void paint(Pen& pen, std::span<const Sample> path) const;
  void line(Pen& pen, SkPoint from, SkPoint to, float startPressure = 1.0f,
            float endPressure = 1.0f) const;
  void flowLine(Pen& pen, SkPoint start, float length, float direction) const;
  /** Paints a curve through the controls and answers it as a plot placed
   *  at its first sample. The plot is the curve as sampled, before the
   *  field bends it. */
  PlacedPlot spline(Pen& pen, std::span<const Sample> controls,
                    float curvature = 0.5f) const;

  // ---- surfaces -------------------------------------------------------------
  /** Paints every active interior and the outline, in wash, fill, mass,
   *  hatch, outline order. */
  Polygon polygon(Pen& pen, std::span<const SkPoint> points) const;
  void polygon(Pen& pen, const Polygon& polygon) const;
  void hatchArray(Pen& pen, std::span<const Polygon> polygons) const;
  void hatchArray(Pen& pen, const Polygon& polygon) const;
  void massArray(Pen& pen, std::span<const Polygon> polygons) const;
  void massArray(Pen& pen, const Polygon& polygon) const;
  /** A rect in the mode named — p5's CORNER, CORNERS or CENTER. */
  void rect(Pen& pen, float x, float y, float width, float height,
            Constant mode = CORNER) const;
  void rect(Pen& pen, float x, float y, float width, float height,
            float radius) const;
  /** A circle of @p radius; irregularity roughens it. The plot answered
   *  is the circle as sampled, placed at its first point. */
  PlacedPlot circle(Pen& pen, float x, float y, float radius) const;
  PlacedPlot circle(Pen& pen, float x, float y, float radius,
                    float irregularity) const;
  /** An arc clockwise from @p start to @p stop, in the pen's angle mode;
   *  an empty or full sweep draws nothing and answers nothing. */
  std::optional<PlacedPlot> arc(Pen& pen, float x, float y, float radius,
                                float start, float stop) const;
  void beginShape(float curvature = 0.0f);
  void vertex(float x, float y, float pressure = 1.0f);
  /** Paints the shape and answers it as a plot placed at its first
   *  vertex; closed, the interiors and the outline share one boundary. */
  std::optional<PlacedPlot> endShape(Pen& pen, bool close = false);

  void draw(Pen& pen, const Polygon& polygon) const;
  void fill(Pen& pen, const Polygon& polygon) const;
  void wash(Pen& pen, const Polygon& polygon) const;
  void hatch(Pen& pen, const Polygon& polygon) const;
  void mass(Pen& pen, const Polygon& polygon) const;
  /** A plot placed at (@p x, @p y) and scaled. */
  void draw(Pen& pen, const Plot& plot, float x = 0.0f, float y = 0.0f,
            float scale = 1.0f) const;
  void fill(Pen& pen, const Plot& plot, float x = 0.0f, float y = 0.0f,
            float scale = 1.0f) const;
  void wash(Pen& pen, const Plot& plot, float x = 0.0f, float y = 0.0f,
            float scale = 1.0f) const;
  void hatch(Pen& pen, const Plot& plot, float x = 0.0f, float y = 0.0f,
             float scale = 1.0f) const;
  void mass(Pen& pen, const Plot& plot, float x = 0.0f, float y = 0.0f,
            float scale = 1.0f) const;
  /** A cursor through the current field. Without a pen the field's clock
   *  is zero and the cursor has no bounds; with one, the field is read
   *  at the pen's clock and the canvas bounds the walk. */
  [[nodiscard]] Position position(float x = 0.0f, float y = 0.0f) const;
  [[nodiscard]] Position position(const Pen& pen, float x = 0.0f,
                                  float y = 0.0f) const;

  // ---- live input -----------------------------------------------------------
  /** Streams absolute stylus or pointer observations through the sampler
   *  and deposits them as they arrive. Device pressure is used directly.
   *  Nothing is deposited before the first movement. */
  void beginInput(Pen& pen, Input input);
  void moveInput(Pen& pen, Input input);
  void endInput(Pen& pen, Input input);
  void cancelInput();

  // ---- relative strokes -----------------------------------------------------
  /** A stroke described by turns: each move appends a point at the angle
   *  and length, in the pen's angle mode; endStroke paints and answers the
   *  centreline. */
  void beginStroke(PlotType kind, SkPoint position);
  void move(const Pen& pen, float angle, float length, float pressure = 1.0f);
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
    std::optional<Tool> hatchTool;
    std::optional<Mass> mass;
    Tool massTool;
    std::string selectedField;
    float fieldAmount;
    std::optional<Clip> clip;
  };

  [[nodiscard]] const Tool* definition() const;
  [[nodiscard]] Tool tool(bool devicePressure) const;
  [[nodiscard]] Tool hatchTool() const;
  [[nodiscard]] const Direction* activeField() const;
  [[nodiscard]] Stroke shaped(const Pen& pen, std::span<const Sample> path,
                              bool applyField) const;
  [[nodiscard]] Stroke shapedBoundary(const Pen& pen,
                                      std::span<const Sample> corners,
                                      bool applyField) const;
  void paintStroke(Pen& pen, const Tool& tool, std::span<const Sample> path,
                   bool applyField) const;
  void paintBoundary(Pen& pen, std::span<const Sample> corners,
                     bool applyField, bool paintOutline) const;
  void paintPolygon(Pen& pen, std::span<const SkPoint> points,
                    bool applyField) const;
  [[nodiscard]] Polygon bent(const Pen& pen,
                             std::span<const SkPoint> points) const;
  void paintWash(Pen& pen, std::span<const SkPoint> points) const;
  void depositInput(Pen& pen, std::span<const Dab> sampled, bool last);
  void applyClip(Pen& pen) const;

  Catalogue m_catalogue;
  std::string m_selected = "HB";
  SkColor4f m_color{0, 0, 0, 1};
  float m_weight = 1.0f;
  bool m_strokeActive = false;
  Sampler m_sampler;
  std::optional<Tool> m_liveTool;
  bool m_liveDeposited = false;
  std::optional<Dab> m_liveLastDab;
  std::optional<Dab> m_liveLastSourceDab;
  std::optional<PlotType> m_strokeKind;
  Stroke m_stroke;
  Stroke m_shape;
  float m_shapeCurvature = 0.0f;
  Wash m_fill;
  bool m_fillActive = false;
  SkColor4f m_washColor{0, 0, 0, 1};
  float m_washOpacity = 150.0f / 255.0f;
  bool m_washActive = false;
  std::optional<Hatch> m_hatch;
  std::optional<Tool> m_hatchTool;
  std::optional<Mass> m_mass;
  Tool m_massTool;
  boost::unordered_flat_map<std::string, Direction, NameHash, std::equal_to<>>
      m_fields;
  std::string m_selectedField;
  float m_fieldAmount = 1.0f;
  std::optional<Clip> m_clip;
  std::vector<State> m_states;
};

}  // namespace sigil::draw::brush
