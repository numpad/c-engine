// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "box2d/api.h"
#include "box2d/callbacks.h"
#include "box2d/event_types.h"
#include "box2d/geometry.h"
#include "box2d/id.h"
#include "box2d/joint_types.h"
#include "box2d/types.h"

typedef struct b2Capsule b2Capsule;
typedef struct b2Circle b2Circle;
typedef struct b2Polygon b2Polygon;
typedef struct b2DebugDraw b2DebugDraw;
typedef struct b2Segment b2Segment;

/**
 * \defgroup WorldAPI Worlds
 * This is the main Box2D API. With this API you can create a simulation world. You can then add bodies and
 * joints to the world and run the simulation. You can get contact information to get contact points
 * and normals as well as events. You can query to world, checking for overlaps and casting rays or shapes.
 * There is also debugging information such as debug draw, timing information, and counters.
 * @{
 */

/// Create a world for rigid body simulation. This contains all the bodies, shapes, and constraints.
BOX2D_API b2WorldId b2CreateWorld(const b2WorldDef* def);

/// Destroy a world.
BOX2D_API void b2DestroyWorld(b2WorldId worldId);

/// Take a time step. This performs collision detection, integration,
/// and constraint solution.
/// @param timeStep the amount of time to simulate, this should not vary.
/// @param velocityIterations for the velocity constraint solver.
/// @param relaxIterations for reducing constraint bounce solver.
BOX2D_API void b2World_Step(b2WorldId worldId, float timeStep, int32_t velocityIterations, int32_t relaxIterations);

/// Call this to draw shapes and other debug draw data. This is intentionally non-const.
BOX2D_API void b2World_Draw(b2WorldId worldId, b2DebugDraw* debugDraw);

/// Get sensor events for the current time step. The event data is transient. Do not store a reference to this data.
BOX2D_API b2SensorEvents b2World_GetSensorEvents(b2WorldId worldId);

/// Get contact events for this current time step. The event data is transient. Do not store a reference to this data.
BOX2D_API b2ContactEvents b2World_GetContactEvents(b2WorldId worldId);

/// Query the world for all shapes that potentially overlap the provided AABB.
BOX2D_API void b2World_QueryAABB(b2WorldId worldId, b2QueryResultFcn* fcn, b2AABB aabb, b2QueryFilter filter, void* context);

/// Query the world for all shapes that overlap the provided circle.
BOX2D_API void b2World_OverlapCircle(b2WorldId worldId, b2QueryResultFcn* fcn, const b2Circle* circle, b2Transform transform,
									 b2QueryFilter filter, void* context);

/// Query the world for all shapes that overlap the provided capsule.
BOX2D_API void b2World_OverlapCapsule(b2WorldId worldId, b2QueryResultFcn* fcn, const b2Capsule* capsule, b2Transform transform,
									  b2QueryFilter filter, void* context);

/// Query the world for all shapes that overlap the provided polygon.
BOX2D_API void b2World_OverlapPolygon(b2WorldId worldId, b2QueryResultFcn* fcn, const b2Polygon* polygon, b2Transform transform,
									  b2QueryFilter filter, void* context);

/// Ray-cast the world for all shapes in the path of the ray. Your callback
/// controls whether you get the closest point, any point, or n-points.
/// The ray-cast ignores shapes that contain the starting point.
/// @param callback a user implemented callback class.
/// @param point1 the ray starting point
/// @param point2 the ray ending point
BOX2D_API void b2World_RayCast(b2WorldId worldId, b2Vec2 origin, b2Vec2 translation, b2QueryFilter filter, b2RayResultFcn* fcn,
							   void* context);

/// Ray-cast closest hit. Convenience function. This is less general than b2World_RayCast and does not allow for custom filtering.
BOX2D_API b2RayResult b2World_RayCastClosest(b2WorldId worldId, b2Vec2 origin, b2Vec2 translation, b2QueryFilter filter);

/// Cast a circle through the world. Similar to a ray-cast except that a circle is cast instead of a point.
BOX2D_API void b2World_CircleCast(b2WorldId worldId, const b2Circle* circle, b2Transform originTransform, b2Vec2 translation,
								  b2QueryFilter filter, b2RayResultFcn* fcn, void* context);

