#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkVertices.h>
#include <include/effects/SkRuntimeEffect.h>
#include <include/pathops/SkPathOps.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilcore/compute/Chance.h>
#include <sigilimage/decode/Decode.h>
#include <sigilimage/encode/Encode.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/MotionBindings.h>
#include <sigilpython/ValueBindings.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

namespace sigil::python {
namespace py = pybind11;
namespace mskia = material::skia;
namespace pattern = material::pattern;
namespace {
constexpr auto fluent = py::return_value_policy::reference_internal;

std::vector<mskia::Stop> stops(py::iterable values) {
  std::vector<mskia::Stop> result;
  for (auto value : values) {
    const auto pair = py::cast<py::sequence>(value);
    if (pair.size() != 2)
      throw py::value_error("A gradient stop is (position, color).");
    result.push_back({py::cast<float>(pair[0]), color(pair[1])});
  }
  if (result.size() < 2)
    throw py::value_error("A gradient needs at least two stops.");
  return result;
}

sk_sp<SkRuntimeEffect> shader(const std::string& source) {
  auto [effect, error] = SkRuntimeEffect::MakeForShader(SkString(source));
  if (!effect) throw py::value_error(error.c_str());
  return effect;
}

mskia::Paint& uniform(mskia::Paint& paint, const std::string& name,
                      py::handle value) {
  if (py::isinstance<py::int_>(value) || py::isinstance<py::float_>(value))
    return paint.uniform(name, py::cast<float>(value));
  if (py::isinstance<SkColor4f>(value))
    return paint.uniform(name, color(value));
  const auto values = py::cast<std::vector<float>>(value);
  if (values.size() == 2)
    return paint.uniform(name, std::array<float, 2>{values[0], values[1]});
  if (values.size() == 4)
    return paint.uniform(
        name, std::array<float, 4>{values[0], values[1], values[2], values[3]});
  return paint.uniform(name, values);
}

mskia::Paint sksl(py::handle effect, py::dict uniforms) {
  auto paint =
      mskia::Paint::sksl(py::isinstance<py::str>(effect)
                             ? shader(py::cast<std::string>(effect))
                             : py::cast<sk_sp<SkRuntimeEffect>>(effect));
  for (const auto& [name, value] : uniforms)
    uniform(paint, py::cast<std::string>(name), value);
  return paint;
}

material::Color materialColor(py::handle value) {
  const auto c = color(value);
  return {c.fR, c.fG, c.fB, c.fA};
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

void bindCore(py::module_& module) {
  auto chance = module.def_submodule("core").def_submodule("chance");
  namespace chanceNative = core::chance;
  py::enum_<chanceNative::Source>(chance, "Source")
      .value("Pcg", chanceNative::Source::Pcg)
      .value("Mix64", chanceNative::Source::Mix64)
      .value("Xorshift", chanceNative::Source::Xorshift)
      .value("Halton", chanceNative::Source::Halton)
      .value("Sobol", chanceNative::Source::Sobol)
      .value("Golden", chanceNative::Source::Golden)
      .value("Stratified", chanceNative::Source::Stratified);
  using Stream = chanceNative::Stream;
  py::class_<Stream>(chance, "Stream")
      .def(py::init<>())
      .def_static("pcg", &Stream::pcg, py::arg("seed"))
      .def_static("mix64", &Stream::mix64, py::arg("seed"))
      .def_static("xorshift", &Stream::xorshift, py::arg("seed"))
      .def_static("halton", &Stream::halton, py::arg("base"),
                  py::arg("skip") = 0)
      .def_static("sobol", &Stream::sobol, py::arg("skip") = 0)
      .def_static("golden", &Stream::golden, py::arg("seed") = 0)
      .def_static("stratified", &Stream::stratified, py::arg("strata"),
                  py::arg("seed") = 0)
      .def_static("of", &Stream::of, py::arg("source"), py::arg("seed"),
                  py::arg("parameter") = 0)
      .def("copy", [](const Stream& self) { return self; })
      .def("__copy__", [](const Stream& self) { return self; })
      .def("bits", &Stream::bits)
      .def("unit", &Stream::unit)
      .def("signedUnit", &Stream::signedUnit)
      .def("range", &Stream::range, py::arg("low"), py::arg("high"))
      .def("below", &Stream::below, py::arg("upper"))
      .def("normal", &Stream::normal)
      .def("source", &Stream::source)
      .def("parameter", &Stream::parameter)
      .def("drawn", &Stream::drawn);
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
  py::class_<SkColor4f>(skia, "Color")
      .def(py::init([](float r, float g, float b, float a) {
             return SkColor4f{r, g, b, a};
           }),
           py::arg("r"), py::arg("g"), py::arg("b"), py::arg("a") = 1)
      .def(py::init([](py::object value) { return color(value); }),
           py::arg("value"))
      .def_readwrite("r", &SkColor4f::fR)
      .def_readwrite("g", &SkColor4f::fG)
      .def_readwrite("b", &SkColor4f::fB)
      .def_readwrite("a", &SkColor4f::fA);
  py::implicitly_convertible<py::tuple, SkColor4f>();
  py::implicitly_convertible<py::list, SkColor4f>();
  py::implicitly_convertible<py::str, SkColor4f>();
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

  auto images = module.def_submodule("image");
  images.def(
      "from_rgba",
      [](py::buffer buffer, int width, int height) {
        if (width < 1 || height < 1 || width > 16384 || height > 16384)
          throw py::value_error(
              "Image dimensions must be between one and 16384 pixels.");
        const auto source = buffer.request();
        if (source.itemsize != 1 ||
            source.size != static_cast<py::ssize_t>(width) * height * 4)
          throw py::value_error(
              "RGBA pixels need exactly four bytes per pixel.");
        py::ssize_t stride = 1;
        for (py::ssize_t axis = source.ndim; axis-- > 0;) {
          if (source.shape[axis] > 1 && source.strides[axis] != stride)
            throw py::value_error("RGBA pixels must be C-contiguous.");
          stride *= source.shape[axis];
        }
        const auto info = SkImageInfo::Make(
            width, height, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
        auto image = SkImages::RasterFromPixmapCopy(
            SkPixmap(info, source.ptr, info.minRowBytes()));
        if (!image)
          throw py::value_error("The pixel image could not be allocated.");
        return image;
      },
      py::arg("pixels"), py::arg("width"), py::arg("height"));
  py::enum_<image::Format>(images, "Format")
      .value("Png", image::Format::Png)
      .value("Jpeg", image::Format::Jpeg)
      .value("Webp", image::Format::Webp)
      .value("Exr", image::Format::Exr);
  py::class_<image::ImageAsset>(images, "ImageAsset")
      .def("width", &image::ImageAsset::width)
      .def("height", &image::ImageAsset::height)
      .def("animated", &image::ImageAsset::animated)
      .def("totalDurationMs", &image::ImageAsset::totalDurationMs)
      .def(
          "frameAt",
          [](const image::ImageAsset& asset, double milliseconds) {
            return asset.frameAt(milliseconds).image;
          },
          py::arg("milliseconds"));
  const auto decode = [](py::bytes encoded, int width, int height,
                         const std::string& hint) {
    const std::string bytes = encoded;
    auto decoded = image::decodeImage(
        reinterpret_cast<const std::byte*>(bytes.data()), bytes.size(),
        {.width = width, .height = height}, hint);
    if (!decoded) throw py::value_error("The image could not be decoded.");
    return *decoded;
  };
  images.def("decodeAsset", decode, py::arg("data"), py::arg("width") = 0,
             py::arg("height") = 0, py::arg("hint") = "");
  images.def(
      "decode",
      [decode](py::bytes data, int w, int h, const std::string& hint) {
        return decode(data, w, h, hint).frameAt(0).image;
      },
      py::arg("data"), py::arg("width") = 0, py::arg("height") = 0,
      py::arg("hint") = "");
  images.def(
      "encode",
      [](const SkImage& image, image::Format format, int quality) {
        auto data = image::encodeImage(image, format, {.quality = quality});
        if (!data) throw py::value_error("The image could not be encoded.");
        return py::bytes(static_cast<const char*>(data->data()), data->size());
      },
      py::arg("image"), py::arg("format") = image::Format::Png,
      py::arg("quality") = 100);

  auto materials = module.def_submodule("material");
  py::class_<material::Material>(materials, "Material")
      .def("copy", [](const material::Material& material) { return material; });
  auto fields = materials.def_submodule("field");
  fields.def("noise", &material::field::noise, py::arg("frequency"),
             py::arg("octaves") = 4, py::arg("seed") = 1,
             py::arg("turbulence") = false);
  fields.def("grain", &material::field::grain, py::arg("frequency"),
             py::arg("octaves") = 4, py::arg("seed") = 1,
             py::arg("contrast") = 1, py::arg("stretch") = 1);
  auto nativePaint = materials.def_submodule("skia");
  py::class_<mskia::Effect>(nativePaint, "Effect")
      .def_static(
          "recipe",
          py::overload_cast<const material::Material&>(&mskia::Effect::recipe),
          py::arg("material"))
      .def_static(
          "glow",
          [](py::object ink, float sigma) {
            return mskia::Effect::glow(color(ink), sigma);
          },
          py::arg("ink"), py::arg("sigma"))
      .def_static("brightPass", &mskia::Effect::brightPass,
                  py::arg("threshold") = 0.68f, py::arg("knee") = 0.30f)
      .def_static("phosphorBloom", &mskia::Effect::phosphorBloom,
                  py::arg("radius") = 9.0f, py::arg("threshold") = 0.52f,
                  py::arg("intensity") = 0.46f, py::arg("chroma") = 0.80f,
                  py::arg("hueDrift") = 0.0f, py::arg("tail") = 0.0f)
      .def_static(
          "shader", &mskia::Effect::shader, py::arg("effect"),
          py::arg("uniforms") = std::vector<std::pair<std::string, float>>{})
      .def_static("directionalBlur", &mskia::Effect::directionalBlur,
                  py::arg("sigma"), py::arg("angleDeg"),
                  py::arg("across") = 0.0f)
      .def_static("blur", &mskia::Effect::blur, py::arg("sigmaMap"),
                  py::arg("maxSigma"))
      .def("slot", &mskia::Effect::slot, py::arg("name"), py::arg("paint"),
           fluent)
      .def(
          "uniform",
          [](mskia::Effect& self, const std::string& name,
             py::object value) -> mskia::Effect& {
            if (py::isinstance<py::list>(value) ||
                py::isinstance<py::tuple>(value))
              return self.uniform(name, value.cast<std::vector<float>>());
            return self.uniform(name, motionAnimatable(value));
          },
          py::arg("name"), py::arg("value"), fluent)
      .def("then", &mskia::Effect::then, py::arg("effect"))
      .def("isAnimated", &mskia::Effect::isAnimated)
      .def("usesWorldSpace", &mskia::Effect::usesWorldSpace)
      .def(py::self == py::self);

  py::enum_<mskia::Fit>(nativePaint, "Fit")
      .value("Contain", mskia::Fit::Contain)
      .value("Cover", mskia::Fit::Cover)
      .value("Stretch", mskia::Fit::Stretch)
      .value("Native", mskia::Fit::Native);
  py::class_<mskia::Paint>(nativePaint, "Paint")
      .def(py::init<>())
      .def(py::init<const mskia::Paint&>(), py::arg("paint"))
      .def("copy", [](const mskia::Paint& p) { return p; })
      .def_static(
          "solid", [](py::handle c) { return mskia::Paint::solid(color(c)); },
          py::arg("color"))
      .def_static(
          "linear",
          [](py::handle a, py::handle b, py::iterable s, SkTileMode tile) {
            return mskia::Paint::linear(point(a), point(b), stops(s), tile);
          },
          py::arg("from_"), py::arg("to"), py::arg("stops"),
          py::arg("tile") = SkTileMode::kClamp)
      .def_static(
          "radial",
          [](py::handle c, float radius, py::iterable s, SkTileMode tile) {
            return mskia::Paint::radial(point(c), radius, stops(s), tile);
          },
          py::arg("center"), py::arg("radius"), py::arg("stops"),
          py::arg("tile") = SkTileMode::kClamp)
      .def_static(
          "conical",
          [](py::handle a, float ra, py::handle b, float rb, py::iterable s) {
            return mskia::Paint::conical(point(a), ra, point(b), rb, stops(s));
          },
          py::arg("start"), py::arg("startRadius"), py::arg("end"),
          py::arg("endRadius"), py::arg("stops"))
      .def_static(
          "sweep",
          [](py::handle c, py::iterable s, float start, float end) {
            return mskia::Paint::sweep(point(c), stops(s), start, end);
          },
          py::arg("center"), py::arg("stops"), py::arg("start") = 0,
          py::arg("end") = 360)
      .def_static(
          "linearUnit",
          [](py::handle a, py::handle b, py::iterable s) {
            return mskia::Paint::linearUnit(point(a), point(b), stops(s));
          },
          py::arg("start"), py::arg("end"), py::arg("stops"))
      .def_static(
          "radialUnit",
          [](py::handle c, float r, py::iterable s) {
            return mskia::Paint::radialUnit(point(c), r, stops(s));
          },
          py::arg("center"), py::arg("radius"), py::arg("stops"))
      .def_static(
          "glowUnit",
          [](py::handle c, float r, py::iterable s) {
            return mskia::Paint::glowUnit(point(c), r, stops(s));
          },
          py::arg("center"), py::arg("radius"), py::arg("stops"))
      .def_static(
          "image",
          [](sk_sp<SkImage> image, SkTileMode tx, SkTileMode ty,
             const SkMatrix& local) {
            return mskia::Paint::image(std::move(image), tx, ty, local);
          },
          py::arg("image"), py::arg("tileX") = SkTileMode::kClamp,
          py::arg("tileY") = SkTileMode::kClamp,
          py::arg("local") = SkMatrix::I())
      .def_static("sksl", &sksl, py::arg("effect"),
                  py::arg("uniforms") = py::dict())
      .def_static("recipe", &mskia::Paint::recipe, py::arg("material"))
      .def_static("blend", &mskia::Paint::blend, py::arg("layers"))
      .def("uniform", &uniform, py::arg("name"), py::arg("value"), fluent)
      .def("slot", &mskia::Paint::slot, py::arg("name"), py::arg("paint"),
           fluent)
      .def("amount", &mskia::Paint::amount, py::arg("amount"), fluent)
      .def("fit", &mskia::Paint::fit, py::arg("fit"), fluent)
      .def("worldSpace", py::overload_cast<bool>(&mskia::Paint::worldSpace),
           py::arg("on") = true, fluent)
      .def("quantizeTime", &mskia::Paint::quantizeTime, py::arg("hz"), fluent)
      .def("isAnimated", &mskia::Paint::isAnimated)
      .def("isNone", &mskia::Paint::isNone)
      .def(py::self == py::self);
  auto patterns = materials.def_submodule("pattern");
  py::class_<pattern::Tile>(patterns, "Tile")
      .def("seed", &pattern::Tile::seed, py::arg("seed"), fluent)
      .def("scale", py::overload_cast<float>(&pattern::Tile::scale),
           py::arg("factor"), fluent)
      .def("rotate", py::overload_cast<float>(&pattern::Tile::rotate),
           py::arg("degrees"), fluent)
      .def("offset", py::overload_cast<SkPoint>(&pattern::Tile::offset),
           py::arg("offset"), fluent)
      .def("image", &pattern::Tile::image)
      .def("paint", [](const pattern::Tile& tile) {
        return mskia::Paint::shader(tile.texture().shader());
      });
  patterns.def(
      "gridLines",
      [](float spacing, float width, py::handle c) {
        return pattern::gridLines(spacing, width, materialColor(c));
      },
      py::arg("spacing"), py::arg("width"), py::arg("color"));
  patterns.def(
      "stripes",
      [](float on, float off, py::handle c) {
        return pattern::stripes(on, off, materialColor(c));
      },
      py::arg("on"), py::arg("off"), py::arg("color"));
  patterns.def(
      "checker",
      [](float cell, py::handle a, py::handle b) {
        return pattern::checker(cell, materialColor(a), materialColor(b));
      },
      py::arg("cell"), py::arg("a"), py::arg("b"));
  patterns.def(
      "halftone",
      [](float spacing, float radius, py::handle c, bool staggered) {
        return pattern::halftone(spacing, radius, materialColor(c), staggered);
      },
      py::arg("spacing"), py::arg("radius"), py::arg("color"),
      py::arg("staggered") = true);

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
  auto length = py::class_<weave::Length>(weave, "Length");
  py::enum_<weave::Length::Unit>(length, "Unit")
      .value("Px", weave::Length::Unit::Px)
      .value("Em", weave::Length::Unit::Em)
      .value("Rem", weave::Length::Unit::Rem)
      .value("Lh", weave::Length::Unit::Lh);
  length.def(py::init<>())
      .def(py::init<float>(), py::arg("value"))
      .def(py::init<float, weave::Length::Unit>(), py::arg("value"),
           py::arg("unit"))
      .def_readwrite("value", &weave::Length::value)
      .def_readwrite("unit", &weave::Length::unit)
      .def("relative", &weave::Length::relative)
      .def(py::self == py::self);
  py::implicitly_convertible<py::float_, weave::Length>();
  py::implicitly_convertible<py::int_, weave::Length>();
  weave.def("em", &weave::em, py::arg("value"))
      .def("rem", &weave::rem, py::arg("value"))
      .def("lh", &weave::lh, py::arg("value"));
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
      .def("empty", &weave::Type::empty)
      .def("copy", [](const weave::Type& self) { return self; })
      .def(py::self == py::self);
}
}  // namespace sigil::python
