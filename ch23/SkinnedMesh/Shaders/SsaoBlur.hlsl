//copy from DX12, by Frank Luna.
//边缘模糊用于防止噪点

//下面的都和Ssao.hlsl一样, 不再重复
cbuffer cbSsao : register(b0)   //Ssao的常量缓冲区是单独的.
{
    float4x4 gProj;     //其中包含了投影矩阵，投影矩阵的逆矩阵，投影采样矩阵。 14个随机偏移向量. 3个Blur权重， 以及一些遮蔽率相关的深度值/半径
    float4x4 gInvProj;
    float4x4 gProjTex;
    float4 gOffsetVectors[14];

    float4 gBlurWeights[3];

    float2 gInvRenderTargetSize;

    float gOcculusinRadius;
    float gOcculusionFadeStart;
    float gOcculusionFadeEnd;
    float gSurfaceEpsilon;
}

cbuffer cbRootConstants : register(b1)
{
    bool gHorizontalBlur;   //是否为横向模糊. 在Ssao中未使用
}

Texture2D gNormalMap : register(t0);    //几个需要的纹理
Texture2D gDepthMap : register(t1);
Texture2D gRandomVecMap : register(t2);

SamplerState gsamPointClamp : register(s0);
SamplerState gsamLinearClamp : register(s1);
SamplerState gsamDepthMap : register(s2);
SamplerState gsamLinearWrap : register(s3);

static const int gBlurRadius = 5; //模糊半径

static const float2 gTexCoords[6] =
{
    float2(0.0f, 1.0f), //我们将一个传入的像素扩展为一个正方形, 其中有两个三角形组成. 这里就是两个三角形的索引
    float2(0.0f, 0.0f), //第一个: 左下 -- 左上 --右上
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f), //第二个: 左下 -- 右上 --右下
    float2(1.0f, 0.0f), //两个三角形均为顺时针
    float2(1.0f, 1.0f),
};

struct VertexOut
{
    float4 PosH : SV_Position;
    float2 TexC : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID)
{
    VertexOut vout;

    vout.TexC = gTexCoords[vid];
    vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);

    return vout;
}


float NdcDepthToViewDepth(float z_ndc)
{
    //z_ndc = A + B / viewZ, A = gProj[2, 2], B = gProj[3, 2]
    float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
    return viewZ;
}

float4 PS(VertexOut pin) : SV_Target
{
    float blurWeights[12] =
    {
        gBlurWeights[0].x, gBlurWeights[0].y, gBlurWeights[0].z, gBlurWeights[0].w,
        gBlurWeights[1].x, gBlurWeights[1].y, gBlurWeights[1].z, gBlurWeights[1].w,
        gBlurWeights[2].x, gBlurWeights[2].y, gBlurWeights[2].z, gBlurWeights[2].w,
    };

}
