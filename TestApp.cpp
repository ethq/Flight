#include "TestApp.h"
#include "GeometryGenerator.h"
#include "Utilities.h"
#include "Mesh.h"

using namespace DirectX;
using namespace Microsoft::WRL;

void TestApp::Update(const Timer& t)
{
	OnKeyboardInput(t);
	mPlane.Update(t);


	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % mNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}


	UpdateGeometry(t);
	UpdateInstanceBuffer(t, mDynamicRenderItems);
	UpdateMaterialBuffer(t);
	UpdateLights(t);
	//UpdateShadowPassCB(t); // called in drawshadowmap
	UpdateMainPassCB(t);
}

void TestApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mMainWnd);
}

void TestApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	// Screen space coordinates never exceed float range
	Pick(static_cast<float>(x), static_cast<float>(y));

	ReleaseCapture();
}

// Takes an input coordinate in screen space.
void TestApp::Pick(float x, float y)
{
	// Determine picking ray in view space
	/*
	In view space, ray starts at origin and goes through some point along the clicked point (x,y)_screen

	First task is to convert from screen space to NDC. We do this by reversing the viewport transformation. 
	Doing so yields the eqns for (x,y)_ndc - shifted towards positive z for convenience.

	We implicitly assume that the backbuffer has dimensions (mClientWidth, mClientHeight)
	*/
	auto xn = (+2.0f * x / static_cast<float>(mClientWidth) - 1.0f) / mProj(0, 0);
	auto yn = (-2.0f * y / static_cast<float>(mClientHeight) + 1.0f) / mProj(1, 1);

	// Picking ray is now in view space
	XMVECTOR rayOrigin = XMVectorSet( 0.0f, 0.0f, 0.0f, 1.0f );
	XMVECTOR rayDir = XMVectorSet( xn, yn, 1.0f, 0.0f );

	// Transform picking ray into each colliding item's local space - V^-1: View --> World
	auto det = XMMatrixDeterminant(mPlane.View());
	auto invView = XMMatrixInverse(&det, mPlane.View());

	for (auto category : mPickableRenderItems)
	{
		for (auto& ri : mRenderItems[category])
		{
			auto world = XMLoadFloat4x4(&ri->Instance(0).World);
			det = XMMatrixDeterminant(world);
			auto invWorld = XMMatrixInverse(&det, world);

			auto toLocal = XMMatrixMultiply(invView, invWorld); // invView* invWorld;

			// ray to local space
			auto locRayOrigin = XMVector3TransformCoord(rayOrigin, toLocal);
			auto locRayDir = XMVector3TransformNormal(rayDir, toLocal);
			locRayDir = XMVector3Normalize(locRayDir);

			// Does our ray intersect the BB?

			// ray parameter at closest collision point
			float tmin = 0.0f;
			if (ri->BoundsB.Intersects(locRayOrigin, locRayDir, tmin))
			{
				::OutputDebugStringA(ri->Name.c_str());

				// TODO make this optional;

				// We hit the bounding box, but we may not have hit the object itself. 
				// To find out we do ray-tri intersections

				// The submesh of a given renderitem belongs to a mesh, which stores copies of v/ibuffers
				auto vertices = (Vertex*)ri->Geo->VertexBufferCPU->GetBufferPointer();
				auto indices = (std::uint16_t*)ri->Geo->IndexBufferCPU->GetBufferPointer();

				UINT triCount = ri->IndexCount / 3;

				// Keep track of which triangle is the closest; for now we iterate over all tris in the mesh
				tmin = Math::Infty;
				for (UINT i = 0; i < triCount; ++i)
				{
					auto i0 = indices[ri->StartIndexLocation + i * 3 + 0];
					auto i1 = indices[ri->StartIndexLocation + i * 3 + 1];
					auto i2 = indices[ri->StartIndexLocation + i * 3 + 2];

					auto v0 = XMLoadFloat3(&vertices[i0].Pos);
					auto v1 = XMLoadFloat3(&vertices[i1].Pos);
					auto v2 = XMLoadFloat3(&vertices[i2].Pos);

					float t = 0.0f;

					if (TriangleTests::Intersects(locRayOrigin, locRayDir, v0, v1, v2, t))
					{
						if (t < tmin)
							tmin = t;
					}
				}

				if (tmin != Math::Infty)
					::OutputDebugStringA("Actual hit");
			}
		}
	}
}

void TestApp::OnKeyboardInput(const Timer& gt)
{
	//const float dt = gt.DeltaTime();

	//if (GetAsyncKeyState('W') & 0x8000)
	//	mCamera.Walk(10.0f * dt);

	//if (GetAsyncKeyState('S') & 0x8000)
	//	mCamera.Walk(-10.0f * dt);

	//if (GetAsyncKeyState('A') & 0x8000)
	//	mCamera.Strafe(-10.0f * dt);

	//if (GetAsyncKeyState('D') & 0x8000)
	//	mCamera.Strafe(10.0f * dt);

	mCamera.UpdateViewMatrix();
}

void TestApp::OnMouseMove(WPARAM btnState, int x, int y)
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

void TestApp::OnKeyUp(WPARAM wParam, LPARAM lParam)
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

void TestApp::OnKeyDown(WPARAM wParam, LPARAM lParam)
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
		mDbgFlag = !mDbgFlag;
		mPlane.Yaw(Plane::STEER::POSITIVE);
		break;
	case 0x48:
		// H
		mPlane.Yaw(Plane::STEER::NEGATIVE);
		break;
	case 0x49:
		// I
		mPlane.SetView(XMLoadFloat4x4(&mLights[0]->View[0]));
		mProj = mLights[0]->Proj;
		switch (gIdx)
		{
		case 0:
			OutputDebugString(L"+X\n");
			break;
		case 1:
			OutputDebugString(L"-X\n");
			break;
		case 2:
			OutputDebugString(L"+Y\n");
			break;
		case 3:
			OutputDebugString(L"-Y\n");
			break;
		case 4:
			OutputDebugString(L"+Z\n");
			break;
		case 5:
			OutputDebugString(L"-Z\n");
			break;
		default:
			break;
		}
		gIdx++;
		if (gIdx > 5)
			gIdx = 0;
		break;
	}
}

// Clears instances from a RenderItem. Since RenderItems are coupled to FrameResources, we need to clear the upload buffers there as well;
// So, for now, it is not enough to have a single member function to clear in RenderItem
void TestApp::ClearInstances(std::shared_ptr<RenderItem> ri)
{
	// Clear out upload buffers
	InstanceData idata;
	ZeroMemory(&idata, sizeof(InstanceData));

	for (auto i = 0; i < ri->InstanceCount(); ++i)
	{
		for (auto j = 0; j < mNumFrameResources; ++j)
			mFrameResources[j]->InstanceBuffers[ri->Id()]->CopyData(i, idata);
	}

	// Clear copy inside render item
	ri->ClearInstances();
}

// Todo: test ClearInstances - seems to work well, but yknow.
void TestApp::UpdateGeometry(const Timer& t)
{
	return;

	auto dbgBoxes = mRenderItems[RENDER_ITEM_TYPE::DEBUG_BOXES][0];
	ClearInstances(dbgBoxes);

	for (auto& ri : mRenderItems[RENDER_ITEM_TYPE::OPAQUE_DYNAMIC])
	{
		// Cube?
		auto iscube = (ri->Name.substr(0, 6) == "Cube.0");
		if (!iscube)
			continue;

		// Move cube
		auto rotup = 2*(rand() % 2) - 1;

		// rotate in local space
		float localRate = 1.0f;
		auto rotx = XMMatrixRotationX(0.1f*rotup*t.DeltaTime() * localRate);
		auto roty = XMMatrixRotationY(t.DeltaTime() * localRate);
		auto rotz = XMMatrixRotationZ(t.DeltaTime() * localRate);
		
		XMMATRIX pos = XMLoadFloat4x4(&ri->Instance(0).World);
		XMMATRIX npos = rotx * roty * pos;
		XMStoreFloat4x4(&ri->Instance(0).World, npos);

		// Update debug bounding box
		InstanceData idata;
		float scaleX = 2.0f * ri->BoundsB.Extents.x;
		float scaleY = 2.0f * ri->BoundsB.Extents.y;
		float scaleZ = 2.0f * ri->BoundsB.Extents.z;
		XMMATRIX btr = XMMatrixTranslation(ri->BoundsB.Center.x, ri->BoundsB.Center.y, ri->BoundsB.Center.z);
		XMMATRIX bsc = XMMatrixScaling(scaleX, scaleY, scaleZ);

		// Then apply world matrix for render item
		XMStoreFloat4x4(&idata.World, bsc * btr * npos);
		dbgBoxes->AddInstance(idata);

		ri->NumFramesDirty = mNumFrameResources;
	}
}

