#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPicture.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkVertices.h>
#include <include/effects/SkRuntimeEffect.h>
#include <include/pathops/SkPathOps.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/skia/Registration.h>
#include <sigilpython/skia/Values.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

namespace sigil::python {
namespace py = pybind11;
namespace {
constexpr auto fluent = py::return_value_policy::reference_internal;

sk_sp<SkRuntimeEffect> shader(const std::string& source) {
  auto [effect, error] = SkRuntimeEffect::MakeForShader(SkString(source));
  if (!effect) throw py::value_error(error.c_str());
  return effect;
}
}  // namespace

SkPoint point(py::handle value) {
  if (py::isinstance<SkPoint>(value)) return py::cast<SkPoint>(value);
  const auto v = py::cast<std::array<float, 2>>(value);
  return {v[0], v[1]};
}

SkRect rect(py::handle value) {
  if (py::isinstance<SkRect>(value)) return py::cast<SkRect>(value);
  const auto v = py::cast<std::array<float, 4>>(value);
  return SkRect::MakeXYWH(v[0], v[1], v[2], v[3]);
}

void bindValues(py::module_& module) {
  auto skia = module.def_submodule("skia");
  py::class_<SkPicture, sk_sp<SkPicture>>(skia, "Picture")
      .def("cullRect", &SkPicture::cullRect)
      .def("uniqueID", &SkPicture::uniqueID);
  py::enum_<SkFilterMode>(skia, "FilterMode")
      .value("Nearest", SkFilterMode::kNearest)
      .value("Linear", SkFilterMode::kLinear);
  py::enum_<SkMipmapMode>(skia, "MipmapMode")
      .value("None_", SkMipmapMode::kNone)
      .value("Nearest", SkMipmapMode::kNearest)
      .value("Linear", SkMipmapMode::kLinear);
  py::class_<SkSamplingOptions>(skia, "SamplingOptions")
      .def(py::init<>())
      .def(py::init<SkFilterMode>(), py::arg("filter"))
      .def(py::init<SkFilterMode, SkMipmapMode>(), py::arg("filter"),
           py::arg("mipmap"));

  py::class_<SkPoint>(skia, "Point")
      .def(py::init([](float x, float y) { return SkPoint{x, y}; }),
           py::arg("x") = 0, py::arg("y") = 0)
      .def(py::init([](py::sequence p) { return point(p); }),
           py::arg("coordinates"))
      .def_readwrite("x", &SkPoint::fX)
      .def_readwrite("y", &SkPoint::fY)
      .def("length", &SkPoint::length)
      .def(
          "__add__", [](SkPoint a, SkPoint b) { return a + b; },
          py::is_operator(), py::arg("other"))
      .def(
          "__sub__", [](SkPoint a, SkPoint b) { return a - b; },
          py::is_operator(), py::arg("other"))
      .def(
          "__mul__",
          [](SkPoint a, float n) { return SkPoint{a.x() * n, a.y() * n}; },
          py::is_operator(), py::arg("factor"))
      .def(
          "__truediv__",
          [](SkPoint a, float n) {
            if (n == 0) throw py::value_error("Cannot divide a point by zero.");
            return SkPoint{a.x() / n, a.y() / n};
          },
          py::is_operator(), py::arg("factor"));
  py::implicitly_convertible<py::tuple, SkPoint>();
  py::implicitly_convertible<py::list, SkPoint>();
  py::class_<SkRect>(skia, "Rect")
      .def(py::init([](float x, float y, float w, float h) {
             return SkRect::MakeXYWH(x, y, w, h);
           }),
           py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"))
      .def(py::init([](py::sequence r) { return rect(r); }),
           py::arg("coordinates"))
      .def_static("MakeXYWH", &SkRect::MakeXYWH, py::arg("x"), py::arg("y"),
                  py::arg("width"), py::arg("height"))
      .def_static("MakeLTRB", &SkRect::MakeLTRB, py::arg("left"),
                  py::arg("top"), py::arg("right"), py::arg("bottom"))
      .def_static("MakeEmpty", &SkRect::MakeEmpty)
      .def_static("MakeWH", &SkRect::MakeWH, py::arg("width"),
                  py::arg("height"))
      .def("join", &SkRect::join, py::arg("rect"))
      .def("width", &SkRect::width)
      .def("height", &SkRect::height)
      .def("centerX", &SkRect::centerX)
      .def("centerY", &SkRect::centerY)
      .def("left", &SkRect::left)
      .def("top", &SkRect::top)
      .def("right", &SkRect::right)
      .def("bottom", &SkRect::bottom)
      .def("contains",
           py::overload_cast<float, float>(&SkRect::contains, py::const_),
           py::arg("x"), py::arg("y"));
  py::implicitly_convertible<py::tuple, SkRect>();
  py::implicitly_convertible<py::list, SkRect>();
  py::class_<SkSize>(skia, "Size")
      .def(py::init([](float w, float h) { return SkSize{w, h}; }),
           py::arg("width"), py::arg("height"))
      .def(py::init([](std::array<float, 2> v) { return SkSize{v[0], v[1]}; }),
           py::arg("dimensions"))
      .def("width", &SkSize::width)
      .def("height", &SkSize::height);
  py::implicitly_convertible<py::tuple, SkSize>();
  py::implicitly_convertible<py::list, SkSize>();
  py::enum_<SkTileMode>(skia, "TileMode")
      .value("Clamp", SkTileMode::kClamp)
      .value("Repeat", SkTileMode::kRepeat)
      .value("Mirror", SkTileMode::kMirror)
      .value("Decal", SkTileMode::kDecal);
  py::enum_<SkBlendMode>(skia, "BlendMode")
      .value("Clear", SkBlendMode::kClear)
      .value("Src", SkBlendMode::kSrc)
      .value("Dst", SkBlendMode::kDst)
      .value("SrcOver", SkBlendMode::kSrcOver)
      .value("DstOver", SkBlendMode::kDstOver)
      .value("SrcIn", SkBlendMode::kSrcIn)
      .value("DstIn", SkBlendMode::kDstIn)
      .value("SrcOut", SkBlendMode::kSrcOut)
      .value("DstOut", SkBlendMode::kDstOut)
      .value("SrcATop", SkBlendMode::kSrcATop)
      .value("DstATop", SkBlendMode::kDstATop)
      .value("Xor", SkBlendMode::kXor)
      .value("Plus", SkBlendMode::kPlus)
      .value("Modulate", SkBlendMode::kModulate)
      .value("Screen", SkBlendMode::kScreen)
      .value("Overlay", SkBlendMode::kOverlay)
      .value("Darken", SkBlendMode::kDarken)
      .value("Lighten", SkBlendMode::kLighten)
      .value("ColorDodge", SkBlendMode::kColorDodge)
      .value("ColorBurn", SkBlendMode::kColorBurn)
      .value("HardLight", SkBlendMode::kHardLight)
      .value("SoftLight", SkBlendMode::kSoftLight)
      .value("Difference", SkBlendMode::kDifference)
      .value("Exclusion", SkBlendMode::kExclusion)
      .value("Multiply", SkBlendMode::kMultiply)
      .value("Hue", SkBlendMode::kHue)
      .value("Saturation", SkBlendMode::kSaturation)
      .value("Color", SkBlendMode::kColor)
      .value("Luminosity", SkBlendMode::kLuminosity);
  py::enum_<SkPathFillType>(skia, "PathFillType")
      .value("Winding", SkPathFillType::kWinding)
      .value("EvenOdd", SkPathFillType::kEvenOdd)
      .value("InverseWinding", SkPathFillType::kInverseWinding)
      .value("InverseEvenOdd", SkPathFillType::kInverseEvenOdd);
  py::enum_<SkPathDirection>(skia, "PathDirection")
      .value("CW", SkPathDirection::kCW)
      .value("CCW", SkPathDirection::kCCW);
  py::class_<SkMatrix>(skia, "Matrix")
      .def(py::init([] { return SkMatrix::I(); }))
      .def_static("Translate",
                  py::overload_cast<float, float>(&SkMatrix::Translate),
                  py::arg("x"), py::arg("y"))
      .def_static("Scale", &SkMatrix::Scale, py::arg("x"), py::arg("y"))
      .def_static("RotateDeg", py::overload_cast<float>(&SkMatrix::RotateDeg),
                  py::arg("degrees"))
      .def("postTranslate", &SkMatrix::postTranslate, py::arg("x"),
           py::arg("y"), fluent)
      .def("postScale", py::overload_cast<float, float>(&SkMatrix::postScale),
           py::arg("x"), py::arg("y"), fluent)
      .def("postRotate", py::overload_cast<float>(&SkMatrix::postRotate),
           py::arg("degrees"), fluent);
  py::class_<SkPath>(skia, "Path")
      .def(py::init<>())
      .def("isEmpty", &SkPath::isEmpty)
      .def("getBounds", &SkPath::getBounds, py::return_value_policy::copy)
      .def("contains",
           py::overload_cast<float, float>(&SkPath::contains, py::const_),
           py::arg("x"), py::arg("y"))
      .def_static(
          "Rect", [](py::handle r) { return SkPath::Rect(rect(r)); },
          py::arg("rect"))
      .def_static(
          "Oval", [](py::handle r) { return SkPath::Oval(rect(r)); },
          py::arg("rect"))
      .def_static(
          "Circle",
          [](float x, float y, float r) { return SkPath::Circle(x, y, r); },
          py::arg("x"), py::arg("y"), py::arg("radius"))
      .def(
          "offset",
          [](const SkPath& p, float x, float y) {
            return SkPathBuilder(p).offset(x, y).detach();
          },
          py::arg("x"), py::arg("y"))
      .def(
          "transform",
          [](const SkPath& p, const SkMatrix& m) {
            return SkPathBuilder(p).transform(m).detach();
          },
          py::arg("matrix"));
  auto builder =
      py::class_<SkPathBuilder>(skia, "PathBuilder")
          .def(py::init<>())
          .def(py::init<const SkPath&>(), py::arg("path"))
          .def("moveTo",
               py::overload_cast<float, float>(&SkPathBuilder::moveTo),
               py::arg("x"), py::arg("y"), fluent)
          .def("moveTo", py::overload_cast<SkPoint>(&SkPathBuilder::moveTo),
               py::arg("point"), fluent)
          .def("lineTo",
               py::overload_cast<float, float>(&SkPathBuilder::lineTo),
               py::arg("x"), py::arg("y"), fluent)
          .def("lineTo", py::overload_cast<SkPoint>(&SkPathBuilder::lineTo),
               py::arg("point"), fluent)
          .def("quadTo",
               py::overload_cast<float, float, float, float>(
                   &SkPathBuilder::quadTo),
               py::arg("x1"), py::arg("y1"), py::arg("x2"), py::arg("y2"),
               fluent)
          .def("conicTo",
               py::overload_cast<float, float, float, float, float>(
                   &SkPathBuilder::conicTo),
               py::arg("x1"), py::arg("y1"), py::arg("x2"), py::arg("y2"),
               py::arg("weight"), fluent)
          .def("cubicTo",
               py::overload_cast<float, float, float, float, float, float>(
                   &SkPathBuilder::cubicTo),
               py::arg("x1"), py::arg("y1"), py::arg("x2"), py::arg("y2"),
               py::arg("x3"), py::arg("y3"), fluent)
          .def("close", &SkPathBuilder::close, fluent)
          .def("reset", &SkPathBuilder::reset, fluent)
          .def("setFillType", &SkPathBuilder::setFillType, py::arg("fillType"),
               fluent)
          .def("setIsVolatile", &SkPathBuilder::setIsVolatile,
               py::arg("isVolatile"), fluent)
          .def(
              "addRect",
              [](SkPathBuilder& b, py::handle r, SkPathDirection dir)
                  -> SkPathBuilder& { return b.addRect(rect(r), dir); },
              py::arg("rect"), py::arg("direction") = SkPathDirection::kCW,
              fluent)
          .def(
              "addOval",
              [](SkPathBuilder& b, py::handle r) -> SkPathBuilder& {
                return b.addOval(rect(r));
              },
              py::arg("rect"), fluent)
          .def(
              "addCircle",
              [](SkPathBuilder& b, float x, float y,
                 float r) -> SkPathBuilder& { return b.addCircle(x, y, r); },
              py::arg("x"), py::arg("y"), py::arg("radius"), fluent)
          .def(
              "addArc",
              [](SkPathBuilder& b, py::handle r, float start, float sweep)
                  -> SkPathBuilder& { return b.addArc(rect(r), start, sweep); },
              py::arg("rect"), py::arg("start"), py::arg("sweep"), fluent)
          .def(
              "addPolygon",
              [](SkPathBuilder& b, const std::vector<SkPoint>& points,
                 bool close) -> SkPathBuilder& {
                return b.addPolygon(points, close);
              },
              py::arg("points"), py::arg("close") = true, fluent)
          .def(
              "addPath",
              [](SkPathBuilder& b, const SkPath& p) -> SkPathBuilder& {
                return b.addPath(p);
              },
              py::arg("path"), fluent)
          .def("offset", &SkPathBuilder::offset, py::arg("x"), py::arg("y"),
               fluent)
          .def("transform", &SkPathBuilder::transform, py::arg("matrix"),
               fluent)
          .def("detach", [](SkPathBuilder& b) { return b.detach(); })
          .def("snapshot", [](const SkPathBuilder& b) { return b.snapshot(); });
  py::enum_<SkPathOp>(skia, "PathOp")
      .value("Difference", kDifference_SkPathOp)
      .value("Intersect", kIntersect_SkPathOp)
      .value("Union", kUnion_SkPathOp)
      .value("Xor", kXOR_SkPathOp)
      .value("ReverseDifference", kReverseDifference_SkPathOp);
  skia.def(
      "pathOp",
      [](const SkPath& a, const SkPath& b, SkPathOp operation) {
        auto result = Op(a, b, operation);
        if (!result)
          throw py::value_error("The path operation could not be evaluated.");
        return *result;
      },
      py::arg("a"), py::arg("b"), py::arg("operation"));
  py::enum_<SkPaint::Style>(skia, "PaintStyle")
      .value("Fill", SkPaint::kFill_Style)
      .value("Stroke", SkPaint::kStroke_Style)
      .value("StrokeAndFill", SkPaint::kStrokeAndFill_Style);
  py::enum_<SkPaint::Cap>(skia, "StrokeCap")
      .value("Butt", SkPaint::kButt_Cap)
      .value("Round", SkPaint::kRound_Cap)
      .value("Square", SkPaint::kSquare_Cap);
  py::enum_<SkPaint::Join>(skia, "StrokeJoin")
      .value("Miter", SkPaint::kMiter_Join)
      .value("Round", SkPaint::kRound_Join)
      .value("Bevel", SkPaint::kBevel_Join);
  py::class_<SkPaint>(skia, "Paint")
      .def(py::init<>())
      .def(py::init<const SkPaint&>(), py::arg("paint"))
      .def(
          "setColor", [](SkPaint& p, py::handle c) { p.setColor(color(c)); },
          py::arg("color"))
      .def("setAlphaf", &SkPaint::setAlphaf, py::arg("alpha"))
      .def("setAntiAlias", &SkPaint::setAntiAlias, py::arg("enabled"))
      .def("setStyle", &SkPaint::setStyle, py::arg("style"))
      .def("setStrokeWidth", &SkPaint::setStrokeWidth, py::arg("width"))
      .def("setStrokeCap", &SkPaint::setStrokeCap, py::arg("cap"))
      .def("setStrokeJoin", &SkPaint::setStrokeJoin, py::arg("join"))
      .def("setBlendMode", &SkPaint::setBlendMode, py::arg("mode"));
  py::enum_<SkVertices::VertexMode>(skia, "VertexMode")
      .value("Triangles", SkVertices::kTriangles_VertexMode)
      .value("TriangleStrip", SkVertices::kTriangleStrip_VertexMode)
      .value("TriangleFan", SkVertices::kTriangleFan_VertexMode);
  py::class_<SkVertices, sk_sp<SkVertices>>(skia, "Vertices")
      .def_static(
          "MakeCopy",
          [](SkVertices::VertexMode mode, const std::vector<SkPoint>& positions,
             const std::vector<SkPoint>& tex,
             const std::vector<SkColor4f>& colors,
             const std::vector<uint16_t>& indices) {
            if ((!tex.empty() && tex.size() != positions.size()) ||
                (!colors.empty() && colors.size() != positions.size()))
              throw py::value_error(
                  "Vertex attributes must match the position count.");
            for (auto index : indices)
              if (index >= positions.size())
                throw py::value_error(
                    "A vertex index is outside the position array.");
            std::vector<SkColor> packed;
            for (auto c : colors) packed.push_back(c.toSkColor());
            return SkVertices::MakeCopy(
                mode, static_cast<int>(positions.size()), positions.data(),
                tex.empty() ? nullptr : tex.data(),
                packed.empty() ? nullptr : packed.data(),
                static_cast<int>(indices.size()),
                indices.empty() ? nullptr : indices.data());
          },
          py::arg("mode"), py::arg("positions"),
          py::arg("texCoords") = std::vector<SkPoint>{},
          py::arg("colors") = std::vector<SkColor4f>{},
          py::arg("indices") = std::vector<uint16_t>{});
  py::class_<SkImage, sk_sp<SkImage>>(skia, "Image")
      .def("width", &SkImage::width)
      .def("height", &SkImage::height)
      .def("rgba", [](const SkImage& image) {
        const auto info =
            SkImageInfo::Make(image.width(), image.height(),
                              kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
        std::string bytes(info.computeMinByteSize(), '\0');
        if (!image.readPixels(nullptr, info, bytes.data(), info.minRowBytes(),
                              0, 0))
          throw py::value_error(
              "The image pixels are not readable on the CPU.");
        return py::bytes(bytes);
      });
  py::class_<SkRuntimeEffect, sk_sp<SkRuntimeEffect>>(skia, "RuntimeEffect")
      .def_static("MakeForShader", &shader, py::arg("source"));

  auto weave = module.def_submodule("weave");
  py::class_<SkTypeface, sk_sp<SkTypeface>>(skia, "Typeface")
      .def("familyName", [](const SkTypeface& face) {
        SkString name;
        face.getFamilyName(&name);
        return std::string(name.c_str());
      });
  weave.def(
      "typeface",
      [](const std::string& family, int weight, bool italic) {
        return weave::ports::systemFontManager()->matchFamilyStyle(
            family.c_str(), SkFontStyle(weight, SkFontStyle::kNormal_Width,
                                        italic ? SkFontStyle::kItalic_Slant
                                               : SkFontStyle::kUpright_Slant));
      },
      py::arg("family"), py::arg("weight") = 400, py::arg("italic") = false);
  py::enum_<weave::Keyword>(weave, "Keyword")
      .value("Inherit", weave::Keyword::Inherit)
      .value("Initial", weave::Keyword::Initial)
      .value("Unset", weave::Keyword::Unset);
  auto length = py::class_<weave::Length>(weave, "Length");
  py::enum_<weave::Length::Unit>(length, "Unit")
      .value("Px", weave::Length::Unit::Px)
      .value("Em", weave::Length::Unit::Em)
      .value("Rem", weave::Length::Unit::Rem)
      .value("Lh", weave::Length::Unit::Lh)
      .value("Ch", weave::Length::Unit::Ch)
      .value("Pt", weave::Length::Unit::Pt);
  length.def(py::init<>())
      .def(py::init<float>(), py::arg("value"))
      .def(py::init<float, weave::Length::Unit>(), py::arg("value"),
           py::arg("unit"))
      .def_readwrite("value", &weave::Length::value)
      .def_readwrite("unit", &weave::Length::unit)
      .def("relative", &weave::Length::relative)
      .def("absolutePx", &weave::Length::absolutePx)
      .def(py::self == py::self);
  py::implicitly_convertible<py::float_, weave::Length>();
  py::implicitly_convertible<py::int_, weave::Length>();
  weave.def("em", &weave::em, py::arg("value"))
      .def("rem", &weave::rem, py::arg("value"))
      .def("lh", &weave::lh, py::arg("value"))
      .def("ch", &weave::ch, py::arg("value"))
      .def("pt", &weave::pt, py::arg("value"));
  py::class_<weave::Type>(weave, "Type")
      .def(py::init([](py::kwargs kwargs) {
        return keywordValue<weave::Type>(kwargs, "Unknown Type field: ");
      }))
      .def_readwrite("face", &weave::Type::face)
      .def_property(
          "size", [](const weave::Type& self) { return self.size; },
          [](weave::Type& self, std::optional<weave::Length> value) {
            self.size = std::move(value);
          })
      .def_property(
          "track", [](const weave::Type& self) { return self.track; },
          [](weave::Type& self, std::optional<weave::Length> value) {
            self.track = std::move(value);
          })
      .def_property(
          "color", [](const weave::Type& self) { return self.color; },
          [](weave::Type& self, py::object value) {
            self.color =
                value.is_none() ? std::nullopt : std::optional{color(value)};
          })
      .def_readwrite("condense", &weave::Type::condense)
      .def_readwrite("weight", &weave::Type::weight)
      .def_readwrite("slant", &weave::Type::slant)
      .def_readwrite("aliased", &weave::Type::aliased)
      .def_readwrite("antiAlias", &weave::Type::antiAlias)
      .def_readwrite("color8", &weave::Type::color8)
      .def_property(
          "variations", [](const weave::Type& self) { return self.variations; },
          [](weave::Type& self, std::vector<weave::FontVariation> value) {
            self.variations = std::move(value);
          })
      .def_readwrite("language", &weave::Type::language)
      .def_property(
          "features", [](const weave::Type& self) { return self.features; },
          [](weave::Type& self,
             std::optional<std::vector<weave::FontFeature>> value) {
            self.features = std::move(value);
          })
      .def_readwrite("opticalKerning", &weave::Type::opticalKerning)
      .def_property(
          "wordSpacing",
          [](const weave::Type& self) { return self.wordSpacing; },
          [](weave::Type& self, std::optional<weave::Length> value) {
            self.wordSpacing = std::move(value);
          })
      .def_readwrite("textTransform", &weave::Type::textTransform)
      .def_readwrite("verticalForm", &weave::Type::verticalForm)
      .def_property(
          "decorations",
          [](const weave::Type& self) { return self.decorations; },
          [](weave::Type& self,
             std::optional<std::vector<weave::Decoration>> value) {
            self.decorations = std::move(value);
          })
      .def_property(
          "underlays", [](const weave::Type& self) { return self.underlays; },
          [](weave::Type& self,
             std::optional<std::vector<weave::PaintLayer>> value) {
            self.underlays = std::move(value);
          })
      .def_property(
          "overlays", [](const weave::Type& self) { return self.overlays; },
          [](weave::Type& self,
             std::optional<std::vector<weave::PaintLayer>> value) {
            self.overlays = std::move(value);
          })
      .def("empty", &weave::Type::empty)
      .def("copy", [](const weave::Type& self) { return self; })
      .def(py::self == py::self);
}
}  // namespace sigil::python
