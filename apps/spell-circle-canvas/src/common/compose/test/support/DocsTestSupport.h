#pragma once
// Support for compose_docs_test: the engine walkthroughs and the generated
// README probes. The probe TU includes every compose header itself, so a
// documented name is never reported missing merely because the harness
// left out the file that owns it.

#include <include/core/SkPathBuilder.h>
#include <include/core/SkString.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/Brushes.h>
#include <sigilcompose/Decorations.h>
#include <sigilcompose/Instances.h>
#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Layouts.h>
#include <sigilcompose/Lines.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>
#include <sigilcompose/TextFx.h>
#include <sigilcompose/Util.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilimage/ImageAsset.h>
#include <sigilweave/Choreograph.h>

#include <algorithm>
#include <cmath>

#include "Effects.h"
#include "Host.h"
#include "Strokes.h"
