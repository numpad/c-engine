// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "box2d/types.h"

/// Distance joint definition. This requires defining an anchor point on both
/// bodies and the non-zero distance of the distance joint. The definition uses
/// local anchor points so that the initial configuration can violate the
/// constraint slightly. This helps when saving and loading a game.
typedef struct b2DistanceJointDef
{
	/// The first attached body.
	b2BodyId bodyIdA;

	/// The second attached body.
	b2BodyId bodyIdB;

	/// The local anchor point relative to bodyA's origin.
	b2Vec2 localAnchorA;

	/// The local anchor point relative to bodyB's origin.
	b2Vec2 localAnchorB;

	/// The rest length of this joint. Clamped to a stable minimum value.
	float length;

	/// Minimum length. Clamped to a stable minimum value.
	float minLength;

	/// Maximum length. Must be greater than or equal to the minimum length.
	float maxLength;

	/// The linear stiffness hertz (cycles per second)
	float hertz;

	/// The linear damping ratio (non-dimensional)
	float dampingRatio;

	/// Set this flag to true if the attached bodies should collide.
	bool collideConnected;

} b2DistanceJointDef;

/// Use this to initialize your joint definition
static inline b2DistanceJointDef b2DefaultDistanceJointDef(void)
{
	b2DistanceJointDef def = B2_ZERO_INIT;
	def.bodyIdA = b2_nullBodyId;
	def.bodyIdB = b2_nullBodyId;
	def.localAnchorA = B2_LITERAL(b2Vec2){0.0f, 0.0f};
	def.localAnchorB = B2_LITERAL(b2Vec2){0.0f, 0.0f};
	def.length = 1.0f;
	def.minLength = 0.0f;
	def.maxLength = b2_huge;
	def.hertz = 0.0f;
	def.dampingRatio = 0.0f;
	def.collideConnected = false;
	return def;
}

/// A motor joint is used to control the relative motion
/// between two bodies. A typical usage is to control the movement
/// of a dynamic body with respect to the ground.
typedef struct b2MotorJointDef
{
	/// The first attached body.
	b2BodyId bodyIdA;

	/// The second attached body.
	b2BodyId bodyIdB;

	/// Position of bodyB minus the position of bodyA, in bodyA's frame, in meters.
	b2Vec2 linearOffset;

	/// The bodyB angle minus bodyA angle in radians.
	float angularOffset;

	/// The maximum motor force in N.
	float maxForce;

	/// The maximum motor torque in N-m.
	float maxTorque;

	/// Position correction factor in the range [0,1].
	float correctionFactor;
} b2MotorJointDef;

/// Use this to initialize your joint definition
static inline b2MotorJointDef b2DefaultMotorJointDef(void)
{
	b2MotorJointDef def = B2_ZERO_INIT;
	def.bodyIdA = b2_nullBodyId;
	def.bodyIdB = b2_nullBodyId;
	def.linearOffset = B2_LITERAL(b2Vec2){0.0f, 0.0f};
	def.angularOffset = 0.0f;
	def.maxForce = 1.0f;
	def.maxTorque = 1.0f;
	def.correctionFactor = 0.3f;
	return def;
}

/// A mouse joint is used to make a point on a body track a
/// specified world point. This a soft constraint with a maximum
/// force. This allows the constraint to stretch without
/// applying huge forces.
typedef struct b2MouseJointDef
{
	/// The first attached body.
	b2BodyId bodyIdA;

	/// The second attached body.
	b2BodyId bodyIdB;

	/// The initial target point in world space
	b2Vec2 target;

	/// The maximum constraint force that can be exerted
	/// to move the candidate body. Usually you will express
	/// as some multiple of the weight (multiplier * mass * gravity).
	float maxForce;

	/// The linear stiffness in N/m
	float stiffness;

	/// The linear damping in N*s/m
	float damping;
} b2MouseJointDef;

/// Use this to initialize your joint definition
static inline b2MouseJointDef b2DefaultMouseJointDef(void)
{
	b2MouseJointDef def = B2_ZERO_INIT;
	def.bodyIdA = b2_nullBodyId;
	def.bodyIdB = b2_nullBodyId;
	def.target = B2_LITERAL(b2Vec2){0.0f, 0.0f};
	def.maxForce = 0.0f;
	def.stiffness = 0.0f;
	def.damping = 0.0f;
	return def;
}

