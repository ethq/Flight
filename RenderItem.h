#pragma once

#include "Math.h"
#include <DirectXMath.h>
#include "Mesh.h"
#include "FrameResource.h"

// Note; _never_ use using namespace X in a header, since it gets "imported" as well

/*
Just a small helper class to store info needed to draw a single item
*/
struct RenderItem
{
	RenderItem(int numFramesDirty, UINT maxInstances = 1u) 
		: NumFramesDirty(numFramesDirty), mId(++nextId), MaxInstances(maxInstances) {};

	// We disable copy and assignment constructors to avoid mess with instance ids. Work around later if needed.
	RenderItem(const RenderItem&) = delete;
	RenderItem& operator=(const RenderItem&) = delete;

	DirectX::BoundingBox BoundsB{};

	std::string Name;

	// We use a circular array of frame "resources" to mitigate cpu/gpu locking. As such, set this to however many
	// frame resources are in our array, indicating that they must all be updated.
	int NumFramesDirty = -1;

	Mesh* Geo = nullptr;
	
	// Currently not used
	UINT MaxInstances;
	std::vector<InstanceData> Instances;

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// Params for DrawIndexed
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int  BaseVertexLocation = 0;

	int Id() const { return mId; }

protected:
	static int nextId;
	int mId = -1;
};