void TestApp::Draw(const Timer& t)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	ThrowIfFailed(cmdListAlloc->Reset());
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

	ID3D12DescriptorHeap* descHeaps[] = { mCbvDescHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descHeaps), descHeaps);

	// use the noshadow variant for the sobel pass(we dont want to have sharp shadow edges!) (?)
	// this is a little sad since it means ever more multipasses... we should have the option to easily turn this off
	mCommandList->SetPipelineState(mPSOs["opaque_noshadow"].Get()); 

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	mCommandList->ResourceBarrier(
		1, &CD3DX12_RESOURCE_BARRIER::Transition(
			mOffscreenRT->Resource(),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_STATE_RENDER_TARGET));

	mCommandList->ClearRenderTargetView(
		mOffscreenRT->Rtv(),
		DirectX::Colors::LightSteelBlue, 0, nullptr);
	
	mCommandList->ClearDepthStencilView(
		DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	mCommandList->OMSetRenderTargets(1, &mOffscreenRT->Rtv(), true, &DepthStencilView());

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	// Texture descriptors are given at the beginning
	auto tex = mCbvDescHeap->GetGPUDescriptorHandleForHeapStart();
	mCommandList->SetGraphicsRootDescriptorTable(0, tex);

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

	auto matSB = mCurrFrameResource->MaterialBuffer->Resource();
	mCommandList->SetGraphicsRootShaderResourceView(3, matSB->GetGPUVirtualAddress());

	// dont bind shadow and environment map
	mCommandList->SetGraphicsRootDescriptorTable(4, mNullSrv);
	mCommandList->SetGraphicsRootDescriptorTable(5, mNullSrv);
	mCommandList->SetGraphicsRootDescriptorTable(6, mNullSrv);

	// TODO: put these in an aggregate list
	DrawRenderItems(mCommandList.Get(), mRenderItems[RENDER_ITEM_TYPE::OPAQUE_DYNAMIC]);
	DrawRenderItems(mCommandList.Get(), mRenderItems[RENDER_ITEM_TYPE::OPAQUE_STATIC]);
	DrawRenderItems(mCommandList.Get(), mRenderItems[RENDER_ITEM_TYPE::WIREFRAME_DYNAMIC]);
	DrawRenderItems(mCommandList.Get(), mRenderItems[RENDER_ITEM_TYPE::WIREFRAME_STATIC]);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOffscreenRT->Resource(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));

	//mBlurFilter->Execute(mCommandList.Get(), mBlurRootSignature.Get(), mPSOs["horzBlur"].Get(), mPSOs["vertBlur"].Get(), mOffscreenRT->Resource(), 1);
	//mSobelFilter->Execute(mCommandList.Get(), mSobelRootSignature.Get(), mPSOs["sobel"].Get(), mBlurFilter->OutputSrv());

	mSobelFilter->Execute(mCommandList.Get(), mSobelRootSignature.Get(), mPSOs["sobel"].Get(), mOffscreenRT->Srv());

	DrawShadowMaps();

	// we have edges and the shadow maps
	// now we need to re-render the scene into the off-screen rtv with shadows, then combine with edges

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOffscreenRT->Resource(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// do we really need to reclear? any way we can reuse at least depth buffer?
	mCommandList->ClearRenderTargetView(
		mOffscreenRT->Rtv(),
		DirectX::Colors::LightSteelBlue, 0, nullptr);

	mCommandList->ClearDepthStencilView(
		DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Viewport reset by clearRTV
	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	mCommandList->SetPipelineState(mPSOs["opaque"].Get());

	// Grab the first shadowmaps, if they exist. They are assumed to be contiguous in memory

	D3D12_GPU_DESCRIPTOR_HANDLE flatShadowMapHandle = mNullSrv;
	D3D12_GPU_DESCRIPTOR_HANDLE cubeShadowMapHandle = mNullSrv;

	if ((mNumDirLights + mNumSpotLights) > 0)
		flatShadowMapHandle = mLights[0]->Shadowmap()->Srv();
	if (mNumPointLights > 0)
		cubeShadowMapHandle = mLights[mNumDirLights + mNumSpotLights]->Shadowmap()->Srv();


	mCommandList->SetGraphicsRootDescriptorTable(4, flatShadowMapHandle);
	mCommandList->SetGraphicsRootDescriptorTable(6, cubeShadowMapHandle);
	mCommandList->SetGraphicsRootDescriptorTable(5, mEnvironmentMapSrv); // presumably we can bind this early; the opaque shader doesn't use it.
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress()); // DrawShadowMap() sets it to the shadowpass cb; so reset

	mCommandList->OMSetRenderTargets(1, &mOffscreenRT->Rtv(), true, &DepthStencilView());

	DrawRenderItems(mCommandList.Get(), mRenderItems[RENDER_ITEM_TYPE::OPAQUE_DYNAMIC]);
	DrawRenderItems(mCommandList.Get(), mRenderItems[RENDER_ITEM_TYPE::OPAQUE_STATIC]);
	DrawRenderItems(mCommandList.Get(), mRenderItems[RENDER_ITEM_TYPE::WIREFRAME_DYNAMIC]);
	DrawRenderItems(mCommandList.Get(), mRenderItems[RENDER_ITEM_TYPE::WIREFRAME_STATIC]);

	//if (mDebugBoundingBoxesEnabled)
	//{
	//	mCommandList->SetPipelineState(mPSOs["opaque_wireframe"].Get());
	//	DrawRenderItems(mCommandList.Get(), mRenderItems[RENDER_ITEM_TYPE::DEBUG_BOXES]);
	//}

	mCommandList->SetPipelineState(mPSOs["envMap"].Get());
	DrawRenderItems(mCommandList.Get(), mRenderItems[RENDER_ITEM_TYPE::ENVIRONMENT_MAP]);

	//// the PSO/shader here renders out the first flat shadowmap
	//mCommandList->SetPipelineState(mPSOs["dbgShadow"].Get());
	//std::vector<RenderItem*> dbgquad;
	//dbgquad.push_back(mDbgQuad.get());
	//DrawRenderItems(mCommandList.Get(), dbgquad);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOffscreenRT->Resource(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));

	// and lastly combined shadow-rendered scene w/ edgemap

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	// This root signature is rather simple. Takes only two textures in t0 and t1.
	mCommandList->SetGraphicsRootSignature(mSobelRootSignature.Get());
	mCommandList->SetPipelineState(mPSOs["composite"].Get());
	mCommandList->SetGraphicsRootDescriptorTable(0, mOffscreenRT->Srv());
	mCommandList->SetGraphicsRootDescriptorTable(1, mSobelFilter->OutputSrv());
	DrawFullscreenQuad(mCommandList.Get());

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	//mCommandList->CopyResource(CurrentBackBuffer(), mBlurFilter->Output());

	//mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
	//	D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT));

	/*mCommandList->ResourceBarrier(1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT));*/

	ThrowIfFailed(mCommandList->Close());

	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	FlushCommandQueue();
}

void TestApp::DrawFullscreenQuad(ID3D12GraphicsCommandList* cmdList)
{
	cmdList->IASetVertexBuffers(0, 1, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	cmdList->DrawInstanced(6, 1, 0, 0);
}

void TestApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<std::shared_ptr<RenderItem>>& ritems)
{
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);
		
		// Bind instance buffer
		auto ib = mCurrFrameResource->InstanceBuffers[ri->Id()]->Resource();
		cmdList->SetGraphicsRootShaderResourceView(1, ib->GetGPUVirtualAddress());

		cmdList->DrawIndexedInstanced(ri->IndexCount, (UINT)ri->InstanceCount(), ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

// Convention: it is the Light classes responsibility to return view matrices that map from world space to view space.
// passIdx is used purely for point lights, as they require 6 rendering passes, and idx tells us which one we want
void TestApp::UpdateShadowPassCB(size_t lightIndex, UINT passIdx = 0)
{
	auto& l = mLights[lightIndex];
	XMMATRIX view = XMLoadFloat4x4(&l->View[passIdx]);
	XMMATRIX proj = XMLoadFloat4x4(&l->Proj);

	// Light view lives in light view space. So it must be inverted in order to take us from world->light viewspace
	// Note: applies only to point lights! For directional; no need. 
	
	//auto det = XMMatrixDeterminant(view);
	//view = XMMatrixInverse(&det, view);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	UINT w = l->Shadowmap()->Width();
	UINT h = l->Shadowmap()->Height();

	XMStoreFloat4x4(&mShadowPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mShadowPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mShadowPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mShadowPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mShadowPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mShadowPassCB.InvViewProj, XMMatrixTranspose(invViewProj));

	for (size_t i = 0; i < MaxLights; ++i)
		XMStoreFloat4x4(&mShadowPassCB.ShadowTransform[i], XMMatrixIdentity());

	mShadowPassCB.EyePosW = l->Light->Position;
	mShadowPassCB.RenderTargetSize = XMFLOAT2((float)w, (float)h);
	mShadowPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / w, 1.0f / h);
	mShadowPassCB.NearZ = l->Near;
	mShadowPassCB.FarZ = l->Far;

	//mShadowPassCB.TotalTime = 0.0f;
	//mShadowPassCB.DeltaTime = 0.0f;

	//// Copy light info - this might not be great.. hm. 
	//mShadowPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	//for (size_t i = 0; i < mLights.size(); ++i)
	//{
	//	mShadowPassCB.Lights[i].Direction = mLights[i]->Light->Direction;
	//	mShadowPassCB.Lights[i].Strength = mLights[i]->Light->Strength;
	//	mShadowPassCB.Lights[i].FalloffEnd = mLights[i]->Light->FalloffEnd;
	//	mShadowPassCB.Lights[i].FalloffStart = mLights[i]->Light->FalloffStart;
	//	mShadowPassCB.Lights[i].Position = mLights[i]->Light->Position;
	//}

	auto currPassCB = mCurrFrameResource->PassCB.get();

	// magic +1 due to the regular "main pass" constants
	// magic +k*6 due to each light having 6 pass constants
	currPassCB->CopyData(1 + lightIndex*6 + passIdx, mShadowPassCB);
}

