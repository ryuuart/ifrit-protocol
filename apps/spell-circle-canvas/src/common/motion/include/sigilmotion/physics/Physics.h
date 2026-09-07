#pragma once

/** @file
 * The umbrella over the physics feature: the point set and its lanes,
 * the forces that push on it, the constraints that hold it together, the
 * stepper that advances it, and the emitter that fills a set with
 * particles that age and die.
 */

#include <sigilmotion/physics/Constraints.h>
#include <sigilmotion/physics/Forces.h>
#include <sigilmotion/physics/Particles.h>
#include <sigilmotion/physics/Points.h>
#include <sigilmotion/physics/Verlet.h>
