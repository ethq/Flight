#pragma once

#include "Utilities.h"
#include "Timer.h"
#include "RenderItem.h"
#include <functional>
#include "reactphysics3d.h"

/*
Long time plans: at frame start, we read keyboard/mouse input and set corresp variables
                 second, the physics engine reads input variables, calculates forces etc and sets object positions/velocities
				 lastly, the rendering engine draws it all
*/

class Plane
{
public:
	enum class STEER
	{
		POSITIVE,
		NEGATIVE,
		NONE
	};

	Plane();
	
	void Pitch(STEER);
	void Pitch(float);

	void Yaw(STEER);
	void Yaw(float);

	void Roll(STEER);
	void Roll(float);

	void Thrust(bool);
	void Reverse(bool);
	void Reverse();

	void Update(const Timer&);
	void UpdatePosition();

	DirectX::XMMATRIX View();
	void SetView(DirectX::XMMATRIX);

	DirectX::XMFLOAT3 GetPos3f();

	float X();
	float Y();
	float Z();

	void AddBody(std::shared_ptr<RenderItem>);
	void AddCollisionMesh(std::shared_ptr<RenderItem>);
	void AddPhysicsBody(rp3d::RigidBody *const);

private:
	void UpdateViewMatrix();

private:
	std::shared_ptr<RenderItem> mBodyRenderItem = nullptr;
	std::shared_ptr<RenderItem> mCollisionMesh = nullptr;
	STEER mYawing = STEER::NONE;
	STEER mRolling = STEER::NONE;
	STEER mPitching = STEER::NONE;
	bool mIsAccelerating = false;

	float mPitch = 0.0f;
	float mYaw = 0.0f;
	float mRoll = 0.0f;

	bool mIsReversing = false;

	DirectX::XMFLOAT4 mPos = { 0.0f, 5.0f, -30.0f, 1.0f };

	DirectX::XMFLOAT4X4 mView;
	DirectX::XMFLOAT4X4 mPlaneView1;

	DirectX::XMFLOAT4 mAxisX = { 1.0f, 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4 mAxisY = { 0.0f, 1.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4 mAxisZ = { 0.0f, 0.0f, 1.0f, 0.0f };

	std::function<DirectX::XMVECTOR(DirectX::XMVECTOR, DirectX::XMVECTOR, float)> mAcceleration;
	
	const float mMass = 1.0f;
	const float mLiftCoef = 1.0f;
	const float mDragCoef = 1.0f;
	const float mGravity = 1.0f;
	
	float mAccelMin = 0.0f;
	float mAccelMax = 5.0f;
	float mAccelRate = 1.0f;

	rp3d::RigidBody* mPhysicsBody;
	rp3d::Transform mTransform;
};