void TestApp::DrawShadowMaps()
{
	for (size_t k = 0; k < mLights.size(); ++k)
	{
		auto& l = mLights[k];

		// point lights do 6 rendering passes for the cubemap
		UINT count = 1;
		if (l->Type == LightType::POINT)
			count = 6;

		for (UINT i = 0; i < count; ++i)
		{
			// Update appropriate shadowmap pass constants - view and proj in particular
			UpdateShadowPassCB(k, i);

			auto sm = l->Shadowmap();
			auto dsv = sm->Dsv();
			dsv.Offset(i, mDsvDescriptorSize);

			mCommandList->RSSetViewports(1, &sm->Viewport());
			mCommandList->RSSetScissorRects(1, &sm->ScissorRect());

			mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(sm->Resource(),
				D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));

			UINT passCBByteSize = Utilities::CalcConstantBufferByteSize(sizeof(PassConstants));

			mCommandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

			mCommandList->OMSetRenderTargets(0, nullptr, false, &dsv);

			auto passCB = mCurrFrameResource->PassCB->Resource();
			// A little hacky:
			// We know that FrameResource contains 1x regular passconstants, then 6 shadow passconstants. So we offset based on that
			// Note that due to memcpy not being buffered, point lights require 6 passconstants to not overwrite memory before we hit gpu execution
			// Note #2: for multiple lights, we unfortunately need even more shadow passconstants; otherwise, they get overwritten as well!
			//          to keep things compatible for every light being a point light, each light gets 6 passconstants - whether it is a point light or not
			// TODO: clean this up; we can do this more tightly.
			D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = passCB->GetGPUVirtualAddress() + (1u + k*6 + i) * passCBByteSize;
			mCommandList->SetGraphicsRootConstantBufferView(2, passCBAddress);

			mCommandList->SetPipelineState(mPSOs["shadowOpaque"].Get());

			DrawRenderItems(mCommandList.Get(), mRenderItems[RENDER_ITEM_TYPE::OPAQUE_DYNAMIC]);
			DrawRenderItems(mCommandList.Get(), mRenderItems[RENDER_ITEM_TYPE::OPAQUE_STATIC]);
			DrawRenderItems(mCommandList.Get(), mRenderItems[RENDER_ITEM_TYPE::WIREFRAME_DYNAMIC]);
			DrawRenderItems(mCommandList.Get(), mRenderItems[RENDER_ITEM_TYPE::WIREFRAME_STATIC]);

			mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(sm->Resource(),
				D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));
		}
	}
}

void TestApp::BuildSceneBounds()
{
	mSceneBoundS.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	mSceneBoundS.Radius = 3000;
}

// Update both direction/position and info needed for shadow mapping
void TestApp::UpdateLights(const Timer& t)
{
	return;
	// Update light data
	
	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	// Update shadow data
	// We need a view/proj matrix to render from each light's POV, as well as near/far planes
	for (auto& l : mLights)
	{
		if (l->Type == LightType::DIRECTIONAL)
		{
			auto rotAngle = 0.1f*t.DeltaTime();
			auto rotY = XMMatrixRotationY(rotAngle);
			auto rotX = XMMatrixRotationX(rotAngle);
			auto rot = XMMatrixMultiply(rotY, rotX);

			XMVECTOR lightDir = XMLoadFloat3(&l->Light->Direction);
			lightDir = XMVector4Transform(lightDir, rotY);
			XMStoreFloat3(&l->Light->Direction, lightDir);
			XMVECTOR lightPos = -mSceneBoundS.Radius * lightDir;
			XMVECTOR targetPos = XMLoadFloat3(&mSceneBoundS.Center);
			XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
			XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);
			XMStoreFloat3(&l->Light->Position, lightPos);

			//auto ls = mRenderItems[0].get();
			//XMStoreFloat4x4(&ls->World, XMMatrixTranslation(l->Light->Position.x, l->Light->Position.y, l->Light->Position.z));
			//ls->NumFramesDirty = mNumFrameResources;
			
			// Transform bounding sphere to light space.
			XMFLOAT3 sphereCenterLS;
			XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

			// Ortho frustum in light space encloses scene.
			float left = sphereCenterLS.x - mSceneBoundS.Radius;
			float bot = sphereCenterLS.y - mSceneBoundS.Radius;
			float near_ = sphereCenterLS.z - mSceneBoundS.Radius;
			float right = sphereCenterLS.x + mSceneBoundS.Radius;
			float top = sphereCenterLS.y + mSceneBoundS.Radius;
			float far_ = sphereCenterLS.z + mSceneBoundS.Radius;

			l->Near = near_;// 0.1f;// near_;
			l->Far = far_;// 200.0f;// far_;
			XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(left, right, bot, top, near_, far_);

			XMMATRIX S = lightView * lightProj * T;
			XMStoreFloat4x4(&l->View[0], lightView);
			XMStoreFloat4x4(&l->Proj, lightProj);
			XMStoreFloat4x4(&l->ShadowTransform[0], S);
		}
		else if (l->Type == LightType::SPOT)
		{
			auto rotAngle = t.DeltaTime() / 3.0f;
			auto rotX = XMMatrixRotationY(rotAngle);

			XMVECTOR lightPos = XMLoadFloat3(&l->Light->Position);
			lightPos = XMVector3Transform(lightPos, rotX);
			XMStoreFloat3(&l->Light->Position, lightPos);

			auto dir = XMLoadFloat3(&l->Light->Direction);
			dir = XMVector3Transform(dir, rotX);
			XMStoreFloat3(&l->Light->Direction, dir);

			auto lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
			auto targetPos = XMLoadFloat3(&mSceneBoundS.Center);
			auto view = XMMatrixLookAtLH(lightPos, targetPos, lightUp);

			auto proj = XMMatrixPerspectiveFovLH(0.25f * Math::Pi, 1.0f, l->Near, l->Far);

			auto S = view * proj * T;
			XMStoreFloat4x4(&l->View[0], view);
			XMStoreFloat4x4(&l->Proj, proj);
			XMStoreFloat4x4(&l->ShadowTransform[0], S);
		}
		else if (l->Type == LightType::POINT)
		{
			if (false)
				return;
			//auto rotAngle = t.DeltaTime() / 2.0f;
			//auto rotX = XMMatrixRotationY(rotAngle);

			//XMVECTOR lightPos = XMLoadFloat3(&l->Light->Position);
			//lightPos = XMVector3Transform(lightPos, rotX);
			//XMStoreFloat3(&l->Light->Position, lightPos);

			//l->BuildPLViewProj();

			//auto ls = mRenderItems[0].get();
			//XMStoreFloat4x4(&ls->World, XMMatrixTranslation(l->Light->Position.x, l->Light->Position.y, l->Light->Position.z));
			//ls->NumFramesDirty = mNumFrameResources;

			//auto movy = XMMatrixTranslation(0.0f, t.DeltaTime()/5.0f, 0.0f);
			//XMVECTOR lp = XMLoadFloat3(&l->Light->Position);
			//lp = XMVector3Transform(lp, movy);
			//XMStoreFloat3(&l->Light->Position, lp);
			//l->BuildPLViewProj();
		}
	}
}

