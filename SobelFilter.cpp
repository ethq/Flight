#include "SobelFilter.h"

SobelFilter::SobelFilter(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format) :
	mD3Device(device), mWidth(width), mHeight(height), mFormat(format)
{
	BuildResource();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE SobelFilter::OutputSrv()
{
	return mhGpuSrv;
}

UINT SobelFilter::DescriptorCount() const
{
	return 2;
}

void SobelFilter::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDesc, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDesc, UINT descSize)
{
	mhCpuSrv = hCpuDesc;
	mhCpuUav = hCpuDesc.Offset(1, descSize);
	mhGpuSrv = hGpuDesc;
	mhGpuUav = hGpuDesc.Offset(1, descSize);

	BuildDescriptors();
}

void SobelFilter::OnResize(UINT newWidth, UINT newHeight)
{
	if ((mWidth == newWidth) && (mHeight == newHeight))
		return;

	BuildResource();
	BuildDescriptors();
}

void SobelFilter::Execute(ID3D12GraphicsCommandList* cmdList, ID3D12RootSignature* rootSig, ID3D12PipelineState* pso, CD3DX12_GPU_DESCRIPTOR_HANDLE input)
{
	cmdList->SetComputeRootSignature(rootSig);
	cmdList->SetPipelineState(pso);

	cmdList->SetComputeRootDescriptorTable(0, input);
	cmdList->SetComputeRootDescriptorTable(2, mhGpuUav);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOutput.Get(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	UINT numGroupsX = (UINT)ceilf(mWidth / 16.0f);
	UINT numGroupsY = (UINT)ceilf(mHeight / 16.0f);
	cmdList->Dispatch(numGroupsX, numGroupsY, 1);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOutput.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void SobelFilter::BuildDescriptors()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = mFormat;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = mFormat;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;

	mD3Device->CreateShaderResourceView(mOutput.Get(), &srvDesc, mhCpuSrv);
	mD3Device->CreateUnorderedAccessView(mOutput.Get(), nullptr, &uavDesc, mhCpuUav);
}

void SobelFilter::BuildResource()
{
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Format = mFormat;
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = mWidth;
	texDesc.Height = mHeight;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	ThrowIfFailed(mD3Device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mOutput)
	));
}