#pragma once

#include "Utilities.h"

class ShadowMap
{
public:
	ShadowMap(Microsoft::WRL::ComPtr<ID3D12Device>& dev, UINT width, UINT height, LightType type, UINT dsvDescriptorSize = 0u);
	ShadowMap(const ShadowMap&) = delete;
	ShadowMap& operator=(const ShadowMap&) = delete;
	~ShadowMap() = default;

	UINT Width() const;
	UINT Height() const;

	ID3D12Resource* Resource();
	CD3DX12_GPU_DESCRIPTOR_HANDLE Srv() const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE Dsv() const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE Rtv() const;

	D3D12_VIEWPORT Viewport() const;
	D3D12_RECT ScissorRect() const;

	void BuildResource();

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv);

	void BuildDescriptors();

	/*
	Returns number of srv/uav/cbv's used by default. 
	
	type = 0: srv/uav/cbv count. (default behaviour)
	type = 1: dsv count
	type = 2: rtv count
	*/
	UINT DescriptorCount(int type = 0) const;

	void OnResize(UINT newWidth, UINT newHeight);

private:
	LightType mType;
	Microsoft::WRL::ComPtr<ID3D12Device>& mD3Device;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mCpuDsv;

	UINT mDsvDescriptorSize = 0;
	UINT mWidth = 0;
	UINT mHeight = 0;
	DXGI_FORMAT mFormat = DXGI_FORMAT_R24G8_TYPELESS;

	Microsoft::WRL::ComPtr<ID3D12Resource> mShadowMap = nullptr;
};

