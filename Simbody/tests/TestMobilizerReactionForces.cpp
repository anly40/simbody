/* -------------------------------------------------------------------------- *
 *                      SimTK Core: SimTK Simbody(tm)                         *
 * -------------------------------------------------------------------------- *
 * This is part of the SimTK Core biosimulation toolkit originating from      *
 * Simbios, the NIH National Center for Physics-Based Simulation of           *
 * Biological Structures at Stanford, funded under the NIH Roadmap for        *
 * Medical Research, grant U54 GM072970. See https://simtk.org.               *
 *                                                                            *
 * Portions copyright (c) 2008 Stanford University and the Authors.           *
 * Authors: Peter Eastman                                                     *
 * Contributors:                                                              *
 *                                                                            *
 * Permission is hereby granted, free of charge, to any person obtaining a    *
 * copy of this software and associated documentation files (the "Software"), *
 * to deal in the Software without restriction, including without limitation  *
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,   *
 * and/or sell copies of the Software, and to permit persons to whom the      *
 * Software is furnished to do so, subject to the following conditions:       *
 *                                                                            *
 * The above copyright notice and this permission notice shall be included in *
 * all copies or substantial portions of the Software.                        *
 *                                                                            *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR *
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   *
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    *
 * THE AUTHORS, CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,    *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR      *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE  *
 * USE OR OTHER DEALINGS IN THE SOFTWARE.                                     *
 * -------------------------------------------------------------------------- */

#include "SimTKsimbody.h"
#include "SimTKcommon/Testing.h"

using namespace SimTK;
using namespace std;

const Real TOL = 1e-10;
const Real BOND_LENGTH = 0.5;

#define ASSERT(cond) {SimTK_ASSERT_ALWAYS(cond, "Assertion failed");}

template <class T>
void assertEqual(T val1, T val2, double tol=TOL) {
    ASSERT(abs(val1-val2) < tol);
}

template <int N>
void assertEqual(Vec<N> val1, Vec<N> val2, double tol) {
    double norm = max(val1.norm(), 1.0);
    for (int i = 0; i < N; ++i)
        ASSERT(abs(val1[i]-val2[i]) < tol*norm);
}

template<>
void assertEqual(Vector val1, Vector val2, double tol) {
    ASSERT(val1.size() == val2.size());
    for (int i = 0; i < val1.size(); ++i)
        assertEqual(val1[i], val2[i], tol);
}

template<>
void assertEqual(SpatialVec val1, SpatialVec val2, double tol) {
    assertEqual(val1[0], val2[0], tol);
    assertEqual(val1[1], val2[1], tol);
}

template<>
void assertEqual(Transform val1, Transform val2, double tol) {
    assertEqual(val1.p(), val2.p(), tol);
    ASSERT(val1.R().isSameRotationToWithinAngle(val2.R(), tol));
}

void compareReactionToConstraint(SpatialVec reactionForce, const Constraint& constraint, const State& state) {
    Vector_<SpatialVec> constraintForce(constraint.getNumConstrainedBodies());
    Vector mobilityForce(constraint.getNumConstrainedU(state));
    constraint.calcConstraintForcesFromMultipliers(state, constraint.getMultipliersAsVector(state), constraintForce, mobilityForce);
    
    // Transform the reaction force from the joint location to the body location.
    
    const MobilizedBody& body = constraint.getMobilizedBodyFromConstrainedBody(ConstrainedBodyIndex(1));
    Vec3 localForce = ~body.getBodyTransform(state).R()*reactionForce[1];
    reactionForce[0] += body.getBodyTransform(state).R()*(body.getOutboardFrame(state).p()%localForce);
    assertEqual(reactionForce, -1*constraint.getAncestorMobilizedBody().getBodyRotation(state)*constraintForce[1]);
}

/**
 * Compare the forces generated by equivalent mobilizers and constraints.
 */