/// Cast a capsule through the world. Similar to a ray-cast except that a capsule is cast instead of a point.
BOX2D_API void b2World_CapsuleCast(b2WorldId worldId, const b2Capsule* capsule, b2Transform originTransform, b2Vec2 translation,
								   b2QueryFilter filter, b2RayResultFcn* fcn, void* context);

/// Cast a capsule through the world. Similar to a ray-cast except that a polygon is cast instead of a point.
BOX2D_API void b2World_PolygonCast(b2WorldId worldId, const b2Polygon* polygon, b2Transform originTransform, b2Vec2 translation,
								   b2QueryFilter filter, b2RayResultFcn* fcn, void* context);

/// Enable/disable sleep. Advanced feature for testing.
BOX2D_API void b2World_EnableSleeping(b2WorldId worldId, bool flag);

/// Enable/disable constraint warm starting. Advanced feature for testing.
BOX2D_API void b2World_EnableWarmStarting(b2WorldId worldId, bool flag);

/// Enable/disable continuous collision. Advanced feature for testing.
BOX2D_API void b2World_EnableContinuous(b2WorldId worldId, bool flag);

/// Adjust the restitution threshold. Advanced feature for testing.
BOX2D_API void b2World_SetRestitutionThreshold(b2WorldId worldId, float value);

/// Adjust contact tuning parameters:
/// - hertz is the contact stiffness (cycles per second)
/// - damping ratio is the contact bounciness with 1 being critical damping (non-dimensional)
/// - push velocity is the maximum contact constraint push out velocity (meters per second)
///	Advanced feature
BOX2D_API void b2World_SetContactTuning(b2WorldId worldId, float hertz, float dampingRatio, float pushVelocity);

/// Get the current profile
BOX2D_API struct b2Profile b2World_GetProfile(b2WorldId worldId);

/// Get counters and sizes
BOX2D_API struct b2Statistics b2World_GetStatistics(b2WorldId worldId);

/** @} */

/**
 * \defgroup BodyAPI Bodies
 * This is the body API.
 * @{
 */

/// Create a rigid body given a definition. No reference to the definition is retained.
/// @warning This function is locked during callbacks.
BOX2D_API b2BodyId b2CreateBody(b2WorldId worldId, const b2BodyDef* def);

/// Destroy a rigid body given an id.
/// @warning This function is locked during callbacks.
BOX2D_API void b2DestroyBody(b2BodyId bodyId);

/// Get the type of a body
BOX2D_API b2BodyType b2Body_GetType(b2BodyId bodyId);

/// Set the type of a body. This has a similar cost to re-creating the body.
BOX2D_API void b2Body_SetType(b2BodyId bodyId, b2BodyType type);

/// Get the user data stored in a body
BOX2D_API void* b2Body_GetUserData(b2BodyId bodyId);

/// Get the world position of a body. This is the location of the body origin.
BOX2D_API b2Vec2 b2Body_GetPosition(b2BodyId bodyId);

/// Get the world angle of a body in radians.
BOX2D_API float b2Body_GetAngle(b2BodyId bodyId);

/// Get the world transform of a body.
BOX2D_API b2Transform b2Body_GetTransform(b2BodyId bodyId);

/// Set the world transform of a body. This acts as a teleport and is fairly expensive.
BOX2D_API void b2Body_SetTransform(b2BodyId bodyId, b2Vec2 position, float angle);

/// Get a local point on a body given a world point
BOX2D_API b2Vec2 b2Body_GetLocalPoint(b2BodyId bodyId, b2Vec2 worldPoint);

/// Get a world point on a body given a local point
BOX2D_API b2Vec2 b2Body_GetWorldPoint(b2BodyId bodyId, b2Vec2 localPoint);

/// Get a local vector on a body given a world vector
BOX2D_API b2Vec2 b2Body_GetLocalVector(b2BodyId bodyId, b2Vec2 worldVector);

/// Get a world vector on a body given a local vector
BOX2D_API b2Vec2 b2Body_GetWorldVector(b2BodyId bodyId, b2Vec2 localVector);

