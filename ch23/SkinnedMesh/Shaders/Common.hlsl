//copy from DX12, by Frank Luna

#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 0
#endif

#include "LightingUtil.hlsl"

struct MaterialData //这里要和CPU侧定义的数据顺序严格一致
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;
    float4x4 MatTransform;
    uint DiffuseMapIndex;
    uint NormalMapIndex;
    uint MatPad1;
    uint MatPad2;
};


TextureCube gCubeMap : register(t0);    //绑定纹理资源
Texture2D gShadowMap : register(t1);
Texture2D gSsaoMap : register(t2);

Texture2D gTextureMaps[48] : register(t3);

StructuredBuffer<MaterialData> gMaterialData : register(t0, space1);    //材质的数据们, 因为是结构化的, 因此也绑定为texture

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

cbuffer cbPerObject : register(b0)  //物体的常量绑定为b0
{
    float4x4 gWorld;
    float4x4 gTexTransform;
    uint gMaterialIndex;
    uint gObjPad0;
    uint gObjPad1;
    uint gObjPad2;
};

cbuffer cbSkinned : register(b1)    //蒙皮的常量绑定为b1
{
    float4x4 gBoneTransforms[96];
}

cbuffer cbPass : register(b2) //帧常量绑定为b2
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float4x4 gViewProjTex;
    float4x4 gShadowTransform;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;

    Light gLights[MaxLights];
};

float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
    float3 normalT = 2.0f * normalMapSample - 1.0f; //从[0, 1]变换到[-1, 1]

    //获取N, T, B各自在原本坐标系下的表示, 我们即可根据normalT, 得到其在原本坐标系下的变换
    float3 N = unitNormalW; //N本来是(0, 0, 1), 现在在世界中变成了unitNormalW
    float3 T = normalize(tangentW - dot(tangentW, N) * N);  //T可以根据tangent - dot(tangentW, N) * N的归一化来得出. 但是为什么要归一化呢 --FIXME
    float3 B = cross(N, T); //B可以通过N和T的叉乘得出

    float3x3 TBN = float3x3(T, B, N);   //联立得出N, T, B在原本坐标系下的表示

    float3 bumpedNormalW = mul(normalT, TBN);   //我们让法线乘以原本的表示, 即得到世界中的法线

    return bumpedNormalW;
}

float CalcShadowFactor(float4 shadowPosH)
{
    shadowPosH.xyz /= shadowPosH.w; //投影变换中的非线性部分

    float depth = shadowPosH.z;

    uint width, height, numMips;
    gShadowMap.GetDimensions(0, width, height, numMips);    //获取阴影图的宽、高、Mip数量

    float dx = 1.0f / (float) width;

    float percentLit = 0.0f;
    const float2 offsets[9] =
    {
        float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx),
    };

    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        percentLit += gShadowMap.SampleCmpLevelZero(gsamShadow, shadowPosH.xy + offsets[i], depth).r;
    }

    return percentLit / 9.0f;
}
