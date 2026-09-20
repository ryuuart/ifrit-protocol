#pragma once

/** @file
 * The registrations that let QML tooling see the two objects the
 * application owns rather than creates, so a property bound to one is
 * typed rather than a bare QObject.
 */

#include <QObject>
#include <QtQml/QtQml>

#include "GraphicsConfig.h"
#include "SpellCircleModel.h"

/** QML tooling registration for the application-owned scene model. */
struct SpellCircleModelQmlType {
  Q_GADGET
  QML_FOREIGN(SpellCircleModel)
  QML_ANONYMOUS
};

/** QML tooling registration for the application-owned graphics settings. */
struct GraphicsConfigQmlType {
  Q_GADGET
  QML_FOREIGN(GraphicsConfig)
  QML_ANONYMOUS
};

/** QML tooling registration for the grouped box-style property. */
struct BoxStyleConfigQmlType {
  Q_GADGET
  QML_FOREIGN(BoxStyleConfig)
  QML_ANONYMOUS
};

/** QML tooling registration for the grouped canvas-size property. */
struct CanvasSizeConfigQmlType {
  Q_GADGET
  QML_FOREIGN(CanvasSizeConfig)
  QML_ANONYMOUS
};
