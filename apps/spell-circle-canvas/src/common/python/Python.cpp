#include <sigilpython/Python.h>
#include <sigilpython/compose/Registration.h>
#include <sigilpython/core/Registration.h>
#include <sigilpython/data/Registration.h>
#include <sigilpython/draw/Registration.h>
#include <sigilpython/geometry/Registration.h>
#include <sigilpython/io/Registration.h>
#include <sigilpython/media/Registration.h>
#include <sigilpython/material/Registration.h>
#include <sigilpython/measure/Registration.h>
#include <sigilpython/motion/Registration.h>
#include <sigilpython/scry/Registration.h>
#include <sigilpython/skia/Registration.h>
#include <sigilpython/usd/Registration.h>
#include <sigilpython/weave/Registration.h>
#include <sigilpython/world/Registration.h>

namespace sigil::python {

// THE ONE PLACE A REGISTRATION IS CALLED. A type has to be registered
// before any signature names it, or the declaration a generator reads
// carries the C++ spelling instead of the Python one, so the order here
// is the dependency order of the libraries and, within a library, of
// the packages. Adding a package adds one line.
void bindLibraries(pybind11::module_& module) {
  bindCore(module);
  bindColor(module);
  bindValues(module);
  bindRecordProtocol(module);
  bindCoreValues(module);
  bindCoreCompute(module);
  bindSkiaEffects(module);
  bindSkiaPaths(module);
  // Every binding that draws through Skia rather than through the pen
  // takes or answers this canvas, and several of them register here,
  // ahead of the pen the first canvas is lent by.
  bindDrawCanvasSeam(module);
  bindSkiaSurfaces(module);
  bindSkiaFonts(module);
  bindMediaImages(module);
  // A tween's fields take a staggered value and resolve at a place, so
  // the schedule vocabulary stands ahead of the values that read it.
  bindMotionSchedule(module);
  bindMotion(module);
  bindMotionSignals(module);
  bindMotionAnimatableForms(module);
  bindMotionClock(module);
  bindMotionLanes(module);
  bindMotionPhysics(module);
  bindMotionParticles(module);
  bindMaterial(module);
  bindMaterialCore(module);
  // The paint is the material a Skia backend draws, and a tile, a
  // stock draw and every pen verb answer one, so it stands ahead of
  // them rather than at the end of its own library.
  bindMaterialPaintEffect(module);
  // The light stands before the builder, whose surface() takes one.
  bindMaterialLighting(module);
  // The builder names the blend modes and filters the paint effects bind.
  bindMaterialBuilder(module);
  bindMaterialTexture(module);
  bindMaterialTextureSets(module);
  bindMaterialEnvironment(module);
  bindMaterialShading(module);
  bindMaterialPattern(module);
  bindMaterialField(module);
  bindMaterialSubstance(module);
  bindMaterialShader(module);
  bindWeavePorts(module);
  bindWeave(module);
  bindWeaveCascade(module);
  bindWeaveSelectorUnicode(module);
  bindWeaveFonts(module);
  bindWeaveTables(module);
  bindGeometryMeshCamera(module);
  bindGeometryMesh(module);
  bindGeometryMeshRender(module);
  bindGeometryMeshCurve(module);
  bindGeometryMeshPoints(module);
  bindGeometryPointOperations(module);
  bindGeometryPointBuilder(module);
  bindGeometryMeshCodec(module);
  bindGeometryPointKernels(module);
  bindGeometryDeviceHandle(module);
  bindGeometryPolylines(module);
  bindGeometryPathOperations(module);
  bindGeometryPathEditing(module);
  bindGeometryFrames(module);
  bindGeometryProfiles(module);
  bindGeometryOutlines(module);
  bindGeometryShapes(module);
  bindGeometryRegions(module);
  bindGeometryStructures(module);
  bindGeometryCharts(module);
  bindGeometrySeams(module);
  bindCompose(module);
  bindComposeSelectors(module);
  bindComposeElementEdges(module);
  bindComposeMasks(module);
  bindComposeTextEffects(module);
  bindComposeDerive(module);
  bindComposeInstancing(module);
  bindComposeFeed(module);
  bindComposeSchemes(module);
  bindComposeSheets(module);
  bindComposePaintPrograms(module);
  bindComposeComposer(module);
  bindComposeTextureScene(module);
  bindComposeDecorationSeam(module);
  bindComposeDecorationPrimitives(module);
  bindComposeBrushComposites(module);
  bindComposeBrushMarks(module);
  bindComposeLines(module);
  bindComposePixelStyles(module);
  bindPen(module);
  bindDrawStandalonePen(module);
  bindWeaveLayout(module);
  bindWeaveShaping(module);
  bindWeaveFlows(module);
  bindWeaveChoreography(module);
  bindWeaveKitPaintLayers(module);
  bindBrush(module);
  bindGeometry(module);
  bindWorld(module);
  bindWorldGeometry(module);
  bindWorldEnvironment(module);
  bindWorldDescription(module);
  bindWorldPasses(module);
  bindWorldView(module);
  bindWorldTargets(module);
  bindWorldPlan(module);
  bindWorldDevice(module);
  bindIO(module);
  bindIOFrames(module);
  bindData(module);
  bindDataConnection(module);
  bindMeasure(module);
  bindMediaVideo(module);
  bindOptionalLibraries(module);
#ifdef SIGIL_PYTHON_HAS_SCRY
  bindScry(module);
#endif
#ifdef SIGIL_PYTHON_HAS_USD
  bindUsd(module);
#endif
  bindComposeMediaLeaves(module);
  bindComposeKit(module);
  bindComposeKitRows(module);
  bindComposeKitTypeset(module);
  bindComposeKitAnnotations(module);
  bindComposeKitLegibility(module);
  bindComposeKitEras(module);
  bindComposeKitRoutes(module);
  bindComposeKitStrokes(module);
  bindComposeKitSprites(module);
  bindComposeKitPixelType(module);
}

}  // namespace sigil::python
