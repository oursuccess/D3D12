//copy of color.hlsl of Frank Luna. see ch06

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;
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

float4 PS(VertexOut pin) : SV_Target
{
    //return pin.Color;
    float4 color = pin.Color;
    color.r = saturate(sin(gTime) * color.r);
    color.g = saturate(cos(gTime) * color.g); 
    color.b = saturate(sin(2*gTime) * color.b);
    return color;
}