/// Get the linear velocity of a body's center of mass
BOX2D_API b2Vec2 b2Body_GetLinearVelocity(b2BodyId bodyId);

/// Get the angular velocity of a body in radians per second
BOX2D_API float b2Body_GetAngularVelocity(b2BodyId bodyId);

/// Set the linear velocity of a body
BOX2D_API void b2Body_SetLinearVelocity(b2BodyId bodyId, b2Vec2 linearVelocity);

/// Set the angular velocity of a body in radians per second
BOX2D_API void b2Body_SetAngularVelocity(b2BodyId bodyId, float angularVelocity);

/// Apply a force at a world point. If the force is not
/// applied at the center of mass, it will generate a torque and
/// affect the angular velocity. This wakes up the body.
/// @param force the world force vector, usually in Newtons (N).
/// @param point the world position of the point of application.
/// @param wake also wake up the body
BOX2D_API void b2Body_ApplyForce(b2BodyId bodyId, b2Vec2 force, b2Vec2 point, bool wake);

/// Apply a force to the center of mass. This wakes up the body.
/// @param force the world force vector, usually in Newtons (N).
/// @param wake also wake up the body
BOX2D_API void b2Body_ApplyForceToCenter(b2BodyId bodyId, b2Vec2 force, bool wake);

/// Apply a torque. This affects the angular velocity
/// without affecting the linear velocity of the center of mass.
/// @param torque about the z-axis (out of the screen), usually in N-m.
/// @param wake also wake up the body
BOX2D_API void b2Body_ApplyTorque(b2BodyId bodyId, float torque, bool wake);

/// Apply an impulse at a point. This immediately modifies the velocity.
/// It also modifies the angular velocity if the point of application
/// is not at the center of mass. This wakes up the body.
/// @param impulse the world impulse vector, usually in N-seconds or kg-m/s.
/// @param point the world position of the point of application.
/// @param wake also wake up the body
BOX2D_API void b2Body_ApplyLinearImpulse(b2BodyId bodyId, b2Vec2 impulse, b2Vec2 point, bool wake);

/// Apply an impulse to the center of mass. This immediately modifies the velocity.
/// @param impulse the world impulse vector, usually in N-seconds or kg-m/s.
/// @param wake also wake up the body
BOX2D_API void b2Body_ApplyLinearImpulseToCenter(b2BodyId bodyId, b2Vec2 impulse, bool wake);

/// Apply an angular impulse.
/// @param impulse the angular impulse in units of kg*m*m/s
/// @param wake also wake up the body
BOX2D_API void b2Body_ApplyAngularImpulse(b2BodyId bodyId, float impulse, bool wake);

/// Get the mass of the body (kilograms)
BOX2D_API float b2Body_GetMass(b2BodyId bodyId);

/// Get the inertia tensor of the body. In 2D this is a single number. (kilograms * meters^2)
BOX2D_API float b2Body_GetInertiaTensor(b2BodyId bodyId);

/// Get the center of mass position of the body in local space.
BOX2D_API b2Vec2 b2Body_GetLocalCenterOfMass(b2BodyId bodyId);

/// Get the center of mass position of the body in world space.
BOX2D_API b2Vec2 b2Body_GetWorldCenterOfMass(b2BodyId bodyId);

/// Override the body's mass properties. Normally this is computed automatically using the
///	shape geometry and density. This information is lost if a shape is added or removed or if the
///	body type changes.
BOX2D_API void b2Body_SetMassData(b2BodyId bodyId, b2MassData massData);

/// Is this body awake?
BOX2D_API bool b2Body_IsAwake(b2BodyId bodyId);

/// Wake a body from sleep. This wakes the entire island the body is touching.
BOX2D_API void b2Body_Wake(b2BodyId bodyId);

/// Is this body enabled?
BOX2D_API bool b2Body_IsEnabled(b2BodyId bodyId);

/// Disable a body by removing it completely from the simulation
BOX2D_API void b2Body_Disable(b2BodyId bodyId);

/// Enable a body by adding it to the simulation
BOX2D_API void b2Body_Enable(b2BodyId bodyId);

