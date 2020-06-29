#include "Light.h"

using namespace DirectX;

void LightPovData::BuildPLViewProj()
{
    // Transform NDC space [-1,+1]^2 to texture space [0,1]^2
    XMMATRIX T(
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, -0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.0f, 1.0f);

    const XMFLOAT3& pos = Light->Position;
    const auto posv = XMVectorSet(pos.x, pos.y, pos.z, 1.0f);

    UINT idx = 0;

    // Lesson learned: xmmatrixlookatlh/rh return an inverted view matrix, e.g. taking us from world -> view. 
    // this creates some problems, especially when combined with a perspective transformation in view space. 
    // Current code inverts these view-space matrices in UpdateMainPassCB; hence we can't supply one of the matrices
    // (like the view matrix) in world-space - both must be in the same space, at least.

    // It is also CRUCIAL to use (x,y,z, 1) to denote positions and (x,y,z, 0) to denote vectors.

    // +X
    auto target = XMVectorSet(pos.x + 10.0f, pos.y, pos.z, 1.0f);
    auto up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    auto view = Utilities::LookAt(posv, target, up); 
    //auto view = XMMatrixTranspose(XMMatrixLookAtLH(posv, target, up));

    auto proj = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, Near, Far);
    XMStoreFloat4x4(&Proj, proj);

    auto S = view * proj * T;
    XMStoreFloat4x4(&View[idx], view);
    XMStoreFloat4x4(&ShadowTransform[idx], S);
    ++idx;

    // -X
    target = XMVectorSet(pos.x - 10.0f, pos.y, pos.z, 1.0f);
    up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    view = Utilities::LookAt(posv, target, up); 

    S = view * proj * T;
    XMStoreFloat4x4(&View[idx], view);
    XMStoreFloat4x4(&ShadowTransform[idx], S);
    ++idx;

    // +Y
    target = XMVectorSet(pos.x, pos.y + 10.0f, pos.z, 1.0f);
    up = XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
    view = Utilities::LookAt(posv, target, up); 

    S = view * proj * T;
    XMStoreFloat4x4(&View[idx], view);
    XMStoreFloat4x4(&ShadowTransform[idx], S);
    ++idx;

    // -Y
    target = XMVectorSet(pos.x, pos.y - 10.0f, pos.z, 1.0f);
    up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    view = Utilities::LookAt(posv, target, up); 

    S = view * proj * T;
    XMStoreFloat4x4(&View[idx], view);
    XMStoreFloat4x4(&ShadowTransform[idx], S);
    idx++;

    // only z-anything works somehow
    // +Z
    target = XMVectorSet(pos.x, pos.y, pos.z + 10.0f, 1.0f);
    up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    view = Utilities::LookAt(posv, target, up);
    
    S = view * proj * T;
    XMStoreFloat4x4(&View[idx], view);
    XMStoreFloat4x4(&ShadowTransform[idx], S);
    ++idx;

    //auto viewf = Utilities::LookAt(posv, target, up); // now; this is the only one contributing to the shadowmap(with xmmatrixlookatlh)
    // -Z
    target = XMVectorSet(pos.x, pos.y, pos.z - 10.0f, 1.0f);
    up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    view = Utilities::LookAt(posv, target, up); 

    S = view * proj * T;
    XMStoreFloat4x4(&View[idx], view);
    XMStoreFloat4x4(&ShadowTransform[idx], S);
    ++idx;
}