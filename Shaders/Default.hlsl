#include "Common.hlsl"

struct VertexIn
{
    float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC    : TEXCOORD;
};

struct VertexOut
{
    float4 PosH    : SV_POSITION;
    float4 ShadowPosH : POSITION0;
    float3 PosW    : POSITION1;
    float3 NormalW : NORMAL;
    float2 TexC    : TEXCOORD;

    nointerpolation uint MatIndex : MATINDEX;
};

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID)
{
    VertexOut vout = (VertexOut)0.0f;

    InstanceData idata = gInstanceData[instanceID];

    float4x4 world = idata.World;
    vout.MatIndex = idata.MaterialIndex;

    // Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), world);
    vout.PosW = posW.xyz;

    // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
    vout.NormalW = mul(vin.NormalL, (float3x3)world);

    // Transform to homogeneous clip space.
    vout.PosH = mul(posW, gViewProj);

    vout.TexC = vin.TexC;

    // Generate projective tex-coords to project shadow map onto scene.
    vout.ShadowPosH = mul(posW, gShadowTransform[0]);

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    MaterialData mdata = gMaterialData[pin.MatIndex];

    float4 diffuseAlbedo = mdata.DiffuseAlbedo; // gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC)* gDiffuseAlbedo;
	
    // Interpolating normal can unnormalize it, so renormalize it.
    pin.NormalW = normalize(pin.NormalW);

    // Vector from point being lit to eye. 
    float3 toEyeW = normalize(gEyePosW - pin.PosW);

    // Light terms.
    float4 ambient = gAmbientLight*diffuseAlbedo;

    // calculate shadowfactor for each light
    ShadowFactors shadowFactors = (ShadowFactors)1.0f;

    int i = 0;

#ifdef SHADOW
#if (NUM_DIR_LIGHTS > 0)
    for (i = 0; i < NUM_DIR_LIGHTS; ++i)
    {
        shadowFactors.sf[i] = CalcShadowFactor(pin.ShadowPosH, i);
    }
#endif

#if (NUM_SPOT_LIGHTS > 0)
    for (i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS + NUM_SPOT_LIGHTS; ++i)
    {
        shadowFactors.sf[i] = CalcShadowFactor(pin.ShadowPosH, i);
    }
#endif 

#if (NUM_POINT_LIGHTS > 0)
    for (i = NUM_DIR_LIGHTS + NUM_SPOT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_SPOT_LIGHTS + NUM_POINT_LIGHTS; ++i)
    {
        shadowFactors.sf[i] = CalcShadowFactorPoint(pin.PosW, i);
    }
#endif
#endif


    const float shininess = 1.0f - mdata.Roughness;
    Material mat = { diffuseAlbedo, mdata.FresnelR0, shininess };
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
        pin.NormalW, toEyeW, shadowFactors);

    float4 litColor = ambient + directLight;

    // Common convention to take alpha from diffuse albedo.
    litColor.a = diffuseAlbedo.a;
    
    return litColor;
}


