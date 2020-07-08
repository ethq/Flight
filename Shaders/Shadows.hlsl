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
	float2 TexC    : TEXCOORD;

	nointerpolation uint MatIndex : MATINDEX;
};

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID)
{
	VertexOut vout = (VertexOut)0.0f;

	InstanceData idata = gInstanceData[instanceID];
	float4x4 world = idata.World;
	
    // Transform to world space
    float4 posW = mul(float4(vin.PosL, 1.0f), world);

    // Transform to homogeneous clip space
	vout.PosH = mul(posW, gViewProj); // Persp divide done by hadrware
	vout.MatIndex = idata.MaterialIndex;
	// Output vertex attributes for interpolation across triangle.
	//float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
	//vout.TexC = mul(texC, gMatTransform).xy;
	
    return vout;
}

// This is only used for alpha cut out geometry, so that shadows 
// show up correctly.  Geometry that does not need to sample a
// texture can use a NULL pixel shader for depth pass.
void PS(VertexOut pin)
{
	// Dynamically look up the texture in the array.
	//float4 diffuseAlbedo = gDiffuseAlbedo*gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC);

//#ifdef ALPHA_TEST
    // Discard pixel if texture alpha < 0.1.  We do this test as soon 
    // as possible in the shader so that we can potentially exit the
    // shader early, thereby skipping the rest of the shader code.
    //clip(diffuseAlbedo.a - 0.1f);
//#endif
}


