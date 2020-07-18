#pragma once

#include "Utilities.h"
#include "UploadBuffer.h"
#include "Light.h"
#include <map>

struct InstanceData
{
    DirectX::XMFLOAT4X4 World = Math::Identity4x4();

    UINT MatIndex = 0;
    UINT ipad0 = 0;
    UINT ipad1 = 0;
    UINT ipad2 = 0;
};

struct MaterialData
{
    DirectX::XMFLOAT4 DiffuseAlbedo;
    DirectX::XMFLOAT3 FresnelR0;
    float Roughness;

    UINT DiffuseMapIndex = 0;
    UINT mpad0 = 0;
    UINT mpad1 = 0;
    UINT mpad2 = 0;
};

struct PassConstants
{
    DirectX::XMFLOAT4X4 View = Math::Identity4x4();
    DirectX::XMFLOAT4X4 InvView = Math::Identity4x4();
    DirectX::XMFLOAT4X4 Proj = Math::Identity4x4();
    DirectX::XMFLOAT4X4 InvProj = Math::Identity4x4();
    DirectX::XMFLOAT4X4 ViewProj = Math::Identity4x4();
    DirectX::XMFLOAT4X4 InvViewProj = Math::Identity4x4();
    DirectX::XMFLOAT4X4 ShadowTransform[MaxLights];
    DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
    float cbPerObjectPad1 = 0.0f;
    DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
    float NearZ = 0.0f;
    float FarZ = 0.0f;
    float TotalTime = 0.0f;
    float DeltaTime = 0.0f;

    DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

    // Array must be sorted to have directional->spot->point to conform with shader code.
    Light Lights[MaxLights];
};

struct Vertex
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT2 TexC;
};


// Idea is to store everything needed to submit a command list for a frame in this class
struct FrameResource
{
    // renderItemsAndInstanceCounts is expected to be given in the form < renderitem id, max number of instances > 
    FrameResource(ID3D12Device* device, UINT passCount, std::map<UINT, UINT> renderItemsAndInstanceCounts, UINT matCount);
    FrameResource(const FrameResource& rhs) = delete;
    FrameResource& operator=(const FrameResource& rhs) = delete;
    ~FrameResource();

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

    // Buffers cannot be updated until the GPU is done with all commands referencing it. 
    // Hence each frame needs its own buffer.
    std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
    
    // Each renderitem has a unique id, to which we associate an upload buffer of instance data. 
    // This upload buffer in turn admits a given maximal number of instances internally. 
    std::map<UINT, std::unique_ptr<UploadBuffer<InstanceData>>> InstanceBuffers;

    std::unique_ptr<UploadBuffer<MaterialData>> MaterialBuffer = nullptr;

    UINT64 Fence = 0;
};