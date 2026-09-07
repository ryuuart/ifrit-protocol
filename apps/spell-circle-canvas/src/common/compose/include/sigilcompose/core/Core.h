#pragma once

/** @file
 * SigilCompose core — the kernel in dependency order. Each header stands
 * on its own; include the one a translation unit needs, or this file for
 * all of them. The streaming feed (Feed.h) is the kernel's too and is
 * included by name.
 */

#include "sigilcompose/core/Composer.h"
#include "sigilcompose/core/Derive.h"
#include "sigilcompose/core/Element.h"
#include "sigilcompose/core/Factories.h"
#include "sigilcompose/core/Instances.h"
#include "sigilcompose/core/Layout.h"
#include "sigilcompose/core/Mask.h"
#include "sigilcompose/core/Measure.h"
#include "sigilcompose/core/Paint.h"
#include "sigilcompose/core/Shape.h"
#include "sigilcompose/core/Stroke.h"
#include "sigilcompose/core/Table.h"
#include "sigilcompose/core/TextPainter.h"
#include "sigilcompose/core/Tiles.h"
