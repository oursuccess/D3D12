//copy of LightingUtil of Frack Luna
#define MaxLights 16

struct Light
{
    float3 Strength;
    float FalloffStart; //point/spot light only
    float3 Direction;   //directional/spot light only
    float FalloffEnd;   //point/spot light only
    float3 Position;    //point light only
    float SpotPower;    //spot light only
};

struct Material
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Shininess;
};

//计算光照衰减
float CalcAttenuation(float d, float falloffStart, float falloffEnd)
{
    //linear falloff
    return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
}

//使用Schlick近似计算镜面反射
//R0 = ((n-1)/(n+1)) ^ 2
float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec)
{
    float cosIncidentAngle = saturate(dot(normal, lightVec));
    float f0 = 1.0f - cosIncidentAngle;
    float3 reflectPercent = R0 + (1.0f - R0) * (f0 * f0 * f0 * f0 * f0);
   
    return reflectPercent;
}

//BlinnPhong光照模型
float3 BlinnPhong(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat)
{
    const float m = mat.Shininess * 256.0f;
    float3 halfVec = normalize(toEye + lightVec);

    float roughnessFactor = (m + 8.0f) * pow(max(dot(halfVec, normal), 0.0f), m) / 8.0f;
    float3 fresnelFactor = SchlickFresnel(mat.FresnelR0, halfVec, lightVec);
    
    float3 specAlbedo = fresnelFactor * roughnessFactor;

    //spec formula is outside[0, 1], so we need to scale it down a bit
    specAlbedo = specAlbedo / (specAlbedo + 1.0f);

    return (mat.DiffuseAlbedo.rgb + specAlbedo) * lightStrength;
}

//平行光
float3 ComputeDirectionalLight(Light L, Material mat, float3 normal, float3 toEye)
{
    //the light vector aims opposite the direction the light rays traval
    float3 lightVec = -L.Direction;

    //lambert's cosine law
    float3 ndotl = max(dot(lightVec, normal), 0.0f);
    float3 lightStrength = L.Strength * ndotl;
    
    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

//点光源
float3 ComputePointLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
    float3 lightVec = L.Position - pos;
    //distance from surface to light
    float d = length(lightVec);

    if (d > L.FalloffEnd)
        return 0.0f;

    //normalize the light vector
    lightVec /= d;

    //lamberts's cosine law
    float3 ndotl = max(dot(lightVec, normal), 0.0f);
    float3 lightStrength = L.Strength * ndotl;

    float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
    lightStrength *= att;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

//spot light
float3 ComputeSpotLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
    float3 lightVec = L.Position - pos;
    float d = length(lightVec);

    if (d > L.FalloffEnd)
        return 0.0f;
    
    lightVec /= d;

    float ndotl = max(dot(lightVec, normal), 0.0f);
    float3 lightStrength = L.Strength * ndotl;
    
    float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
    lightStrength *= att;

    float spotFactor = pow(max(dot(-lightVec, L.Direction), 0.0f), L.SpotPower);
    lightStrength *= spotFactor;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

//计算所有光源
float4 ComputeLighting(Light gLights[MaxLights], Material mat, float3 pos, float3 normal, float3 toEye, float3 shadowFactor)
{
    float3 res = 0.0f;
    int i = 0;
#if (NUM_DIR_LIGHTS > 0)
    for (int i = 0; i < NUM_DIR_LIGHTS; ++i)
    {
        //只有平行光才计算阴影
        res += shadowFactor[i] * ComputeDirectionalLight(gLights[i], mat, normal, toEye);
    }
#endif

#if (NUM_POINT_LIGHTS > 0)
    for (int i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; ++i)
    {
        res += ComputePointLight(gLights[i], mat, pos, normal, toEye);
    }
#endif

#if (NUM_SPOT_LIGHTS > 0)
    for (int i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIHGTS + NUM_SPOT_LIGHTS; ++i)
    {
        res += ComputeSpotLight(gLights[i], mat, pos, normal, toEye);
    }
#endif

    return res;
}