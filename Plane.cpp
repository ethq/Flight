#include "Plane.h"

using namespace DirectX;

Plane::Plane()
{
	// Returns an acceleration vector given pos/vel
	mAcceleration = [this](DirectX::XMVECTOR pos, DirectX::XMVECTOR vel, float t) 
	{
		const static XMVECTOR worldUp = { 0.0f, 1.0f, 0.0f };

		auto rollAxis = XMLoadFloat4(&mAxisZ);
		auto yawAxis = XMLoadFloat4(&mAxisY);
		auto pitchAxis = XMLoadFloat4(&mAxisX);

		// 
		auto lift = 1.0f / mMass * mLiftCoef * XMVector3Dot(rollAxis, vel) * yawAxis;
		auto gravity = -mGravity * worldUp;
		auto accel = mAccelRate * rollAxis;
		auto drag = -mDragCoef / mMass * vel * XMVector3Length(vel);


		return lift + gravity + accel + drag;
	};
}

void Plane::AddPhysicsBody(rp3d::RigidBody *const rb)
{
	mPhysicsBody = rb;
}

void Plane::Yaw(STEER yaw)
{
	mYawing = yaw;
}

void Plane::Roll(STEER roll)
{
	mRolling = roll;
}

void Plane::Pitch(STEER pitch)
{
	mPitching = pitch;
}

void Plane::Pitch(float d)
{
	mPitch += d;
}

void Plane::Yaw(float d)
{
	mYaw += d;
}

void Plane::Roll(float d)
{
	mRoll += d;
}

void Plane::Thrust(bool thrust)
{
	mIsAccelerating = thrust;
}

void Plane::Reverse(bool rev)
{
	mIsReversing = true;
}
void Plane::Reverse()
{
	mIsReversing = !mIsReversing;
}

/*

Roll is about nose-axis (Z)
Yaw is about vertical axis (Y)
Pitch is about transverse axis (X)

*/
void Plane::Update(const Timer& t)
{
	if (mPitching != STEER::NONE)
		mPitch += ((mPitching == STEER::POSITIVE) ? 1.0f : -1.0f) * t.DeltaTime();
	if (mYawing != STEER::NONE)
		mYaw += ((mYawing == STEER::POSITIVE) ? 1.0f : -1.0f) * t.DeltaTime();
	if (mRolling != STEER::NONE)
		mRoll += ((mRolling == STEER::POSITIVE) ? 1.0f : -1.0f) * t.DeltaTime();

	UpdatePosition(); // todo use a dirty flag here; we dont want to needlessly spam these calculations
}

XMMATRIX Plane::View()
{
	auto& currTransform = mPhysicsBody->getTransform();
	if (currTransform != mTransform)
	{
		mTransform = currTransform;
		UpdateViewMatrix();
	}

	return DirectX::XMLoadFloat4x4(&mView);
}

// This is not good. Not good. We assume [v] has xaxis in row0 etc, but the view matrix we STORE(mView) is INVERTED. That's why 
// regular rendering using this camera works, and yet the shadow rendering doesn't.
void Plane::SetView(DirectX::XMMATRIX v)
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

float Plane::X()
{
	return mPos.x;
}
float Plane::Y()
{
	return mPos.y;
}
float Plane::Z()
{
	return mPos.z;
}

DirectX::XMFLOAT3 Plane::GetPos3f()
{
	return { mPos.x, mPos.y, mPos.z };
}

