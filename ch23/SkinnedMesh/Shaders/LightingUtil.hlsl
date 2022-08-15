//LightingUtil copyed from DX12, by Frank Luna

#define MaxLights 16    //定义最大光照数量为16

struct Light    //光源的结构体. 平行光光源包含了光强与方向. 点光源包含了光强, 位置, 衰减开始距离、衰减结束距离. 聚光灯包含了光强, 位置, 衰减开始、结束距离, 方向, 聚光幅度
{
    float3 Strength;    //看好我们的数据结构组织. 这是为了保证按照我们的顺序进行对齐
    float FalloffStart;
    float3 Direction;
    float FalloffEnd;
    float3 Position;
    float SpotPower;
};

struct Material //材质. 材质包含了漫反射, R0和光泽度
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Shininess;
};

float CalcAttenuation(float d, float falloffStart, float falloffEnd)
{
    //我们直接使用线性衰减即可
    return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
}

//根据Schlick近似计算Fresnel反射
//Schlick近似: R0 + (1 - R0) * pow(f0, 5), 其中f0 = 1 - cos(θ)， cos(θ) = dot(normal, lightVec)
//Fresnel方程: 入射(/观察)方向与恰好位于两个方向中心的表面法线夹角越大, 反射越大
float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec)
{
    float cosIncidentAngle = saturate(dot(normal, lightVec));
    
    float f0 = 1 - cosIncidentAngle;
    float3 reflectPercent = R0 + (1.0f - R0) * (f0 * f0 * f0 * f0 * f0);

    return reflectPercent;
}

//使用Blinn-Phong方程模拟计算表面接受光线照射后, 在指定角度观察看到的效果. 我们需要光强、入射方向, 法线, 观察角度与材质信息
//我们使用的方程中, 主要分为漫反射和高光反射部分. 其中漫反射直接用漫反射*入射光强即可. 高光反射我们使用fresnel方程计算出高光反射, 再根据光泽度计算出反射系数， 最终将能量归一化得出
//反射系数为(m + 8.0f) * pow(max(dot(halfVec, normal), 0.0f), m) / 8.0f, 其中m = shininess * 256.0f;
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

//计算方向光光源
float3 ComputeDirectionalLight(Light L, Material mat, float3 normal, float3 toEye)
{
    float3 lightVec = -L.Direction;

    float ndotl = max(dot(lightVec, normal), 0.0f); //入射光强也要看入射方向和法线夹角的
    float3 lightStrength = L.Strength * ndotl;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

//计算点光源. 在点光源的计算中, 材质的位置是重要的了, 因为光源也有了位置
float3 ComputePointLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
    float3 lightVec = L.Position - pos; //是光向量, 从pos指向光源!

    float d = length(lightVec);

    if (d > L.FalloffEnd)
        return 0.0f;

    lightVec /= d;

    float ndotl = max(dot(lightVec, normal), 0.0f);
    float3 lightStrength = L.Strength * ndotl;

    float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
    lightStrength *= att;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

//计算聚光灯
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

    float spotFactor = pow(max(dot(-lightVec, L.Direction), 0.0f), L.SpotPower);    //聚光灯还有一个额外的聚光范围
    lightStrength *= spotFactor;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

float4 ComputeLighting(Light gLights[MaxLights], Material mat, float3 pos, float3 normal, float3 toEye, float3 shadowFactor)
{
    float3 res = 0.0f;

    int i = 0;

#if (NUM_DIR_LIGHTS > 0)
    for(i = 0; i < NUM_DIR_LIGHTS; ++i) 
        res += shadowFactor[i] * ComputeDirectionalLight(gLights[i], mat, normal, toEye);
#endif

#if (NUM_POINT_LIGHTS > 0)
    for(i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; ++I)
        res += ComputePointLight(gLights[i], mat, pos, normal, toEye);
#endif

#if (NUM_SPOT_LIGHTS > 0)
    for(i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; ++i)
        res += ComputeSpotLight(gLights[i], mat, pos, normal, toEye);
#endif

    return float4(res, 0.0f);
}