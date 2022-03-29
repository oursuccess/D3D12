//copy of color.hlsl of Frank Luna. see ch06

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;
    //add this
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

//add this
VertexOut VS06(VertexIn vin)
{
    vin.PosL.xy += 0.5f * sin(vin.PosL.x) * sin(3.0f * gTime);
    vin.PosL.z *= 0.6f + 0.4f * sin(2.0f * gTime);
    return VS(vin);
}

float4 PS(VertexOut pin) : SV_Target
{
    return pin.Color;
}