/// Iterate over shapes on a body
BOX2D_API b2ShapeId b2Body_GetFirstShape(b2BodyId bodyId);
BOX2D_API b2ShapeId b2Body_GetNextShape(b2ShapeId shapeId);

/// Get the maximum capacity required for retrieving all the touching contacts on a body
BOX2D_API int32_t b2Body_GetContactCapacity(b2BodyId bodyId);

/// Get the touching contact data for a body
BOX2D_API int32_t b2Body_GetContactData(b2BodyId bodyId, b2ContactData* contactData, int32_t capacity);

/** @} */

/**
 * \defgroup ShapeAPI Shapes
 * This is the shape API.
 * @{
 */

/// Create a circle shape and attach it to a body. The shape defintion and geometry are fully cloned.
/// Contacts are not created until the next time step.
///	@return the shape id for accessing the shape
BOX2D_API b2ShapeId b2CreateCircleShape(b2BodyId bodyId, const b2ShapeDef* def, const b2Circle* circle);

/// Create a line segment shape and attach it to a body. The shape defintion and geometry are fully cloned.
/// Contacts are not created until the next time step.
///	@return the shape id for accessing the shape
BOX2D_API b2ShapeId b2CreateSegmentShape(b2BodyId bodyId, const b2ShapeDef* def, const b2Segment* segment);

/// Create a capsule shape and attach it to a body. The shape defintion and geometry are fully cloned.
/// Contacts are not created until the next time step.
///	@return the shape id for accessing the shape
BOX2D_API b2ShapeId b2CreateCapsuleShape(b2BodyId bodyId, const b2ShapeDef* def, const b2Capsule* capsule);

/// Create a polygon shape and attach it to a body. The shape defintion and geometry are fully cloned.
/// Contacts are not created until the next time step.
///	@return the shape id for accessing the shape
BOX2D_API b2ShapeId b2CreatePolygonShape(b2BodyId bodyId, const b2ShapeDef* def, const b2Polygon* polygon);

/// Destroy any shape type
BOX2D_API void b2DestroyShape(b2ShapeId shapeId);

/// Create a chain shape
///	@see b2ChainDef for details
BOX2D_API b2ChainId b2CreateChain(b2BodyId bodyId, const b2ChainDef* def);

/// Destroy a chain shape
BOX2D_API void b2DestroyChain(b2ChainId chainId);

/// Get the body that a shape is attached to
BOX2D_API b2BodyId b2Shape_GetBody(b2ShapeId shapeId);

/// Get the user data for a shape. This is useful when you get a shape id
///	from an event or query
BOX2D_API void* b2Shape_GetUserData(b2ShapeId shapeId);

/// Test a point for overlap with a shape
BOX2D_API bool b2Shape_TestPoint(b2ShapeId shapeId, b2Vec2 point);

/// Set the friction on a shape. Normally this is specified in b2ShapeDef.
BOX2D_API void b2Shape_SetFriction(b2ShapeId shapeId, float friction);

/// Set the restitution (bounciness) on a shape. Normally this is specified in b2ShapeDef.
BOX2D_API void b2Shape_SetRestitution(b2ShapeId shapeId, float restitution);

/// Get the type of a shape.
BOX2D_API b2ShapeType b2Shape_GetType(b2ShapeId shapeId);

/// Access the circle geometry of a shape.
BOX2D_API const b2Circle* b2Shape_GetCircle(b2ShapeId shapeId);

/// Access the line segment geometry of a shape.
BOX2D_API const b2Segment* b2Shape_GetSegment(b2ShapeId shapeId);

/// Access the smooth line segment geometry of a shape. These come from chain shapes.
BOX2D_API const b2SmoothSegment* b2Shape_GetSmoothSegment(b2ShapeId shapeId);

/// Access the capsule geometry of a shape.
BOX2D_API const b2Capsule* b2Shape_GetCapsule(b2ShapeId shapeId);

/// Access the convex polygon geometry of a shape.
BOX2D_API const b2Polygon* b2Shape_GetPolygon(b2ShapeId shapeId);

