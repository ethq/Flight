#pragma once

#include "Utilities.h"
#include "Timer.h"

// Link necessary d3d12 libraries. This is very much temporary; in a real project, link them in project properties
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

/*
Handles basic DX initialization. Main renderer inherits from this class.
Expects the window to be fully created 
*/
class D3Base
{
protected:
	D3Base(HWND);
	D3Base(const D3Base& rhs) = delete; // remove assignment constructor
	D3Base& operator=(const D3Base& rhs) = delete; // remove copy constructor
	virtual ~D3Base(); // virtual constructor is essential if we inherit from this class, as we intend to

public:
	static D3Base* GetApp();

	float AspectRatio() const;
	void  SetScreenDimensions(UINT, UINT);

	bool Get4xMsaaState() const;
	void Set4xMsaaState(bool state);
	
	virtual void OnResize();
	virtual void Update(const Timer&) {};
	virtual void Draw(const Timer&) {};
	
	// Part of initialization is virtual, hence we move it out of the constructor.
	virtual bool Initialize(bool keepQueueOpen = false);

protected:
	virtual void CreateRtvAndDsvDescriptorHeaps();
	
	bool InitDirect3D();
	void CreateCommandObjects();
	void CreateSwapChain();
	void FlushCommandQueue();

	ID3D12Resource* CurrentBackBuffer() const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

	void LogAdapters() const;
	void LogAdapterOutputs(IDXGIAdapter* adapter) const;
	void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format) const;

protected:
	static D3Base* mApp;

	HWND mMainWnd = NULL; // Swap chain needs to know which window to render to

	// Multisampling support
	bool m4xMsaaEnabled = false;
	UINT m4xMsaaQuality = 0;

	Microsoft::WRL::ComPtr<IDXGIFactory4> mDxgiFactory;
	Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
	Microsoft::WRL::ComPtr<ID3D12Device> mD3Device;

	// CPU/GPU synchronization
	Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
	UINT64 mCurrentFence = 0;

	// Command list stuff
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

	// Swap chain
	static const int SwapChainBufferCount = 2;
	int mCurrBackBuffer = 0;
	Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
	Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;

	D3D12_VIEWPORT	mScreenViewport = {};
	D3D12_RECT	mScissorRect = {};

	UINT mRtvDescriptorSize = 0;
	UINT mDsvDescriptorSize = 0;
	UINT mCbvSrvUavDescriptorSize = 0;

	//D3D_DRIVER_TYPE D3D_DRIVER_TYPE_HARDWARE;
	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	UINT mClientWidth = 800;
	UINT mClientHeight = 600;
};