void testByComparingToConstraints() {
    MultibodySystem system;
    SimbodyMatterSubsystem matter(system);
    GeneralForceSubsystem forces(system);
    Force::UniformGravity(forces, matter, Vec3(0, -9.8, 0));
    
    // Create two free joints (which should produce no reaction forces).
    
    Body::Rigid body = Body::Rigid(MassProperties(1.3, Vec3(0), Inertia(1.3)));
    MobilizedBody::Free f1(matter.updGround(), Transform(Vec3(0)), body, Transform(Vec3(BOND_LENGTH, 0, 0)));
    MobilizedBody::Free f2(f1, Transform(Vec3(0)), body, Transform(Vec3(BOND_LENGTH, 0, 0)));
    
    // Two ball joints, and two free joints constrained to act like ball joints.
    
    MobilizedBody::Free fb1(matter.updGround(), Transform(Vec3(0)), body, Transform(Vec3(BOND_LENGTH, 0, 0)));
    MobilizedBody::Free fb2(fb1, Transform(Vec3(0, 0, BOND_LENGTH)), body, Transform(Vec3(BOND_LENGTH, 0, 0)));
    Constraint::Ball fb1constraint(matter.updGround(), Vec3(0, 0, 0), fb1, Vec3(BOND_LENGTH, 0, 0));
    Constraint::Ball fb2constraint(fb1, Vec3(0, 0, BOND_LENGTH), fb2, Vec3(BOND_LENGTH, 0, 0));
    MobilizedBody::Ball b1(matter.updGround(), Transform(Vec3(0)), body, Transform(Vec3(BOND_LENGTH, 0, 0)));
    MobilizedBody::Ball b2(b1, Transform(Vec3(0, 0, BOND_LENGTH)), body, Transform(Vec3(BOND_LENGTH, 0, 0)));
    Force::ConstantTorque(forces, fb2, Vec3(0.1, 0.1, 1.0));
    Force::ConstantTorque(forces, b2, Vec3(0.1, 0.1, 1.0));
    
    // Two translation joints, and two free joints constrained to act like translation joints.

    MobilizedBody::Free ft1(matter.updGround(), Transform(Vec3(0)), body, Transform(Vec3(BOND_LENGTH, 0, 0)));
    MobilizedBody::Free ft2(ft1, Transform(Vec3(0)), body, Transform(Vec3(0, BOND_LENGTH, 0)));
    Constraint::ConstantOrientation ft1constraint(matter.updGround(), Rotation(0, Vec3(1)), ft1, Rotation(0, Vec3(1)));
    Constraint::ConstantOrientation ft2constraint(ft1, Rotation(0, Vec3(1)), ft2, Rotation(0, Vec3(1)));
    MobilizedBody::Translation t1(matter.updGround(), Transform(Vec3(0)), body, Transform(Vec3(BOND_LENGTH, 0, 0)));
    MobilizedBody::Translation t2(t1, Transform(Vec3(0)), body, Transform(Vec3(0, BOND_LENGTH, 0)));
    Force::ConstantTorque(forces, ft2, Vec3(0.1, 0.1, 1.0));
    Force::ConstantTorque(forces, t2, Vec3(0.1, 0.1, 1.0));
    
    // Create the state.
    
    system.realizeTopology();
    State state = system.getDefaultState();
    Random::Gaussian random;
    int nq = state.getNQ()/2;
    for (int i = 0; i < state.getNY(); ++i)
        state.updY()[i] = random.getValue();
    system.realize(state, Stage::Velocity);
    Transform b1transform = b1.getMobilizerTransform(state);
    Transform b2transform = b2.getMobilizerTransform(state);
    SpatialVec b1velocity = b1.getMobilizerVelocity(state);
    SpatialVec b2velocity = b2.getMobilizerVelocity(state);
    Transform t1transform = t1.getMobilizerTransform(state);
    Transform t2transform = t2.getMobilizerTransform(state);
    SpatialVec t1velocity = t1.MobilizedBody::getMobilizerVelocity(state);
    SpatialVec t2velocity = t2.MobilizedBody::getMobilizerVelocity(state);
    fb1.setQToFitTransform(state, b1transform);
    fb2.setQToFitTransform(state, b2transform);
    fb1.setUToFitVelocity(state, b1velocity);
    fb2.setUToFitVelocity(state, b2velocity);
    ft1.setQToFitTransform(state, t1transform);
    ft2.setQToFitTransform(state, t2transform);
    ft1.setUToFitVelocity(state, t1velocity);
    ft2.setUToFitVelocity(state, t2velocity);
    Vector temp;
    system.project(state, TOL, Vector(state.getNY(), 1.0), Vector(state.getNYErr(), 1.0), temp);
    system.realize(state, Stage::Acceleration);
    
    // Make sure the free and constrained bodies really are identical.
    
    assertEqual(b1.getBodyTransform(state), fb1.getBodyTransform(state));
    assertEqual(b2.getBodyTransform(state), fb2.getBodyTransform(state));
    assertEqual(b1.getBodyVelocity(state), fb1.getBodyVelocity(state));
    assertEqual(b2.getBodyVelocity(state), fb2.getBodyVelocity(state));
    assertEqual(t1.getBodyTransform(state), ft1.getBodyTransform(state));
    assertEqual(t2.getBodyTransform(state), ft2.getBodyTransform(state));
    assertEqual(t1.getBodyVelocity(state), ft1.getBodyVelocity(state));
    assertEqual(t2.getBodyVelocity(state), ft2.getBodyVelocity(state));
    
    // Calculate the mobility reaction forces.

    Vector_<SpatialVec> reactionForce(matter.getNumBodies());
    matter.calcMobilizerReactionForces(state, reactionForce);

    // Make sure all free bodies have no reaction force on them.
    
    assertEqual((reactionForce[f1.getMobilizedBodyIndex()]), SpatialVec(Vec3(0), Vec3(0)));
    assertEqual((reactionForce[f2.getMobilizedBodyIndex()]), SpatialVec(Vec3(0), Vec3(0)));
    assertEqual((reactionForce[fb1.getMobilizedBodyIndex()]), SpatialVec(Vec3(0), Vec3(0)));
    assertEqual((reactionForce[fb2.getMobilizedBodyIndex()]), SpatialVec(Vec3(0), Vec3(0)));
    assertEqual((reactionForce[ft1.getMobilizedBodyIndex()]), SpatialVec(Vec3(0), Vec3(0)));
    assertEqual((reactionForce[ft2.getMobilizedBodyIndex()]), SpatialVec(Vec3(0), Vec3(0)));
    
    // The reaction forces should match the corresponding constraint forces.
    
    compareReactionToConstraint(reactionForce[b1.getMobilizedBodyIndex()], fb1constraint, state);
    compareReactionToConstraint(reactionForce[b2.getMobilizedBodyIndex()], fb2constraint, state);
    compareReactionToConstraint(reactionForce[t1.getMobilizedBodyIndex()], ft1constraint, state);
    compareReactionToConstraint(reactionForce[t2.getMobilizedBodyIndex()], ft2constraint, state);
}