void TestApp::UpdateMainPassCB(const Timer& gt)
{
	XMMATRIX view = mPlane.View(); // XMLoadFloat4x4(&mLights[0]->View[gIdx]); // mPlane.View();
	XMMATRIX proj = XMLoadFloat4x4(&mProj); // XMLoadFloat4x4(&mLights[0]->Proj[gIdx]); // mProj
	
	//auto det = XMMatrixDeterminant(view);
	//view = XMMatrixInverse(&det, view);

	 //XMMATRIX view = XMLoadFloat4x4(&mLights[0]->View[gIdx]);
	 //XMMATRIX proj = XMLoadFloat4x4(&mLights[0]->Proj[gIdx]);

	//XMMATRIX view = mCamera.GetView();
	//XMMATRIX proj = mCamera.GetProj();

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);


	XMStoreFloat4x4(&mPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	
	size_t k = 0;
	for (size_t i = 0; i < mLights.size(); ++i, ++k)
	{
		auto shadowTransform = XMLoadFloat4x4(&mLights[i]->ShadowTransform[0]); // dont need all the SF's for point lights.. only farplane Z
		XMStoreFloat4x4(&mPassCB.ShadowTransform[k], XMMatrixTranspose(shadowTransform));
	}
	// fill the rest? YES. Shader expects MaxLights float4x4s, so thats what we gotta give - otherwise itll read random crap for following variables
	for (size_t i = k; i < MaxLights; ++i, ++k)
		mPassCB.ShadowTransform[k] = Math::Identity4x4();

	mPassCB.EyePosW = mPlane.GetPos3f();

	mPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mPassCB.NearZ = 10.0f;
	mPassCB.FarZ = 30.0f;
	mPassCB.TotalTime = gt.TotalTime();
	mPassCB.DeltaTime = gt.DeltaTime();
	
	// Copy light info - this might not be great.. hm. 
	mPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };

	for (size_t i = 0; i < mLights.size(); ++i)
	{
		mPassCB.Lights[i].Direction = mLights[i]->Light->Direction;
		mPassCB.Lights[i].Strength = mLights[i]->Light->Strength;
		mPassCB.Lights[i].FalloffEnd = mLights[i]->Light->FalloffEnd;
		mPassCB.Lights[i].FalloffStart = mLights[i]->Light->FalloffStart;
		mPassCB.Lights[i].Position = mLights[i]->Light->Position;
	}

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mPassCB);
}


// Render items are guaranteed to be constructed at this point
void TestApp::BuildFrameResources()
{
	// one for doing regular passes, six for doing shadow passes. 
	// in a previous iteration of the holy code, these were getting overwritten and giving very strange rendering results
	const UINT passCount = 1 + 6*(mNumPointLights + mNumDirLights + mNumSpotLights); 

	// each renderitem must have an uploadbuffer of instance data
	std::map<UINT, UINT> rItemInstances;
	for (auto& categ : mRenderItems)
	{
		for (auto& ri: categ.second)
			rItemInstances[ri->Id()] = ri->MaxInstances;
	}

	for (UINT i = 0; i < mNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(mD3Device.Get(), passCount, rItemInstances, (UINT)mMaterials.size()));
	}
}

void TestApp::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	//opaquePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaEnabled ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaEnabled ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(mD3Device->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	//
	// PSO for debugging shadowmap
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPsoDesc = opaquePsoDesc;
	debugPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["dbgShadowVS"]->GetBufferPointer()),
		mShaders["dbgShadowVS"]->GetBufferSize()
	};
	debugPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["dbgShadowPS"]->GetBufferPointer()),
		mShaders["dbgShadowPS"]->GetBufferSize()
	};
	ThrowIfFailed(mD3Device->CreateGraphicsPipelineState(&debugPsoDesc, IID_PPV_ARGS(&mPSOs["dbgShadow"])));
	// 
	// PSO for opaque objects, but without shadows
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueNoshadowPsoDesc = opaquePsoDesc;
	opaqueNoshadowPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["standardVS_noshadow"]->GetBufferPointer()),
		mShaders["standardVS_noshadow"]->GetBufferSize()
	};
	opaqueNoshadowPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["opaquePS_noshadow"]->GetBufferPointer()),
		mShaders["opaquePS_noshadow"]->GetBufferSize()
	};
	ThrowIfFailed(mD3Device->CreateGraphicsPipelineState(&opaqueNoshadowPsoDesc, IID_PPV_ARGS(&mPSOs["opaque_noshadow"])));

	//
	// PSO for environment mapping
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC envmapPso = opaquePsoDesc;
	envmapPso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // we're rendering inside out

	// set depth test to less_equal so as not to fail at z = 1(which we clear to)
	envmapPso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	envmapPso.pRootSignature = mRootSignature.Get();
	envmapPso.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["envVS"]->GetBufferPointer()),
		mShaders["envVS"]->GetBufferSize()
	};
	envmapPso.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["envPS"]->GetBufferPointer()),
		mShaders["envPS"]->GetBufferSize()
	};
	ThrowIfFailed(mD3Device->CreateGraphicsPipelineState(&envmapPso, IID_PPV_ARGS(&mPSOs["envMap"])));

	//
	// PSO for opaque wireframe objects.
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(mD3Device->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));

	// 
	// PSOs for blurring
	//

	D3D12_COMPUTE_PIPELINE_STATE_DESC horzBlurPSO = {};
	horzBlurPSO.pRootSignature = mBlurRootSignature.Get();
	horzBlurPSO.CS = {
		reinterpret_cast<BYTE*>(mShaders["horzBlurCS"]->GetBufferPointer()),
		mShaders["horzBlurCS"]->GetBufferSize()
	};
	horzBlurPSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(mD3Device->CreateComputePipelineState(&horzBlurPSO, IID_PPV_ARGS(&mPSOs["horzBlur"])));

	D3D12_COMPUTE_PIPELINE_STATE_DESC vertBlurPSO = {};
	vertBlurPSO.pRootSignature = mBlurRootSignature.Get();
	vertBlurPSO.CS = {
		reinterpret_cast<BYTE*>(mShaders["vertBlurCS"]->GetBufferPointer()),
		mShaders["vertBlurCS"]->GetBufferSize()
	};
	vertBlurPSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(mD3Device->CreateComputePipelineState(&vertBlurPSO, IID_PPV_ARGS(&mPSOs["vertBlur"])));

	//
	// PSO for combining textures
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC combPso = opaquePsoDesc;
	combPso.pRootSignature = mSobelRootSignature.Get();
	combPso.DepthStencilState.DepthEnable = false;
	combPso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	combPso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	combPso.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["compositeVS"]->GetBufferPointer()),
		mShaders["compositeVS"]->GetBufferSize()
	};
	combPso.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["compositePS"]->GetBufferPointer()),
		mShaders["compositePS"]->GetBufferSize()
	};
	ThrowIfFailed(mD3Device->CreateGraphicsPipelineState(&combPso, IID_PPV_ARGS(&mPSOs["composite"])));

	//
	// PSO for sobel filter
	//
	D3D12_COMPUTE_PIPELINE_STATE_DESC sobelPso = {};
	sobelPso.pRootSignature = mSobelRootSignature.Get();
	sobelPso.CS =
	{
		reinterpret_cast<BYTE*>(mShaders["sobelCS"]->GetBufferPointer()),
		mShaders["sobelCS"]->GetBufferSize()
	};
	sobelPso.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(mD3Device->CreateComputePipelineState(&sobelPso, IID_PPV_ARGS(&mPSOs["sobel"])));

	// Notes to self: the horrendous point light shadow bugs that plagued me were due to:
	// 1. Not realizing that I kept overwriting memory with memcpy in DrawShadowMaps(); even though the draw calls occur sequentially on the GPU,
	//    GPU execution doesn't begin until the command list is submitted. Hence the memcpys all hit before GPU exec begins => overwriting => bad times.

	// 2. Simply depth scaling; no error with the depth comparison itself, which I thought for a long time(since I need to manually conver to NDC coords).

	//
	// PSO for shadow map
	//	
	D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPso = opaquePsoDesc;
	shadowPso.RasterizerState.DepthBias = 100; // 150 for point light; // very difficult to avoid shadow acne when the light is almost perpendicular to surface normal
	shadowPso.RasterizerState.DepthBiasClamp = 1.0f; 
	shadowPso.RasterizerState.SlopeScaledDepthBias = 4.0f;  // depth = 10000, depthscaled = 1.0f works OK with spotlight
	//shadowPso.RasterizerState.DepthClipEnable = true;
	shadowPso.pRootSignature = mRootSignature.Get();        // depth = 250000, depthscaled = 1.0f works OK with pointlight; 750000, 10, (5,35) pt
	shadowPso.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["shadowVS"]->GetBufferPointer()),
		mShaders["shadowVS"]->GetBufferSize()
	};
	shadowPso.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["shadowOpaquePS"]->GetBufferPointer()),
		mShaders["shadowOpaquePS"]->GetBufferSize()
	};

	shadowPso.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	shadowPso.NumRenderTargets = 0;
	ThrowIfFailed(mD3Device->CreateGraphicsPipelineState(&shadowPso, IID_PPV_ARGS(&mPSOs["shadowOpaque"])));
}

