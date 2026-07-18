#pragma once

// Qt-free SpellCircle scene document shared by the Qt receiver app and the
// native macOS app: entt components mirroring the SpellCircle.fbs schema,
// plus FlatBuffers payload verification and decoding into a registry.
// Feed/logging presentation stays in each application; this layer only
// translates packet data into entities/components.

#include <entt/entt.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

namespace spellcircle {

/** Mirrors SpellCircle.fbs `Circle` — a named circle at an author-space
 *  position, as decoded from the packet. Scaling to native coordinates and
 *  all other positioning math is left to resolveScene(). */
struct CircleComponent {
  std::string name; // UTF-8
  float centerX = 0.0f;
  float centerY = 0.0f;
  uint32_t radius = 0;
  float textStart =
      0.0f;            // fraction [0, 1] where the label begins along the path
  float active = 0.0f; // background fill alpha/intensity [0, 1]; 0 = no fill
};

/** Mirrors SpellCircle.fbs `Point` — a fractional position along a circle's
 *  perimeter. The wire format embeds a full Circle per Point rather than
 *  referencing Scene.circles, so `circle` is a value copy used only to
 *  resolve this point's position. A Point referenced by more than one
 *  Edge/Box in the same packet (FlatBuffers is zero-copy, so this shows up
 *  as the same underlying Point table under multiple fields) gets one
 *  entity, shared via the entt::entity handles below. */
struct PointComponent {
  std::string value; // UTF-8
  CircleComponent circle;
  float position = 0.0f; // fraction [0, 1] clockwise from 12 o'clock
};

/** Mirrors SpellCircle.fbs `Edge` — a connector between two Point entities,
 *  which may or may not be distinct from other Edges'/Boxes' Point entities
 *  (see PointComponent). */
struct EdgeComponent {
  entt::entity first = entt::null;
  entt::entity second = entt::null;
};

/** Mirrors SpellCircle.fbs `Box` — a labelled box anchored to a Point
 *  entity, which may be shared with other Edges/Boxes (see PointComponent).
 */
struct BoxComponent {
  std::string value; // UTF-8
  entt::entity point = entt::null;
  float active = 0.0f; // background fill alpha/intensity [0, 1]; 0 = no fill
};

/** Entity counts from the most recent decode, for feed/log presentation. */
struct SceneStats {
  int circles = 0;
  int edges = 0;
  int boxes = 0;

  bool hasGeometry() const { return circles > 0 || edges > 0 || boxes > 0; }
};

/** Returns whether @p payload is a structurally valid FlatBuffers Scene.
 *  Callers must verify before decode() — decoding an unverified buffer is
 *  undefined behavior. */
bool verifyScenePayload(const void *payload, size_t size);

/**
 * The decoded scene: an entt::registry of Circle/Point/Edge/Box components
 * plus the author-space canvas dimensions from the packet's Scene table.
 */
class SceneDocument {
public:
  /** Scene entities decoded from the most recently parsed packet. */
  const entt::registry &registry() const { return m_registry; }

  /** Author-space canvas dimensions from the most recently parsed Scene
   *  (Scene.width/height); 0 means coordinates are already native. */
  float sceneWidth() const { return m_sceneWidth; }
  float sceneHeight() const { return m_sceneHeight; }

  /** Replaces the current registry with entities parsed from @p payload,
   *  which must already have passed verifyScenePayload(). */
  SceneStats decode(const void *payload, size_t size);

  /** Removes all scene entities and resets the author-space dimensions. */
  void clear();

private:
  entt::registry m_registry;
  float m_sceneWidth = 0.0f;
  float m_sceneHeight = 0.0f;
};

} // namespace spellcircle
