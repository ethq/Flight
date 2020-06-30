#include "Plane.h"

using namespace DirectX;

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


	xax = DirectX::XMVector4Transform(xax, T);
	yax = DirectX::XMVector4Transform(yax, T);
	zax = DirectX::XMVector4Transform(zax, T);

	DirectX::XMStoreFloat4(&mAxisX, xax);
	DirectX::XMStoreFloat4(&mAxisY, yax);
	DirectX::XMStoreFloat4(&mAxisZ, zax);

	// Update position

	// acceleration modulation hack
	float accel = 0.1f;
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

	mPitch = 0.0f;
	mYaw = 0.0f;
	mRoll = 0.0f;
}