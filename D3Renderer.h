#pragma once

#include <DirectXColors.h>

#include "D3Base.h"
#include "RenderItem.h"
#include "FrameResource.h"
#include "BlurFilter.h"
#include "SobelFilter.h"
#include "Plane.h"
#include "RenderTarget.h"
#include "Mesh.h"
#include "Light.h"
#include "ShadowMap.h"

#include "Camera.h" // temporary!



class D3Renderer :
	public D3Base
{
public:
	D3Renderer(HWND);
	~D3Renderer();

	virtual void OnResize() override;
	virtual void Update(const Timer& t) override;
	virtual void Draw(const Timer& t) override;

	// If additional user-defined initialization is to be done, then the command queue can be kept open.
	// If this is done, InitializeEnd() must be called after.
	// To add static geometry, Initialize(true) -> InitializeEnd() must be used.
	virtual bool Initialize(bool keepQueueOpen = false) override;
	void InitializeEnd();

	void SetView(const DirectX::XMFLOAT4X4&);
	void SetView(const DirectX::XMMATRIX&);

	std::vector<std::shared_ptr<RenderItem>> AddRenderItem(const std::string&, const std::vector<RENDERITEM_PARAMS>&);
	std::shared_ptr<RenderItem> AddRenderItem(const std::string&, const RENDERITEM_PARAMS&);
	void RemoveRenderItems(const std::vector<int>&);

protected:
	void DrawRenderItems(ID3D12GraphicsCommandList*, const std::vector<std::shared_ptr<RenderItem>>&);
	void DrawFullscreenQuad(ID3D12GraphicsCommandList*);
	void DrawShadowMaps();

	void ClearInstances(std::shared_ptr<RenderItem>);

	void UpdateGeometry(const Timer&);
	void UpdateInstanceBuffer(const std::vector<RENDER_ITEM_TYPE>&);
	void UpdateMaterialBuffer(const Timer&);
	void UpdateMainPassCB(const Timer&);
	void UpdateShadowPassCB(size_t lightIndex, UINT passIndex);
	void UpdateLights(const Timer&);

	void LoadTextures();
	void BuildMaterials();

	void Pick(float x, float y);

	virtual void CreateRtvAndDsvDescriptorHeaps() override;
	void BuildDescriptorHeaps();
	void BuildDescriptors();
	void BuildStaticGeometry();
	void BuildRootSignature();
	void BuildPostprocessRootSignature();
	void BuildBlurRootSignature();
	void BuildSobelRootSignature();
	void BuildPSOs();
	void BuildShadersAndInputLayout();
	
	void InitLights();

	void BuildSceneBounds();

	void BuildFrameResources();
	void BuildRenderItems();	
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();

private:
	int gIdx = 0;
	std::map<RENDER_ITEM_TYPE, std::vector<std::shared_ptr<RenderItem>>> mRenderItems;
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	UINT mNumFrameResources = 3;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mBlurRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mSobelRootSignature = nullptr;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCbvDescHeap = nullptr;

	// Looks like a relic to me; delete. 
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvDescHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<Mesh>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

	DirectX::BoundingSphere mSceneBoundS;

	std::string mLevel = "Level_Airstrip";

	CD3DX12_GPU_DESCRIPTOR_HANDLE mNullSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mEnvironmentMapSrv;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	std::vector<std::shared_ptr<LightPovData>> mLights;
	UINT mNumDirLights = 0;
	UINT mNumSpotLights = 0;
	UINT mNumPointLights = 0;

	PassConstants mPassCB;
	UINT mPassCbvOffset = 0;

	PassConstants mShadowPassCB;
	UINT mPassShadowOffset = 0;

	bool isWireFrame = false;
	DirectX::XMFLOAT4X4 mProj = Math::Identity4x4();

	std::unique_ptr<BlurFilter> mBlurFilter;
	std::unique_ptr<SobelFilter> mSobelFilter;
	std::unique_ptr<RenderTarget> mOffscreenRT;

	DirectX::XMFLOAT4X4 mView = Math::Identity4x4();
	DirectX::XMFLOAT4 mPosition = { 0.0f, 1.0f, 0.0f, 1.0f };

	Camera mCamera;

	// Settings
	std::wstring mProjectPath;
	bool mDbgFlag = false;
	bool mDebugBoundingBoxesEnabled = true;

	const std::vector<RENDER_ITEM_TYPE> mPickableRenderItems = {
		RENDER_ITEM_TYPE::OPAQUE_DYNAMIC,
		RENDER_ITEM_TYPE::OPAQUE_STATIC,
		RENDER_ITEM_TYPE::WIREFRAME_DYNAMIC,
		RENDER_ITEM_TYPE::WIREFRAME_STATIC,
		RENDER_ITEM_TYPE::TRANSPARENT_DYNAMIC,
		RENDER_ITEM_TYPE::TRANSPARENT_STATIC
	};

	const std::vector<RENDER_ITEM_TYPE> mDynamicRenderItems = {
		RENDER_ITEM_TYPE::OPAQUE_DYNAMIC,
		RENDER_ITEM_TYPE::TRANSPARENT_DYNAMIC,
		RENDER_ITEM_TYPE::WIREFRAME_DYNAMIC,
		RENDER_ITEM_TYPE::DEBUG_BOXES
	};

	const std::vector<RENDER_ITEM_TYPE> mStaticRenderItems = {
		RENDER_ITEM_TYPE::ENVIRONMENT_MAP,
		RENDER_ITEM_TYPE::OPAQUE_STATIC,
		RENDER_ITEM_TYPE::TRANSPARENT_STATIC,
		RENDER_ITEM_TYPE::WIREFRAME_STATIC,
		RENDER_ITEM_TYPE::DEBUG_QUAD_SHADOWMAP
	};
};

