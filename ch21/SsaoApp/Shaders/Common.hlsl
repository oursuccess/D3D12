//Common

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

struct MaterialData //和cpp中定义的材质数据对应的GPU侧数据. 顺序等要严格一样
{
    float4 DiffuseAlbedo;   //漫反射值
    float3 FresnelR0;   //R0值
    float Roughness;    //粗糙度
    float4x4 MatTransform;  //纹理采样矩阵
    uint DiffuseMapIndex;   //纹理图在SRV堆中的偏移
    uint NormalMapIndex;    //法线图在SRV堆中的偏移
    uint MatPad1;   //下面两个都是不使用的空数据， 主要是为了保证对齐
    uint MatPad2;
};

TextureCube gCubeMap : register(t0);    //立方体贴图, 绑定在t0，用于实现天空盒
Texture2D gShadowMap : register(t1);    //阴影贴图，绑定在t1, 用于绘制阴影图
Texture2D gSsaoMap : register(t2);  //Ssao贴图, 绑定在t2, 用于实现环境光遮蔽效果

Texture2D gTextureMap[10] : register(t3);   //10个贴图纹理的数组, 绑定在t3开始, 用于在一次drawcall时可以提交多个纹理

StructuredBuffer<MaterialData> gMaterialData : register(t0, space1);    //我们在Draw方法中传入了当前帧(pass)持有的材质的数据信息们，并将其与根签名的第3个(下标为2)的描述符绑定， 我们并不知道当前帧持有多少个材质，因此我们直接绑定到space1中, 从而避免和上面的重叠. 但是这个为什么绑定了t

SamplerState gsamPointWrap : register(s0);  //绑定我们声明并通过根签名设置的7个采样器. 第一个为点采样， 在超出[0, 1]范围后重复(值采样小数部分)
SamplerState gsamPointClamp : register(s1); //第2个为点采样，但是在超出[0, 1]范围后采样距离其最近的坐标的纹理值(钳位模式)
SamplerState gsamLinearWrap : register(s2); //第3个为线性采样，并且为重复模式
SamplerState gsamLinearClamp : register(s3);    //第4个为线性采样，且为钳位模式
SamplerState gsamAnisotropicWrap : register(s4);    //第5个为各向异性采样，且为重复模式
SamplerState gsamAnisotropicClamp : register(s5);   //第6个为各向异性采样，且为钳位模式
SamplerComparisonState gsamShadow : register(s6);   //第7个用于阴影采样, 其直接采样4个点并计算

cbuffer cbPerObject : register(b0) //每个物体的常量缓冲区，绑定到b0上
{
    float4x4 gWorld; //世界矩阵，用于从局部空间变更到世界空间
    float4x4 gTexTransform; //采样矩阵，用于对纹理进行采样
    uint gMaterialIndex; //材质在材质缓冲区中的偏移
    uint gObjPad0; //从下面开始，全都是为了保证对齐而添加的未使用成员
    uint gObjPad1;
    uint gObjPad2;
};

cbuffer cbPass : register(b1) //每帧的常量缓冲区，绑定到b1上
{
    float4x4 gView; //相机的观察矩阵. 用于从世界空间转换到观察空间
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

//将一个对法线图的采样转换到世界空间中的采样. (默认的采样为在NDC空间中)
float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
    float3 normalT = 2.0f * normalMapSample - 1.0f; //将NDC空间中的[0, 1]转换到投影空间中的[-1, 1]

    float3 N = unitNormalW;
    float3 T = normalize(tangentW - dot(tangentW, N) * N);
    float3 B = cross(N, T);

    float3x3 TBN = float3x3(T, B, N);

    float3 bumpedNormalW = mul(normalT, TBN);

    return bumpedNormalW;
}
