#pragma once

#include "D3Base.h"
#include "RenderItem.h"
#include <DirectXColors.h>
#include "FrameResource.h"
#include "BlurFilter.h"
#include "SobelFilter.h"
#include "Plane.h"
#include "RenderTarget.h"
#include "Mesh.h"
#include "Light.h"
#include "ShadowMap.h"

#include "Camera.h" // temporary!

class TestApp :
	public D3Base
{
public:
	TestApp(HINSTANCE hInst);
	~TestApp();

	virtual bool Initialize() override;

private:
	virtual void OnResize() override;
	virtual void Update(const Timer& t) override;
	virtual void Draw(const Timer& t) override;
	void DrawRenderItems(ID3D12GraphicsCommandList*, const std::vector<std::shared_ptr<RenderItem>>&);
	void DrawFullscreenQuad(ID3D12GraphicsCommandList*);
	void DrawShadowMaps();

	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

	void OnKeyboardInput(const Timer&);
	virtual void OnKeyDown(WPARAM, LPARAM) override;
	virtual void OnKeyUp(WPARAM, LPARAM) override;

	void ClearInstances(std::shared_ptr<RenderItem>);

	void UpdateGeometry(const Timer&);
	void UpdateInstanceBuffer(const Timer&, const std::vector<RENDER_ITEM_TYPE>&);
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

	std::string mLevel = "Level5";

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

	Plane mPlane;
	POINT mLastMousePos;

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

// Entry point
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE prevInst, PSTR cmdLine, int showCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		TestApp ta(hInst);
		if (!ta.Initialize())
			return 0;

		return ta.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

TestApp::TestApp(HINSTANCE hInst) : D3Base(hInst) {};
TestApp::~TestApp() {};

void TestApp::OnResize()
{
	D3Base::OnResize();

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	DirectX::XMMATRIX P = DirectX::XMMatrixPerspectiveFovLH(0.25f * Math::Pi, AspectRatio(), 1.0f, 1000.0f);
	DirectX::XMStoreFloat4x4(&mProj, P);

	mCamera.SetLens(0.25f * Math::Pi, AspectRatio(), 1.0f, 1000.0f);
}

