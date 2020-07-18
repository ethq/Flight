#pragma once

#include <Windows.h>
#include <DirectXMath.h>
#include <cstdint>

class Math
{
public:
	// return random number in [0, 1]
	static float RandF()
	{
		return static_cast<float>(rand() / static_cast<float>(RAND_MAX));
	}

	// Return random number in [a, b]
	static float RandF(float a, float b)
	{
		return a + (b - a) * RandF();
	}

	// Return random integer in [a, b]
	static int Rand(int a, int b)
	{
		return a + rand() % ((b - a) + 1);
	}

	template<typename T>
	static T Min(const T& a, const T& b)
	{
		return a > b ? b : a;
	}

	template<typename T>
	static T Max(const T& a, const T& b)
	{
		return a > b ? a : b;
	}

	// Linear interpolation between a and b. t expected to be somewhere in [0, 1]
	template<typename T>
	static T Lerp(const T& a, const T& b, float t)
	{
		return a + (b - a) * t;
	}

	template<typename T>
	static T Clamp(const T& x, const T& low, const T& high)
	{
		return (x < low) ? low : (x > high ? high : x);
	}

	static float AngleFromXY(float x, float y);

	static DirectX::XMVECTOR SphericalToCartesian(float r, float theta, float phi);
	
	static DirectX::XMVECTOR CartesianToSpherical(float x, float y, float z);

	static DirectX::XMMATRIX InverseTranspose(DirectX::CXMMATRIX);

	static DirectX::XMFLOAT4X4 Identity4x4();

	static DirectX::XMVECTOR RandUnitVec3();

	static DirectX::XMVECTOR RandHemisphereUnitVec3(DirectX::XMVECTOR n);

	static const float Infty;
	static const float Pi;

};