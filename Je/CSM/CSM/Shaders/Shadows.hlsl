//Copy from DX12, by Frank Luna
//绘制阴影. 我们在VS中正常计算顶点位置(有动画的需要特殊处理), 然后在PS中什么都不用返回. 如果开启了透明度测试, 我们还需要进行额外的clip

#include "Common.hlsl"

struct VertexIn
{
    float3 PosL : POSITION;
    float2 TexC : TEXCOORD;
#ifdef SKINNED  //可能要为了蒙皮动画特殊处理
    float3 BoneWeights : WEIGHTS;
    uint4 BoneIndices : BONEINDICES;
#endif
};

struct VertexOut
{
    float4 PosH : SV_Position;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;

    MaterialData matData = gMaterialData[gMaterialIndex];

#ifdef SKINNED
    float weights[4] = {vin.BoneWeights.x, vin.BoneWeights.y, vin.BoneWeights.z, 1 - vin.BoneWeights.x - vin.BoneWeights.y - vin.BoneWeights.z};

    float3 posL = float3(0.0f, 0.0f, 0.0f); //计算每个骨骼对该顶点的位置的影响. 顶点仅收到骨骼影响, 而不会受到自己上一帧所在位置的影响!!!
    for (int i = 0; i < 4; ++i)
    {
        posL += weights[i] * mul(float4(vin.PosL, 1.0f), gBoneTransforms[vin.BoneIndices[i]]).xyz; 
    }

    vin.PosL = posL;
#endif

    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosH = mul(posW, gViewProj);

    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = mul(texC, matData.MatTransform).xy; //还要额外采样一次材质的纹理偏移

    return vout;
}

void PS(VertexOut pin)  //绘制深度时, 不需要真的返回值
{
    MaterialData matData = gMaterialData[gMaterialIndex];
    float4 diffuseAlbedo = matData.DiffuseAlbedo;
    uint diffuseMapIndex = matData.DiffuseMapIndex;

    diffuseAlbedo *= gTextureMaps[diffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);

#ifdef ALPHA_TEST
    clip(diffuseAlbedo.a - 0.1f);
#endif
}
