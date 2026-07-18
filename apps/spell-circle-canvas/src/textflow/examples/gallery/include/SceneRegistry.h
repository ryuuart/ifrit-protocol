#pragma once

// Scene self-registration for the TextFlow gallery. Each scene .cpp
// declares everything the GUI needs — display name, default text, typed
// parameters, an optional per-scene controls QML file — in one
// SceneDescriptor next to the scene class, and registers it with
// REGISTER_GALLERY_SCENE. Nothing else is edited to add a scene: the
// sidebar builds its controls from the descriptor (generic delegates, or
// the scene's own QML when it names one), and the render thread constructs
// instances through the descriptor's factory.

#include "GalleryScenes.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariant>

#include <functional>
#include <memory>
#include <vector>

namespace gallery {

/** One typed, ranged control a scene exposes to the sidebar. */
struct SceneParameter {
  enum class Type { kBool, kFloat, kInt, kChoice };

  QString id;    ///< stable key into SceneParams::values
  QString label; ///< sidebar text
  Type type = Type::kFloat;
  QVariant defaultValue;
  double minimum = 0; ///< numeric types only
  double maximum = 1; ///< numeric types only
  QString suffix;     ///< value readout suffix, e.g. "px", "×"
  QStringList choices; ///< kChoice: display strings; value = index
};

/** Everything the GUI needs to present a scene without instantiating it. */
struct SceneDescriptor {
  QString name;
  /// Initial editable text; empty → the scene owns its text.
  QString defaultText;
  bool textEditable = true;
  QList<SceneParameter> parameters;
  /// Optional scene-owned controls QML. When set, the sidebar loads this
  /// instead of auto-building generic delegates from `parameters`. By
  /// convention scenes place the file in the gallery QML module and use
  ///   QUrl("qrc:/qt/qml/TextFlow/Gallery/<Name>Controls.qml")
  QUrl controlsQml;
  /// Constructs the render-thread instance (called on the render thread).
  std::function<std::unique_ptr<Scene>()> make;
  /// Sort key for the scene switcher; ties break by name.
  int displayOrder = 0;
};

/** Returns every registered scene, sorted by displayOrder. */
const std::vector<SceneDescriptor> &sceneRegistry();

namespace detail {
/// Static-init hook behind REGISTER_GALLERY_SCENE. Safe because every
/// scene TU is compiled directly into the gallery executable (never
/// archived into a static library where the linker could drop it).
struct SceneRegistrar {
  explicit SceneRegistrar(SceneDescriptor descriptor);
};
} // namespace detail

#define GALLERY_SCENE_CONCAT_INNER(a, b) a##b
#define GALLERY_SCENE_CONCAT(a, b) GALLERY_SCENE_CONCAT_INNER(a, b)
/** Registers a SceneDescriptor at static-initialization time. */
#define REGISTER_GALLERY_SCENE(...)                                            \
  static const ::gallery::detail::SceneRegistrar GALLERY_SCENE_CONCAT(         \
      gallerySceneRegistrar_, __COUNTER__){__VA_ARGS__};

} // namespace gallery
