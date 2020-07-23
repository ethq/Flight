#pragma once
#include "WindowsBase.h"
#include "Utilities.h"
#include "D3Renderer.h"
#include "Plane.h"
#include "reactphysics3d.h"

class Main :
	public WindowsBase
{
public:
	Main(HINSTANCE hInst);
	Main(const Main& rhs) = delete; // remove assignment constructor
	Main& operator=(const Main& rhs) = delete; // remove copy constructor
	virtual ~Main();

	virtual void Update(const Timer&);
	virtual bool Initialize();

protected:
	//virtual void OnMouseUp(WPARAM state, int x, int y) override;
	//virtual void OnMouseDown(WPARAM state, int x, int y) override;
	virtual void OnMouseMove(WPARAM state, int x, int y);
	virtual void OnKeyUp(WPARAM, LPARAM) override;
	virtual void OnKeyDown(WPARAM, LPARAM) override;

protected:
	std::unique_ptr<D3Renderer> mRenderer;

	Plane mPlane;

	POINT mLastMousePos = {};

	// Physics variables
	rp3d::PhysicsCommon mPhysicsCommon;
	rp3d::PhysicsWorld* mPhysicsWorld;
	
	// Time between two rendering frames is unlikely to be a direct multiple of physics timesteps. 
	// To make sure all time is "used", we accumulate time remainders between frames
	float mTimeAccumulator;

	// Time in seconds for each iteration of physics engine
	const float mPhysicsTimeStep = 1 / 60.0f;

	bool mAllowPhysics = false;
};

// Entry point. Main.Initialize() is responsible for creating the window. 
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE prevInst, PSTR cmdLine, int showCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		Main m(hInst);
		if (!m.Initialize())
			return 0;

		// Begin message loop
		return m.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

