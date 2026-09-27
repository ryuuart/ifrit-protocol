# The physics feature: the point set as lanes, the forces that push on
# it, the constraints that hold it together, and the Verlet stepper over
# the three. It is the one feature here that carries STATE rather than a
# function of a progress — a position and a velocity stepped forward —
# which is why it stands beside the values rather than inside them: the
# spring there is the closed form of one value flying at a target, and
# this is a set of them pushing on each other, with no closed form to
# have.
#
# It links no clock and no timeline. A step is a length of time the
# caller states — the time unit is the one header it links for it — so a
# step-rate timer on an engine, a bake writing frames
# to disk and a test stepping a hundred times in a loop all drive the
# same body. SigilCoreCompute is the noise field a wind reads and is a
# header-only leaf over the standard library — the same kind of edge out
# of this library the comparable pin is. glm is the vector type every
# position, velocity and force lane holds, header-only and public.
sigil_library(SigilMotionPhysics
  SOURCES Constraints.cpp Forces.cpp Neighbourhood.cpp Particles.cpp Points.cpp
          Verlet.cpp
  HEADERS Constraints.h Forces.h Neighbourhood.h Particles.h Points.h
          Verlet.h
  PUBLIC SigilMotionTime SigilCoreCompute glm::glm)

# Links the feature alone: nothing here reads a clock, and a link edge
# that pulled one in would show up as a build failure here.
sigil_test(motion_test
  SOURCES test/NeighbourhoodTest.cpp test/ParticlesTest.cpp test/PhysicsTest.cpp
  LIBRARIES SigilMotionPhysics)

sigil_bench(motion_bench
  SOURCES bench/ParticlesBench.cpp bench/PhysicsBench.cpp
  LIBRARIES SigilMotionPhysics)
