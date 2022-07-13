//Default

#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS
#endif

#include "Common.hlsl"

struct VertexIn //对应InputLayout
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
    float3 TangentU : TANGENT;
};

struct VertexOut    //Vert的输出
{
    float4 PosH : SV_POSITION;  //系统值，用于确认在渲染窗口中的位置
    float4 ShadowPosH : POSITION0;  //Shadow
    float4 SsaoPosH : POSITION1;    //Ssao.  之所以需要float4, 是因为我们需要在PS中进行归一化(不能在默认的栅格化过程中进行自动插值)
    float3 PosW : POSITION2;    //世界空间下的位置
    float3 NormalW : NORMAL;    //世界空间下的法线向量
    float3 TangentW : TANGENT;  //世界空间下的切线向量
    float2 TexC : TEXCOORD; //纹理坐标(纹理坐标是可以默认插值的)
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;  //还能这样生成? 

    MaterialData matData = gMaterialData[gMaterialIndex];   //先获取材质

    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);  //根据世界变换矩阵，将顶点从局部空间变换到世界空间中
    vout.PosW = posW.xyz;

    vout.NormalW = mul(vin.NormalL, (float3x3) gWorld); //这里我们假定了其没有非统一缩放. 否则我们需要使用世界矩阵的逆转置矩阵进行法线的变换

    vout.TangentW = mul(vin.TangentU, (float3x3) gWorld);   //切线变换

    vout.PosH = mul(posW, gViewProj);   //获取其在投影空间中的坐标

    vout.SsaoPosH = mul(posW, gViewProjTex);    //获取其对Ssao图进行采样时的坐标. 其实就是在投影坐标的基础上额外乘了一个NDC到纹理空间的矩阵

    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform); //获取纹理采样的坐标
    vout.TexC = mul(texC, matData.MatTransform).xy; //将采样坐标与材质本身的偏移相乘，从而得到对实际贴图的采样坐标

    vout.ShadowPosH = mul(posW, gShadowTransform);  //获取对阴影图采样时的坐标

    return vout;
}

float4 PS(VertexOut pin) : SV_Target    //我们声明返回的float4为对应像素的颜色值
{
    MaterialData matData = gMaterialData[gMaterialIndex];
    float4 diffuseAlbedo = matData.DiffuseAlbedo;   //获取漫反射, R0, 粗糙度, 纹理图索引, 法线图索引
    float3 fresnelR0 = matData.FresnelR0;
    float roughness = matData.Roughness;
    uint diffuseMapIndex = matData.DiffuseMapIndex;
    uint normalMapIndex = matData.NormalMapIndex;

    diffuseAlbedo *= gTextureMaps[diffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);   //我们使用重复采样的各向异性过滤对指定的纹理图进行采样, 采样后乘以漫反射颜色，即得默认的色彩

#ifdef ALPHA_TEST
    clip(diffuseAlbedo.abort - 0.1f);   //若开启了Alpha测试，则我们应当尽可能早的进行透明测试
#endif

    pin.NormalW = normalize(pin.NormalW);   //因为我们上面的法线变换并非逆转置矩阵，且由于插值，可能导致法线现在模非1，因此需要我们归一化

    float4 normalMapSample = gTextureMaps[normalMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);    //同样的，对法线图进行采样，得到法线
    float3 bumpedNormalW = NormalSampleToWorldSpace(normalMapSample.rgb, pin.NormalW, pin.TangentW);    //我们将法线数据从NDC空间变换回世界空间
    //若没有使用法线贴图，则我们可以简单的使用如下方式:
    //float3 bumpedNormalW = pin.NormalW;

    float3 toEyeW = normalize(gEyePosW - pin.PosW); //根据观察点和顶点，得到观察向量(从顶点指向观察点)

    pin.SsaoPosH /= pin.SsaoPosH.w; //将Ssao坐标归一化到[-1, 1]范围内
    float ambientAccess = gSsaoMap.Sample(gsamLinearClamp, pin.SsaoPosH.xy, 0.0f).r;    //获取根据Ssao贴图得到的遮蔽率. 遮蔽率为一个值. 我们直接使用线性混合即可. 当超出范围时，我们直接采用最接近其坐标的值

    float4 ambient = ambientAccess * gAmbientLight * diffuseAlbedo; //根据漫反射、环境光、环境光触达率，我们可以得到该物体的环境光

    float3 shadowFactor = float3(1.0f, 1.0f, 1.0f); //默认的阴影采样为1, 1, 1. 不被遮挡的越多，则该值越大
    shadowFactor[0] = CalcShadowFactor(pin.ShadowPosH); //计算实际的阴影采样率. 我们当然是从光线方向进行检测. 我们仅仅为第一个光源计算阴影

    const float shininess = (1.0f - roughness) * normalMapSample.a; //获得光泽度. 光泽度为1 - 粗糙度
    Material mat = { diffuseAlbedo, fresnelR0, shininess };
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW, bumpedNormalW, toEyeW, shadowFactor);  //根据材质计算光照

    float4 litColor = ambient + directLight;    //最终颜色为环境光 + 光照颜色

    //我们还要加上对环境的高光反射
    float3 r = reflect(-toEyeW, bumpedNormalW); //折射光方向为根据观察向量，在法线方向上的对称向量
    float4 reflectionColor = gCubeMap.Sample(gsamLinearWrap, r);    //我们根据折射光方向，在环境光贴图上采样，得到其反射
    float3 fresnelFactor = SchlickFresnel(fresnelR0, bumpedNormalW, r); //得到fresnel
    litColor.rgb += shininess * fresnelFactor * reflectionColor.rgb;    //我们再让最后的颜色加上反射

    litColor.a = diffuseAlbedo.a;

    return litColor;
}
