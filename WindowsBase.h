#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#define NOMINMAX

#include <Windows.h>
#include <windowsx.h>
#include <wrl.h>
#include "Timer.h"
#include <string>

/*
Singleton class that provides a single window & message loop
*/
class WindowsBase
{
protected:
	WindowsBase(HINSTANCE);
	WindowsBase(const WindowsBase& rhs) = delete; // remove assignment constructor
	WindowsBase& operator=(const WindowsBase& rhs) = delete; // remove copy constructor
	virtual ~WindowsBase(); // virtual destructor is essential if we inherit from this class, as we intend to

	bool InitMainWindow();
	void CalculateFramestats();

	virtual void OnResize();
	virtual void Update(const Timer&) {};

	virtual void OnMouseUp(WPARAM state, int x, int y) {};
	virtual void OnMouseDown(WPARAM state, int x, int y) {};
	virtual void OnMouseMove(WPARAM state, int x, int y) {};
	virtual void OnKeyUp(WPARAM, LPARAM) {};
	virtual void OnKeyDown(WPARAM, LPARAM) {};

public:
	HWND MainWnd() const;
	HINSTANCE GetAppInst() const;
	static WindowsBase* GetApp();

	float AspectRatio() const;
	int Run();

	virtual bool Initialize();
	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
	static WindowsBase* mApp;

	std::wstring mMainWndCaption = L"Ello thar";
	HINSTANCE mAppInst = nullptr;
	HWND mMainWnd = nullptr;
	bool mMaximized = false;
	bool mMinimized = false;
	bool mResizing = false;
	bool mFullScreenState = false;
	bool mAppPaused = false;
	Timer mTimer;

	UINT mClientWidth = 800;
	UINT mClientHeight = 600;
};

