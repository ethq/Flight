#pragma once

#include "Utilities.h"
#include "ShadowMap.h"

#define MaxLights 16

// TODO: swap structure names; LightPovData should encapsulate Light, which is just a GPU type

struct Light
{
    DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
    float FalloffStart = 1.0f;                          // point/spot light only
    DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };// directional/spot light only
    float FalloffEnd = 10.0f;                           // point/spot light only
    DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };  // point/spot light only
    float SpotPower = 0.0f;                             // spot light only
};

class LightPovData
{
public:
    std::vector<DirectX::XMFLOAT4X4> View;
    DirectX::XMFLOAT4X4 Proj;
    std::vector<DirectX::XMFLOAT4X4> ShadowTransform;

    LightPovData() = delete;
    LightPovData(LightType type, Microsoft::WRL::ComPtr<ID3D12Device>& dev, UINT dsvDescriptorSize)
        : Type(type)
    {
        // We only need dsv desc size for point lights, as it needs to create 6 DSV's, and we need the size to offset through the array
        if (type == LightType::POINT)
            assert(dsvDescriptorSize);

        Light = std::make_shared<::Light>();
        mShadowMap = std::make_shared<ShadowMap>(dev, SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT, type, dsvDescriptorSize);

        for (size_t i = 0; i < 6; ++i)
        {
            View.push_back(Math::Identity4x4());
            Proj = Math::Identity4x4();
            ShadowTransform.push_back(Math::Identity4x4());
        }
    };

    // should be called whenever light position is modified - TODO
    void BuildPLViewProj();
    
    std::shared_ptr<ShadowMap> Shadowmap()
    {
        return mShadowMap;
    }

public:
    const LightType Type;

    std::shared_ptr<::Light> Light = nullptr; // position/direction

    float Near = 1.0f;
    float Far = 50.0f;

private:
    std::shared_ptr<ShadowMap> mShadowMap;
    const UINT SHADOW_MAP_WIDTH = 1024*4;
    const UINT SHADOW_MAP_HEIGHT = 1024*4;
};
