//***************************************************************************************
// ShadowDebug.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

// Include common HLSL code.
#include "Common.hlsl"

struct VertexIn
{
	float3 PosL    : POSITION;
	float2 TexC    : TEXCOORD;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
	float2 TexC    : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;

    // Already in homogeneous clip space.
    vout.PosH = float4(vin.PosL, 1.0f);
	
	vout.TexC = vin.TexC;
	
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    /*
    float r = gShadowMap[0].Sample(gsamLinearWrap, pin.TexC).r;
    float g = gShadowMap[1].Sample(gsamLinearWrap, pin.TexC).r;
    float b = gShadowMap[2].Sample(gsamLinearWrap, pin.TexC).r;
    float a = gShadowMap[3].Sample(gsamLinearWrap, pin.TexC).r;
    return float4(r, g, b, 1.0f);
    */
    //return gShadowMap[0].Sample(gsamLinearWrap, pin.TexC).rrrr;
    int i = (pin.TexC.r > 0.5f) * 2 + (pin.TexC.g > 0.5f);  //0: 左上, 1: 左下, 2: 右上, 3: 右下
    /*
    float2 texC;
    switch (i)
    {
        case (0):
            texC = pin.TexC * 2;
            break;
        case (1):
            texC = (pin.TexC - float2(0.0f, 0.5f)) * 2;
            break;
        case (2):
            texC = (pin.TexC - float2(0.5f, 0.0f)) * 2;
            break;
        case (3):
            texC = (pin.TexC - float2(0.5f, 0.5f)) * 2;
            break;
    }
    return gShadowMap[i].Sample(gsamLinearWrap, texC).rrrr;
    */
    return float4(1.0f, gShadowMap[i].Sample(gsamLinearWrap, pin.TexC % 0.5f * 2.0f).rr, 1.0f);
}


