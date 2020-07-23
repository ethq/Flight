#include "WindowsBase.h"
#include <cassert>

/*
The reason for putting wndproc outside class def is that it cannot accept an (implicit) this pointer. Static removes it, but also removes
instance access. One option is to store a pointer to WindowsBase instance in HWND using SetWindowLong, then retrieve it from a static method.
Worth it? Not sure. see e.g. https://stackoverflow.com/questions/21369256/how-to-use-wndproc-as-a-class-function for more.
*/

LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return WindowsBase::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

WindowsBase* WindowsBase::mApp = nullptr;

WindowsBase* WindowsBase::GetApp()
{
	return mApp;
}

float WindowsBase::AspectRatio() const
{
	return static_cast<float>(mClientWidth) / static_cast<float>(mClientHeight);
}

HWND WindowsBase::MainWnd() const
{
	return mMainWnd;
}

HINSTANCE WindowsBase::GetAppInst() const
{
	return mAppInst;
}

WindowsBase::WindowsBase(HINSTANCE hInstance)
	: mAppInst(hInstance)
{
	// Only one instance can be constructed.
	assert(mApp == nullptr);
	mApp = this;
}

WindowsBase::~WindowsBase()
{

}

int WindowsBase::Run()
{
	MSG msg = { 0 };

	mTimer.Reset();

	while (msg.message != WM_QUIT)
	{
		// If there are Window messages then process them.
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		// Otherwise, do animation/game stuff.
		else
		{
			mTimer.Tick();

			if (!mAppPaused)
			{
				CalculateFramestats();
				Update(mTimer);
			}
			else
			{
				Sleep(100);
			}
		}
	}

	return (int)msg.wParam;
}

void WindowsBase::CalculateFramestats()
{
	// Code computes the average frames per second, and also the 
	// average time it takes to render one frame.  These stats 
	// are appended to the window caption bar.

	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	// Compute averages over one second period.
	if ((mTimer.TotalTime() - timeElapsed) >= 1.0f)
	{
		float fps = (float)frameCnt; // fps = frameCnt / 1
		float mspf = 1000.0f / fps;

		std::wstring fpsStr = std::to_wstring(fps);
		std::wstring mspfStr = std::to_wstring(mspf);

		std::wstring windowText = mMainWndCaption +
			L"    fps: " + fpsStr +
			L"   mspf: " + mspfStr;

		SetWindowText(mMainWnd, windowText.c_str());

		// Reset for next average.
		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

bool WindowsBase::Initialize()
{
	if (!InitMainWindow())
		return false;

	// Do the initial resize code.
	OnResize();

	return true;
}

bool WindowsBase::InitMainWindow()
{
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = mAppInst;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"MainWnd";

	if (!RegisterClass(&wc))
	{
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return false;
	}

	// Compute window rectangle dimensions based on requested client area dimensions.
	RECT R = { 0L, 0L, (LONG)mClientWidth, (LONG)mClientHeight };

	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	mMainWnd = CreateWindow(L"MainWnd", mMainWndCaption.c_str(),
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mAppInst, 0);
	if (!mMainWnd)
	{
		MessageBox(0, L"CreateWindow Failed.", 0, 0);
		return false;
	}

	ShowWindow(mMainWnd, SW_SHOW);
	UpdateWindow(mMainWnd);

	return true;
}


LRESULT WindowsBase::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		// WM_ACTIVATE is sent when the window is activated or deactivated.  
		// We pause the game when the window is deactivated and unpause it 
		// when it becomes active.  
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			mAppPaused = true;
			mTimer.Stop();
		}
		else
		{
			mAppPaused = false;
			mTimer.Start();
		}
		return 0;

		// WM_SIZE is sent when the user resizes the window.  
	case WM_SIZE:
		// Save the new client area dimensions.
		mClientWidth = LOWORD(lParam);
		mClientHeight = HIWORD(lParam);
		
		if (wParam == SIZE_MINIMIZED)
		{
			mAppPaused = true;
			mMinimized = true;
			mMaximized = false;
		}
		else if (wParam == SIZE_MAXIMIZED)
		{
			mAppPaused = false;
			mMinimized = false;
			mMaximized = true;
			OnResize();
		}
		else if (wParam == SIZE_RESTORED)
		{

			// Restoring from minimized state?
			if (mMinimized)
			{
				mAppPaused = false;
				mMinimized = false;
				OnResize();
			}

			// Restoring from maximized state?
			else if (mMaximized)
			{
				mAppPaused = false;
				mMaximized = false;
				OnResize();
			}
			else if (mResizing)
			{
				// If user is dragging the resize bars, we do not resize 
				// the buffers here because as the user continuously 
				// drags the resize bars, a stream of WM_SIZE messages are
				// sent to the window, and it would be pointless (and slow)
				// to resize for each WM_SIZE message received from dragging
				// the resize bars.  So instead, we reset after the user is 
				// done resizing the window and releases the resize bars, which 
				// sends a WM_EXITSIZEMOVE message.
			}
			else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
			{
				OnResize();
			}
		}
		return 0;

		// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
	case WM_ENTERSIZEMOVE:
		mAppPaused = true;
		mResizing = true;
		mTimer.Stop();
		return 0;

		// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
		// Here we reset everything based on the new window dimensions.
	case WM_EXITSIZEMOVE:
		mAppPaused = false;
		mResizing = false;
		mTimer.Start();
		OnResize();
		return 0;

		// WM_DESTROY is sent when the window is being destroyed.
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

		// The WM_MENUCHAR message is sent when a menu is active and the user presses 
		// a key that does not correspond to any mnemonic or accelerator key. 
	case WM_MENUCHAR:
		// Don't beep when we alt-enter.
		return MAKELRESULT(0, MNC_CLOSE);

		// Catch this message so to prevent the window from becoming too small.
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_KEYUP:
		OnKeyUp(wParam, lParam);
		return 0;
	case WM_KEYDOWN:
		OnKeyDown(wParam, lParam);
		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void WindowsBase::OnResize()
{

}