/// Prismatic joint definition. This requires defining a line of
/// motion using an axis and an anchor point. The definition uses local
/// anchor points and a local axis so that the initial configuration
/// can violate the constraint slightly. The joint translation is zero
/// when the local anchor points coincide in world space.
typedef struct b2PrismaticJointDef
{
	/// The first attached body.
	b2BodyId bodyIdA;

	/// The second attached body.
	b2BodyId bodyIdB;

	/// The local anchor point relative to bodyA's origin.
	b2Vec2 localAnchorA;

	/// The local anchor point relative to bodyB's origin.
	b2Vec2 localAnchorB;

	/// The local translation unit axis in bodyA.
	b2Vec2 localAxisA;

	/// The constrained angle between the bodies: bodyB_angle - bodyA_angle.
	float referenceAngle;

	/// Enable/disable the joint limit.
	bool enableLimit;

	/// The lower translation limit, usually in meters.
	float lowerTranslation;

	/// The upper translation limit, usually in meters.
	float upperTranslation;

	/// Enable/disable the joint motor.
	bool enableMotor;

	/// The maximum motor force, usually in N.
	float maxMotorForce;

	/// The desired motor speed in radians per second.
	float motorSpeed;

	/// Set this flag to true if the attached bodies should collide.
	bool collideConnected;
} b2PrismaticJointDef;

/// Use this to initialize your joint definition
static inline b2PrismaticJointDef b2DefaultPrismaticJointDef(void)
{
	b2PrismaticJointDef def = B2_ZERO_INIT;
	def.bodyIdA = b2_nullBodyId;
	def.bodyIdB = b2_nullBodyId;
	def.localAnchorA = B2_LITERAL(b2Vec2){0.0f, 0.0f};
	def.localAnchorB = B2_LITERAL(b2Vec2){0.0f, 0.0f};
	def.localAxisA = B2_LITERAL(b2Vec2){1.0f, 0.0f};
	def.referenceAngle = 0.0f;
	def.enableLimit = false;
	def.lowerTranslation = 0.0f;
	def.upperTranslation = 0.0f;
	def.enableMotor = false;
	def.maxMotorForce = 0.0f;
	def.motorSpeed = 0.0f;
	def.collideConnected = false;
	return def;
}

/// Revolute joint definition. This requires defining an anchor point where the
/// bodies are joined. The definition uses local anchor points so that the
/// initial configuration can violate the constraint slightly. You also need to
/// specify the initial relative angle for joint limits. This helps when saving
/// and loading a game.
/// The local anchor points are measured from the body's origin
/// rather than the center of mass because:
/// 1. you might not know where the center of mass will be.
/// 2. if you add/remove shapes from a body and recompute the mass,
///    the joints will be broken.
typedef struct b2RevoluteJointDef
{
	/// The first attached body.
	b2BodyId bodyIdA;

	/// The second attached body.
	b2BodyId bodyIdB;

	/// The local anchor point relative to bodyA's origin.
	b2Vec2 localAnchorA;

	/// The local anchor point relative to bodyB's origin.
	b2Vec2 localAnchorB;

	/// The bodyB angle minus bodyA angle in the reference state (radians).
	/// This defines the zero angle for the joint limit.
	float referenceAngle;

	/// A flag to enable joint limits.
	bool enableLimit;

	/// The lower angle for the joint limit (radians).
	float lowerAngle;

	/// The upper angle for the joint limit (radians).
	float upperAngle;

	/// A flag to enable the joint motor.
	bool enableMotor;

	/// The desired motor speed. Usually in radians per second.
	float motorSpeed;

	/// The maximum motor torque used to achieve the desired motor speed.
	/// Usually in N-m.
	float maxMotorTorque;

	/// Scale the debug draw
	float drawSize;

	/// Set this flag to true if the attached bodies should collide.
	bool collideConnected;
} b2RevoluteJointDef;

/// Use this to initialize your joint definition
static inline b2RevoluteJointDef b2DefaultRevoluteJointDef(void)
{
	b2RevoluteJointDef def = B2_ZERO_INIT;
	def.bodyIdA = b2_nullBodyId;
	def.bodyIdB = b2_nullBodyId;
	def.localAnchorA = B2_LITERAL(b2Vec2){0.0f, 0.0f};
	def.localAnchorB = B2_LITERAL(b2Vec2){0.0f, 0.0f};
	def.referenceAngle = 0.0f;
	def.lowerAngle = 0.0f;
	def.upperAngle = 0.0f;
	def.maxMotorTorque = 0.0f;
	def.motorSpeed = 0.0f;
	def.enableLimit = false;
	def.enableMotor = false;
	def.drawSize = 0.25f;
	def.collideConnected = false;
	return def;
}

