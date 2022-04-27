//copy of Default.hlsl by Frank Luna, ch10

// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3 //因为我们采用的是3点布光系统
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

#include "LightingUtil.hlsl"

//声明贴图。贴图被绑定在t开头的寄存器上
Texture2D gDiffuseMap : register(t0);

//声明采样器。我们声明了6个
SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    //添加贴图矩阵
    float4x4 gTexTransform;
};

//这里，我们认为材质比Pass更稳定，因此Material为b2，而Pass为b1!!!
cbuffer cbMaterial : register(b2)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
    float4x4 gMatTransform;
};

//参看cbMaterial上面的注释
cbuffer cbPass : register(b1)
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
    //shunxu不能错...
    float4 gAmbientLight;

    //ch10，我们添加雾效相关的参数
    float4 gFogColor;
    float gFogStart;
    float gFogRange;
    float2 cbPerObjectPad2;

    Light gLights[MaxLights];
};

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
};

struct VertexOut
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
};

struct GeoOut
{
    float4 PosH : SV_POSITION;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;

    //直接将坐标传入gs即可
    vout.PosL = vin.PosL;
    vout.NormalL = vin.NormalL;
    
    return vout;
}

[maxvertexcount(50)]    //这里，我们将一个圆变成了5层的圆柱. 10是因为我们在cpp中将一个圆转为了10边形
void GS(line VertexOut gin[2], inout LineStream<GeoOut> lineStream)
{
    GeoOut gout[20];
    [unroll]
    for (int i = 0; i < 5; ++i)
    {
        float4 a = float4(gin[0].PosL, 1);
        a.y += i; //y
        a = mul(a, gWorld);
        gout[i * 3 + 1].PosH = mul(a, gViewProj);

        float4 b = float4(gin[1].PosL, 1);
        b.y += i; //y
        b = mul(b, gWorld);
        gout[i * 3 + 2].PosH = mul(b, gViewProj);

        float3 na = mul((float3x3)gWorld, gin[0].NormalL);
        float4 nap = float4(a.x + na.x, a.y + na.y, a.z + na.z, a.w);
        gout[i * 3].PosH = mul(mul(nap, gWorld), gViewProj);

        lineStream.Append(gout[i * 3]);
        lineStream.Append(gout[i * 3 + 1]);
        lineStream.Append(gout[i * 3 + 2]);
        lineStream.RestartStrip();
    }
}

float4 PS(GeoOut pin) : SV_Target
{
    float4 col = float4(1, 1, 1, 1);
    return col;
}