/// Set the friction of a chain. Normally this is set in b2ChainDef.
BOX2D_API void b2Chain_SetFriction(b2ChainId chainId, float friction);

/// Set the restitution (bounciness) on a chain. Normally this is specified in b2ChainDef.
BOX2D_API void b2Chain_SetRestitution(b2ChainId chainId, float restitution);

/// Get the maximum capacity required for retrieving all the touching contacts on a shape
BOX2D_API int32_t b2Shape_GetContactCapacity(b2ShapeId shapeId);

/// Get the touching contact data for a shape. The provided shapeId will be either shapeIdA or shapeIdB on the contact data.
BOX2D_API int32_t b2Shape_GetContactData(b2ShapeId shapeId, b2ContactData* contactData, int32_t capacity);

/** @} */

/**
 * \defgroup JointAPI Joints
 * This is the joint API.
 * @{
 */

/// Create a distance joint
///	@see b2DistanceJointDef for details
BOX2D_API b2JointId b2CreateDistanceJoint(b2WorldId worldId, const b2DistanceJointDef* def);

/// Create a motor joint
///	@see b2MotorJointDef for details
BOX2D_API b2JointId b2CreateMotorJoint(b2WorldId worldId, const b2MotorJointDef* def);

/// Create a mouse joint
///	@see b2MouseJointDef for details
BOX2D_API b2JointId b2CreateMouseJoint(b2WorldId worldId, const b2MouseJointDef* def);

/// Create a prismatic (slider) joint
///	@see b2PrismaticJointDef for details
BOX2D_API b2JointId b2CreatePrismaticJoint(b2WorldId worldId, const b2PrismaticJointDef* def);

/// Create a revolute (hinge) joint
///	@see b2RevoluteJointDef for details
BOX2D_API b2JointId b2CreateRevoluteJoint(b2WorldId worldId, const b2RevoluteJointDef* def);

/// Create a weld joint
///	@see b2WeldJointDef for details
BOX2D_API b2JointId b2CreateWeldJoint(b2WorldId worldId, const b2WeldJointDef* def);

/// Create a wheel joint
///	@see b2WheelJointDef for details
BOX2D_API b2JointId b2CreateWheelJoint(b2WorldId worldId, const b2WheelJointDef* def);

/// Destroy any joint type
BOX2D_API void b2DestroyJoint(b2JointId jointId);

/// Get body A on a joint
BOX2D_API b2BodyId b2Joint_GetBodyA(b2JointId jointId);

/// Get body B on a joint
BOX2D_API b2BodyId b2Joint_GetBodyB(b2JointId jointId);

/// Get the constraint force on a distance joint
BOX2D_API float b2DistanceJoint_GetConstraintForce(b2JointId jointId, float timeStep);

/// Set the length parameters of a distance joint
///	@see b2DistanceJointDef for details
BOX2D_API void b2DistanceJoint_SetLength(b2JointId jointId, float length, float minLength, float maxLength);

/// Get the current length of a distance joint
BOX2D_API float b2DistanceJoint_GetCurrentLength(b2JointId jointId);

/// Adjust the softness of a distance joint
///	@see b2DistanceJointDef for details
BOX2D_API void b2DistanceJoint_SetTuning(b2JointId jointId, float hertz, float dampingRatio);

/// Set the linear offset target for a motor joint
BOX2D_API void b2MotorJoint_SetLinearOffset(b2JointId jointId, b2Vec2 linearOffset);

/// Set the angular offset target for a motor joint in radians
BOX2D_API void b2MotorJoint_SetAngularOffset(b2JointId jointId, float angularOffset);

/// Set the maximum force for a motor joint
BOX2D_API void b2MotorJoint_SetMaxForce(b2JointId jointId, float maxForce);

/// Set the maximum torque for a motor joint
BOX2D_API void b2MotorJoint_SetMaxTorque(b2JointId jointId, float maxTorque);

/// Set the correction factor for a motor joint
BOX2D_API void b2MotorJoint_SetCorrectionFactor(b2JointId jointId, float correctionFactor);

