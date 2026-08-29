#pragma once
// Support for compose_core_test: the kernel, the authoring grammar, masks
// and the field walks.

#include <include/core/SkColorFilter.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRRect.h>
#include <include/core/SkStream.h>
#include <include/core/SkString.h>
#include <include/core/SkStrokeRec.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <include/effects/SkTrimPathEffect.h>
#include <include/encode/SkPngEncoder.h>
#include <sigilcompose/Brushes.h>
#include <sigilcompose/Debug.h>
#include <sigilcompose/Decorations.h>
#include <sigilcompose/Feed.h>
#include <sigilcompose/Instances.h>
#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Routers.h>
#include <sigilcompose/Sdf.h>
#include <sigilcompose/Shapes.h>
#include <sigilcompose/Util.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilimage/ImageAsset.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <type_traits>

#include "Effects.h"
#include "Host.h"
#include "Profile.h"
#include "Strokes.h"
