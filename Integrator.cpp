#include "Integrator.h"

using namespace DirectX;

// Steps using RK4
void Integrator::Step(FXMVECTOR pos, FXMVECTOR vel, float t, XMVECTOR& newPos, XMVECTOR& newVel)
{
	auto k1v = mCallback(pos, vel, t) * dt;
	auto k1x = vel * dt;

	auto k2v = mCallback(pos + k1x / 2.0f, vel + k1v / 2.0f, t + dt / 2.0f) * dt;
	auto k2x = (vel + k1v / 2.0f) * dt;

	auto k3v = mCallback(pos + k2x / 2.0f, vel + k2v / 2.0f, t + dt / 2.0f) * dt;
	auto k3x = (vel + k2v / 2.0f) * dt;

	auto k4v = mCallback(pos + k3x, vel + k3v, t + dt) * dt;
	auto k4x = (vel + k3v) * dt;

	newVel = vel + 1.0f / 6.0f * (k1v + 2.0f * k2v + 2.0f * k3v + k4v);
	newPos = pos + 1.0f / 6.0f * (k1x + 2.0f * k2x + 2.0f * k3x + k4x);
}