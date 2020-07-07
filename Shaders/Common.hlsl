#include "Lighting.hlsl"

Texture2D    gDiffuseMap         : register(t0);
TextureCube  gCubeMap            : register(t1);
Texture2D    gShadowMap[max(NUM_SPOT_LIGHTS + NUM_DIR_LIGHTS, 1)]       : register(t2);
TextureCube  gPointShadowMap[max(NUM_POINT_LIGHTS, 1)]  : register(t0, space1);

SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

struct InstanceData
{
    float4x4 World;

    uint MaterialIndex;
    uint ipad0;
    uint ipad1;
    uint ipad2;
};

struct MaterialData
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;

    uint DiffuseMapIndex;
    uint mpad0;
    uint mpad1;
    uint mpad2;
};


StructuredBuffer<InstanceData> gInstanceData : register(t0, space2);
StructuredBuffer<MaterialData> gMaterialData : register(t1, space2);

cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float4x4 gShadowTransform[MAX_LIGHTS];
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light gLights[MAX_LIGHTS];
};

//---------------------------------------------------------------------------------------
// PCF for shadow mapping.
//---------------------------------------------------------------------------------------

float CalcShadowFactor(float4 shadowPosH, uint lightIdx)
{
    // Complete projection by doing division by w
    shadowPosH.xyz /= shadowPosH.w;

    // Depth in NDC space.
    float depth = shadowPosH.z;

    // directional/spotlights always come first, and are always looped over first. Hence we can be sure that lightidx indexes correctly into the maps
    uint width, height, numMips;
    gShadowMap[lightIdx].GetDimensions(0, width, height, numMips);

    // Texel size
    float dx = 1.0f / (float)width;

    float percentLit = 0.0f;
    const float2 offsets[9] =
    {
        float2(-dx,  -dx), float2(0.0f,  -dx), float2(dx,  -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx,  +dx), float2(0.0f,  +dx), float2(dx,  +dx)
    };

    [unroll]
    for(int i = 0; i < 9; ++i)
    {
        percentLit += gShadowMap[lightIdx].SampleCmpLevelZero(gsamShadow,
            shadowPosH.xy + offsets[i], depth).r;
    }
    
    return percentLit / 9.0f;
}

float GetNormalizedDepth(float z)
{
    // For the rotating cubes w/ pl, I used
    // near = 1, far = 800
    float near = 1.0f;
    float far = 800.0f;

    return far / (far - near) - near * far / ((far - near) * z);
}


// note that lightidx is relative to the light array, e.g. there are directional/spotlights preceding 
// otoh, point lights sample cube shadowmaps, which live apart from the regular flat shadowmaps
// hence, the index into the (cube) shadowmap array has to be adjusted
float CalcShadowFactorPoint(float3 posw, uint lightIdx)
{
    static const float pi = 3.14159265f;

    uint mapIdx = lightIdx - NUM_DIR_LIGHTS - NUM_SPOT_LIGHTS;

    float3 lookup = posw - gLights[lightIdx].Position;

    // Determine which frustum we are in so we can properly project the correct z coordinate
    float phi = pi + atan2(lookup.z, lookup.x); // problems with x,y = 0 here.. well well
    float theta = acos(lookup.y / length(lookup));

    float z = 0.0f;
    
    // +Y frustum
    if (theta > 3 * pi / 4)
    {
        z = lookup.y;
    }
    // -Y frustum
    else if (theta < pi / 4)
    {
        z = -lookup.y;
    }
    // theta in [pi/4, 3pi/4] => one of the middle frustums. 
    else if (phi > 7 * pi / 4)
    {
        z = lookup.x;
    }
    //if phi > 5pi/4, its -Z
    else if (phi > 5 * pi / 4)
    {
        z = -lookup.z;
    }
    // -X
    else if (phi > 3 * pi / 4)
    {
        z = -lookup.x;
    }
    // +Z
    else if (phi > pi / 4)
    {
        z = lookup.z;
    }
    else
    {
        z = lookup.x;
    }

    float depth = GetNormalizedDepth(abs(z)); // solution is here finally; fix the lookup vector
    float depth2 = gPointShadowMap[mapIdx].Sample(gsamPointWrap, lookup);
    
    //return 1.0f;
    //return depth2 > depth;

    return gPointShadowMap[mapIdx].SampleCmp(gsamShadow, lookup, depth).r; // also do averaging, but..
}