/*
 * (sherm 110919) None of the existing tests caught the problem reported
 * in bug #1535 -- incorrect reaction torques sometimes.
 * This is a pair of identical two-body pendulums, one done with pin joints
 * and one done with equivalent constraints.
 */
void testByComparingToConstraints2() {
    MultibodySystem system;
    SimbodyMatterSubsystem matter(system);
    GeneralForceSubsystem forces(system);
    Force::UniformGravity gravity(forces, matter, Vec3(10, -9.8, 3));

    Body::Rigid pendulumBody(MassProperties(1.0, Vec3(0), Inertia(1)));
    pendulumBody.addDecoration(Transform(), DecorativeSphere(0.1).setColor(Red));

    // First double pendulum, using Pin joints.
    Rotation x45(Pi/4, XAxis);
    MobilizedBody::Pin pendulum1(matter.updGround(), 
                                Transform(x45,Vec3(0,-1,0)), 
                                pendulumBody, 
                                Transform(Vec3(0, 1, 0)));
    MobilizedBody::Pin pendulum1b(pendulum1, 
                                Transform(x45,Vec3(0,-1,0)), 
                                pendulumBody, 
                                Transform(Vec3(0, 1, 0)));

    // Second double pendulum, using Free joints plus 5 constraints.
    MobilizedBody::Free pendulum2(matter.updGround(), 
                                  Transform(x45,Vec3(2,-1,0)),
                                  pendulumBody, 
                                  Transform(Vec3(0,1,0)));
    Constraint::Ball ballcons2(matter.updGround(), Vec3(2,-1,0),
                               pendulum2, Vec3(0,1,0));
    const Transform& X_GF2 = pendulum2.getDefaultInboardFrame();
    const Transform& X_P2M = pendulum2.getDefaultOutboardFrame();
    Constraint::ConstantAngle angx2(matter.Ground(), X_GF2.x(),
                              pendulum2, X_P2M.z());
    Constraint::ConstantAngle angy2(matter.Ground(), X_GF2.y(),
                              pendulum2, X_P2M.z());

    MobilizedBody::Free pendulum2b(pendulum2, 
                                   Transform(x45,Vec3(0,-1,0)),
                                   pendulumBody, 
                                   Transform(Vec3(0,1,0)));
    Constraint::Ball ballcons2b(pendulum2, Vec3(0,-1,0),
                                pendulum2b, Vec3(0,1,0));
    const Transform& X_GF2b = pendulum2b.getDefaultInboardFrame();
    const Transform& X_P2Mb = pendulum2b.getDefaultOutboardFrame();
    Constraint::ConstantAngle angx2b(pendulum2, X_GF2b.x(),
                              pendulum2b, X_P2Mb.z());
    Constraint::ConstantAngle angy2b(pendulum2, X_GF2b.y(),
                              pendulum2b, X_P2Mb.z());

    // Uncomment if you want to see this.
    //Visualizer viz(system);
    
    // Initialize the system and state.
    
    system.realizeTopology();
    State state = system.getDefaultState();
    pendulum1.setOneQ(state, 0, Pi/4);
    pendulum1.setOneU(state, 0, 1.0); // initial velocity 1 rad/sec

    pendulum1b.setOneU(state, 0, 1.0); // initial velocity 1 rad/sec
    pendulum1b.setOneQ(state, 0, Pi/4);

    pendulum2.setQToFitRotation(state, Rotation(Pi/4, ZAxis));
    pendulum2.setUToFitAngularVelocity(state, Vec3(0,0,1));
    pendulum2b.setQToFitRotation(state, Rotation(Pi/4, ZAxis));
    pendulum2b.setUToFitAngularVelocity(state, Vec3(0,0,1));

    system.realize(state);
    //viz.report(state);

    const MobodIndex p2x = pendulum2.getMobilizedBodyIndex();
    const MobodIndex p2bx = pendulum2b.getMobilizedBodyIndex();

    // Shift the reaction forces to body origins for easy comparison with
    // the reported constraint forces.
    Vector_<SpatialVec> reactionForcesInG;
    matter.calcMobilizerReactionForces(state, reactionForcesInG);
    const MobodIndex p1x = pendulum1.getMobilizedBodyIndex();
    const MobodIndex p1bx = pendulum1b.getMobilizedBodyIndex();
    const Rotation& R_G1 = pendulum1.getBodyTransform(state).R();
    const Rotation& R_G1b = pendulum1b.getBodyTransform(state).R();
    reactionForcesInG[p1x] = shiftForceFromTo(reactionForcesInG[p1x],
                                         R_G1*Vec3(0,1,0), Vec3(0));
    reactionForcesInG[p1bx] = shiftForceFromTo(reactionForcesInG[p1bx],
                                         R_G1b*Vec3(0,1,0), Vec3(0));

    // The constraints apply forces to parent and body; we want to compare
    // forces on the body, which will be the second entry here. We're assuming
    // the ball and constant angle constraints are ordered the same way; if
    // that ever changes the constraints can be queried to find the mobilized
    // body index corresponding to the constrained body index.
    Vector_<SpatialVec> cons2Forces = 
        -(ballcons2.getConstrainedBodyForcesAsVector(state)
          + angx2.getConstrainedBodyForcesAsVector(state)
          + angy2.getConstrainedBodyForcesAsVector(state));
    Vector_<SpatialVec> cons2bForces = 
        -(ballcons2b.getConstrainedBodyForcesAsVector(state) 
          + angx2b.getConstrainedBodyForcesAsVector(state)
          + angy2b.getConstrainedBodyForcesAsVector(state));

    SimTK_TEST_EQ(cons2Forces[1], reactionForcesInG[p1x]);
    SimTK_TEST_EQ(cons2bForces[1], reactionForcesInG[p1bx]);
}

