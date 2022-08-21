//copy from DX12, by Frank Luna
//要采样天空盒, 我们简单对立方体贴图进行采样即可. 需要注意的是, 我们应该让相机处于天空的中心(即让我们真正的采样点加上相机的位置)

#include "Common.hlsl"

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_Position;
    float3 PosL : POSITION;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    vout.PosL = vin.PosL;

    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);  //将顶点位置变换到世界空间
    posW.xyz += gEyePosW;   //我们要让相机总是处于世界空间中心!

    vout.PosH = mul(posW, gViewProj).xyww;  //天空应该总是在无限远处(z = 1)

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    return gCubeMap.Sample(gsamLinearWrap, pin.PosL);
}