// Lights are expected to be stored in the order directional -> spot -> point.
// This is due to 1) shader compatibility and 2) assumption that flat shadowmaps and cubic shadowmaps lie contiguously in memory
void TestApp::InitLights()
{
	//mLights.push_back(std::make_shared<LightPovData>(LightType::SPOT, mD3Device));

	//mLights[0]->Light->Direction = { 15.7735f, -5.7735f, 15.7735f };
	//mLights[0]->Light->Strength = { 0.8f, 0.8f, 0.8f };
	//mLights[0]->Light->FalloffEnd = 1000.0f;
	//mLights[0]->Light->FalloffStart = 10.0f;
	//mLights[0]->Light->SpotPower = 10.0f;
	//mLights[0]->Light->Position = { -15.7735f, 5.7735f, -15.7735f };
	//mLights[0]->Near = 10.0f;
	//mLights[0]->Far = 50.0f;

	//++mNumSpotLights;

	auto dl = std::make_shared<LightPovData>(LightType::DIRECTIONAL, mD3Device);
	dl->Light->Direction = { 1.0f, -1.0f, 1.0f };
	dl->Light->Strength = { 0.5f, 0.5f, 0.5f };
	dl->Light->FalloffEnd = 1500.0f;
	dl->Light->FalloffStart = 900.0f;

	mLights.push_back(dl);
	mNumDirLights++;

	auto pl = std::make_shared<LightPovData>(LightType::POINT, mD3Device, mDsvDescriptorSize);
	pl->Light->Direction = { 0.0f, 0.0f, 0.0f };
	pl->Light->Strength = { 0.7f, 0.7f, 0.7f };
	pl->Light->FalloffEnd = 1000.0f;
	pl->Light->FalloffStart = 50.0f;
	pl->Light->SpotPower = 0.0f;
	pl->Light->Position = { 0.0f, 0.0f, 0.0f };
	pl->Near = 1.0f;
	pl->Far = 800.0f; // TODO: calculate these based on scene bounds from light view

	pl->BuildPLViewProj();
	mLights.push_back(pl);
	++mNumPointLights;

	// keep track of this for the shader. NOTE: depending on how i do things, may have to recompile shader if numlights changes at runtime

	// Write code to make sure light array is sorted in the order dir/spot -> point last. That lets us supply the first descriptor to a textable
	// of shadowmaps
}

bool TestApp::Initialize()
{
	if (!D3Base::Initialize())
		return false;
	
	wchar_t buffer[MAX_PATH];
	GetModuleFileNameW(NULL, buffer, MAX_PATH);

	// Since the exec changes(sometimes its debug, sometimes its release) location, the exec location is not useful.
	// So we need the project path, or do as recommended, store in user/documents or something like that.
	mProjectPath = L"F:\\Code from the dungeon\\PLS\\"; // hardcode for now, TODO
	mProjectPath = L"D:\\GitHub\\Flight\\";


	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	mBlurFilter = std::make_unique<BlurFilter>(mD3Device.Get(), mClientWidth, mClientHeight, DXGI_FORMAT_R8G8B8A8_UNORM);
	mSobelFilter = std::make_unique<SobelFilter>(mD3Device.Get(), mClientWidth, mClientHeight, DXGI_FORMAT_R8G8B8A8_UNORM);
	mOffscreenRT = std::make_unique<RenderTarget>(mD3Device.Get(), mClientWidth, mClientHeight, DXGI_FORMAT_R8G8B8A8_UNORM);
	
	mCamera.SetPosition(0.0f, 2.0f, -15.0f);
	
	BuildSceneBounds();

	// InitLights must precede BuildDescriptorHeaps, because it needs to know how many descriptors each shadow map needs
	// Note that Light constructors do not begin to build descriptors, so that they can provide information to the construction
	// of the heap.
	InitLights(); 

	/*
	mPlane.SetView(XMLoadFloat4x4(&mLights[0]->View[0]));
	mProj = mLights[0]->Proj;
	*/

	//XMStoreFloat4x4(&mProj, XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, 70.0f, 110.0f));

	LoadTextures();
	BuildRootSignature();
	BuildPostprocessRootSignature();
	BuildShadersAndInputLayout();
	BuildStaticGeometry();
	BuildMaterials();
	BuildRenderItems();
	BuildFrameResources();
	BuildDescriptorHeaps();
	BuildDescriptors();
	BuildPSOs();
	
	for (size_t i = 0; i < mNumFrameResources; ++i)
	{
		mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % mNumFrameResources;
		mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();
		UpdateInstanceBuffer(mTimer, mStaticRenderItems);
	}

	// Turn camera around
	mPlane.Yaw(XM_PI);


	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	// Wait for init to complete
	FlushCommandQueue();

	return true;
}

// Rename to BuildDescriptors to make things a bit clearer(for myself..)

/*
Note: As it is, the heap needs to be built in order:
1) Object CBVs
2) Texture SRVs
3) Random crap (shadowmap, sobel, blur, etc)
*/
void TestApp::BuildDescriptors()
{
	// We put textures at the end of the object constants. Could as well have put the pass cb in here but w/e
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mCbvDescHeap->GetCPUDescriptorHandleForHeapStart());
	CD3DX12_GPU_DESCRIPTOR_HANDLE gDescriptor(mCbvDescHeap->GetGPUDescriptorHandleForHeapStart());

	auto bricksTex = mTextures["bricksTex"]->Resource;
	auto stoneTex = mTextures["stoneTex"]->Resource;
	auto tileTex = mTextures["tileTex"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = bricksTex->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = bricksTex->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	mD3Device->CreateShaderResourceView(bricksTex.Get(), &srvDesc, hDescriptor);

	hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	gDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

	srvDesc.Format = stoneTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = stoneTex->GetDesc().MipLevels;
	mD3Device->CreateShaderResourceView(stoneTex.Get(), &srvDesc, hDescriptor);

	hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	gDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

	srvDesc.Format = tileTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = tileTex->GetDesc().MipLevels;
	mD3Device->CreateShaderResourceView(tileTex.Get(), &srvDesc, hDescriptor);

	hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	gDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

	// save a reference so we can bind
	mEnvironmentMapSrv = CD3DX12_GPU_DESCRIPTOR_HANDLE(gDescriptor);

	ZeroMemory(&srvDesc, sizeof(D3D12_SHADER_RESOURCE_VIEW_DESC));
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = mTextures["envTex"]->Resource->GetDesc().MipLevels;
	srvDesc.Texture1D.ResourceMinLODClamp = 0.0f;
	srvDesc.Format = mTextures["envTex"]->Resource->GetDesc().Format;

	mD3Device->CreateShaderResourceView(mTextures["envTex"]->Resource.Get(), &srvDesc, hDescriptor);
	
	hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	gDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

	mBlurFilter->BuildDescriptors(
		hDescriptor,
		gDescriptor,
		mCbvSrvUavDescriptorSize
	);

	// TODO make the filters take handles by ref, so that they remain offset after use?
	hDescriptor.Offset(mBlurFilter->DescriptorCount(), mCbvSrvUavDescriptorSize);
	gDescriptor.Offset(mBlurFilter->DescriptorCount(), mCbvSrvUavDescriptorSize);
	
	mSobelFilter->BuildDescriptors(
		hDescriptor,
		gDescriptor,
		mCbvSrvUavDescriptorSize
	);

	auto rtvStart = mRtvHeap->GetCPUDescriptorHandleForHeapStart();
	UINT rtvOffset = SwapChainBufferCount;

	hDescriptor.Offset(mSobelFilter->DescriptorCount(), mCbvSrvUavDescriptorSize);
	gDescriptor.Offset(mSobelFilter->DescriptorCount(), mCbvSrvUavDescriptorSize);

	mOffscreenRT->BuildDescriptors(
		hDescriptor,
		gDescriptor,
		CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvStart, rtvOffset, mRtvDescriptorSize)
	);

	hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	gDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

	// when we get to shadow mapping, we need(?) to bind a null cube map. (Yes - everything in root sig must be bound, even if unused.
	// alternative is to switch root signatures, but that is apparently not a cheap operation)
	mNullSrv = CD3DX12_GPU_DESCRIPTOR_HANDLE(gDescriptor);

	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	mD3Device->CreateShaderResourceView(nullptr, &srvDesc, hDescriptor);

	hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	gDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

	// IMPORTANT: MUST BE SORTED BY LIGHT TYPE PRIOR TO DESCRIPTOR CONSTRUCTION (add check TODO)

	for (size_t i = 0; i < mNumDirLights + mNumSpotLights; ++i)
	{
		// magic +1 because the default rendering uses a depth stencil as well
		mLights[i]->Shadowmap()->BuildDescriptors(
			hDescriptor,
			gDescriptor,
			CD3DX12_CPU_DESCRIPTOR_HANDLE(mDsvHeap->GetCPUDescriptorHandleForHeapStart(), (INT)i + 1, mDsvDescriptorSize));

		// Directional and spot lights use a single SRV
		hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
		gDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	}
	
	for (size_t i = mNumDirLights + mNumSpotLights; i < mLights.size(); ++i)
	{
		// magic +1 because the default rendering uses a depth stencil view as well
		mLights[i]->Shadowmap()->BuildDescriptors(
			hDescriptor,
			gDescriptor,
			CD3DX12_CPU_DESCRIPTOR_HANDLE(mDsvHeap->GetCPUDescriptorHandleForHeapStart(), 1 + (INT)mNumDirLights+mNumSpotLights + i*6, mDsvDescriptorSize));

		// Point lights use a single SRV, but six depth views
		hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
		hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	}
}

