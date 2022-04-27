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
    float3 NormalW : NORMAL;
    //添加贴图采样的uv坐标
    float2 TexC : TEXCOORD;
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

[maxvertexcount(3)] //我们最多区分细分两次，一次为3*3，二次为3*3*3
void GS(triangle VertexOut gin[3], inout TriangleStream<GeoOut> triStream)
{
    //三角片元的法线（因为要乘以缩放系数，此处无需归一化）
    float3 triangelNormal = gin[0].NormalL + gin[1].NormalL + gin[2].NormalL;
    float p = 10.0f; //法线缩放系数
    //三个顶点随时间变化的平移坐标
    gin[0].PosL += triangelNormal * p * gTotalTime;
    gin[1].PosL += triangelNormal * p * gTotalTime;
    gin[2].PosL += triangelNormal * p * gTotalTime;

    [unroll]
    for (int i = 0; i < 3; ++i)
    {
        GeoOut o;
        float4 world = mul(float4(gin[i].PosL, 1.0f), gWorld);
        o.PosW = world.xyz;
        o.PosH = mul(world, gViewProj);
        o.NormalW = mul((float3x3)gWorld, gin[i].NormalL);
        o.TexC = float2(0, 0);

        triStream.Append(o);
    }
}

float4 PS(GeoOut pin) : SV_Target
{
    float4 diffuseAlbedo = gDiffuseAlbedo;

    //是否开启AlphaTest
#ifdef ALPHA_TEST
    clip(diffuseAlbedo.a - 0.1f);
#endif

    pin.NormalW = normalize(pin.NormalW);

    //ch10,我们现在需要一个distToEye参数
    float3 toEyeW = gEyePosW - pin.PosW;
    float distToEye = length(toEyeW);
    toEyeW /= distToEye;    //其实就是normalize，但是这样我们减少了一次求距离的运算

    float4 ambient = gAmbientLight * gDiffuseAlbedo;
    
    const float shininess = 1.0f - gRoughness;
    //现在diffuseAlbedo是通过gDiffuseAlbedo和贴图采样共同计算获得的
    Material mat = { diffuseAlbedo, gFresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW, pin.NormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;

    //是否开启雾效
#ifdef FOG
    float fogAmount = saturate((distToEye - gFogStart) / gFogRange);
    litColor = lerp(litColor, gFogColor, fogAmount);
#endif

    litColor.a = diffuseAlbedo.a;

    return litColor;
}