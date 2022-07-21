//Lighting Util

#define MaxLights 16

struct Light    //定义光源需要的属性
{
    float3 Strength;    //光强. 需要注意我们的成员变量分布. 我们刻意将变量按照4bytes * 4进行排布, 从而保证和CPU传入的数据结构相同
    float FalloffStart; //光强开始衰减的距离. 方向光没有该属性
    float3 Direction;   //光照方向. 点光源没有该属性, 因为其向四面八方照射. 聚光灯的该属性表示其照射方向的中心点
    float FalloffEnd;   //光照完全衰减的距离. 到达该距离后光强为0. 方向光没有该属性
    float3 Position;     //光源位置. 方向光没有该属性
    float SpotPower;    //光的聚集程度. 仅聚光灯有该属性
};

struct Material //定义对于计算光照来说, 材质需要的属性
{
    float4 DiffuseAlbedo;   //漫反射. 分为argb4个通道
    float3 FresnelR0;       //R0值, 用于计算菲涅尔效应
    float Shininess;        //光泽度. 其为(1 - 粗糙度) * 256. 同样用于计算菲涅尔效应和高光反射
};

float CalcAttenuation(float d, float falloffStart, float falloffEnd)
{
    //我们假定光强按照线性衰减，从而计算出其衰减幅度
    return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
}

//使用Schlick近似计算菲涅尔效应. 菲涅尔效应和(由观察点和光向量得到的实际应该在的)法线与光向量有关
//r(θ) = r0 + (1 - r0) * (1 - cos(θ)) ^ 5. 其中cos(θ) = dot(normal, lightVec)
float SchlickFresnel(float3 R0, float3 normal, float3 lightVec)
{
    float cosIncidentAngle = saturate(dot(normal, lightVec));
    float f0 = 1.0f - cosIncidentAngle;
    float3 reflectPercent = R0 + (1.0f - R0) * (f0 * f0 * f0 * f0 * f0);

    return reflectPercent;
}

//根据光强、光向量、法线、观察向量与材质计算实际的光照. 我们这里采用的是(有能量守恒的)Blinn Phong模型
//col = lightStrength * (diffuse + specular). 其中specular = r(θ) * roughnessFactor. 而roughnessFactor为:
//roughnessFactor = (m + 8.0f) / 8.0f * shi. 后面shi即为dot(halfVec, normal) ^ m
float BlinnPhong(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat)
{
    const float m = mat.Shininess * 256.0f;
    float3 halfVec = normalize(toEye + lightVec);   //我们期望的那些刚好能反射到我们眼球的表面的法线应该刚好出在观察向量和光向量的中间位置

    float roughnessFactor = (m + 8.0f) / 8.0f * pow(max(dot(halfVec, normal), 0.0f), m);
    float fresnelFactor = SchlickFresnel(mat.FresnelR0, halfVec, lightVec); //注意这里传入的对应SchilickFresnel中normal的参数是halfVec!

    float3 specAlbedo = fresnelFactor * roughnessFactor;

    specAlbedo = specAlbedo / (specAlbedo + 1.0f);  //因为我们计算出来的高光反射颜色可能在HDR范围, 因此需要将其转换到LDR区间

    return (mat.DiffuseAlbedo.rgb + specAlbedo) * lightStrength;    //返回综合了高光和漫反射的颜色
}

//计算接受方向光照射后的结果
float3 ComputeDirectionalLight(Light L, Material mat, float3 normal, float3 toEye)
{
    float3 lightVec = -L.Direction;  //在方向光的计算中，光向量和光的入射方向相反

    float3 ndotl = max(dot(lightVec, normal), 0.0f);
    float3 lightStrength = L.Strength * ndotl;  //光强也要实际照射入物体. 那些沿着切线的部分都和物体直接擦边过去了

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

//计算pos位置的点接受点光源照射后的结果
float3 ComputePointLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
    float3 lightVec = L.Position - pos; //光向量为物体指向光源!

    float d = length(lightVec);

    if (d > L.FalloffEnd)
        return 0.0f;    //我们可以返回0.0, 而底层会自动帮我们变为float3

    lightVec /= d;  //对光向量进行归一化

    float ndotl = max(dot(lightVec, normal), 0.0f); //同样的，计算实际照射入物体的光源
    float3 lightStrength = L.Strength * ndotl;

    float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
    lightStrength *= att;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

//计算pos位置的接受聚光灯照射后的结果
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

    //我们还要对聚光灯进行按照距离其照射中心的偏离程度的光强衰减计算
    float spotFactor = pow(max(dot(-lightVec, L.Direction), 0.0f), L.SpotPower);    //我们的聚光灯光强按照与光照中心的偏移程度指数性衰减, 衰减速度取决于SpotPower(越大, 则衰减越快)
    lightStrength *= spotFactor;

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

//计算光照和阴影
float4 ComputeLighting(Light gLights[MaxLights], Material mat, float3 pos, float3 normal, float3 toEye, float3 shadowFactor)    //这里的shadowFactor之所以为3, 是因为我们只计算平行光的阴影, 而平行光我们设定了只有3个
{
    float3 res = 0.0f;

    int i = 0;  //开始逐光源遍历

#if (NUM_DIR_LIGHTS > 0)
    for(i = 0; i < NUM_DIR_LIGHTS; ++i)
    {
        res += shadowFactor[i] * ComputeDirectionalLight(gLights[i], mat, normal, toEye);   //只有在计算平行光的时候我们才计算阴影
    }
#endif

#if (NUM_POINT_LIGHTS > 0)
    for(i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; ++i)
    {
        res += ComputePointLight(gLights[i], mat, pos, normal, toEye);
    }
#endif

#if (NUM_SPOT_LIGHTS > 0)
    for(i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; ++i)
    {
    res += ComputeSpotLight(gLights[i], mat, pos, normal, toEye);
    }
#endif

    return float4(res, 0.0f);
}
