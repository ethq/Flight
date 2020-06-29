#pragma once

#include "Utilities.h"

/*
To blur, we first render the scene to texture. Then the blurring process will use two textures with associated SRV/UAV views(for R/W access)
We assume a separable blur, in the sense that Blur(image) = Blur_x(Blur_y(image)).
*/

class BlurFilter
{
public:
	BlurFilter(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format);

	BlurFilter(const BlurFilter& rhs) = delete;
	BlurFilter& operator=(const BlurFilter& rhs) = delete;
	~BlurFilter() = default;

	ID3D12Resource* Output();

	CD3DX12_GPU_DESCRIPTOR_HANDLE OutputSrv() const;

	UINT DescriptorCount() const;

	void SetGaussianWeights(float sigma);


	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDesc,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDesc,
		UINT descSize);

	void OnResize(UINT newWidth, UINT newHeight);

	void Execute(
		ID3D12GraphicsCommandList* cmdList,
		ID3D12RootSignature* rootSig,
		ID3D12PipelineState* horzBlurPSO,
		ID3D12PipelineState* vertBlurPSO,
		ID3D12Resource* input,
		int blurCount
	);

private:
	void BuildDescriptors();
	void BuildResources();


private:
	std::vector<float> mWeights;
	const int MaxBlurRadius = 5;

	ID3D12Device* mD3Device = nullptr;

	UINT mWidth = 0;
	UINT mHeight = 0;
	DXGI_FORMAT mFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mBlur0CpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mBlur1CpuSrv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mBlur0CpuUav;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mBlur1CpuUav;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mBlur0GpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mBlur1GpuSrv;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mBlur0GpuUav;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mBlur1GpuUav;

	Microsoft::WRL::ComPtr<ID3D12Resource> mBlurMap0 = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> mBlurMap1 = nullptr;
};

