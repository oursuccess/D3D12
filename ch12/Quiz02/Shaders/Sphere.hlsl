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
    float3 PosW : POSITION;
    float2 SizeW : SIZE;
};

struct VertexOut
{
    float3 CenterW : POSITION;
    float2 SizeW : SIZE;
};

struct GeoOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    //添加贴图采样的uv坐标
    float2 TexC : TEXCOORD;
    //id
    uint PrimID : SV_PrimitiveID;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;

    //直接将坐标和尺寸传入gs即可
    vout.CenterW = vin.PosW;
    vout.SizeW = vin.SizeW;
    
    return vout;
}

[maxvertexcount(4)]
void GS(point VertexOut gin[1], uint primID : SV_PrimitiveID, inout TriangleStream<GeoOut> triStream)
{
    float3 up = float3(0.0f, 1.0f, 0.0f);
    float3 look = gEyePosW - gin[0].CenterW;
    look.y = 0.0f;
    look = normalize(look);
    float3 right = cross(up, look); //dx为左手坐标系，使用左手定则; 在opengl中为右手坐标系

    float halfWidth = 0.5f * gin[0].SizeW.x;
    float halfHeight = 0.5f * gin[0].SizeW.y;

    float4 v[4];
    v[0] = float4(gin[0].CenterW + halfWidth * right - halfHeight * up, 1.0f);  //右下。这个右，在经过我们的叉乘后，实际上对应的是屏幕的左边。因为我们算出来的这个是正面朝向镜头的，因此，它的右边是镜头的左边。
    v[1] = float4(gin[0].CenterW + halfWidth * right + halfHeight * up, 1.0f);  //右上
    v[2] = float4(gin[0].CenterW - halfWidth * right - halfWidth * up, 1.0f);   //左下
    v[3] = float4(gin[0].CenterW - halfWidth * right + halfHeight * up, 1.0f);  //左上

    float2 texC[4] =
    {
        float2(0.0f, 1.0f), //左下    //为啥这里和上面的比，左右颠倒了？？
        float2(0.0f, 0.0f), //左上
        float2(1.0f, 1.0f), //右下
        float2(1.0f, 0.0f), //右上
    };

    GeoOut gout;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        gout.PosH = mul(v[i], gViewProj);
        gout.PosW = v[i].xyz;
        gout.NormalW = look;
        gout.TexC = texC[i];
        gout.PrimID = primID;

        triStream.Append(gout);
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