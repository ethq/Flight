#pragma once

#include "Utilities.h"
#include "Timer.h"
#include "Camera.h"

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
	
	void Yaw(STEER yaw)
	{
		mYawing = yaw;
	}

	void Roll(STEER roll)
	{
		mRolling = roll;
	}

	void Pitch(STEER pitch)
	{
		mPitching = pitch;
	}

	void Pitch(float d)
	{
		mPitch += d;
	}

	void Yaw(float d)
	{
		mYaw += d;
	}

	void Roll(float d)
	{
		mRoll += d;
	}

	void Thrust(bool thrust)
	{
		mIsAccelerating = thrust;
	}

	void Reverse(bool rev)
	{
		mIsReversing = true;
	}
	void Reverse()
	{
		mIsReversing = !mIsReversing;
	}

	/*

	Roll is about nose-axis (Z)
	Yaw is about vertical axis (Y)
	Pitch is about transverse axis (X)

	*/
	void Update(const Timer& t)
	{		
		if (mPitching != STEER::NONE)
			mPitch += ((mPitching == STEER::POSITIVE) ? 1.0f : -1.0f) * t.DeltaTime();
		if (mYawing != STEER::NONE)
			mYaw += ((mYawing == STEER::POSITIVE) ? 1.0f : -1.0f) * t.DeltaTime();
		if (mRolling != STEER::NONE)
			mRoll += ((mRolling == STEER::POSITIVE) ? 1.0f : -1.0f) * t.DeltaTime();

		UpdatePosition(); // todo use a dirty flag here; we dont want to needlessly spam these calculations
	}

	void UpdatePosition();

	DirectX::XMMATRIX View()
	{
		return DirectX::XMLoadFloat4x4(&mView);
	}

	// This is not good. Not good. We assume [v] has xaxis in row0 etc, but the view matrix we STORE(mView) is INVERTED. That's why 
	// regular rendering using this camera works, and yet the shadow rendering doesn't.
	void SetView(DirectX::XMMATRIX v)
	{
		// we reconstruct the view every update()
		auto& x = v.r[0];
		auto& y = v.r[1];
		auto& z = v.r[2];
		auto& pos = v.r[3];

		DirectX::XMStoreFloat4(&mAxisX, x);
		DirectX::XMStoreFloat4(&mAxisY, y);
		DirectX::XMStoreFloat4(&mAxisZ, z);
		DirectX::XMStoreFloat4(&mPos, pos);
	}

	float X()
	{
		return mPos.x;
	}
	float Y()
	{
		return mPos.y;
	}
	float Z()
	{
		return mPos.z;
	}

	DirectX::XMFLOAT3 GetPos3f()
	{
		return { mPos.x, mPos.y, mPos.z };
	}

private:
	STEER mYawing = STEER::NONE;
	STEER mRolling = STEER::NONE;
	STEER mPitching = STEER::NONE;
	bool mIsAccelerating = false;

	float mPitch = 0.0f;
	float mYaw = 0.0f;
	float mRoll = 0.0f;

	bool mIsReversing = false;

	DirectX::XMFLOAT4 mPos = { 0.0f, 2.0f, -15.0f, 1.0f };

	DirectX::XMFLOAT4X4 mView;

	DirectX::XMFLOAT4 mAxisX = { 1.0f, 0.0f, 0.0f, 0.0f};
	DirectX::XMFLOAT4 mAxisY = { 0.0f, 1.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4 mAxisZ = { 0.0f, 0.0f, 1.0f, 0.0f };
};

