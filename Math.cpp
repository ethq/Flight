#include "Math.h"

const float Math::Infty = FLT_MAX;
const float Math::Pi = 3.1415926535f;

float Math::AngleFromXY(float x, float y)
{
	float theta = 0.0f;

	// Quadrant I or IV
	if (x >= 0.0f)
	{
		// If x = 0, then atanf(y/x) = +pi/2 if y > 0
		//                atanf(y/x) = -pi/2 if y < 0
		theta = atanf(y / x); // in [-pi/2, +pi/2]

		if (theta < 0.0f)
			theta += 2.0f * Pi; // in [0, 2*pi).
	}

	// Quadrant II or III
	else
		theta = atanf(y / x) + Pi; // in [0, 2*pi).

	return theta;
}


DirectX::XMVECTOR Math::CartesianToSpherical(float x, float y, float z)
{
	float r = sqrt(x * x + y * y + z * z);

	float theta = atan2(y, x);

	float phi = atan2(z, sqrt(x * x + y * y));

	return DirectX::XMVectorSet(r, theta, phi, 1.0f);
}


DirectX::XMVECTOR Math::SphericalToCartesian(float radius, float theta, float phi)
{
	return DirectX::XMVectorSet(
		radius * sinf(phi) * cosf(theta),
		radius * cosf(phi),
		radius * sinf(phi) * sinf(theta),
		1.0f);
}


DirectX::XMVECTOR Math::RandUnitVec3()
{
	DirectX::XMVECTOR One = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
	DirectX::XMVECTOR Zero = DirectX::XMVectorZero();

	// Keep trying until we get a point on/in the hemisphere.
	while (true)
	{
		// Generate random point in the cube [-1,1]^3.
		DirectX::XMVECTOR v = DirectX::XMVectorSet(Math::RandF(-1.0f, 1.0f), Math::RandF(-1.0f, 1.0f), Math::RandF(-1.0f, 1.0f), 0.0f);

		// Ignore points outside the unit sphere in order to get an even distribution 
		// over the unit sphere.  Otherwise points will clump more on the sphere near 
		// the corners of the cube.

		if (DirectX::XMVector3Greater(DirectX::XMVector3LengthSq(v), One))
			continue;

		return DirectX::XMVector3Normalize(v);
	}
}


DirectX::XMVECTOR Math::RandHemisphereUnitVec3(DirectX::XMVECTOR n)
{
	DirectX::XMVECTOR One = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
	DirectX::XMVECTOR Zero = DirectX::XMVectorZero();

	// Keep trying until we get a point on/in the hemisphere.
	while (true)
	{
		// Generate random point in the cube [-1,1]^3.
		DirectX::XMVECTOR v = DirectX::XMVectorSet(Math::RandF(-1.0f, 1.0f), Math::RandF(-1.0f, 1.0f), Math::RandF(-1.0f, 1.0f), 0.0f);

		// Ignore points outside the unit sphere in order to get an even distribution 
		// over the unit sphere.  Otherwise points will clump more on the sphere near 
		// the corners of the cube.

		if (DirectX::XMVector3Greater(DirectX::XMVector3LengthSq(v), One))
			continue;

		// Ignore points in the bottom hemisphere.
		if (DirectX::XMVector3Less(DirectX::XMVector3Dot(n, v), Zero))
			continue;

		return DirectX::XMVector3Normalize(v);
	}
}


DirectX::XMFLOAT4X4 Math::Identity4x4()
{
    static DirectX::XMFLOAT4X4 I(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);

    return I;
}


DirectX::XMMATRIX Math::InverseTranspose(DirectX::CXMMATRIX M)
{
    // We zero out affine param because this should only be applied to vectors, not positions.
    DirectX::XMMATRIX A = M;
    A.r[3] = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

    DirectX::XMVECTOR det = DirectX::XMMatrixDeterminant(A);
    return DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(&det, A));
}


