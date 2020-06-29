#include "ShadowMap.h"

ShadowMap::ShadowMap(Microsoft::WRL::ComPtr<ID3D12Device>& dev, UINT width, UINT height, LightType type, UINT dsvDescriptorSize) 
	: mD3Device(dev), mType(type), mDsvDescriptorSize(dsvDescriptorSize)
{
	mWidth = width;
	mHeight = height;

	mViewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
	mScissorRect = { 0, 0, (int)width, (int)height };

	BuildResource();
}

UINT ShadowMap::Width()const
{
	return mWidth;
}

UINT ShadowMap::Height()const
{
	return mHeight;
}

ID3D12Resource* ShadowMap::Resource()
{
	return mShadowMap.Get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE ShadowMap::Srv() const
{
	return mGpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE ShadowMap::Dsv() const
{
	return mCpuDsv;
}

D3D12_VIEWPORT ShadowMap::Viewport() const
{
	return mViewport;
}

D3D12_RECT ShadowMap::ScissorRect() const
{
	return mScissorRect;
}

void ShadowMap::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
	CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
	CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv)
{
	// Save references to the descriptors. 
	mCpuSrv = hCpuSrv;
	mGpuSrv = hGpuSrv;
	mCpuDsv = hCpuDsv;
	
	//  Create the descriptors
	BuildDescriptors();
}

void ShadowMap::OnResize(UINT newWidth, UINT newHeight)
{
	if ((mWidth != newWidth) || (mHeight != newHeight))
	{
		mWidth = newWidth;
		mHeight = newHeight;

		BuildResource();

		// New resource, so we need new descriptors to that resource.
		BuildDescriptors();
	}
}

/*
Returns number of srv/uav/cbv's used by default.

type = 0: srv/uav/cbv count. (default behaviour)
type = 1: dsv count.
type = 2: rtv count
*/
UINT ShadowMap::DescriptorCount(int type) const
{
	switch (type)
	{
	// SRV/UAV/CBV
	case 0:
		return 1;
	// DSV
	case 1:
		if (mType == LightType::POINT)
			return 6; // for the current non-GS technique, that is..
		return 1;
	// RTV
	case 2:
		return 0;
	default:
		ThrowIfFailed(1);
		return 0;
	}
}

// Note that this is delayed; it's called by the main app as it creates the descriptors
// That means we can construct the shadowmap _before_ creating the descriptor heaps etc.
void ShadowMap::BuildDescriptors()
{
	D3D12_SRV_DIMENSION viewDim = {};
	ZeroMemory(&viewDim, sizeof(D3D12_SRV_DIMENSION));

	if (mType == LightType::DIRECTIONAL || mType == LightType::SPOT)
		viewDim = D3D12_SRV_DIMENSION_TEXTURE2D;
	else if (mType == LightType::POINT)
		viewDim = D3D12_SRV_DIMENSION_TEXTURECUBE;
	else
		ThrowIfFailed(1);

	// Create SRV to resource so we can sample the shadow map in a shader program.
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = viewDim;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.PlaneSlice = 0;
	mD3Device->CreateShaderResourceView(mShadowMap.Get(), &srvDesc, mCpuSrv);

	// Create DSV to resource so we can render to the shadow map.
	if (mType != LightType::POINT)
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.Texture2D.MipSlice = 0;
		mD3Device->CreateDepthStencilView(mShadowMap.Get(), &dsvDesc, mCpuDsv);
	}
	// The alternative here is to use the geometry shader to 6-fold duplicate scene geometry and so do only 1 render pass
	else
	{
		auto dsvHandle = mCpuDsv; // mCpuDsv points to the start of where we can allocate our 6 dsvs
		for (size_t i = 0; i < 6; ++i) // 6 faces in a cube!
		{
			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
			dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			dsvDesc.Texture2DArray.FirstArraySlice = i;
			dsvDesc.Texture2DArray.ArraySize = 1;
			dsvDesc.Texture2DArray.MipSlice = 0;
			mD3Device->CreateDepthStencilView(mShadowMap.Get(), &dsvDesc, dsvHandle);

			dsvHandle.Offset(1, mDsvDescriptorSize);
		}
	}

}

void ShadowMap::BuildResource()
{
	if (mType == LightType::DIRECTIONAL || mType == LightType::SPOT)
	{
		D3D12_RESOURCE_DESC texDesc;
		ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Alignment = 0;
		texDesc.Width = mWidth;
		texDesc.Height = mHeight;
		texDesc.DepthOrArraySize = 1;
		texDesc.MipLevels = 1;
		texDesc.Format = mFormat;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE optClear;
		optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		optClear.DepthStencil.Depth = 1.0f;
		optClear.DepthStencil.Stencil = 0;

		ThrowIfFailed(mD3Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			&optClear,
			IID_PPV_ARGS(&mShadowMap)));

	}
	else if (mType == LightType::POINT)
	{
		D3D12_RESOURCE_DESC texDesc;
		ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Alignment = 0;
		texDesc.Width = mWidth;
		texDesc.Height = mHeight;
		texDesc.DepthOrArraySize = 6;
		texDesc.MipLevels = 1;
		texDesc.Format = mFormat;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE optClear;
		optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		optClear.DepthStencil.Depth = 1.0f;
		optClear.DepthStencil.Stencil = 0;

		ThrowIfFailed(mD3Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			&optClear,
			IID_PPV_ARGS(&mShadowMap)
		));
	}
	else
	{
		ThrowIfFailed(1); // HRESULT is a fail if it is non-negative
	}
}