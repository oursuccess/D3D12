//Default

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

struct MaterialData
{
    float4 DiffuseAlbedo;   //漫反射
    float3 FresnelR0;       //R0
    float Roughness;        //粗糙度
    float4x4 MatTransform;  //材质变换矩阵
    uint DiffuseMapIndex;   //漫反射贴图索引
    uint MatPad0;           //3个用来保证内存布局的填充数字
    uint MatPad1;
    uint MatPad2;
};

Texture2D gDiffuseMap[5] : register(t0);    //漫反射贴图. 我们将其绑定到t0上

//StructuredBuffer: https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/sm5-object-structuredbuffer
StructuredBuffer<MaterialData> gMaterialData : register(t0, space1);    //材质数据们, 我们将其绑定到space1的t0上. 之索引为什么要将其绑定到t(对应ShaderResourceView), 是因为该类型就是需要绑定到SRV格式上的!

SamplerState gsamPointWrap  : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gTexTransform;
    uint gMaterialIndex;
    uint gObjPad0;
    uint gObjPad1;
    uint gObjPad2;
};

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
    float4 gAmbientLight;
    
    Light gLights[MaxLights];
};

struct VertexIn //VertexIn在VertexBuffer中定义
{
    float3 PosL : POSITION; //POSITION, NORMAL, TEXCOORD分别与InputLayout之中的语义绑定
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_Position;  //系统值变量, 我们必须在光栅化前传入该值, 用来表示其在投影时的坐标变换
    float3 PosW : POSITION; //后面的这三个也需要约定语义. 我们在VS/PS等着色器中定义的结构体都是需要约定语义的!
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;

    MaterialData matData = gMaterialData[gMaterialIndex];

    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;

    vout.NormalW = mul(vin.NormalL, (float3x3) gWorld); //在没有非对称缩放的情况下, 我们可以直接乘以该矩阵来变换法线. 否则我们将需要乘以变换矩阵的逆转置矩阵

    vout.PosH = mul(posW, gViewProj);   //求得其在投影空间中的坐标

    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform); //先进行纹理采样, 然后再按照材质进行变换
    vout.TexC = mul(texC, matData.MatTransform).xy;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    MaterialData matData = gMaterialData[gMaterialIndex];
    float4 diffuseAlbedo = matData.DiffuseAlbedo;
    float3 fresnelR0 = matData.FresnelR0;
    float roughness = matData.Roughness;
    uint diffuseTexIndex = matData.DiffuseMapIndex;
    
    diffuseAlbedo *= gDiffuseMap[diffuseTexIndex].Sample(gsamLinearWrap, pin.TexC); //获取其对应的漫反射

    pin.NormalW = normalize(pin.NormalW);

    float3 toEyeW = normalize(gEyePosW - pin.PosW); //观察向量即为从观察点减去顶点

    float4 ambient = gAmbientLight * diffuseAlbedo;

    const float shininess = 1.0f - roughness;   //光泽度为1 - 粗糙度
    Material mat = { diffuseAlbedo, fresnelR0, shininess };

    float3 shadowFactor = 1.0f; //没计算阴影
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW, pin.NormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;

    litColor.a = diffuseAlbedo.a;

    return litColor;
}