void TestApp::BuildSobelRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE srvTable0;
	srvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE srvTable1;
	srvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

	CD3DX12_DESCRIPTOR_RANGE uavTable0;
	uavTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	CD3DX12_ROOT_PARAMETER slotRootParameter[3];
	slotRootParameter[0].InitAsDescriptorTable(1, &srvTable0);
	slotRootParameter[1].InitAsDescriptorTable(1, &srvTable1);
	slotRootParameter[2].InitAsDescriptorTable(1, &uavTable0);

	auto staticSamplers = GetStaticSamplers();

	CD3DX12_ROOT_SIGNATURE_DESC	rootSigDesc(3, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());

	ThrowIfFailed(hr);

	ThrowIfFailed(mD3Device->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mSobelRootSignature.GetAddressOf())
	));
}

void TestApp::BuildBlurRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE srvTable;
	srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE uavTable;
	uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	CD3DX12_ROOT_PARAMETER slotRootParameter[3];

	slotRootParameter[0].InitAsConstants(12, 0);
	slotRootParameter[1].InitAsDescriptorTable(1, &srvTable);
	slotRootParameter[2].InitAsDescriptorTable(1, &uavTable);

	CD3DX12_ROOT_SIGNATURE_DESC rootDesc(3, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootsig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	HRESULT hr = D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootsig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());

	ThrowIfFailed(hr);

	ThrowIfFailed(mD3Device->CreateRootSignature(
		0,
		serializedRootsig->GetBufferPointer(),
		serializedRootsig->GetBufferSize(),
		IID_PPV_ARGS(mBlurRootSignature.GetAddressOf())
	));
}

void TestApp::BuildPostprocessRootSignature()
{
	BuildSobelRootSignature();
	BuildBlurRootSignature();
}