/// Get the current constraint force for a motor joint
BOX2D_API b2Vec2 b2MotorJoint_GetConstraintForce(b2JointId jointId, float inverseTimeStep);

/// Get the current constraint torque for a motor joint
BOX2D_API float b2MotorJoint_GetConstraintTorque(b2JointId jointId, float inverseTimeStep);

/// Set the target for a mouse joint
BOX2D_API void b2MouseJoint_SetTarget(b2JointId jointId, b2Vec2 target);

/// Enable/disable a prismatic joint limit
BOX2D_API void b2PrismaticJoint_EnableLimit(b2JointId jointId, bool enableLimit);

/// Enable/disable a prismatic joint motor
BOX2D_API void b2PrismaticJoint_EnableMotor(b2JointId jointId, bool enableMotor);

/// Set the motor speed for a prismatic joint
BOX2D_API void b2PrismaticJoint_SetMotorSpeed(b2JointId jointId, float motorSpeed);

/// Get the current motor force for a prismatic joint
BOX2D_API float b2PrismaticJoint_GetMotorForce(b2JointId jointId, float inverseTimeStep);

/// Set the maximum force for a pristmatic joint motor
BOX2D_API void b2PrismaticJoint_SetMaxMotorForce(b2JointId jointId, float force);

/// Get the current constraint force for a prismatic joint
BOX2D_API b2Vec2 b2PrismaticJoint_GetConstraintForce(b2JointId jointId, float inverseTimeStep);

/// Get the current constraint torque for a prismatic joint
BOX2D_API float b2PrismaticJoint_GetConstraintTorque(b2JointId jointId, float inverseTimeStep);

/// Enable/disable a revolute joint limit
BOX2D_API void b2RevoluteJoint_EnableLimit(b2JointId jointId, bool enableLimit);

/// Enable/disable a revolute joint motor
BOX2D_API void b2RevoluteJoint_EnableMotor(b2JointId jointId, bool enableMotor);

/// Set the motor speed for a revolute joint in radians per second
BOX2D_API void b2RevoluteJoint_SetMotorSpeed(b2JointId jointId, float motorSpeed);

/// Get the current motor torque for a revolute joint
BOX2D_API float b2RevoluteJoint_GetMotorTorque(b2JointId jointId, float inverseTimeStep);

/// Set the maximum torque for a revolute joint motor
BOX2D_API void b2RevoluteJoint_SetMaxMotorTorque(b2JointId jointId, float torque);

/// Get the current constraint force for a revolute joint
BOX2D_API b2Vec2 b2RevoluteJoint_GetConstraintForce(b2JointId jointId, float inverseTimeStep);

/// Get the current constraint torque for a revolute joint
BOX2D_API float b2RevoluteJoint_GetConstraintTorque(b2JointId jointId, float inverseTimeStep);

/// Set the wheel joint stiffness
BOX2D_API void b2WheelJoint_SetStiffness(b2JointId jointId, float stiffness);

/// Set the wheel joint damping
BOX2D_API void b2WheelJoint_SetDamping(b2JointId jointId, float damping);

/// Enable/disable the wheel joint limit
BOX2D_API void b2WheelJoint_EnableLimit(b2JointId jointId, bool enableLimit);

/// Enable/disable the wheel joint motor
BOX2D_API void b2WheelJoint_EnableMotor(b2JointId jointId, bool enableMotor);

/// Set the wheel joint motor speed in radians per second
BOX2D_API void b2WheelJoint_SetMotorSpeed(b2JointId jointId, float motorSpeed);

/// Get the wheel joint current motor torque
BOX2D_API float b2WheelJoint_GetMotorTorque(b2JointId jointId, float inverseTimeStep);

/// Set the wheel joint maximum motor torque
BOX2D_API void b2WheelJoint_SetMaxMotorTorque(b2JointId jointId, float torque);

/// Get the current wheel joint constraint force
BOX2D_API b2Vec2 b2WheelJoint_GetConstraintForce(b2JointId jointId, float inverseTimeStep);

/// Get the current wheel joint constraint torque
BOX2D_API float b2WheelJoint_GetConstraintTorque(b2JointId jointId, float inverseTimeStep);

/** @} */