/**
 * Construct a system of several bodies, and compare the reaction forces to those calculated by SD/FAST.
 */

void testByComparingToSDFAST() {
    MultibodySystem system;
    SimbodyMatterSubsystem matter(system);
    GeneralForceSubsystem forces(system);
    Force::UniformGravity(forces, matter, Vec3(0, -9.8, 0));

    // Construct the set of bodies.
    
    Inertia inertia = Inertia(Mat33(0.1, 0.01, 0.01,
                                    0.01, 0.1, 0.01,
                                    0.01, 0.01, 0.1));
    MobilizedBody::Slider body1(matter.updGround(), MassProperties(10.0, Vec3(0), inertia));
    MobilizedBody::Pin body2(body1, Vec3(0.1, 0.1, 0), MassProperties(20.0, Vec3(0), inertia), Vec3(0, -0.2, 0));
    MobilizedBody::Gimbal body3(body2, Vec3(0, 0.2, 0), MassProperties(20.0, Vec3(0), inertia), Vec3(0, -0.2, 0));
    MobilizedBody::Pin body4(body3, Vec3(0, 0.2, 0), MassProperties(30.0, Vec3(0), inertia), Vec3(0, -0.2, 0));
    State state = system.realizeTopology();
    system.realize(state, Stage::Acceleration);
    
    // Calculate reaction forces, and compare to the values that were generated by SD/FAST.
    
    Vector_<SpatialVec> reaction(matter.getNumBodies());
    matter.calcMobilizerReactionForces(state, reaction);
    assertEqual(~body1.getBodyTransform(state).R()*reaction[body1.getMobilizedBodyIndex()], SpatialVec(Vec3(0, 0, 68.6), Vec3(0, 784.0, 0)));
    assertEqual(~body2.getBodyTransform(state).R()*reaction[body2.getMobilizedBodyIndex()], SpatialVec(Vec3(0, 0, 0), Vec3(0, 686.0, 0)));
    assertEqual(~body3.getBodyTransform(state).R()*reaction[body3.getMobilizedBodyIndex()], SpatialVec(Vec3(0, 0, 0), Vec3(0, 490.0, 0)));
    assertEqual(~body4.getBodyTransform(state).R()*reaction[body4.getMobilizedBodyIndex()], SpatialVec(Vec3(0, 0, 0), Vec3(0, 294.0, 0)));
    
    // Now set it to a different configuration and try again.
    
    body1.setLength(state, 1.0);
    body2.setAngle(state, 0.5);
    Rotation r;
    r.setRotationFromThreeAnglesThreeAxes(BodyRotationSequence, 0.2, ZAxis, -0.1, XAxis, 2.0, YAxis);
    body3.setQToFitRotation(state, r);
    body4.setAngle(state, -0.5);
    system.realize(state, Stage::Acceleration);
    matter.calcMobilizerReactionForces(state, reaction);
    assertEqual(~body1.getBodyTransform(state).R()*reaction[body1.getMobilizedBodyIndex()], SpatialVec(Vec3(1.647327, 0.783211, 34.088183), Vec3(0, 359.274099, 3.342380)), 1e-5);
    assertEqual(~body2.getBodyTransform(state).R()*reaction[body2.getMobilizedBodyIndex()], SpatialVec(Vec3(1.688077, 0.351125, 0), Vec3(55.399123, 267.455570, 3.342380)), 1e-5);
    assertEqual(~body3.getBodyTransform(state).R()*reaction[body3.getMobilizedBodyIndex()], SpatialVec(Vec3(0, 0, 0), Vec3(-17.757553, 174.663042, -11.383057)), 1e-5);
    assertEqual(~body4.getBodyTransform(state).R()*reaction[body4.getMobilizedBodyIndex()], SpatialVec(Vec3(0.910890, 0.082353, 0), Vec3(-13.977214, 74.444715, 4.943682)), 1e-5);
    
    // Try giving it momentum.

    state.updQ() = 0.0;
    body2.setOneU(state, 0, 1);
    body3.setUToFitAngularVelocity(state, Vec3(3, 4, 2));
    body4.setOneU(state, 0, 5);
    system.realize(state, Stage::Acceleration);
    matter.calcMobilizerReactionForces(state, reaction);
    assertEqual(~body1.getBodyTransform(state).R()*reaction[body1.getMobilizedBodyIndex()], SpatialVec(Vec3(-13.549253, 2.723897, -6.355912), Vec3(0, 34.0, -27.088584)), 1e-5);
    assertEqual(~body2.getBodyTransform(state).R()*reaction[body2.getMobilizedBodyIndex()], SpatialVec(Vec3(-10.840395, 0.015039, 0), Vec3(-0.440882, -64.0, -27.088584)), 1e-5);
    assertEqual(~body3.getBodyTransform(state).R()*reaction[body3.getMobilizedBodyIndex()], SpatialVec(Vec3(0, 0, 0), Vec3(0.692814, -256.000000, -27.088584)), 1e-5);
    assertEqual(~body4.getBodyTransform(state).R()*reaction[body4.getMobilizedBodyIndex()], SpatialVec(Vec3(3.276930, -0.281928, 0), Vec3(3.796164, -372.0, 21.472977)), 1e-5);
}

