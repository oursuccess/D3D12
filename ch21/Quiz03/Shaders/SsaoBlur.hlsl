//SsaoBlur

//几个绑定的常量/SRV都是和Ssao一样的，因此不再重复抄了

cbuffer cbSsao : register(b0)   //该常量缓冲区通过根描述符在Ssao中被传递绑定至b0
{
    float4x4 gProj; //相机投影矩阵
    float4x4 gInvProj;  //相机投影矩阵的逆矩阵
    float4x4 gProjTex;  //相机投影采样矩阵
    float4 gOffsetVectors[14];  //随机采样的周围向量
    float4 gBlurWeights[3]; //混合权重
    float2 gInvRenderTargetSize;    //渲染目标尺寸的倒数
    float gOcclusionRadius; //遮蔽半径
    float gOcclusionFadeStart;  //从何处开始遮蔽衰减
    float gOcclusionFadeEnd;    //到何距离时遮蔽衰减至0
    float gSurfaceEpsilon;  //距离小于该值, 我们认为处于同一平面，因此没有遮蔽
};

cbuffer cbRootConstants : register(b1)  //根常量们. 通过根常量在Ssao中被绑定至b1
{
    bool gHorizontalBlur;   //记录是否为水平混合
};

Texture2D gNormalMap : register(t0);    //绑定一些纹理. 直接指向的纹理无法通过常量缓冲区绑定. 法线贴图
Texture2D gDepthMap : register(t1); //深度贴图. 该贴图是在NormalMap的后面被一同绑定的. 可以参见SsaoApp::BuildSsaoRootSignature(), 其中TexTable0元素数量为2, 0为gNormalMap(t0), 1为gDepthMap(t1)
//Texture2D gRandomVecMap : register(t2); //随机采样贴图
Texture2D gInputMap : register(t2); //输入贴图(遮蔽率贴图), 不再是随机采样贴图了. 这就是为什么我们要分成两个TexTable的原因

SamplerState gsamPointClamp : register(s0); //绑定几个静态采样器
SamplerState gsamLinearClamp : register(s1);
SamplerState gsamDepthMap : register(s2);
SamplerState gsamLinearWrap : register(s3);

//static const int gSampleCount = 14; //这里的14和上面gOffsetVectors的数组长度严格一致
static const int gBlurRadius = 5;   //这是和Ssao.hlsl的区别

static const float2 gTexCoords[6] = //采样的纹理坐标. 我们假设传入顶点着色器的均为三角形列表，且顺时针旋转的三个顶点为正向. 6个顶点组成的两个三角形刚好组成了一个矩形，分别对应了左上部分的三角和右下部分的三角
{
    /*              u
     p1 o-----------------------> p2, p4
        |                        |
        |                        
        |                        |
       v|                        
        |                        |
        |                        
        |                        |
 p0, p3 v- - - - - - - - - - - -  p5
    */
    float2(0.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(1.0f, 1.0f),
};

struct VertexOut
{
    float4 PosH : SV_Position;  //必须传入
    float2 TexC : TEXCOORD; //采样坐标
};

VertexOut VS(uint vid : SV_VertexID)
{
    VertexOut vout;

    vout.TexC = gTexCoords[vid];

    vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);   //其ndc坐标可以根据纹理坐标简单换算得出

    return vout;
}

float NdcDepthToViewDepth(float z_ndc)
{
    float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
    return viewZ;   //和Ssao.hlsl一样
}

