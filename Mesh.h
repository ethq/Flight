#pragma once

#include "Utilities.h"

// Defines a subrange of geometry in a MeshGeometry.  This is for when multiple
// geometries are stored in one vertex and index buffer.  It provides the offsets
// and data needed to draw a subset of geometry stores in the vertex and index 
// buffers.
struct SubmeshGeometry
{
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    INT BaseVertexLocation = 0;

    // Bounding box of the geometry defined by this submesh. 
    DirectX::BoundingBox Bounds;
};

class Mesh
{
public:
    // If manually constructing the mesh
    Mesh() {};
    // Initialize with device and command list if intending to use LoadOBJ
    Mesh(const Microsoft::WRL::ComPtr<ID3D12Device>& device, 
        const Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cmdList) 
        : mD3Device(device), mCommandList(cmdList) {};
    // Give it a name so we can look it up by name.
    std::string Name;

    // System memory copies.  Use Blobs because the vertex/index format can be generic.
    // It is up to the client to cast appropriately.  
    Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

    // Data about the buffers.
    UINT VertexByteStride = 0;
    UINT VertexBufferByteSize = 0;
    DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
    UINT IndexBufferByteSize = 0;

    // A Mesh may store multiple geometries in one vertex/index buffer.
    // Use this container to define the Submesh geometries so we can draw
    // the Submeshes individually.
    std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

    D3D12_VERTEX_BUFFER_VIEW VertexBufferView()const;

    D3D12_INDEX_BUFFER_VIEW IndexBufferView()const;

    // We can free this memory after we finish upload to the GPU.
    void DisposeUploaders();

    int LoadOBJ(std::wstring filename);

private:
    const Microsoft::WRL::ComPtr<ID3D12Device>& mD3Device = nullptr;
    const Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& mCommandList = nullptr;
};

