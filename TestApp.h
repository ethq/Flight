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
	void DrawRenderItems(ID3D12GraphicsCommandList*, const std::vector<RenderItem*>&);
	void DrawFullscreenQuad(ID3D12GraphicsCommandList*);
	void DrawShadowMaps();

	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

	void OnKeyboardInput(const Timer&);
	virtual void OnKeyDown(WPARAM, LPARAM) override;
	virtual void OnKeyUp(WPARAM, LPARAM) override;

	void UpdateGeometry(const Timer&);
	void UpdateObjectCBs(const Timer&);
	void UpdateMaterialCBs(const Timer&);
	void UpdateMainPassCB(const Timer&);
	void UpdateShadowPassCB(size_t lightIndex, UINT passIndex);
	void UpdateLights(const Timer&);

	void LoadTextures();
	void BuildMaterials();

	void Pick();

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
	std::vector<std::unique_ptr<RenderItem>> mRenderItems;
	std::vector<RenderItem*> mOpaqueItems;
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	UINT mNumFrameResources = 3;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mBlurRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mSobelRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCbvDescHeap = nullptr;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvDescHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<Mesh>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

	DirectX::BoundingSphere mSceneBoundS;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mNullSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mEnvironmentMapSrv;

	std::unique_ptr<RenderItem> mSkydome;
	std::unique_ptr<RenderItem> mDbgQuad;

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

	std::wstring mProjectPath;
	bool mDbgFlag = false;
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