/**
 * Construct a system of several bodies, and compare the reaction forces to those calculated by SD/FAST.
 */

void testByComparingToSDFAST2() {
    MultibodySystem system;
    SimbodyMatterSubsystem matter(system);
    GeneralForceSubsystem forces(system);
    Force::UniformGravity(forces, matter, Vec3(0, -9.8065, 0));

    // Construct the set of bodies.
    
    Body::Rigid femur(MassProperties(8.806, Vec3(0), Inertia(Vec3(0.1268, 0.0332, 0.1337))));
    Body::Rigid tibia(MassProperties(3.510, Vec3(0), Inertia(Vec3(0.0477, 0.0048, 0.0484))));
    MobilizedBody::Pin p1(matter.Ground(), Transform(Vec3(0.0000, -0.0700, 0.0935)), femur, Transform(Vec3(0.0020, 0.1715, 0)));
    MobilizedBody::Slider p2(p1, Transform(Vec3(0.0033, -0.2294, 0)), tibia, Transform(Vec3(0.0, 0.1862, 0.0)));
    State state = system.realizeTopology();
    system.realize(state, Stage::Acceleration);
    
    // Calculate reaction forces, and compare to the values that were generated by SD/FAST.
    
    Vector_<SpatialVec> reaction(matter.getNumBodies());
    matter.calcMobilizerReactionForces(state, reaction);
    assertEqual(~p1.getBodyTransform(state).R()*reaction[p1.getMobilizedBodyIndex()], SpatialVec(Vec3(0, 0, 0), Vec3(0.438079, 120.773069, 0)), 1e-5);
    assertEqual(~p2.getBodyTransform(state).R()*reaction[p2.getMobilizedBodyIndex()], SpatialVec(Vec3(0, 0, 0.014040), Vec3(0, 34.422139, 0)), 1e-5);
    
    // Now set it to a different configuration and try again.
    
    p1.setOneQ(state, 0, -90*NTraits<Real>::getPi()/180);
    p2.setOneQ(state, 0, 0.1);
    system.realize(state, Stage::Acceleration);
    matter.calcMobilizerReactionForces(state, reaction);
    assertEqual(~p1.getBodyTransform(state).R()*reaction[p1.getMobilizedBodyIndex()], SpatialVec(Vec3(0, 0, 0), Vec3(-39.481457, 10.489344, 0)), 1e-5);
    assertEqual(~p2.getBodyTransform(state).R()*reaction[p2.getMobilizedBodyIndex()], SpatialVec(Vec3(0, 0, 1.502242), Vec3(0, 11.035987, 0)), 1e-5);
}

