#pragma once

#include "Math.h"
#include <DirectXMath.h>
#include "Mesh.h"
#include "FrameResource.h"

// Note; _never_ use using namespace X in a header, since it gets "imported" as well

enum class RENDER_ITEM_TYPE
{
	// General render items
	OPAQUE_DYNAMIC = 1,
	TRANSPARENT_DYNAMIC,
	WIREFRAME_DYNAMIC,

	OPAQUE_STATIC,
	TRANSPARENT_STATIC,
	WIREFRAME_STATIC,

	// Unique render items
	DEBUG_BOXES,
	ENVIRONMENT_MAP,
	DEBUG_QUAD_SHADOWMAP
};

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

	// Given in local space
	DirectX::BoundingBox BoundsB{};

	std::string Name;

	// We use a circular array of frame "resources" to mitigate cpu/gpu locking. As such, set this to however many
	// frame resources are in our array, indicating that they must all be updated.
	int NumFramesDirty = -1;

	Mesh* Geo = nullptr;
	
	const UINT MaxInstances;

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// Params for DrawIndexed
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int  BaseVertexLocation = 0;

public:
	int Id() const { return mId; }
	
	InstanceData& Instance(size_t idx) { return mInstances[idx]; }
	size_t InstanceCount() { return mInstances.size(); }

	// Attempt to add an instance. If the item already has MaxInstances instances, -1 is returned.
	// This is because we need to reserve memory in the associated upload buffer.
	int AddInstance(InstanceData& data)
	{
		if (mInstances.size() >= MaxInstances)
		{
			::OutputDebugStringA("Attempting to add instance beyond capacity in RenderItem::AddInstance\n");
			return -1;
		}

		mInstances.push_back(data);
		return 1;
	}

	void ClearInstances()
	{
		mInstances.clear();
	}

private:
	static int nextId;

	int mId = -1;
	std::vector<InstanceData> mInstances;
};



