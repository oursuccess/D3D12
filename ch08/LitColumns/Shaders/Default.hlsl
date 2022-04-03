//copy of Default.hlsl by Frank Luna, ch07

#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIHGTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIHGTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

#include "LightingUtil.hlsl"

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
};

cbuffer cbMaterial : register(b1)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
    float4x4 gMatTransform;
};

cbuffer cbPass : register(b2)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;

    Light gLights[MaxLights];
};

struct VertexIn
{
    float3 PosL : POSITIONT;
    float3 NormalL : NORMAL;
};

struct VertexOut
{
    float4 PosH : SV_POSTION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;

    //这里和Unity Shader入门精要的顺序是相反的。 是因为这里的gWorld是按照行来排列的
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;

    //这里同样和UnityShader入门精要里方向相反了 因为这里的gWorld是Object2World
    vout.NormalW = mul(vin.NormalL, (float3x3) gWorld);
    vout.PosH = mul(posW, gViewProj);

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    pin.NormalW = normalize(pin.NormalW);

    float3 toEyeW = normalize(gEyePosW - pin.PosW);

    //漫反射
    float4 ambient = gAmbientLight * gDiffuseAlbedo;
    //高光反射
    const float shiniess = 1.0f - gRoughness;
    Material mat = { gDiffuseAlbedo, gFresnelR0, shiniess };
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW, pin.NormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;

    litColor.a = gDiffuseAlbedo.a;

    return litColor;
    
}
