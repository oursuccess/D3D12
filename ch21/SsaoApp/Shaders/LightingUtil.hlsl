//LightUtil

#define MaxLights 16    //我们只处理最多16个光源的情况

struct Light
{
    float3 Strength;    //光源强度
    float FalloffStart; //只有点光源/聚光灯才有该属性, 用于表示从什么距离开始衰减
    float3 Direction;   //只有方向光/聚光灯才有该属性. 中心光照的方向.注意我们的成员变量分布. 我们让4个8Byte元素凑在一起，从而尽量满足对齐要求
    float FalloffEnd;   //只有点光源/聚光灯才有该属性，用于表示到达该距离后光强衰减为0
    float3 Position;    //只有点光源/聚光灯才有该属性. 光源中心点的位置
    float SpotPower;    //只有聚光灯才有该属性. 控制聚光灯的聚光范围
};

struct Material
{
    float4 DiffuseAlbedo;   //漫反射颜色
    float3 FresnelR0;   //R0值
    float Shininess;    //光泽度. 与粗糙度相对
};

float CalcAttenuation(float d, float falloffStart, float falloffEnd)    //计算在指定的end和start的情况下, 距离光源位置为d时的实际光强衰减幅度
{
    //我们假定光强线性衰减
    return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
}

//Schlick公式: R(θ) = R0 + (1 - R0) * (1 - cosθ)^5
float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec)    //希力克方法计算菲涅尔效应
{
    float cosIncidentAngle = saturate(dot(normal, lightVec));   //cosθ. 该值越大, 菲涅尔效应中的折射光占比越高, (入射之后再)反射光占比越低

    float f0 = 1.0f - cosIncidentAngle;
    float3 reflectPercent = R0 + (1.0f - R0) * (f0 * f0 * f0 * f0 * f0);    //根据R0和f0计算出实际的反射光量值

    return reflectPercent;
}

float3 BlinnPhong(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat) //BlinnPhong接受光强、光向量(从顶点指向光源)、顶点法线、观察向量(从顶点指向观察点)、材质
{
    const float m = mat.Shininess * 256.0f; //m为我们建模粗糙度时, cos(θ)的幂次. 这里我们指定了256倍的粗糙度，从而让表面更加平滑一些
    float3 halfVec = normalize(toEye + lightVec);   //h为观察向量和光向量的中间向量

    float roughnessFactor = (m + 8.0f) * pow(max(dot(halfVec, normal), 0.0f), m) / 8.0f;    //这是根据粗糙度模拟镜像反射光量的函数. (m + 8.0f) / 8.0f用于对光能进行归一化，从而保证光能守恒
    float3 fresnelFactor = SchlickFresnel(mat.FresnelR0, halfVec, lightVec);

    float3 specAlbedo = fresnelFactor * roughnessFactor;    //根据粗糙度和菲涅尔效应，我们计算镜面反射到观察者眼中的实际光量

    specAlbedo = specAlbedo / (specAlbedo + 1.0f);  //我们将HDR映射回LDR的范围内[0, 1]

    return (mat.DiffuseAlbedo.rgb + specAlbedo) * lightStrength;    //返回结合入射之后又被漫反射出去的部分和入射时被高光反射出去的部分的光照实际强度
}

//计算平行光照射到指定法线的材质后，在指定观察点观察时的光强. 平行光没有位置，也不在乎材质的位置
float3 ComputeDirectionalLight(Light L, Material mat, float3 normal, float3 toEye)
{
    float3 lightVec = -L.Direction; //光向量为入射光线的反向量

    float ndotl = max(dot(lightVec, normal), 0.0f); //ndotl为cosθ. 用于计量光线和顶点法线之间的夹角. 夹角越小，该值越大，漫反射越高
    float3 lightStrength = L.Strength * ndotl;  //这些为实际的射入材质的部分

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat); //根据BlinnPhong计算实际的光强
}

//计算点光源照射到指定法线指定顶点的材质后，在指定观察点时观察到的光强. 与平行光相比，我们有了点光源的位置
float3 ComputePointLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
    float3 lightVec = L.Position - pos; //此即从顶点处看向光源的光向量

    float d = length(lightVec); //光向量的长度
    
    if (d > L.FalloffEnd)
        return 0.0f;    //衰减为0

    lightVec /= d;  //将光向量映射到[0, 1]范围内

    float ndotl = max(dot(lightVec, normal), 0.0f); //开始计算漫反射
    float3 lightStrength = L.Strength * ndotl;

    float att = CalcAttenuation(d, L.FalloffEnd, L.FalloffStart);    //计算衰减幅度
    lightStrength *= att;   //在距离d后实际衰减的光强

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

//计算聚光灯照射到指定位置的指定材质的顶点后，在指定观察点时看到的光强
float3 ComputeSpotLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
    float3 lightVec = L.Position - pos;

    float d = length(lightVec);

    if (d > L.FalloffEnd)
        return 0.0f;

    lightVec /= d;  

    float ndotl = max(dot(normal, lightVec), 0);
    float3 lightStrength = L.Strength * ndotl;

    float att = CalcAttenuation(d, L.FalloffEnd, L.FalloffStart);
    lightStrength *= att; //至今为止都和点光源一样

    float spotFactor = pow(max(dot(-lightVec, L.Direction), 0.0f), L.SpotPower);    //我们需要根据当前光向量方向和聚光灯中心光源的方向，来计算出来从中心到边缘的衰减, 其衰减为(L · center) ^ spotPower
    lightStrength *= spotFactor;    //然后我们根据衰减来计算最终的实际光强

    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

//计算多个光源时最终的光强
float4 ComputeLighting(Light gLights[MaxLights], Material mat, float3 pos, float3 normal, float3 toEye, float3 shadowFactor)
{
    float3 res = 0.0f;

    int i = 0;

#if (NUM_DIR_LIGHTS > 0)
    for (i = 0; i < NUM_DIR_LIGHTS; ++i)
        res += shadowFactor[i] * ComputeDirectionalLight(gLights[i], mat, normal, toEye);   //我们只有平行光才计算阴影, 不同的光源针对同一个材质，阴影参数可能是不同的，要看其不同的遮蔽率
#endif

#if (NUM_POINT_LIGHTS > 0)
    for (i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; ++i)
        res += ComputePointLight(gLights[i], mat, pos, normal, toEye);
#endif

#if (NUM_SPOT_LIGHTS > 0)
    for (i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; ++i)
        res += ComputeSpotLight(gLights[i], mat, pos, normal, toEye);
#endif

    return float4(res, 0.0f);
}