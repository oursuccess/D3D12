//Shadows

#include "Common.hlsl"

struct VertexIn
{
    float3 PosL : POSITION; //只需要位置和纹理，连法线都不需要了
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_Position;  //系统值，必须传入
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;

    MaterialData matData = gMaterialData[gMaterialIndex];

    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);

    vout.PosH = mul(posW, gViewProj);

    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform); //我们通过与材质的变换矩阵联立得到实际的纹理贴图采样坐标
    vout.TexC = mul(texC, matData.MatTransform).xy;

    return vout;
}

void PS(VertexOut pin)   //我们不返回值，因为绘制阴影时，不需要绘制如渲染对象. 事实上，我们这儿啥也不做，也没区别...
{
    MaterialData matData = gMaterialData[gMaterialIndex];
    float4 diffuseAlbedo = matData.DiffuseAlbedo;
    uint diffuseMapIndex = matData.DiffuseMapIndex;

    diffuseAlbedo *= gTextureMaps[diffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);   //我们在这里进行了一次计算. 是因为我们可能有透明度测试的几何. 如果不需要透明度混合，则可以直接使用一个什么都不做的PS

#ifdef ALPHA_TEST
    clip(diffuseAlbedo.a - 0.1f);
#endif
}