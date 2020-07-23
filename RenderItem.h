#pragma once

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

// The engine takes care of instantiating the RenderItem proper, which is then returned. 
struct RENDERITEM_PARAMS
{
	RENDER_ITEM_TYPE Type = RENDER_ITEM_TYPE::OPAQUE_DYNAMIC;
	std::string MeshName = "";
	UINT MaterialIndex = 0;
	UINT TextureIndex = 0;
	DirectX::XMFLOAT4X4 World = Math::Identity4x4();
	UINT MaxInstances = 1;
};

class D3Renderer; // For friend

class RenderItem
{
	friend D3Renderer;
public:
	RenderItem(int numFramesDirty, UINT maxInstances = 1u) 
		: NumFramesDirty(numFramesDirty), mId(++nextId), MaxInstances(maxInstances) {};

	// We disable copy and assignment constructors to avoid mess with ids. Work around later if needed.
	RenderItem(const RenderItem&) = delete;
	RenderItem& operator=(const RenderItem&) = delete;

// These variables are notationally "public", as they are used by the rendering engine.
protected:
	// Given in local space
	DirectX::BoundingBox BoundsB{};

	// Main mesh name - the render item will be loaded from /Models/Name.obj
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