void Plane::UpdateViewMatrix()
{
	auto orientation = mTransform.getOrientation().getMatrix();
	auto position = mTransform.getPosition();

	rp3d::Vector3 xax(mAxisX.x, mAxisX.y, mAxisX.z);
	rp3d::Vector3 yax(mAxisY.x, mAxisY.y, mAxisY.z);
	rp3d::Vector3 zax(mAxisZ.x, mAxisZ.y, mAxisZ.z);

	xax = orientation * xax;
	yax = orientation * yax;
	zax = orientation * zax;

	mAxisX.x = xax.x;
	mAxisX.y = xax.y;
	mAxisX.z = xax.z;

	mAxisY.x = yax.x;
	mAxisY.y = yax.y;
	mAxisY.z = yax.z;

	mAxisZ.x = zax.x;
	mAxisZ.y = zax.y;
	mAxisZ.z = zax.z;

	mPos.x = position.x;
	mPos.y = position.y;
	mPos.z = position.z;

	auto dxpos = XMLoadFloat4(&mPos);
	auto dxxax = XMLoadFloat4(&mAxisX);
	auto dxyax = XMLoadFloat4(&mAxisY);
	auto dxzax = XMLoadFloat4(&mAxisZ);

	DirectX::XMMATRIX view = DirectX::XMMATRIX(dxxax, dxyax, dxzax, dxpos);
	auto det = DirectX::XMMatrixDeterminant(view);
	DirectX::XMStoreFloat4x4(&mView, DirectX::XMMatrixInverse(&det, view)); // maybe dont need transpose if using colmajor but well see

	float pAddZ = 10.0f;
	float pAddY = 5.0f;
	auto planepos = dxpos;
	planepos += pAddZ * dxzax;
	planepos -= pAddY * dxyax;
	auto pview = XMMATRIX(-1.0f * dxxax, dxyax, -1.0f * dxzax, planepos);

	if (mBodyRenderItem)
		XMStoreFloat4x4(&mBodyRenderItem->Instance(0).World, pview);
	if (mCollisionMesh)
		XMStoreFloat4x4(&mCollisionMesh->Instance(0).World, pview);
}

void Plane::UpdatePosition()
{
	// Handle rotations first
	auto xax = DirectX::XMLoadFloat4(&mAxisX);
	auto yax = DirectX::XMLoadFloat4(&mAxisY);
	auto zax = DirectX::XMLoadFloat4(&mAxisZ);

	DirectX::XMMATRIX P = DirectX::XMMatrixIdentity();
	DirectX::XMMATRIX Y = DirectX::XMMatrixIdentity();
	DirectX::XMMATRIX R = DirectX::XMMatrixIdentity();

	P = DirectX::XMMatrixRotationAxis(xax, mPitch);
	Y = DirectX::XMMatrixRotationAxis(yax, mYaw);
	R = DirectX::XMMatrixRotationAxis(zax, mRoll);

	// Experiment with order... or disable all but one?
	DirectX::XMMATRIX T = P * R * Y;

	xax = XMVector3Normalize(XMVector4Transform(xax, T));
	yax = XMVector3Normalize(XMVector4Transform(yax, T));
	zax = XMVector3Normalize(XMVector4Transform(zax, T));

	DirectX::XMStoreFloat4(&mAxisX, xax);
	DirectX::XMStoreFloat4(&mAxisY, yax);
	DirectX::XMStoreFloat4(&mAxisZ, zax);

	// Update position

	// acceleration modulation hack
	float accel = 1.1f;
	auto pos = DirectX::XMLoadFloat4(&mPos);

	if (mIsAccelerating)
	{
		if (!mIsReversing)
			pos = DirectX::XMVectorAdd(pos, accel*zax); 
		else
			pos = DirectX::XMVectorSubtract(pos, accel*zax);
		DirectX::XMStoreFloat4(&mPos, pos);
	}

	// Create view matrix [right, up, forward, pos]
	DirectX::XMMATRIX view = DirectX::XMMATRIX(xax, yax, zax, pos);
	auto det = DirectX::XMMatrixDeterminant(view);
	DirectX::XMStoreFloat4x4(&mView, DirectX::XMMatrixInverse(&det, view)); // maybe dont need transpose if using colmajor but well see

	float pAddZ = 10.0f;
	float pAddY = 5.0f;
	auto planepos = pos;
	planepos += pAddZ * zax;
	planepos -= pAddY * yax;
	auto pview = XMMATRIX(-1.0f*xax, yax, -1.0f*zax, planepos);

	if (mBodyRenderItem)
		XMStoreFloat4x4(&mBodyRenderItem->Instance(0).World, pview);
	if (mCollisionMesh)
		XMStoreFloat4x4(&mCollisionMesh->Instance(0).World, pview);

	mPitch = 0.0f;
	mYaw = 0.0f;
	mRoll = 0.0f;
}

void Plane::AddBody(std::shared_ptr<RenderItem> pri)
{
	mBodyRenderItem = pri;
}
void Plane::AddCollisionMesh(std::shared_ptr<RenderItem> pri)
{
	mCollisionMesh = pri;
}