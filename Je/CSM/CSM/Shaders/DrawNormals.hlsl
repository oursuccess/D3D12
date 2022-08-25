//copy from DX12, by Frank Luna
//绘制法线. 我们是直接绘制了一遍所有不透明物体(包含了有动画的, 但是其shader我们通过宏来区分), 并存储其在观察空间中的法线!!!

#include "Common.hlsl"

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
    float3 TangentL : TANGENT;
    #ifdef SKINNED
    float3 BoneWeights : WEIGHTS;
    uint4 BoneIndices : BONEINDICES;
    #endif
};

struct VertexOut
{
    float4 PosH : SV_Position;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut)0.0f;

    MaterialData matData = gMaterialData[gMaterialIndex];

    #ifdef SKINNED
    float weights[4] = { vin.BoneWeights.x, vin.BoneWeights.y, vin.BoneWeights.z, 1.0f - vin.BoneWeights.x - vin.BoneWeights.y - vin.BoneWeights.z };
    float3 posL = float3(0.0f, 0.0f, 0.0f);
    float3 normalL = float3(0.0f, 0.0f, 0.0f);
    float3 tangentL = float3(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 4; ++i)
    {
        posL += weights[i] * mul(float4(vin.PosL, 1.0f), gBoneTransforms[vin.BoneIndices[i]]).xyz;
        normalL += weights[i] * mul(vin.NormalL, (float3x3) gBoneTransforms[vin.BoneIndices[i]]);   //这里假定了没有非统一的缩放变换, 否则我们这儿乘的就要是transform的逆转置矩阵了
        tangentL += weights[i] * mul(vin.NormalL, (float3x3) gBoneTransforms[vin.BoneIndices[i]]);  //为什么这儿能这么直接加上来? 这就是向量加法的几何意义!
    }

    vin.PosL = posL;
    vin.TangentL = tangentL;
    vin.NormalL = normalL;
    #endif

    vout.NormalW = mul(vin.NormalL, (float3x3) gWorld);
    vout.TangentW = mul(vin.TangentL, (float3x3) gWorld);

    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosH = mul(posW, gViewProj);

    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = mul(texC, matData.MatTransform).xy;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    MaterialData matData = gMaterialData[gMaterialIndex];
    float4 diffuseAlbedo = matData.DiffuseAlbedo;
    uint diffuseMapIndex = matData.DiffuseMapIndex;
    uint normalMapIndex = matData.NormalMapIndex;

    diffuseAlbedo *= gTextureMaps[diffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);

    #ifdef ALPHA_TEST
    clip(diffuseAlbedo.a - 0.1f);
    #endif

    pin.NormalW = normalize(pin.NormalW);

    float3 normalV = mul(pin.NormalW, (float3x3) gView);
    return float4(normalV, 1.0f);
}
