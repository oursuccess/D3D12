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
    //添加贴图采样的uv坐标
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    //添加贴图采样的uv坐标
    float2 TexC : TEXCOORD;
};

struct GeoOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    //float3 NormalW : NORMAL;
    //添加贴图采样的uv坐标
    //float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;

    //直接将坐标和尺寸传入gs即可
    vout.PosL = vin.PosL;
    vout.NormalL = vin.NormalL;
    vout.TexC = vin.TexC;
    
    return vout;
}

[maxvertexcount(2)] //我们最多区分细分两次，一次为3*3，二次为3*3*3
void GS(triangle VertexOut gin[3], inout LineStream<GeoOut> triStream)
{
    GeoOut m, n;
    //一个中点，一个中点加上法线
    //中点
    float4 mPosW = mul(float4(0.333f * (gin[0].PosL + gin[1].PosL + gin[2].PosL), 1.0f), gWorld);
    float3 normalW = normalize(mul(gin[0].NormalL + gin[1].NormalL + gin[2].NormalL, (float3x3) gWorld));
    float4 nPosW = mPosW + float4(normalW, 0.0f);

    m.PosW = mPosW.xyz;
    n.PosW = nPosW.xyz;
    m.PosH = mul(mPosW, gViewProj);
    n.PosH = mul(nPosW, gViewProj);

    triStream.Append(m);
    triStream.Append(n);
}

float4 PS(GeoOut pin) : SV_Target
{
    float4 col = (1, 1, 1, 1);
   
    return col;
}