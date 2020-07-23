#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, std::map<int, UINT> renderItemsAndInstanceCounts, UINT matCount)
	: mDevice(device)
{
	ThrowIfFailed(device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

	PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
	
	// Instance buffers are structured buffers in the shader - for now - as we typically update every frame.
	// However, much of the scene geometry will be singular and static, and a cbuffer would be perfectly fine for that.
	for (auto& riia : renderItemsAndInstanceCounts)
		InstanceBuffers[riia.first] = std::make_unique<UploadBuffer<InstanceData>>(device, riia.second, false);

	MaterialBuffer = std::make_unique<UploadBuffer<MaterialData>>(device, matCount, false);
}

void FrameResource::AddRenderItems(std::map<int, UINT>& renderItemsAndInstanceCounts)
{
	for (auto& riia : renderItemsAndInstanceCounts)
		InstanceBuffers[riia.first] = std::make_unique<UploadBuffer<InstanceData>>(mDevice, riia.second, false);
}

void FrameResource::RemoveRenderItems(std::vector<int> ids)
{
	for (auto& id : ids)
	{
		// Corresponding buffer doesn't exist
		if (InstanceBuffers.find(id) == InstanceBuffers.end())
			continue;

		// Buffer exists - memory cleaned up by ~UploadBuffer()
		InstanceBuffers.erase(id);
	}
}

FrameResource::~FrameResource()
{

}