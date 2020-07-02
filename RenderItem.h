#pragma once

#include "Math.h"
#include <DirectXMath.h>
#include "Mesh.h"

// Note; _never_ use using namespace X in a header, since it gets "imported" as well

/*
Just a small helper class to store info needed to draw a single item
*/
struct RenderItem
{
	RenderItem(int numFramesDirty) : NumFramesDirty(numFramesDirty) {};

	DirectX::XMFLOAT4X4 World = Math::Identity4x4();

	DirectX::XMFLOAT4X4 TexTransform = Math::Identity4x4();

	std::string Name;

	// We use a circular array of frame "resources" to mitigate cpu/gpu locking. As such, set this to however many
	// frame resources are in our array, indicating that they must all be updated.
	int NumFramesDirty = -1;

	// Index into GPU CBuffer for this item
	UINT cbObjectIndex = -1;

	Material* Mat = nullptr;
	Mesh* Geo = nullptr;

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// Params for DrawIndexed
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

