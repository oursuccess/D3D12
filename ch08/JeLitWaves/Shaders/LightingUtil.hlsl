//copy of LightingUtil.hlsl, CH08-LitWaves, Frank Luna

#define MaxLights 16

struct Light
{
    float3 Strength;
    float FalloffStart; //point/spot light only
    float3 Direction;   //directional/ spot light only
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

float CalcAttenuation(float d, float falloffStart, float falloffEnd)
{
    //linear
    return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
}

//schlick gives an approximation to Fresnel reflectance
//R0 = ((n-1)/(n+1))^2, where n is the index of refraction
//r = r0 + (1 - r0) * pow(max(cos¦È, 0), 5)
//cos¦È = dot(normal, lightVec)
float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec)
{
    float cosIncidentAngle = saturate(dot(normal, lightVec));

    float f0 = 1.0f - cosIncidentAngle;
    float3 reflectPercent = R0 + (1.0f - R0) * (f0 * f0 * f0 * f0 * f0);

    return reflectPercent;
}

//blinnPhong light model
//lightStrength * (diffuse + specular)
//specular = fresnelFactor * roughnessFactor
//         =  (r0 + (1 - r0) * pow(max(cos¦È, 0), 5)) * ((m + 8) / 8 * pow(max(sin¦Á, 0), m)
//cos¦È = dot(normal, lightVec)
//sin¦Á = dot(halfVec, normal)
//halfVec = normalize(toEye + lightVec)
//m = shiness * 256
float3 BlinnPhong(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat)
{
    const float m = mat.Shininess * 256.0f;
    float3 halfVec = normalize(toEye + lightVec);

    float roughnessFactor = (m + 8.0f) * pow(max(dot(halfVec, normal), 0.0f), m) / 8.0f;
    float3 fresnelFactor = SchlickFresnel(mat.FresnelR0, halfVec, lightVec);

    float3 specAlbedo = fresnelFactor * roughnessFactor;

    specAlbedo = specAlbedo / (specAlbedo + 1.0f);

    return (mat.DiffuseAlbedo.rgb + specAlbedo) * lightStrength;
}

//Evaluates the lighting equation for directional lights
float3 ComputeDirectionalLight(Light L, Material mat, float3 normal, float3 toEye)
{
    //directional light has no position
    float3 lightVec = -L.Direction;

    //we do not calc the lights that from behind
    float ndotl = max(dot(lightVec, normal), 0.0f);
    float3 lightStrength = L.Strength * ndotl;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

//Evaluates the lighting equation for point lights
float3 ComputePointLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
    float3 lightVec = L.Position - pos;

    //distance from surface to light
    float d = length(lightVec);

    //range test
    if (d > L.FalloffEnd)
        return 0.0f;

    //normalize the light vector
    lightVec /= d;

    //we do not calc the lights from behind
    float ndotl = max(dot(lightVec, normal), 0.0f);
    float3 lightStrength = L.Strength * ndotl;

    //Attenuate light by distance
    float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
    lightStrength *= att;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

//evaluates the lighting equation for spot lights
float3 ComputeSpotLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
    //vector from the surface to the light
    float3 lightVec = L.Position - pos;

    //distance from surface to light
    float d = length(lightVec);

    //range test
    if (d > L.FalloffEnd)
        return 0.0f;

    //normalize the light vector
    lightVec /= d;

    //no light from behind
    float ndotl = max(dot(lightVec, normal), 0.0f);
    float3 lightStrength = L.Strength * ndotl;

    //attenuate light by distance
    float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
    lightStrength *= att;

    //scale by spotlight
    float spotFactor = pow(max(dot(-lightVec, L.Direction), 0.0f), L.SpotPower);
    lightStrength *= spotFactor;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

//Evaluates a bound of lights. which sorted by orders: directionalLight --> pointLight --> spotLight
float4 ComputeLighting(Light gLights[MaxLights], Material mat, float3 pos, float3 normal, float3 toEye, float3 shadowFactor)
{
    float3 result = 0.0f;
    
    int i = 0;

#if (NUM_DIR_LIGHTS > 0)
    for (i = 0; i < NUM_DIR_LIGHTS; ++i)
    {
        result += shadowFactor[i] * ComputeDirectionalLight(gLights[i], mat, normal, toEye);
    }
#endif

#if (NUM_POINT_LIGHTS > 0)
    for (i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; ++i)
    {
        result += ComputePointLight(gLights[i], mat, pos, normal, toEye);
    }
#endif

#if (NUM_SPOT_LIGHTS > 0)
    for (i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; ++i)
    {
        result += ComputeSpotLight(gLights[i], mat, pos, normal, toEye);
    }
#endif

    return float4(result, 0.0f);
}
