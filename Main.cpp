#include "Main.h"

using namespace DirectX;

Main::~Main()
{

}

// -> constructor?
bool Main::Initialize()
{
	// Window initialization
	WindowsBase::Initialize();

	// Window created; initialize renderer
	mRenderer = std::make_unique<D3Renderer>(mMainWnd);
	mRenderer->Initialize(true);

	// Load mesh
	std::vector<RENDERITEM_PARAMS> rips;
	RENDERITEM_PARAMS rip;
	rip.MeshName = "Plane";
	rip.MaterialIndex = 0;
	rip.TextureIndex = 0;
	rip.Type = RENDER_ITEM_TYPE::OPAQUE_DYNAMIC;
	rip.MaxInstances = 1;
	rips.push_back(rip);

	rip.MeshName = "CollisionMesh";
	rips.push_back(rip);

	auto planeMesh = mRenderer->AddRenderItem("Scythe2", rips);

	assert(planeMesh.size() == 2);
	mPlane.AddBody(planeMesh[0]);
	mPlane.AddCollisionMesh(planeMesh[1]);

	rip.MeshName = "Level";
	rip.Type = RENDER_ITEM_TYPE::OPAQUE_STATIC;
	mRenderer->AddRenderItem("Level_Airstrip", rip);

	mRenderer->InitializeEnd();

	return true;
}

void Main::Update(const Timer& t)
{
	mPlane.Update(t);
	mRenderer->SetView(mPlane.View());

	mRenderer->Update(t);
	mRenderer->Draw(t);
}

void Main::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		//mCamera.Pitch(dy);
		//mCamera.RotateY(-dx);

		mPlane.Pitch(dy);
		mPlane.Yaw(-dx);
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		// Make each pixel correspond to 0.2 unit in the scene.
		float dx = 0.05f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.05f * static_cast<float>(y - mLastMousePos.y);

		//// Update the camera radius based on input.
		//mRadius += dx - dy;

		//// Restrict the radius.
		//mRadius = Math::Clamp(mRadius, 5.0f, 150.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void Main::OnKeyUp(WPARAM wParam, LPARAM lParam)
{
	switch (wParam)
	{
	case 0x41:
	case 0x44:
		mPlane.Roll(Plane::STEER::NONE);
		break;
	case VK_SPACE:
	case 0x46:
		mPlane.Pitch(Plane::STEER::NONE);
		break;
	case 0x47:
		// G
		mPlane.Yaw(Plane::STEER::NONE);
		break;
	case 0x48:
		// H
		mPlane.Yaw(Plane::STEER::NONE);
		break;
	case VK_BACK:
		mPlane.Thrust(false);
		break;
	}
}

void Main::OnKeyDown(WPARAM wParam, LPARAM lParam)
{
	switch (wParam)
	{
	case 0x41:
		// A
		mPlane.Roll(Plane::STEER::POSITIVE);
		break;
	case VK_BACK:
		mPlane.Thrust(true);
		break;
	case VK_DELETE:
		mPlane.Reverse();
		break;
	case 0x44:
		// D
		mPlane.Roll(Plane::STEER::NEGATIVE);
		break;
	case VK_SPACE:
		mPlane.Pitch(Plane::STEER::NEGATIVE);
		break;
	case 0x46:
		// F
		mPlane.Pitch(Plane::STEER::POSITIVE);
		break;
	case 0x47:
		// G
		mPlane.Yaw(Plane::STEER::POSITIVE);
		break;
	case 0x48:
		// H
		mPlane.Yaw(Plane::STEER::NEGATIVE);
		break;
	case 0x49:
		// I
		break;
	}
}