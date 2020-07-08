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
	float3 PosL    : POSITION;

	nointerpolation uint MatIndex : MATINDEX;
};

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID)
{
	VertexOut vout;

	InstanceData idata = gInstanceData[instanceID];
	float4x4 world = idata.World;

	vout.PosL = vin.PosL; // use local vertex coord as cubemap lookup
	float4 posW = mul(float4(vin.PosL, 1.0f), world);

	//posW.xyz += gEyePosW; // center sky about camera

	vout.PosH = mul(posW, gViewProj).xyww; // note z = w; we put the sky on the far plane
	vout.MatIndex = idata.MaterialIndex;

	return vout;
}

float4 PS(VertexOut pin) : SV_TARGET
{
	return gCubeMap.Sample(gsamLinearWrap, pin.PosL);
}

