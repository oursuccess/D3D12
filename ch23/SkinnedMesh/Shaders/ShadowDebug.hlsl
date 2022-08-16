//copy from DX12, by Frank Luna
//这就是一个简单绘制一张图的实现

#include "Common.hlsl"

struct VertexIn
{
    float3 PosL : POSITION; //我们只需要位置和纹理. 位置也不需要经过投影, 而是已经是齐次裁剪空间的. 其它传入的属性我们并不需要声明!
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_Position;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;
    //位置已经是齐次裁剪空间的了
    vout.PosH = float4(vin.PosL, 1.0f);

    vout.TexC = vin.TexC;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    return float4(gSsaoMap.Sample(gsamLinearWrap, pin.TexC).rrr, 1.0f);
}
