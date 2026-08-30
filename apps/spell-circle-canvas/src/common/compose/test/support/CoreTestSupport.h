#pragma once
// Support for compose_core_test: the kernel — elements, the reconciler,
// layout, paint, transitions, text, the feed and the instanced leaf — and
// the field walks. Only kernel headers: the binary links SigilComposeCore
// alone, which is what proves the kernel stands without its catalogs.

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
#include <sigilcompose/Compose.h>
#include <sigilcompose/Feed.h>
#include <sigilcompose/Instances.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/testing/Checks.h>
#include <sigilimage/ImageAsset.h>
#include <sigilweave/choreograph/Choreograph.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <type_traits>

#include "Atlas.h"
#include "Effects.h"
#include "Host.h"
#include "Profile.h"
#include "Strokes.h"