/**
 * Construct a system of several bodies and a constraint, and compare the reaction forces to those calculated by SD/FAST.
 */

void testByComparingToSDFASTWithConstraint() {
    MultibodySystem system;
    SimbodyMatterSubsystem matter(system);
    GeneralForceSubsystem forces(system);
    Force::UniformGravity(forces, matter, Vec3(0, -9.8, 0));

    // Construct the set of bodies.
    
    Inertia inertia = Inertia(Mat33(0.1, 0.01, 0.01,
                                    0.01, 0.1, 0.01,
                                    0.01, 0.01, 0.1));
    MobilizedBody::Gimbal body1(matter.updGround(), MassProperties(10.0, Vec3(0), inertia));
    MobilizedBody::Gimbal body2(body1, Vec3(0, -0.1, 0.2), MassProperties(20.0, Vec3(0), inertia), Vec3(0, 0.2, 0));
    MobilizedBody::Gimbal body3(body1, Vec3(0, -0.1, -0.2), MassProperties(20.0, Vec3(0), inertia), Vec3(0, 0.2, 0));
    MobilizedBody::Gimbal body4(body2, Vec3(0, -0.2, 0), MassProperties(30.0, Vec3(0), inertia), Vec3(0, 0.2, 0));
    MobilizedBody::Gimbal body5(body3, Vec3(0, -0.2, 0), MassProperties(30.0, Vec3(0), inertia), Vec3(0, 0.2, 0));
    Constraint::Rod constraint(body4, body5, 0.15);
    State state = system.realizeTopology();
    system.realize(state, Stage::Velocity);
    Vector temp;
    system.project(state, 1e-10, Vector(state.getNY(), 1), Vector(1, 1), temp);
    system.realize(state, Stage::Acceleration);
    
    // Calculate reaction forces, and compare to the values that were generated by SD/FAST.
    
    Vector_<SpatialVec> reaction(matter.getNumBodies());
    matter.calcMobilizerReactionForces(state, reaction);
    assertEqual(~body1.getBodyTransform(state).R()*reaction[body1.getMobilizedBodyIndex()], SpatialVec(Vec3(0, 0, 0), Vec3(-0.000626, 1077.988912, 0.000030)), 1e-5);
    assertEqual(~body2.getBodyTransform(state).R()*reaction[body2.getMobilizedBodyIndex()], SpatialVec(Vec3(0, 0, 0), Vec3(-0.005038, 495.288692, -18.767467)), 1e-5);
    assertEqual(~body3.getBodyTransform(state).R()*reaction[body3.getMobilizedBodyIndex()], SpatialVec(Vec3(0, 0, 0), Vec3(0.004236, 495.287857, 18.767535)), 1e-5);
    assertEqual(~body4.getBodyTransform(state).R()*reaction[body4.getMobilizedBodyIndex()], SpatialVec(Vec3(0, 0, 0), Vec3(0.006251, 303.365940, -0.202330)), 1e-5);
    assertEqual(~body5.getBodyTransform(state).R()*reaction[body5.getMobilizedBodyIndex()], SpatialVec(Vec3(0, 0, 0), Vec3(-0.005933, 303.365472, 0.202301)), 1e-5);
    
    // Now set it to a different configuration and try again.
    
    Rotation r;
    r.setRotationFromThreeAnglesThreeAxes(BodyRotationSequence, 1.0, ZAxis, 1.0, XAxis, 1.0, YAxis);
    body1.setQToFitRotation(state, r);
    r.setRotationFromThreeAnglesThreeAxes(BodyRotationSequence, 0.433843, ZAxis, 0.647441, XAxis, 0.500057, YAxis);
    body2.setQToFitRotation(state, r);
    r.setRotationFromThreeAnglesThreeAxes(BodyRotationSequence, 0.066156, ZAxis, -0.117266, XAxis, -0.047605, YAxis);
    body3.setQToFitRotation(state, r);
    r.setRotationFromThreeAnglesThreeAxes(BodyRotationSequence, 0.000997, ZAxis, 0.055206, XAxis, 0.0, YAxis);
    body4.setQToFitRotation(state, r);
    r.setRotationFromThreeAnglesThreeAxes(BodyRotationSequence, 1.008746, ZAxis, 0.951972, XAxis, 1.0, YAxis);
    body5.setQToFitRotation(state, r);
    system.realize(state, Stage::Acceleration);
    matter.calcMobilizerReactionForces(state, reaction);
    assertEqual(~body1.getBodyTransform(state).R()*reaction[body1.getMobilizedBodyIndex()], SpatialVec(Vec3(0, 0, 0), Vec3(99.121319, 139.500095, 95.065409)), 1e-5);
    assertEqual(~body2.getBodyTransform(state).R()*reaction[body2.getMobilizedBodyIndex()], SpatialVec(Vec3(0, 0, 0), Vec3(15.359115, 55.876994, 22.508078)), 1e-5);
    assertEqual(~body3.getBodyTransform(state).R()*reaction[body3.getMobilizedBodyIndex()], SpatialVec(Vec3(0, 0, 0), Vec3(15.696393, 65.002065, 13.133021)), 1e-5);
    assertEqual(~body4.getBodyTransform(state).R()*reaction[body4.getMobilizedBodyIndex()], SpatialVec(Vec3(0, 0, 0), Vec3(-6.262023, 32.714510, -9.770708)), 1e-5);
    assertEqual(~body5.getBodyTransform(state).R()*reaction[body5.getMobilizedBodyIndex()], SpatialVec(Vec3(0, 0, 0), Vec3(10.471620, 0.963822, -4.640161)), 1e-5);
}

