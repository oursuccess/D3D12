//copy of Default.hlsl by Frank Luna, ch18
//Default为默认的包含光照的Shader实现
//VS: 将顶点转换到投影空间
//PS: 计算光照并采样贴图、采样立方体图

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
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
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
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);

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
    pin.NormalW = normalize(pin.NormalW);

    // Vector from point being lit to eye. 
    float3 toEyeW = normalize(gEyePosW - pin.PosW);

    // Light terms.
    float4 ambient = gAmbientLight*diffuseAlbedo;

	const float shininess = 1.0f - roughness;
    Material mat = { diffuseAlbedo, fresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
        pin.NormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;

	// Add in specular reflections.
	float3 r = reflect(-toEyeW, pin.NormalW);
	float4 reflectionColor = gCubeMap.Sample(gsamLinearWrap, r);

    //Quiz1804,弄一次模糊，我们直接6次偏移完事儿
    int blurLayer = 3;
    float step = 0.1f;

    float3 r0s[18];
    r0s[0] = r + float3(step, step, 0);
    r0s[1] = r - float3(step, step, 0);
    r0s[2] = r + float3(step, 0, step);
    r0s[3] = r - float3(step, 0, step);
    r0s[4] = r + float3(0, step, step);
    r0s[5] = r - float3(0, step, step);

    r0s[6] = r + float3(2 * step, 2 * step, 0);
    r0s[7] = r - float3(2 * step, 2 * step, 0);
    r0s[8] = r + float3(2 * step, 0, 2 * step);
    r0s[9] = r - float3(2 * step, 0, 2 * step);
    r0s[10] = r + float3(0, 2 * step, 2 * step);
    r0s[11] = r - float3(0, 2 * step, 2 * step);

    r0s[12] = r + float3(3 * step, 3 * step, 0);
    r0s[13] = r - float3(3 * step, 3 * step, 0);
    r0s[14] = r + float3(3 * step, 0, 3 * step);
    r0s[15] = r - float3(3 * step, 0, 3 * step);
    r0s[16] = r + float3(0, 3 * step, 3 * step);
    r0s[17] = r - float3(0, 3 * step, 3 * step);

    for (int i = 0, iMax = 6 * blurLayer; i < iMax; ++i)
    {
        reflectionColor += gCubeMap.Sample(gsamLinearWrap, r0s[i]);
    }
    reflectionColor = reflectionColor / (blurLayer * 6 + 1);
    //Quiz1804在这里结束

    float3 fresnelFactor = SchlickFresnel(fresnelR0, pin.NormalW, r);
	litColor.rgb += shininess * fresnelFactor * reflectionColor.rgb;

    // Common convention to take alpha from diffuse albedo.
    litColor.a = diffuseAlbedo.a;

    return litColor;
}


