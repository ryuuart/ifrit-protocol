#pragma once

/** @file
 * SigilDraw, whole: the pen and everything it is spelled with.
 */

/** @defgroup draw-pen The pen
 *  An immediate-mode canvas carrying p5's verbs with p5's names,
 *  argument orders and defaults: the drawing state, the shapes, the
 *  transform, text and images, and the colour, maths and noise helpers a
 *  sketch reaches for beside them (Pen.h, Graphics.h, Color.h, Math.h,
 *  Noise.h, Constants.h). */
/** @defgroup draw-brush The brush chapter
 *  Natural media over the pen: a tool whose stroke is thousands of dabs
 *  laid along a path, the dynamics and pressure that shape them, the
 *  grain and wash that colour them, and the brush files a painting
 *  application exports (brush/Brush.h). */
/** @defgroup draw-retained The retained guest
 *  What a pen keeps between frames for a guest that is not immediate: a
 *  store keyed by the call site that painted it, so a retained runtime
 *  can be handed a pen and still find its own state next frame
 *  (Retained.h). */

#include <sigildraw/Color.h>
#include <sigildraw/Constants.h>
#include <sigildraw/Graphics.h>
#include <sigildraw/Math.h>
#include <sigildraw/Noise.h>
#include <sigildraw/Pen.h>
#include <sigildraw/Retained.h>
