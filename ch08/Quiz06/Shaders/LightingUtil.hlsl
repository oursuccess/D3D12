//copy of LightingUtil, Frank Luna, ch08, Lit Waves
//add simple toon light shader by Je

#define MaxLights 16

struct Light
{
    float3 Strength;
    float FalloffStart;
    float3 Direction;
    float FalloffEnd;
    float3 Position;
    float SpotPower;
};

struct Material
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Shineness;
};

//计算光强衰减
float CalcAttenuation(float d, float falloffStart, float falloffEnd)
{
    //线性
    return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
}

//Schlick Fresnel
//R0 = ( (n-1)/(n+1) ) ^ 2
float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec)
{
    float cosIncidentAngle = saturate(dot(normal, lightVec));
    float f0 = 1.0f - cosIncidentAngle;
    float3 reflectPercent = R0 + (1.0f - R0) * (f0 * f0 * f0 * f0 * f0);

    return reflectPercent;
}

//BlinnPhong
float3 BlinnPhong(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat)
{
    //是光泽度。和粗糙度刚好相反了！
    const float m = mat.Shineness * 256.0f;
    float3 halfVec = normalize(toEye + lightVec);

    float roughnessFactor = (m + 8.0f) * pow(max(dot(halfVec, normal), 0.0f), m) / 8.0f;
    float3 fresnelFactor = SchlickFresnel(mat.FresnelR0, halfVec, lightVec);

    float3 specAlbedo = fresnelFactor * roughnessFactor;

    specAlbedo = specAlbedo / (specAlbedo + 1.0f);

    return (mat.DiffuseAlbedo.rgb + specAlbedo) * lightStrength;
}

//自行实现的ToonLight
float3 ToonLight(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat)
{
    //是光泽度。和粗糙度刚好相反了！
    const float m = mat.Shineness * 256.0f;
    float3 halfVec = normalize(toEye + lightVec);

    float roughnessFactor = pow(max(dot(halfVec, normal), 0.0f), m);
    //在这里加上一个ks的离散化
    roughnessFactor = roughnessFactor <= 0.1f ? 0.0f : (roughnessFactor <= 0.8f ? 0.5f : 0.8f);
    roughnessFactor *= (m + 8.0f) / 8.0f;

    float3 fresnelFactor = SchlickFresnel(mat.FresnelR0, halfVec, lightVec);

    float3 specAlbedo = fresnelFactor * roughnessFactor;

    specAlbedo = specAlbedo / (specAlbedo + 1.0f);

    return (mat.DiffuseAlbedo.rgb + specAlbedo) * lightStrength;
}

//在下面的所有的光照中，我们要将ndotl进行离散化，并将最后的光照调用修改为ToonLight。后面所有光源都要修改，因此不再赘述
float3 ComputeDirectionalLight(Light L, Material mat, float3 normal, float3 toEye)
{
    float3 lightVec = -L.Direction;

    //离散化，后面相同，不再赘述
    float ndotl = max(dot(lightVec, normal), 0.0f);
    ndotl = ndotl <= 0.02f ? 0.4f : (ndotl <= 0.5f ? 0.6f : 1.0f);

    float3 lightStrength = L.Strength * ndotl;

    //将BlinnPhong修改为ToonLight，后面相同 不再赘述
    return ToonLight(lightStrength, lightVec, normal, toEye, mat);
}

float3 ComputePointLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
    float3 lightVec = L.Position - pos;
    float d = length(lightVec);

    if (d > L.FalloffEnd)
        return 0.0f;
    
    lightVec /= d;

    //离散化，后面相同，不再赘述
    float ndotl = max(dot(lightVec, normal), 0.0f);
    ndotl = ndotl <= 0.02f ? 0.4f : (ndotl <= 0.5f ? 0.6f : 1.0f);

    float3 lightStrength = L.Strength * ndotl;
    
    float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
    lightStrength *= att;
    
    return ToonLight(lightStrength, lightVec, normal, toEye, mat);
}

float3 ComputeSpotLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
    float3 lightVec = L.Position - pos;
    float d = length(lightVec);

    if (d > L.FalloffEnd)
        return 0.0f;
    
    lightVec /= d;

    //离散化，后面相同，不再赘述
    float ndotl = max(dot(lightVec, normal), 0.0f);
    ndotl = ndotl <= 0.02f ? 0.4f : (ndotl <= 0.5f ? 0.6f : 1.0f);
    
    float3 lightStrength = L.Strength * ndotl;
    
    float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
    lightStrength *= att;
    
    float spotFactor = pow(max(dot(-lightVec, L.Direction), 0.0f), L.SpotPower);
    lightStrength *= spotFactor;
    
    return ToonLight(lightStrength, lightVec, normal, toEye, mat);
}

float4 ComputeLighting(Light gLights[MaxLights], Material mat, float3 pos, float3 normal, float3 toEye, float3 shadowFactor)
{
    float3 result = 0.0f;
    int i = 0;

    #if (NUM_DIR_LIGHTS > 0)
    for(i = 0; i < NUM_DIR_LIGHTS; ++i)
    {
        result += shadowFactor[i] * ComputeDirectionalLight(gLights[i], mat, normal, toEye);
    }
#endif

#if (NUM_POINT_LIGHTS > 0)
    for(i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS+NUM_POINT_LIGHTS; ++i)
    {
        result += ComputePointLight(gLights[i], mat, pos, normal, toEye);
    }
#endif

#if (NUM_SPOT_LIGHTS > 0)
    for(i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; ++i)
    {
        result += ComputeSpotLight(gLights[i], mat, pos, normal, toEye);
    }
#endif 

    return float4(result, 0.0f);
}
