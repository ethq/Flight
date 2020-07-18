#pragma once

#include <functional>
#include <DirectXMath.h>

class Integrator
{
public:
	Integrator(std::function<DirectX::XMVECTOR (DirectX::FXMVECTOR pos, DirectX::FXMVECTOR vel, float time)> callback, float timestep)
		: mCallback(callback), dt(timestep)
	{

	}

	void Step(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR vel, float t, DirectX::XMVECTOR& newPos, DirectX::XMVECTOR& newVel);
	

private:
	std::function<DirectX::XMVECTOR (DirectX::FXMVECTOR pos, DirectX::FXMVECTOR vel, float t)> mCallback;
	float dt;
};

