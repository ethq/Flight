#include "Utilities.h"
#include <comdef.h>
#include <fstream>
#include <vector>
#include <string>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// Assuming optimization; take words by ref otherwise
std::vector<std::string> split(const std::string& str, char delim)
{
    std::vector<std::string> words;
    std::stringstream ss(str);
    std::string word;
    while (std::getline(ss, word, delim))
    {
        words.push_back(word);
    }

    return words;
}

DxException::DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber) :
    ErrorCode(hr),
    FunctionName(functionName),
    Filename(filename),
    LineNumber(lineNumber)
{
}

UINT Utilities::CalcConstantBufferByteSize(UINT byteSize)
{
    // Constant buffers must be a multiple of the minimum hardware
    // allocation size (usually 256 bytes).  So round up to nearest
    // multiple of 256.  We do this by adding 255 and then masking off
    // the lower 2 bytes which store all bits < 256.
    // Example: Suppose byteSize = 300.
    // (300 + 255) & ~255
    // 555 & ~255
    // 0x022B & ~0x00ff
    // 0x022B & 0xff00
    // 0x0200
    // 512
    return (byteSize + 255) & ~255;
}

bool Utilities::IsKeyDown(int vkeyCode)
{
    return (GetAsyncKeyState(vkeyCode) & 0x8000) != 0;
}

ComPtr<ID3DBlob> Utilities::LoadBinary(const std::wstring& filename)
{
    std::ifstream fin(filename, std::ios::binary);

    fin.seekg(0, std::ios_base::end);
    std::ifstream::pos_type size = (int)fin.tellg();
    fin.seekg(0, std::ios_base::beg);

    ComPtr<ID3DBlob> blob;
    ThrowIfFailed(D3DCreateBlob(size, blob.GetAddressOf()));

    fin.read((char*)blob->GetBufferPointer(), size);
    fin.close();

    return blob;
}

XMMATRIX Utilities::LookAt(FXMVECTOR pos, FXMVECTOR target, FXMVECTOR worldUp)
{
    XMVECTOR L = XMVector3Normalize(XMVectorSubtract(target, pos));
    XMVECTOR R = XMVector3Normalize(XMVector3Cross(worldUp, L));
    XMVECTOR U = XMVector3Cross(L, R);

    XMFLOAT3 mPosition;
    XMFLOAT3 mLook;
    XMFLOAT3 mRight;
    XMFLOAT3 mUp;
    XMFLOAT3 tar;

    XMStoreFloat3(&tar, target);

    XMStoreFloat3(&mPosition, pos);
    XMStoreFloat3(&mLook, L);
    XMStoreFloat3(&mRight, R);
    XMStoreFloat3(&mUp, U);

    R = XMLoadFloat3(&mRight);
    U = XMLoadFloat3(&mUp);
    L = XMLoadFloat3(&mLook);
    XMVECTOR P = XMVectorSet(mPosition.x, mPosition.y, mPosition.z, 1.0f);

    // Keep camera's axes orthogonal to each other and of unit length.
    L = XMVector3Normalize(L);
    U = XMVector3Normalize(XMVector3Cross(L, R));

    // U, L already ortho-normal, so no need to normalize cross product.
    R = XMVector3Cross(U, L);

    // Fill in the view matrix entries.
    float r = XMVectorGetX(XMVector3Dot(P, R));
    float u = XMVectorGetX(XMVector3Dot(P, U));
    float l = XMVectorGetX(XMVector3Dot(P, L));

    //P = XMVectorSet(r,u,l, 1.0f);

    XMStoreFloat3(&mRight, R);
    XMStoreFloat3(&mUp, U);
    XMStoreFloat3(&mLook, L);

    XMFLOAT4X4 mView;


    mView(0, 0) = mRight.x;
    mView(0, 1) = mRight.y;
    mView(0, 2) = mRight.z;
    mView(0, 3) = r;

    mView(1, 0) = mUp.x;
    mView(1, 1) = mUp.y;
    mView(1, 2) = mUp.z;
    mView(1, 3) = u;

    mView(2, 0) = mLook.x;
    mView(2, 1) = mLook.y;
    mView(2, 2) = mLook.z;
    mView(2, 3) = l;

    mView(3, 0) = 0.0f;
    mView(3, 1) = 0.0f;
    mView(3, 2) = 0.0f;
    mView(3, 3) = 1.0f;

    //mView(0, 0) = mRight.x;
    //mView(1, 0) = mRight.y;
    //mView(2, 0) = mRight.z;
    //mView(3, 0) = x;

    //mView(0, 1) = mUp.x;
    //mView(1, 1) = mUp.y;
    //mView(2, 1) = mUp.z;
    //mView(3, 1) = y;

    //mView(0, 2) = mLook.x;
    //mView(1, 2) = mLook.y;
    //mView(2, 2) = mLook.z;
    //mView(3, 2) = z;

    //mView(0, 3) = 0.0f;
    //mView(1, 3) = 0.0f;
    //mView(2, 3) = 0.0f;
    //mView(3, 3) = 1.0f;

    //return XMLoadFloat4x4(&mView);

    return XMMATRIX(R, U, L, P);
}

Microsoft::WRL::ComPtr<ID3D12Resource> Utilities::CreateDefaultBuffer(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    const void* initData,
    UINT64 byteSize,
    Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
{
    ComPtr<ID3D12Resource> defaultBuffer;

    // Create the actual default buffer resource.
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(byteSize),
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

    // In order to copy CPU memory data into our default buffer, we need to create
    // an intermediate upload heap. 
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(byteSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(uploadBuffer.GetAddressOf())));


    // Describe the data we want to copy into the default buffer.
    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = initData;
    subResourceData.RowPitch = (LONG_PTR)byteSize;
    subResourceData.SlicePitch = subResourceData.RowPitch;

    // Schedule to copy the data to the default buffer resource.  At a high level, the helper function UpdateSubresources
    // will copy the CPU memory into the intermediate upload heap.  Then, using ID3D12CommandList::CopySubresourceRegion,
    // the intermediate upload heap data will be copied to mBuffer.
    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
    UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);
    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

    // Note: uploadBuffer has to be kept alive after the above function calls because
    // the command list has not been executed yet that performs the actual copy.
    // The caller can Release the uploadBuffer after it knows the copy has been executed.


    return defaultBuffer;
}

ComPtr<ID3DBlob> Utilities::CompileShader(
    const std::wstring& filename,
    const D3D_SHADER_MACRO* defines,
    const std::string& entrypoint,
    const std::string& target)
{
    UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)  
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = S_OK;

    ComPtr<ID3DBlob> byteCode = nullptr;
    ComPtr<ID3DBlob> errors;
    hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

    OutputDebugStringW(filename.c_str());

    if (errors != nullptr)
        OutputDebugStringA((char*)errors->GetBufferPointer());

    ThrowIfFailed(hr);

    return byteCode;
}

std::wstring DxException::ToString() const
{
    // Get the string description of the error code.
    _com_error err(ErrorCode);
    std::wstring msg = err.ErrorMessage();

    return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
}