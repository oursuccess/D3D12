//ShadowDebug

#include "Common.hlsl"

struct VertexIn
{
    float3 PosL : POSITION;
    float3 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;

    vout.PosH = float4(vin.PosL, 1.0f); //我们要绘制的就是一张图片. 不需要进行什么坐标变换了. 已然处于齐次空间中

    vout.TexC = vin.TexC;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    return float4(gSsaoMap.Sample(gsamLinearWrap, pin.TexC).rrr, 1.0f); //这个文件名字诈骗. 绘制的是Ssao. 而不是阴影!!
}