void TestApp::BuildRootSignature()
{
	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[7]; 

	// Diffuse textures
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, (UINT)mTextures.size(), 0, 3); // Hurray for dynamic indexing. Does kill support for HLSL < 5.1 though.

	// Environment cubemap
	CD3DX12_DESCRIPTOR_RANGE envTable;
	envTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // cube map. We use 3 tables since theyre typically not used at the same time

	// Flat shadow maps
	// Note: it is crucial that the descriptors for the flat shadowmaps are not mixed with the point light descriptors!
	CD3DX12_DESCRIPTOR_RANGE shdTable;
	shdTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, std::max<UINT>(mNumDirLights + mNumSpotLights, 1u), 1);

	// Point shadow maps
	CD3DX12_DESCRIPTOR_RANGE ptShdTable;
	ptShdTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, std::max<UINT>(mNumPointLights, 1u), 0, 1); // use space1 as shader is unaware of the size of t3

	// Create root CBVs.
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable);  // texture cb
	//slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable0); // object cb
	slotRootParameter[1].InitAsShaderResourceView(0, 2); // Instance buffer
	slotRootParameter[2].InitAsConstantBufferView(0); // pass cb
	//slotRootParameter[3].InitAsConstantBufferView(2); // material cb
	slotRootParameter[3].InitAsShaderResourceView(1, 2); // Material buffer
	slotRootParameter[4].InitAsDescriptorTable(1, &shdTable); 
	slotRootParameter[5].InitAsDescriptorTable(1, &envTable);
	slotRootParameter[6].InitAsDescriptorTable(1, &ptShdTable);

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(_countof(slotRootParameter), slotRootParameter, (UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(mD3Device->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void TestApp::BuildRenderItems()
{
	auto skydome = std::make_shared<RenderItem>(mNumFrameResources);

	InstanceData idata;
	XMStoreFloat4x4(&idata.World, XMMatrixScaling(10000.0f, 10000.0f, 10000.0f));
	// TODO SET MATERIAL INDEX
	skydome->AddInstance(idata);

	skydome->Geo = mGeometries["shapes"].get();
	skydome->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skydome->IndexCount = skydome->Geo->DrawArgs["sphere"].IndexCount;
	skydome->StartIndexLocation = skydome->Geo->DrawArgs["sphere"].StartIndexLocation;
	skydome->BaseVertexLocation = skydome->Geo->DrawArgs["sphere"].BaseVertexLocation;

	mRenderItems[RENDER_ITEM_TYPE::ENVIRONMENT_MAP].push_back(std::move(skydome));
	
	auto dbgQuad = std::make_shared<RenderItem>(mNumFrameResources);
	idata.World = Math::Identity4x4();
	//TODO SET MATERIAL INDEX
	dbgQuad->AddInstance(idata);

	dbgQuad->Geo = mGeometries["shapes"].get();
	dbgQuad->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	dbgQuad->IndexCount = dbgQuad->Geo->DrawArgs["dbgQuad"].IndexCount;
	dbgQuad->StartIndexLocation = dbgQuad->Geo->DrawArgs["dbgQuad"].StartIndexLocation;
	dbgQuad->BaseVertexLocation = dbgQuad->Geo->DrawArgs["dbgQuad"].BaseVertexLocation;

	mRenderItems[RENDER_ITEM_TYPE::DEBUG_QUAD_SHADOWMAP].push_back(std::move(dbgQuad));

	//auto ls = std::make_unique<RenderItem>(mNumFrameResources);
	//auto pos = XMFLOAT3(10.0f, 0.0f, 10.0f);
	//XMStoreFloat4x4(&idata.World, XMMatrixScaling(1.0f, 1.0f, 1.0f)*XMMatrixTranslation(pos.x, pos.y, pos.z));
	//ls->Instances.push_back(idata);
	//
	//ls->Geo = mGeometries["shapes"].get();
	//ls->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	//ls->IndexCount = ls->Geo->DrawArgs["sphere2"].IndexCount;
	//ls->StartIndexLocation = ls->Geo->DrawArgs["sphere2"].StartIndexLocation;
	//ls->BaseVertexLocation = ls->Geo->DrawArgs["sphere2"].BaseVertexLocation;
	//
	//mRenderItems.push_back(std::move(ls));

	//auto grid = std::make_unique<RenderItem>(mNumFrameResources);
	//XMStoreFloat4x4(&grid->World, XMMatrixScaling(10.0f, 10.0f, 10.0f));
	//XMStoreFloat4x4(&grid->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	//grid->cbObjectIndex = cbObjectIndex++;
	//grid->Mat = mMaterials["tile0"].get();
	//grid->Geo = mGeometries["shapes"].get();
	//grid->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	//grid->IndexCount = grid->Geo->DrawArgs["grid"].IndexCount;
	//grid->StartIndexLocation = grid->Geo->DrawArgs["grid"].StartIndexLocation;
	//grid->BaseVertexLocation = grid->Geo->DrawArgs["grid"].BaseVertexLocation;

	//mRenderItems.push_back(std::move(grid));

	auto box = std::make_shared<RenderItem>(mNumFrameResources, 100);
	box->Geo = mGeometries["shapes"].get();
	box->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	box->IndexCount = box->Geo->DrawArgs["debugBoxes"].IndexCount;
	box->StartIndexLocation = box->Geo->DrawArgs["debugBoxes"].StartIndexLocation;
	box->BaseVertexLocation = box->Geo->DrawArgs["debugBoxes"].BaseVertexLocation;

	mRenderItems[RENDER_ITEM_TYPE::DEBUG_BOXES].push_back(box);

	
	// In case the geometry/mesh has many submeshes, we'll create the corresp renderitems in a loop
	for (auto& g : mGeometries[mLevel.c_str()]->DrawArgs)
	{
		auto ri = std::make_shared<RenderItem>(mNumFrameResources);

		idata.World = Math::Identity4x4();
		//XMStoreFloat4x4(&idata.World, XMMatrixScaling(0.1f, 0.1f, 0.1f));
		// TODO SET MATINDEX
		ri->AddInstance(idata);
		
		ri->Geo = mGeometries[mLevel.c_str()].get();
		ri->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		ri->IndexCount = g.second.IndexCount;
		ri->StartIndexLocation = g.second.StartIndexLocation;
		ri->BaseVertexLocation = g.second.BaseVertexLocation;
		ri->Name = g.first;
		ri->BoundsB = g.second.Bounds;

		// Add debug box
		// First move/scale boundingbox in local space. The debug box is at the origin and scaled at (1, 1, 1) by default.
		float scaleX = 2.0f * ri->BoundsB.Extents.x;
		float scaleY = 2.0f * ri->BoundsB.Extents.y;
		float scaleZ = 2.0f * ri->BoundsB.Extents.z;
		XMMATRIX btr = XMMatrixTranslation(ri->BoundsB.Center.x, ri->BoundsB.Center.y, ri->BoundsB.Center.z);
		XMMATRIX bsc = XMMatrixScaling(scaleX, scaleY, scaleZ);

		// Then apply world matrix for render item
		XMStoreFloat4x4(&idata.World, bsc*btr);
		box->AddInstance(idata);

		mRenderItems[RENDER_ITEM_TYPE::OPAQUE_DYNAMIC].push_back(ri);
	}

	auto plane = mGeometries["Scythe"]->DrawArgs["Plane"];
	auto ri = std::make_shared<RenderItem>(mNumFrameResources);

	idata.World = Math::Identity4x4();
	//XMStoreFloat4x4(&idata.World, XMMatrixTranslation(0.0f, 0.0f, -200.0f));
	//XMStoreFloat4x4(&idata.World, XMMatrixScaling(0.1f, 0.1f, 0.1f));
	// TODO SET MATINDEX
	ri->AddInstance(idata);

	ri->Geo = mGeometries["Scythe"].get();
	ri->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	ri->IndexCount = plane.IndexCount;
	ri->StartIndexLocation = plane.StartIndexLocation;
	ri->BaseVertexLocation = plane.BaseVertexLocation;
	ri->Name = "Plane";
	ri->BoundsB = plane.Bounds;
	ri->CollisionMesh = mGeometries["Scythe"]->DrawArgs["CollisionMesh"];

	// Add debug box
	// First move/scale boundingbox in local space. The debug box is at the origin and scaled at (1, 1, 1) by default.
	float scaleX = 2.0f * ri->BoundsB.Extents.x;
	float scaleY = 2.0f * ri->BoundsB.Extents.y;
	float scaleZ = 2.0f * ri->BoundsB.Extents.z;
	XMMATRIX btr = XMMatrixTranslation(ri->BoundsB.Center.x, ri->BoundsB.Center.y, ri->BoundsB.Center.z);
	XMMATRIX bsc = XMMatrixScaling(scaleX, scaleY, scaleZ);

	// Then apply world matrix for render item
	XMStoreFloat4x4(&idata.World, bsc * btr);
	box->AddInstance(idata);

	mRenderItems[RENDER_ITEM_TYPE::OPAQUE_DYNAMIC].push_back(ri);
	mPlane.AddRenderItem(ri);
}

void TestApp::BuildStaticGeometry()
{
	GeometryGenerator geo;
	auto box = geo.CreateBox(1.0f, 1.0f, 1.0f, 0);
	auto grid = geo.CreateGrid(10.0f, 10.0f, 2, 2);
	auto sphere = geo.CreateSphere(1.0f, 20, 20); // environment map
	auto sphere2 = geo.CreateSphere(0.5f, 10, 10); // debug to track position of light
	auto dbgquad = geo.CreateQuad(0.0f, 0.0f, 1.0f, 1.0f, 0.0f); // we just give ndc coordinates directly(e.g. identity view/world). they live in [-1, 1]^2

	size_t boxVertexOffset = 0;
	size_t gridVertexOffset = box.Vertices.size();
	size_t sphereVertexOffset = gridVertexOffset + grid.Vertices.size();
	size_t sphere2VertexOffset = sphereVertexOffset + sphere.Vertices.size();
	size_t dbgquadVertexOffset = sphere2VertexOffset + sphere2.Vertices.size();

	size_t boxIndexOffset = 0;
	size_t gridIndexOffset = box.Indices32.size();
	size_t sphereIndexOffset = gridIndexOffset + grid.Indices32.size();
	size_t sphere2IndexOffset = sphereIndexOffset + sphere.Indices32.size();
	size_t dbgquadIndexOffset = sphere2IndexOffset + sphere2.Indices32.size();

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = (UINT)boxIndexOffset;
	boxSubmesh.BaseVertexLocation = (INT)boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = (UINT)gridIndexOffset;
	gridSubmesh.BaseVertexLocation = (INT)gridVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = (UINT)sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = (INT)sphereVertexOffset;

	SubmeshGeometry sphere2Submesh;
	sphere2Submesh.IndexCount = (UINT)sphere2.Indices32.size();
	sphere2Submesh.StartIndexLocation = (UINT)sphere2IndexOffset;
	sphere2Submesh.BaseVertexLocation = (INT)sphere2VertexOffset;

	SubmeshGeometry dbgquadSubmesh;
	dbgquadSubmesh.IndexCount = (UINT)dbgquad.Indices32.size();
	dbgquadSubmesh.StartIndexLocation = (UINT)dbgquadIndexOffset;
	dbgquadSubmesh.BaseVertexLocation = (INT)dbgquadVertexOffset;

	auto totalVertices = box.Vertices.size() + grid.Vertices.size() + sphere.Vertices.size() + sphere2.Vertices.size() + dbgquad.Vertices.size();

	std::vector<Vertex> vertices(totalVertices);

	// Copy vertices from generator
	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;
	}
	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].TexC = grid.Vertices[i].TexC;
	}
	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;
	}
	for (size_t i = 0; i < sphere2.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere2.Vertices[i].Position;
		vertices[k].Normal = sphere2.Vertices[i].Normal;
		vertices[k].TexC = sphere2.Vertices[i].TexC;
	}
	for (size_t i = 0; i < dbgquad.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = dbgquad.Vertices[i].Position;
		vertices[k].Normal = dbgquad.Vertices[i].Normal;
		vertices[k].TexC = dbgquad.Vertices[i].TexC;
	}

	// Copy verts/indices into one big vbuffer/ibuffer
	std::vector<uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere2.GetIndices16()), std::end(sphere2.GetIndices16()));
	indices.insert(indices.end(), std::begin(dbgquad.GetIndices16()), std::end(dbgquad.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(uint16_t);

	auto geomesh = std::make_unique<Mesh>();
	geomesh->Name = "shapes";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geomesh->VertexBufferCPU));
	CopyMemory(geomesh->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geomesh->IndexBufferCPU));
	CopyMemory(geomesh->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);
	
	geomesh->VertexBufferGPU = Utilities::CreateDefaultBuffer(mD3Device.Get(), mCommandList.Get(), vertices.data(), vbByteSize, geomesh->VertexBufferUploader);
	geomesh->IndexBufferGPU = Utilities::CreateDefaultBuffer(mD3Device.Get(), mCommandList.Get(), indices.data(), ibByteSize, geomesh->IndexBufferUploader);

	geomesh->VertexByteStride = sizeof(Vertex);
	geomesh->VertexBufferByteSize = vbByteSize;
	geomesh->IndexFormat = DXGI_FORMAT_R16_UINT;
	geomesh->IndexBufferByteSize = ibByteSize;

	geomesh->DrawArgs["debugBoxes"] = boxSubmesh;
	geomesh->DrawArgs["grid"] = gridSubmesh;
	geomesh->DrawArgs["sphere"] = sphereSubmesh;
	geomesh->DrawArgs["sphere2"] = sphere2Submesh;
	geomesh->DrawArgs["dbgQuad"] = dbgquadSubmesh;

	mGeometries[geomesh->Name] = std::move(geomesh);

	// Let's load the static canyon geometry
	auto m = std::make_unique<Mesh>(mD3Device, mCommandList);
	auto success = m->LoadOBJ(mProjectPath + L"Models//" + std::wstring(mLevel.begin(), mLevel.end()) + L".obj");
	assert(success >= 0);
	m->Name = mLevel;
	mGeometries[m->Name] = std::move(m);

	auto scythe = std::make_unique<Mesh>(mD3Device, mCommandList);
	success = scythe->LoadOBJ(mProjectPath + L"Models//Scythe2.obj");
	assert(success >= 0);
	scythe->Name = "Scythe";
	mGeometries["Scythe"] = std::move(scythe);
}

