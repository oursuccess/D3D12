//Ssao copy from DX12, by Frank Luna
//要绘制Ssao, 我们需要计算表面的遮蔽率. 而要计算表面遮蔽率, 我们进行如下操作: 逐个像素找到该像素最接近相机的点p，计算其深度； 然后在其到相机的方向的半球上随机采样一些点，计算这些点与相机连成的方向上距离相机最近的点到相机的深度，判断这个深度和我们要检测的深度的差。 若差刚好落在遮蔽区间中，则增加遮蔽率，且增加幅度和深度差相关。
//法线用于判断随机采样的点是不是比p更接近相机

cbuffer cbSsao : register(b0)   //Ssao的常量缓冲区是单独的.
{
    float4x4 gProj;     //其中包含了投影矩阵，投影矩阵的逆矩阵，投影采样矩阵。 14个随机偏移向量. 3个Blur权重， 以及一些遮蔽率相关的深度值/半径
    float4x4 gInvProj;
    float4x4 gProjTex;
    float4 gOffsetVectors[14];

    float4 gBlurWeights[3];

    float2 gInvRenderTargetSize;

    float gOcculusinRadius;
    float gOcculusionFadeStart;
    float gOcculusionFadeEnd;
    float gSurfaceEpsilon;
}

cbuffer cbRootConstants : register(b1)
{
    bool gHorizontalBlur;   //是否为横向模糊. 在Ssao中未使用
}

Texture2D gNormalMap : register(t0);    //几个需要的纹理
Texture2D gDepthMap : register(t1);
Texture2D gRandomVecMap : register(t2);

SamplerState gsamPointClamp : register(s0);
SamplerState gsamLinearClamp : register(s1);
SamplerState gsamDepthMap : register(s2);
SamplerState gsamLinearWrap : register(s3);

static const int gSampleCount = 14; //采样点数量

static const float2 gTexCoords[6] =
{
    float2(0.0f, 1.0f), //我们将一个传入的像素扩展为一个正方形, 其中有两个三角形组成. 这里就是两个三角形的索引
    float2(0.0f, 0.0f), //第一个: 左下 -- 左上 --右上
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f), //第二个: 左下 -- 右上 --右下
    float2(1.0f, 0.0f), //两个三角形均为顺时针
    float2(1.0f, 1.0f),
};

struct VertexOut
{
    float4 PosH : SV_Position;
    float3 PosV : POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID)    //我们在绘制时的调用为： DrawInstanced(6, 1, 0, 0), 其中6指定了我们绘制了6个顶点! 刚好和我们扩展的正方形对应!
{
    VertexOut vout;
    vout.TexC = gTexCoords[vid];

    vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);   //计算出来对应的栅格化后的NDC投影坐标(从[0, 1]变到[-1, 1]中, 且需要注意y方向相反了)

    float4 ph = mul(vout.PosH, gInvProj);
    vout.PosV = ph.xyz / ph.w;  //从ndc空间变回到观察空间

    return vout;
}

float OcculusionFunction(float distZ)
{
    float occulusion = 0.0f;
    if (distZ > gSurfaceEpsilon)
    {
        float fadeLength = gOcculusionFadeEnd - gOcculusionFadeStart;
        occulusion = saturate((gOcculusionFadeEnd - distZ) / fadeLength);
    }

    return occulusion;
}

float NdcDepthToViewDepth(float z_ndc)
{
    //z_ndc = A + B / viewZ, A = gProj[2, 2], B = gProj[3, 2]
    float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
    return viewZ;
}

float4 PS(VertexOut pin) : SV_Target
{
    float3 n = gNormalMap.SampleLevel(gsamPointClamp, pin.TexC, 0.0f).xyz;  //计算法线
    float pz = gDepthMap.SampleLevel(gsamDepthMap, pin.TexC, 0.0f).r;   //计算该像素对应的深度
    pz = NdcDepthToViewDepth(pz);   //将深度变换到观察空间

    float3 p = (pz / pin.PosV.z) * pin.PosV;    //根据相似三角形原理, 计算出实际的从相机连接到该像素上距离相机最近点p的向量

    float3 randVec = 2.0f * gRandomVecMap.SampleLevel(gsamLinearWrap, 4.0f * pin.TexC, 0.0f).rgb - 1.0f;    //计算出随机采样向量, 并将其从uv空间变换到[-1, 1]. FIXME: 为什么这里是uv坐标乘了个4.0? 

    float occulusionSum = 0.0f;

    for (int i = 0; i < gSampleCount; ++i)
    {
        float3 offset = reflect(gOffsetVectors[i].xyz, randVec);    //计算采样的偏移点
        float flip = sign(dot(offset, n));  //如果采样的偏移刚好在p的背面, 则我们需要将其翻转

        float3 q = p + flip * gOcculusinRadius * offset;    //获取观察空间中的q, 但是此时q只是一个方向, 我们还不知道该方向上真正的最近点
        float4 projQ = mul(float4(q, 1.0f), gProjTex);  //将q变换到纹理空间, 准备在纹理空间中计算采样深度, 这样才能知道最近的点
        projQ /= projQ.w;

        float rz = gDepthMap.SampleLevel(gsamDepthMap, projQ.xy, 0.0f).r;   //计算采样深度
        rz = NdcDepthToViewDepth(rz);   //将q方向上采样得到的深度变换到观察空间中

        float3 r = (rz / q.z) * q;  //故技重施, 我们根据相似三角形得到q方向上实际的距离相机最近的点r

        float distZ = p.z - r.z;    //r现在最好是更靠近p的, 因此我们期望是p.z - r.z大于0
        float dp = max(dot(n, normalize(r - p)), 0.0f); //判断r到底有多大程度在p的法线的正前方. --FIXME: 为什么这里乘的是法线, 而不是p - gEyePosW?  不同的实现版本中不同, 在LearnOpenGL中的教程中并没有这个乘以n的步骤!
        float occulusion = dp * OcculusionFunction(distZ);  //r点的最终遮蔽率

        occulusionSum += occulusion;
    }

    occulusionSum /= gSampleCount;

    float access = 1.0f - occulusionSum;

    return saturate(pow(access, 2.0f)); //增强对比度， 从而提升戏剧性
}
