#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "Utilities.h"
#include "Timer.h"

// Link necessary d3d12 libraries. This is very much temporary; in a real project, link them in project properties
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

class D3Base
{
protected:
	D3Base(HINSTANCE);
	D3Base(const D3Base& rhs) = delete; // remove assignment constructor
	D3Base& operator=(const D3Base& rhs) = delete; // remove copy constructor
	virtual ~D3Base(); // virtual constructor is essential if we inherit from this class, as we intend to

public:
	static D3Base* GetApp();

	HWND MainWnd() const;
	HINSTANCE GetAppInst() const;

	float AspectRatio() const;

	bool Get4xMsaaState() const;
	void Set4xMsaaState(bool state);

	int Run();

	virtual bool Initialize();
	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:

	virtual void CreateRtvAndDsvDescriptorHeaps();
	virtual void OnResize();
	virtual void Update(const Timer&) {};
	virtual void Draw(const Timer&) {};

	virtual void OnMouseUp(WPARAM state, int x, int y) {};
	virtual void OnMouseDown(WPARAM state, int x, int y) {};
	virtual void OnMouseMove(WPARAM state, int x, int y) {};
	virtual void OnKeyUp(WPARAM, LPARAM);
	virtual void OnKeyDown(WPARAM, LPARAM);

	bool InitMainWindow();
	bool InitDirect3D();
	void CreateCommandObjects();
	void CreateSwapChain();
	void FlushCommandQueue();

	ID3D12Resource* CurrentBackBuffer() const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

	void CalculateFrameStats();

	void LogAdapters() const;
	void LogAdapterOutputs(IDXGIAdapter* adapter) const;
	void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format) const;

protected:
	static D3Base* mApp;

	// Window variables
	HINSTANCE mAppInst = nullptr;
	HWND mMainWnd = nullptr;
	bool mMaximized = false;
	bool mMinimized = false;
	bool mResizing = false;
	bool mFullScreenState = false;
	bool mAppPaused = false;
	Timer mTimer;

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

	D3D12_VIEWPORT	mScreenViewport;
	D3D12_RECT	mScissorRect;

	UINT mRtvDescriptorSize = 0;
	UINT mDsvDescriptorSize = 0;
	UINT mCbvSrvUavDescriptorSize = 0;

	std::wstring mMainWndCaption = L"Ello Wold";
	D3D_DRIVER_TYPE D3D_DRIVER_TYPE_HARDWARE;
	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	UINT mClientWidth = 800;
	UINT mClientHeight = 600;
};