float4 PS(VertexOut pin) : SV_Target
{
    float blurWeights[12] = //将blurWeights重新解包. 其本来就是一个数组
    {
        gBlurWeights[0].x, gBlurWeights[0].y, gBlurWeights[0].z, gBlurWeights[0].w,
        gBlurWeights[1].x, gBlurWeights[1].y, gBlurWeights[1].z, gBlurWeights[1].w,
        gBlurWeights[2].x, gBlurWeights[2].y, gBlurWeights[2].z, gBlurWeights[2].w,
    };

    float2 texOffset = gHorizontalBlur ? float2(gInvRenderTargetSize.x, 0.0f) : float2(0.0f, gInvRenderTargetSize.y);   //根据当前计算的纵横方向，我们求出tex每次的偏移量

    float4 color = blurWeights[gBlurRadius] * gInputMap.SampleLevel(gsamPointClamp, pin.TexC, 0.0); //根据中心点的混合权重和混合中心的采样值，得到初始的颜色(中心点总是要对颜色增加贡献的)
    float4 totalWeight = blurWeights[gBlurRadius];  //计量总权重(中心点总是增加贡献)

    float3 centerNormal = gNormalMap.SampleLevel(gsamPointClamp, pin.TexC, 0.0f).xyz;   //获得中心点对应的法线和深度值
    float centerDepth = NdcDepthToViewDepth(gDepthMap.SampleLevel(gsamDepthMap, pin.TexC, 0.0f).r);

    for (float i = -gBlurRadius; i <= gBlurRadius; ++i)
    {
        if (i == 0) //中心点我们在初始的时候已经计量过了
            continue;   

        float2 tex = pin.TexC + i * texOffset;  //求出第i个邻居

        float3 neighborNormal = gNormalMap.SampleLevel(gsamPointClamp, tex, 0.0f).xyz;  //求出来邻居的法线、深度
        float neighborDepth = NdcDepthToViewDepth(gDepthMap.SampleLevel(gsamDepthMap, tex, 0.0f).r);

        if (dot(neighborNormal, centerNormal) >= 0.8f && abs(neighborDepth - centerDepth) <= 0.2f)  //若两个点的方向/距离差的过大，我们则直接跳过; 只有在相差不多的情况下我们才进行混合
        {
            float weight = blurWeights[i + gBlurRadius];

            color += weight * gInputMap.SampleLevel(gsamPointClamp, tex, 0.0);  //颜色增加

            totalWeight += weight;
        }
    }

    return color / totalWeight; //返回归一化后的颜色
}

//Quiz2103, 添加一个CS方法
#define N 256   //遮蔽率图的分辨率

RWTexture2D<float4> gOutput : register(u0);

[numthreads(N, 1, 1)]
void CS(int3 groupThreadID : SV_GroupThreadID, int3 disptachThreadID : SV_DispatchThreadID)
{
    float blurWeights[12] = //将blurWeights重新解包. 其本来就是一个数组
    {
        gBlurWeights[0].x, gBlurWeights[0].y, gBlurWeights[0].z, gBlurWeights[0].w,
        gBlurWeights[1].x, gBlurWeights[1].y, gBlurWeights[1].z, gBlurWeights[1].w,
        gBlurWeights[2].x, gBlurWeights[2].y, gBlurWeights[2].z, gBlurWeights[2].w,
    };
    
    float2 texOffset = gHorizontalBlur ? float2(gInvRenderTargetSize.x, 0.0f) : float2(0.0f, gInvRenderTargetSize.y);

    float2 texC = disptachThreadID.xy / (float) N;
    float4 color = blurWeights[gBlurRadius] * gInputMap.SampleLevel(gsamPointClamp, texC, 0.0); //根据中心点的混合, 得到最初的颜色
    float4 totalWeight = blurWeights[gBlurRadius];  //准备计算总权重

    float3 centerNormal = gNormalMap.SampleLevel(gsamPointClamp, texC, 0.0f).xyz;
    float centerDepth = NdcDepthToViewDepth(gDepthMap.SampleLevel(gsamDepthMap, texC, 0.0f).r);

    for (float i = -gBlurRadius; i <= gBlurRadius; ++i)
    {
        if (i == 0)
            continue;

        float2 tex = texC + i * texOffset;

        float3 neighborNormal = gNormalMap.SampleLevel(gsamPointClamp, tex, 0.0f).xyz;
        float neighborDepth = NdcDepthToViewDepth(gDepthMap.SampleLevel(gsamDepthMap, tex, 0.0f).r);

        if (dot(neighborNormal, centerNormal) >= 0.8f && abs(neighborDepth - centerDepth) <= 0.2f)
        {
            float weight = blurWeights[i * gBlurRadius];
            color += weight * gInputMap.SampleLevel(gsamPointClamp, tex, 0.0f);
            totalWeight += weight;
        }
    }

    gOutput[disptachThreadID.xy] = color / totalWeight;
}