/// A weld joint connect to bodies together rigidly. This constraint can be made soft to mimic
///	soft-body simulation.
/// @warning the approximate solver in Box2D cannot hold many bodies together rigidly
typedef struct b2WeldJointDef
{
	/// The first attached body.
	b2BodyId bodyIdA;

	/// The second attached body.
	b2BodyId bodyIdB;

	/// The local anchor point relative to bodyA's origin.
	b2Vec2 localAnchorA;

	/// The local anchor point relative to bodyB's origin.
	b2Vec2 localAnchorB;

	/// The bodyB angle minus bodyA angle in the reference state (radians).
	float referenceAngle;

	/// Linear stiffness expressed as hertz (oscillations per second). Use zero for maximum stiffness.
	float linearHertz;

	/// Angular stiffness as hertz (oscillations per second). Use zero for maximum stiffness.
	float angularHertz;

	/// Linear damping ratio, non-dimensional. Use 1 for critical damping.
	float linearDampingRatio;

	/// Linear damping ratio, non-dimensional. Use 1 for critical damping.
	float angularDampingRatio;

	/// Set this flag to true if the attached bodies should collide.
	bool collideConnected;
} b2WeldJointDef;

static const b2WeldJointDef b2_defaultWeldJointDef = {
	{-1, -1, 0}, {-1, -1, 0}, {0.0f, 0.0f}, {0.0f, 0.0f}, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, false,
};

/// Use this to initialize your joint definition
static inline b2WeldJointDef b2DefaultWeldJointDef(void)
{
	b2WeldJointDef def = B2_ZERO_INIT;
	def.bodyIdA = b2_nullBodyId;
	def.bodyIdB = b2_nullBodyId;
	def.localAnchorA = B2_LITERAL(b2Vec2){0.0f, 0.0f};
	def.localAnchorB = B2_LITERAL(b2Vec2){0.0f, 0.0f};
	def.referenceAngle = 0.0f;
	def.linearHertz = 0.0f;
	def.angularHertz = 0.0f;
	def.linearDampingRatio = 1.0f;
	def.angularDampingRatio = 1.0f;
	def.collideConnected = false;
	return def;
}

/// Wheel joint definition. This requires defining a line of
/// motion using an axis and an anchor point. The definition uses local
/// anchor points and a local axis so that the initial configuration
/// can violate the constraint slightly. The joint translation is zero
/// when the local anchor points coincide in world space. Using local
/// anchors and a local axis helps when saving and loading a game.
typedef struct b2WheelJointDef
{
	/// The first attached body.
	b2BodyId bodyIdA;

	/// The second attached body.
	b2BodyId bodyIdB;

	/// The local anchor point relative to bodyA's origin.
	b2Vec2 localAnchorA;

	/// The local anchor point relative to bodyB's origin.
	b2Vec2 localAnchorB;

	/// The local translation unit axis in bodyA.
	b2Vec2 localAxisA;

	/// Enable/disable the joint limit.
	bool enableLimit;

	/// The lower translation limit, usually in meters.
	float lowerTranslation;

	/// The upper translation limit, usually in meters.
	float upperTranslation;

	/// Enable/disable the joint motor.
	bool enableMotor;

	/// The maximum motor torque, usually in N-m.
	float maxMotorTorque;

	/// The desired motor speed in radians per second.
	float motorSpeed;

	/// The linear stiffness in N/m
	float stiffness;

	/// The linear damping in N*s/m
	float damping;

	/// Set this flag to true if the attached bodies should collide.
	bool collideConnected;
} b2WheelJointDef;

/// Use this to initialize your joint definition
static inline b2WheelJointDef b2DefaultWheelJointDef(void)
{
	b2WheelJointDef def = B2_ZERO_INIT;
	def.bodyIdA = b2_nullBodyId;
	def.bodyIdB = b2_nullBodyId;
	def.localAnchorA = B2_LITERAL(b2Vec2){0.0f, 0.0f};
	def.localAnchorB = B2_LITERAL(b2Vec2){0.0f, 0.0f};
	def.localAxisA = B2_LITERAL(b2Vec2){1.0f, 0.0f};
	def.enableLimit = false;
	def.lowerTranslation = 0.0f;
	def.upperTranslation = 0.0f;
	def.enableMotor = false;
	def.maxMotorTorque = 0.0f;
	def.motorSpeed = 0.0f;
	def.stiffness = 0.0f;
	def.damping = 0.0f;
	def.collideConnected = false;
	return def;
}
