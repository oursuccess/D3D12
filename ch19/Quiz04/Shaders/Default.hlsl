//copy of Default.hlsl by Frank Luna, ch18
//Default为默认的包含光照的Shader实现
//VS: 将顶点转换到投影空间
//PS: 计算光照并采样贴图、采样立方体图
//ch19中添加了法线贴图所需要的TangentU

// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

// Include common HLSL code.
#include "Common.hlsl"

struct VertexIn
{
	float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
    float3 TangentU : TANGENT;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalL : NORMAL;
    float3 TangentU : TANGENT;
	float2 TexC    : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;

	// Fetch the material data.
	MaterialData matData = gMaterialData[gMaterialIndex];
	
    // Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;

    // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
    //vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);
    vout.NormalL = vin.NormalL;

    //ch19. 传入TangentW
    //vout.TangentW = mul(vin.TangentU, (float3x3)gWorld);
    vout.TangentU = vin.TangentU;

    // Transform to homogeneous clip space.
    vout.PosH = mul(posW, gViewProj);
	
	// Output vertex attributes for interpolation across triangle.
	float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
	vout.TexC = mul(texC, matData.MatTransform).xy;
	
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	// Fetch the material data.
	MaterialData matData = gMaterialData[gMaterialIndex];
	float4 diffuseAlbedo = matData.DiffuseAlbedo;
	float3 fresnelR0 = matData.FresnelR0;
	float  roughness = matData.Roughness;
	uint diffuseTexIndex = matData.DiffuseMapIndex;

	// Dynamically look up the texture in the array.
	diffuseAlbedo *= gDiffuseMap[diffuseTexIndex].Sample(gsamAnisotropicWrap, pin.TexC);
	
    // Interpolating normal can unnormalize it, so renormalize it.
    //pin.NormalW = normalize(pin.NormalW);
    pin.NormalL = normalize(pin.NormalL);

    //ch19. 采样法线,并将之转换到世界空间中
    uint normalMapIndex = matData.NormalMapIndex;
    float4 normalMapSample = gDiffuseMap[normalMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);
    //float3 bumpedNormalW = NormalSampleToWorldSpace(normalMapSample.rgb, pin.NormalW, pin.TangentW);
    //将切线空间中的法向量映射到[-1, 1]范围内
    float3 normalU = 2.0f * normalMapSample - 1.0f;

    // Vector from point being lit to eye. 
    float3 toEyeW = normalize(gEyePosW - pin.PosW);
    //得到局部空间中的观察向量, 矩阵在左边，表示乘以其逆矩阵
    float3 N = pin.NormalL;
    float3 T = normalize(pin.TangentU - dot(pin.TangentU, N) * N);
    float3 B = cross(N, T);
    float3x3 TBN = { T, B, N };
    float3 worldToU = mul(TBN, (float3x3) gWorld);
    //求出观察向量在切线空间中的位置
    float3 toEyeU = mul(worldToU, toEyeW);
    //求出顶点在切线空间中的位置
    float3 posU = mul(worldToU, pin.PosW);
    //将光向量转换到切线空间中，我们在这里利用了只有平行光源的事实
    Light lights[MaxLights];
    int i = 0;
    while (i < NUM_DIR_LIGHTS)
    {
        Light cur = gLights[i];
        Light tmp;
        tmp.Strength = cur.Strength;
        tmp.Direction = mul(worldToU, cur.Direction);
        lights[i++] = tmp;
    }

    // Light terms.
    float4 ambient = gAmbientLight*diffuseAlbedo;

    const float shininess = (1.0f - roughness) * normalMapSample.a;
    Material mat = { diffuseAlbedo, fresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    //float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
        //bumpedNormalW, toEyeW, shadowFactor);
    //将光照计算改为在局部空间中
    float4 directLight = ComputeLighting(lights, mat, posU, normalU, toEyeU, shadowFactor);

    float4 litColor = ambient + directLight;

	// Add in specular reflections.
	//float3 r = reflect(-toEyeW, bumpedNormalW);
    float3 r = reflect(-toEyeU, normalU);
	float4 reflectionColor = gCubeMap.Sample(gsamLinearWrap, r);
	//float3 fresnelFactor = SchlickFresnel(fresnelR0, bumpedNormalW, r);
    float3 fresnelFactor = SchlickFresnel(fresnelR0, normalU, r);
	litColor.rgb += shininess * fresnelFactor * reflectionColor.rgb;

    // Common convention to take alpha from diffuse albedo.
    litColor.a = diffuseAlbedo.a;

    return litColor;
}


