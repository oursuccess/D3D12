//copy of color.hlsl of Frank Luna. see ch06

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;
    //add these
    float4 gPulseColor;
    float gTime;
};

struct VertexIn
{
    float3 PosL : POSITION;
    float4 Color : COLOR;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float4 Color : COLOR;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
    vout.Color = vin.Color;
    return vout;
}

//change ps
float4 PS(VertexOut pin) : SV_Target
{
    //return pin.Color;
    
    const float pi = 3.14159;
    //随着时间流逝，令正弦函数的值在[0, 1]区间内周期性变化
    float s = 0.5f * sin(2 * gTime - 0.25f * pi) + 0.5f;

    //基于参数s在pin.Color和gPulseColor之间进行线性插值
    float4 c = lerp(pin.Color, gPulseColor, s);

    return c;
}