void TestApp::BuildShadersAndInputLayout()
{
	// Why do shaders not compile if i pass in NumXLights as a macro, as opposed to a regular #define? TODO
	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"ALPHA_TEST", "1", NULL, NULL
	};

	const D3D_SHADER_MACRO shadowDefines[] =
	{
		"SHADOW", "1",
		NULL, NULL
	}; // looks like these things terminate with a null,null macro?
	// TODO fix light numbers

	mShaders["standardVS"] = Utilities::CompileShader(mProjectPath + L"Shaders\\Default.hlsl", shadowDefines, "VS", "vs_5_1");
	mShaders["opaquePS"] = Utilities::CompileShader(mProjectPath + L"Shaders\\Default.hlsl", shadowDefines, "PS", "ps_5_1");

	mShaders["standardVS_noshadow"] = Utilities::CompileShader(mProjectPath + L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["opaquePS_noshadow"] = Utilities::CompileShader(mProjectPath + L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["horzBlurCS"] = Utilities::CompileShader(mProjectPath + L"Shaders\\Blur.hlsl", nullptr, "HorzBlurCS", "cs_5_0");
	mShaders["vertBlurCS"] = Utilities::CompileShader(mProjectPath + L"Shaders\\Blur.hlsl", nullptr, "VertBlurCS", "cs_5_0");

	mShaders["dbgShadowVS"] = Utilities::CompileShader(mProjectPath + L"Shaders\\ShadowDebug.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["dbgShadowPS"] = Utilities::CompileShader(mProjectPath + L"Shaders\\ShadowDebug.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["sobelCS"] = Utilities::CompileShader(mProjectPath + L"Shaders\\Sobel.hlsl", nullptr, "SobelCS", "cs_5_0");

	mShaders["envVS"] = Utilities::CompileShader(mProjectPath + L"Shaders\\envmap.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["envPS"] = Utilities::CompileShader(mProjectPath + L"Shaders\\envmap.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["compositePS"] = Utilities::CompileShader(mProjectPath + L"Shaders\\Composite.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["compositeVS"] = Utilities::CompileShader(mProjectPath + L"Shaders\\Composite.hlsl", nullptr, "VS", "vs_5_1");

	mShaders["shadowVS"] = Utilities::CompileShader(mProjectPath + L"Shaders\\Shadows.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["shadowOpaquePS"] = Utilities::CompileShader(mProjectPath + L"Shaders\\Shadows.hlsl", nullptr, "PS", "ps_5_1");
	// One more for alpha testing and so on, but we haven't done transparency yet.

	// TODO: some of my shaders dont accept a normal, and yet interpret pos/texc perfectly fine. how/why?
	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};


}

void TestApp::LoadTextures()
{
	auto bricksTex = std::make_unique<Texture>();
	bricksTex->Name = "bricksTex";
	bricksTex->Filename = mProjectPath + L"Textures/bricks.dds";

	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(mD3Device.Get(),
		mCommandList.Get(), bricksTex->Filename.c_str(),
		bricksTex->Resource, bricksTex->UploadHeap));

	auto stoneTex = std::make_unique<Texture>();
	stoneTex->Name = "stoneTex";
	stoneTex->Filename = mProjectPath + L"Textures/stone.dds";

	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(mD3Device.Get(),
		mCommandList.Get(), stoneTex->Filename.c_str(),
		stoneTex->Resource, stoneTex->UploadHeap));

	auto tileTex = std::make_unique<Texture>();
	tileTex->Name = "tileTex";
	tileTex->Filename = mProjectPath + L"Textures/tile.dds";

	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(mD3Device.Get(),
		mCommandList.Get(), tileTex->Filename.c_str(),
		tileTex->Resource, tileTex->UploadHeap));

	auto envTex = std::make_unique<Texture>();
	envTex->Name = "envTex";
	envTex->Filename = mProjectPath + L"Textures/envmap.dds";

	ThrowIfFailed(CreateDDSTextureFromFile12(mD3Device.Get(),
		mCommandList.Get(), envTex->Filename.c_str(),
		envTex->Resource, envTex->UploadHeap));

	mTextures[bricksTex->Name] = std::move(bricksTex);
	mTextures[stoneTex->Name] = std::move(stoneTex);
	mTextures[tileTex->Name] = std::move(tileTex);
	mTextures[envTex->Name] = std::move(envTex);
}

void TestApp::BuildMaterials()
{
	auto bricks0 = std::make_unique<Material>(mNumFrameResources);
	bricks0->Name = "bricks0";
	bricks0->MatTransform = Math::Identity4x4();
	bricks0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	bricks0->Roughness = 0.1f;
	bricks0->NumFramesDirty = mNumFrameResources;
	bricks0->DiffuseSrvHeapIndex = 0;
	bricks0->MatCBIndex = 0;

	auto stone0 = std::make_unique<Material>(mNumFrameResources);
	stone0->Name = "stone0";
	stone0->MatCBIndex = 1;
	stone0->DiffuseSrvHeapIndex = 1;
	stone0->MatTransform = Math::Identity4x4();
	stone0->Roughness = 0.3f;
	stone0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	stone0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);

	auto tile0 = std::make_unique<Material>(mNumFrameResources);
	tile0->Name = "tile0";
	tile0->MatTransform = Math::Identity4x4();
	tile0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	tile0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	tile0->Roughness = 0.3f;
	tile0->DiffuseSrvHeapIndex = 2;
	tile0->MatCBIndex = 2;

	auto sky = std::make_unique<Material>(mNumFrameResources);
	sky->Name = "envMap";
	sky->MatTransform = Math::Identity4x4();
	sky->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	sky->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	sky->Roughness = 1.0f;
	sky->DiffuseSrvHeapIndex = 3;
	sky->MatCBIndex = 3;

	mMaterials[bricks0->Name] = std::move(bricks0);
	mMaterials[stone0->Name] = std::move(stone0);
	mMaterials[tile0->Name] = std::move(tile0);
	mMaterials[sky->Name] = std::move(sky);
}

void TestApp::UpdateInstanceBuffer(const Timer& t, const std::vector<RENDER_ITEM_TYPE>& categories)
{
	for (auto category : categories)
	{
		for (auto& ri : mRenderItems[category])
		{
			auto& currInstanceBuffer = mCurrFrameResource->InstanceBuffers[ri->Id()];

			for (size_t i = 0; i < ri->InstanceCount(); ++i)
			{
				XMMATRIX world = XMLoadFloat4x4(&ri->Instance(i).World);

				InstanceData data;
				XMStoreFloat4x4(&data.World, XMMatrixTranspose(world));
				data.MatIndex = ri->Instance(i).MatIndex;

				// Write the instance data to structured buffer
				currInstanceBuffer->CopyData(i, data);
			}
		}
	}
}

void TestApp::UpdateMaterialBuffer(const Timer& t)
{
	auto currMaterialBuffer = mCurrFrameResource->MaterialBuffer.get();
	for (auto& e : mMaterials)
	{
		// Only update if there are changes
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialData matData;
			matData.DiffuseAlbedo = mat->DiffuseAlbedo;
			matData.FresnelR0 = mat->FresnelR0;
			matData.Roughness = mat->Roughness;
			matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;

			currMaterialBuffer->CopyData(mat->MatCBIndex, matData);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> TestApp::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC shadow(
		6, // shaderRegister
		D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
		0.0f,                               // mipLODBias
		16,                                 // maxAnisotropy
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp,
		shadow
	};
}

void TestApp::BuildDescriptorHeaps()
{
	UINT objCount = (UINT)mRenderItems.size();
	UINT texCount = (UINT)mTextures.size();

	UINT blurDescCount = mBlurFilter->DescriptorCount(); // We use 2 texture resources to blur, each of which is used in turn as SRV/UAV
	UINT sobelDescCount = mSobelFilter->DescriptorCount();

	UINT shadowDescCount = 0;
	for (auto& l : mLights)
		shadowDescCount += l->Shadowmap()->DescriptorCount();

	// Todo: recalculate; we no longer use obj consts
	// +1 for offscreen render target
	// +1*3 for dbg quad
	// +1 for null srv (which we bind during shadowmapping - ultimately to avoid swapping root sigs)
	// +4 for environmonment map (+1 for texture, +3 for sphere*munumframeresuorces) TODO magic
	UINT oddDescCount = 6 + mNumFrameResources;
	UINT numDescriptors = objCount * mNumFrameResources + texCount + blurDescCount + shadowDescCount + sobelDescCount + oddDescCount; 

	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = numDescriptors;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	ThrowIfFailed(mD3Device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCbvDescHeap)));
}

void TestApp::CreateRtvAndDsvDescriptorHeaps()
{
	// Unfortunately, this is called during base class initialization - at which point the shadowmaps are not constructed.
	// So for now, we'll just add in the max space lighting could possibly need. 
	// TODO: move light init prior to rtv/dsv initialization

	// If every light is a point light, then we need 6 DSVs for each - for the non-GS technique
	const UINT nDSV = 6 * MaxLights;

	// Add +1 descriptor for offscreen render target.
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount + 1;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(mD3Device->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

	// Worst case is MaxLights point lights; each of these needs 6 dsv's
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1 + nDSV;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(mD3Device->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));
}