// Create a free body in space and apply some forces to it.
// As long as we don't apply mobility forces, the reaction force
// in the mobilizer should be zero.
// It is important to do this with a full inertia, offset com,
// non-unit mass, twisted frames, non-zero velocities, etc.

const Real d = 1.5; // length (m)
const Real mass = 2; // kg
const Transform X_GF(Rotation(Pi/3, Vec3(.1,-.3,.3)), Vec3(-4,-5,-1));
const Transform X_BM(Rotation(-Pi/10, Vec3(7,5,3)), Vec3(0,d,0));

void testFreeMobilizer() {
    MultibodySystem forward;
    SimbodyMatterSubsystem fwdMatter(forward);
    GeneralForceSubsystem fwdForces(forward);
    Force::UniformGravity(fwdForces, fwdMatter, Vec3(0, -1, 0));

    const Vec3 com(1,2,3);
    const UnitInertia centralGyration(1, 1.5, 2, .1, .2, .3);
    Body::Rigid body(MassProperties(mass, com, mass*centralGyration.shiftFromMassCenter(com, 1)));

    MobilizedBody::Free fwdA (fwdMatter.Ground(),  X_GF, body, X_BM);

    Force::ConstantForce(fwdForces, fwdA, Vec3(-1,.27,4), Vec3(5,.6,-1));
    Force::ConstantTorque(fwdForces, fwdA, Vec3(-5.5,1.6,-1.1));

    State fwdState  = forward.realizeTopology();
    fwdA.setQToFitTransform(fwdState, Transform(Rotation(Pi/9,Vec3(-1.8,4,2.2)), Vec3(.1,.2,.7)));

    forward.realize (fwdState,  Stage::Position);

    fwdA.setUToFitVelocity(fwdState, SpatialVec(Vec3(.99,2,4), Vec3(-1.2,4,.000333)));
    forward.realize (fwdState,  Stage::Velocity);
    forward.realize (fwdState,  Stage::Acceleration);

    Vector_<SpatialVec> fwdReac;
    fwdMatter.calcMobilizerReactionForces(fwdState, fwdReac);

    // We expect no reaction from a Free joint.
    assertEqual(fwdReac[0], SpatialVec(Vec3(0)));
    assertEqual(fwdReac[1], SpatialVec(Vec3(0)));
}

int main() {
    SimTK_START_TEST("TestMobilizerReactionForces");
        SimTK_SUBTEST(testByComparingToConstraints);
        SimTK_SUBTEST(testByComparingToConstraints2);
        SimTK_SUBTEST(testByComparingToSDFAST);
        SimTK_SUBTEST(testByComparingToSDFAST2);
        SimTK_SUBTEST(testByComparingToSDFASTWithConstraint);
        SimTK_SUBTEST(testFreeMobilizer);
    SimTK_END_TEST();
}
